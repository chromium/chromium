// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_PIPE_BYTES_CONSUMER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_PIPE_BYTES_CONSUMER_H_

#include <memory>

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;

class CORE_EXPORT DataPipeBytesConsumer final : public BytesConsumer {
 public:
  DataPipeBytesConsumer(ExecutionContext*, mojo::ScopedDataPipeConsumerHandle);
  ~DataPipeBytesConsumer() override;

  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  mojo::ScopedDataPipeConsumerHandle DrainAsDataPipe() override;
  void SetClient(BytesConsumer::Client*) override;
  void ClearClient() override;

  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override {
    DCHECK(state_ == InternalState::kErrored);
    return error_;
  }
  String DebugName() const override { return "DataPipeBytesConsumer"; }

  void Trace(blink::Visitor*) override;

  // One of these methods must be called to signal the end of the data
  // stream.  We cannot assume that the end of the pipe completes the
  // stream successfully since errors can occur after the last byte is
  // written into the pipe.
  void SignalComplete();
  void SignalError();

 private:
  bool IsReadableOrWaiting() const;
  void MaybeClose();
  void SetError();
  void Notify(MojoResult);
  void ClearDataPipe();

  Member<ExecutionContext> execution_context_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher watcher_;
  Member<BytesConsumer::Client> client_;
  InternalState state_ = InternalState::kWaiting;
  Error error_;
  bool is_in_two_phase_read_ = false;
  bool has_pending_notification_ = false;
  bool has_pending_complete_ = false;
  bool has_pending_error_ = false;
  bool completion_signaled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_DATA_PIPE_BYTES_CONSUMER_H_
