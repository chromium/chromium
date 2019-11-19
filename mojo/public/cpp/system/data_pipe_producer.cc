// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_producer.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace mojo {

namespace {

// No good reason not to attempt very large pipe transactions in case the data
// pipe in use has a very large capacity available, so we default to trying
// 64 MB chunks whenever a producer is writable.
constexpr uint32_t kDefaultMaxReadSize = 64 * 1024 * 1024;

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

  void Cancel() {
    base::AutoLock lock(lock_);
    is_cancelled_ = true;
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
          base::SequencedTaskRunnerHandle::Get());
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
      // Lock as much of the pipe as we can.
      void* pipe_buffer;
      uint32_t size = kDefaultMaxReadSize;
      uint64_t max_data_size = data_source_->GetLength();
      if (static_cast<uint64_t>(size) > max_data_size)
        size = static_cast<uint32_t>(max_data_size);

      MojoResult mojo_result = producer_handle_->BeginWriteData(
          &pipe_buffer, &size, MOJO_WRITE_DATA_FLAG_NONE);
      if (mojo_result == MOJO_RESULT_SHOULD_WAIT)
        return;
      if (mojo_result != MOJO_RESULT_OK) {
        data_source_->Abort();
        Finish(mojo_result);
        return;
      }
      base::span<char> read_buffer(static_cast<char*>(pipe_buffer), size);

      DataSource::ReadResult result =
          data_source_->Read(bytes_transferred_, read_buffer);
      producer_handle_->EndWriteData(result.bytes_read);

      if (result.result != MOJO_RESULT_OK) {
        Finish(result.result);
        return;
      }

      bytes_transferred_ += result.bytes_read;

      if (result.bytes_read < read_buffer.size()) {
        // DataSource::Read makes a best effort to read all requested bytes. We
        // reasonably assume if it fails to read what we ask for, we've hit EOF.
        Finish(MOJO_RESULT_OK);
        return;
      }
    }
  }

  void Finish(MojoResult result) {
    watcher_.reset();
    data_source_.reset();
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  std::move(producer_handle_), result));
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

  DISALLOW_COPY_AND_ASSIGN(SequenceState);
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
  // TODO(crbug.com/924416): Re-evaluate how TaskPriority is set here and in
  // other file URL-loading-related code. Some callers require USER_VISIBLE
  // (i.e., BEST_EFFORT is not enough).
  auto file_task_runner = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  sequence_state_ = new SequenceState(
      std::move(producer_), file_task_runner,
      base::BindOnce(&DataPipeProducer::OnWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      base::SequencedTaskRunnerHandle::Get());
}

void DataPipeProducer::OnWriteComplete(CompletionCallback callback,
                                       ScopedDataPipeProducerHandle producer,
                                       MojoResult ready_result) {
  producer_ = std::move(producer);
  sequence_state_ = nullptr;
  std::move(callback).Run(ready_result);
}

}  // namespace mojo
