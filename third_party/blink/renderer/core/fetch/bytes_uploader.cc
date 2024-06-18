// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/bytes_uploader.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

BytesUploader::BytesUploader(
    ExecutionContext* execution_context,
    BytesConsumer* consumer,
    mojo::PendingReceiver<network::mojom::blink::ChunkedDataPipeGetter>
        pending_receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Client* client)
    : ExecutionContextLifecycleObserver(execution_context),
      consumer_(consumer),
      client_(client),
      receiver_(this, execution_context),
      upload_pipe_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                           task_runner) {
  DCHECK(consumer_);
  DCHECK_EQ(consumer_->GetPublicState(),
            BytesConsumer::PublicState::kReadableOrWaiting);

  receiver_.Bind(std::move(pending_receiver), std::move(task_runner));
}

BytesUploader::~BytesUploader() = default;

void BytesUploader::Trace(blink::Visitor* visitor) const {
  visitor->Trace(consumer_);
  visitor->Trace(client_);
  visitor->Trace(receiver_);
  BytesConsumer::Client::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void BytesUploader::GetSize(GetSizeCallback get_size_callback) {
  DCHECK(!get_size_callback_);
  get_size_callback_ = std::move(get_size_callback);
}

void BytesUploader::StartReading(
    mojo::ScopedDataPipeProducerHandle upload_pipe) {
  DVLOG(3) << this << " StartReading()";
  DCHECK(upload_pipe);
  if (!get_size_callback_ || upload_pipe_) {
    // When StartReading() is called while |upload_pipe_| is valid, it means
    // replay was asked by the network service.
    CloseOnError();
    return;
  }
  upload_pipe_ = std::move(upload_pipe);
  upload_pipe_watcher_.Watch(upload_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                             WTF::BindRepeating(&BytesUploader::OnPipeWriteable,
                                                WrapWeakPersistent(this)));
  consumer_->SetClient(this);
  if (consumer_->GetPublicState() ==
      BytesConsumer::PublicState::kReadableOrWaiting) {
    WriteDataOnPipe();
  }
}

void BytesUploader::ContextDestroyed() {
  CloseOnError();
  Dispose();
}

void BytesUploader::OnStateChange() {
  DVLOG(3) << this << " OnStateChange(). consumer_->GetPublicState()="
           << consumer_->GetPublicState();
  DCHECK(get_size_callback_);
  switch (consumer_->GetPublicState()) {
    case BytesConsumer::PublicState::kReadableOrWaiting:
      WriteDataOnPipe();
      return;
    case BytesConsumer::PublicState::kClosed:
      Close();
      return;
    case BytesConsumer::PublicState::kErrored:
      CloseOnError();
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void BytesUploader::OnPipeWriteable(MojoResult unused) {
  WriteDataOnPipe();
}

void BytesUploader::WriteDataOnPipe() {
  DVLOG(3) << this << " WriteDataOnPipe(). consumer_->GetPublicState()="
           << consumer_->GetPublicState();
  if (!upload_pipe_.is_valid())
    return;

  while (true) {
    const char* buffer;
    size_t available;
    auto consumer_result = consumer_->BeginRead(&buffer, &available);
    DVLOG(3) << "  consumer_->BeginRead()=" << consumer_result
             << ", available=" << available;
    switch (consumer_result) {
      case BytesConsumer::Result::kError:
        CloseOnError();
        return;
      case BytesConsumer::Result::kShouldWait:
        return;
      case BytesConsumer::Result::kDone:
        Close();
        return;
      case BytesConsumer::Result::kOk:
        break;
    }
    DCHECK_EQ(consumer_result, BytesConsumer::Result::kOk);
    // SAFETY: `BeginRead` promises to return a valid pointer and size.
    base::span<const char> chars =
        UNSAFE_BUFFERS(base::span(buffer, available));
    base::span<const uint8_t> bytes = base::as_bytes(chars);

    size_t actually_written_bytes = 0;
    const MojoResult mojo_result = upload_pipe_->WriteData(
        bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
    DVLOG(3) << "  upload_pipe_->WriteData()=" << mojo_result
             << ", mojo_written=" << actually_written_bytes;
    if (mojo_result == MOJO_RESULT_SHOULD_WAIT) {
      // Wait for the pipe to have more capacity available
      consumer_result = consumer_->EndRead(0);
      upload_pipe_watcher_.ArmOrNotify();
      return;
    }
    if (mojo_result != MOJO_RESULT_OK) {
      CloseOnError();
      return;
    }

    consumer_result = consumer_->EndRead(actually_written_bytes);
    DVLOG(3) << "  consumer_->EndRead()=" << consumer_result;

    if (!base::CheckAdd(total_size_, actually_written_bytes)
             .AssignIfValid(&total_size_)) {
      CloseOnError();
      return;
    }

    switch (consumer_result) {
      case BytesConsumer::Result::kError:
        CloseOnError();
        return;
      case BytesConsumer::Result::kShouldWait:
        NOTREACHED_IN_MIGRATION();
        return;
      case BytesConsumer::Result::kDone:
        Close();
        return;
      case BytesConsumer::Result::kOk:
        break;
    }
  }
}

void BytesUploader::Close() {
  DVLOG(3) << this << " Close(). total_size=" << total_size_;
  if (get_size_callback_)
    std::move(get_size_callback_).Run(net::OK, total_size_);
  consumer_->Cancel();
  if (Client* client = client_) {
    client_ = nullptr;
    client->OnComplete();
  }
  Dispose();
}

void BytesUploader::CloseOnError() {
  DVLOG(3) << this << " CloseOnError(). total_size=" << total_size_;
  if (get_size_callback_)
    std::move(get_size_callback_).Run(net::ERR_FAILED, total_size_);
  consumer_->Cancel();
  if (Client* client = client_) {
    client_ = nullptr;
    client->OnError();
  }
  Dispose();
}

void BytesUploader::Dispose() {
  receiver_.reset();
  upload_pipe_watcher_.Cancel();
}

}  // namespace blink
