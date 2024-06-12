// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_producer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace mojo {

namespace {

// No good reason not to attempt very large pipe transactions in case the data
// pipe in use has a very large capacity available, so we default to trying
// 64 MB chunks whenever a producer is writable.
constexpr size_t kDefaultMaxReadSize = 64 * 1024 * 1024;

}  // namespace

class DataPipeProducer::SequenceState
    : public base::RefCountedDeleteOnSequence<SequenceState> {
 public:
  using CompletionCallback =
      base::OnceCallback<void(ScopedDataPipeProducerHandle producer,
                              MojoResult result)>;

  SequenceState(ScopedDataPipeProducerHandle producer_handle,
                scoped_refptr<base::SequencedTaskRunner> file_task_runner,
                CompletionCallback callback,
                scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : base::RefCountedDeleteOnSequence<SequenceState>(
            std::move(file_task_runner)),
        callback_task_runner_(std::move(callback_task_runner)),
        producer_handle_(std::move(producer_handle)),
        callback_(std::move(callback)) {}

  SequenceState(const SequenceState&) = delete;
  SequenceState& operator=(const SequenceState&) = delete;

  void Cancel() {
    base::AutoLock lock(lock_);
    is_cancelled_ = true;
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&SequenceState::CancelOnSequence, this));
  }

  void Start(std::unique_ptr<DataSource> data_source) {
    owning_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&SequenceState::StartOnSequence, this,
                                  std::move(data_source)));
  }

 private:
  friend class base::DeleteHelper<SequenceState>;
  friend class base::RefCountedDeleteOnSequence<SequenceState>;

  ~SequenceState() = default;

  void StartOnSequence(std::unique_ptr<DataSource> data_source) {
    data_source_ = std::move(data_source);
    TransferSomeBytes();
    if (producer_handle_.is_valid()) {
      // If we didn't nail it all on the first transaction attempt, setup a
      // watcher and complete the read asynchronously.
      watcher_ = std::make_unique<SimpleWatcher>(
          FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC,
          base::SequencedTaskRunner::GetCurrentDefault());
      watcher_->Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                      MOJO_WATCH_CONDITION_SATISFIED,
                      base::BindRepeating(&SequenceState::OnHandleReady, this));
    }
  }

  void OnHandleReady(MojoResult result, const HandleSignalsState& state) {
    {
      // Stop ourselves from doing redundant work if we've been cancelled from
      // another thread. Note that we do not rely on this for any kind of thread
      // safety concerns.
      base::AutoLock lock(lock_);
      if (is_cancelled_)
        return;
    }

    if (result != MOJO_RESULT_OK) {
      // Either the consumer pipe has been closed or something terrible
      // happened. In any case, we'll never be able to write more data.
      data_source_->Abort();
      Finish(result);
      return;
    }

    TransferSomeBytes();
  }

  void TransferSomeBytes() {
    while (true) {
      DCHECK_LE(bytes_transferred_, data_source_->GetLength());
      const uint64_t max_data_size =
          data_source_->GetLength() - bytes_transferred_;
      if (max_data_size == 0) {
        // There's no more data to transfer.
        Finish(MOJO_RESULT_OK);
        return;
      }

      size_t size_hint = kDefaultMaxReadSize;
      if (static_cast<uint64_t>(size_hint) > max_data_size) {
        size_hint = static_cast<size_t>(max_data_size);
      }

      base::span<uint8_t> pipe_buffer;
      MojoResult mojo_result = producer_handle_->BeginWriteData(
          size_hint, MOJO_WRITE_DATA_FLAG_NONE, pipe_buffer);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT)
        return;
      if (mojo_result != MOJO_RESULT_OK) {
        data_source_->Abort();
        Finish(mojo_result);
        return;
      }
      DataSource::ReadResult result = data_source_->Read(
          bytes_transferred_, base::as_writable_chars(pipe_buffer));
      producer_handle_->EndWriteData(result.bytes_read);
      // result.bytes_read == 0 is used to determine if the read operation did
      // not retrieve any bytes, which typically occurs when reaching the end of
      // the file (EOF).
      if (result.result != MOJO_RESULT_OK || result.bytes_read == 0) {
        Finish(result.result);
        return;
      }

      bytes_transferred_ += result.bytes_read;
    }
  }

  void Finish(MojoResult result) {
    watcher_.reset();
    data_source_.reset();
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  std::move(producer_handle_), result));
  }

  void CancelOnSequence() {
    if (!data_source_)
      return;
    data_source_->Abort();
    Finish(MOJO_RESULT_CANCELLED);
  }

  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // State which is effectively owned and used only on the file sequence.
  ScopedDataPipeProducerHandle producer_handle_;
  std::unique_ptr<DataPipeProducer::DataSource> data_source_;
  size_t bytes_transferred_ = 0;
  CompletionCallback callback_;
  std::unique_ptr<SimpleWatcher> watcher_;

  base::Lock lock_;
  bool is_cancelled_ GUARDED_BY(lock_) = false;
};

DataPipeProducer::DataPipeProducer(ScopedDataPipeProducerHandle producer)
    : producer_(std::move(producer)) {}

DataPipeProducer::~DataPipeProducer() {
  if (sequence_state_)
    sequence_state_->Cancel();
}

void DataPipeProducer::Write(std::unique_ptr<DataSource> data_source,
                             CompletionCallback callback) {
  InitializeNewRequest(std::move(callback));
  sequence_state_->Start(std::move(data_source));
}

void DataPipeProducer::InitializeNewRequest(CompletionCallback callback) {
  DCHECK(!sequence_state_);
  // TODO(crbug.com/41436919): Re-evaluate how TaskPriority is set here and in
  // other file URL-loading-related code. Some callers require USER_VISIBLE
  // (i.e., BEST_EFFORT is not enough).
  auto file_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  sequence_state_ = new SequenceState(
      std::move(producer_), file_task_runner,
      base::BindOnce(&DataPipeProducer::OnWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      base::SequencedTaskRunner::GetCurrentDefault());
}

void DataPipeProducer::OnWriteComplete(CompletionCallback callback,
                                       ScopedDataPipeProducerHandle producer,
                                       MojoResult ready_result) {
  producer_ = std::move(producer);
  sequence_state_ = nullptr;
  std::move(callback).Run(ready_result);
}

const DataPipeProducerHandle& DataPipeProducer::GetProducerHandle() const {
  return producer_.get();
}

}  // namespace mojo
