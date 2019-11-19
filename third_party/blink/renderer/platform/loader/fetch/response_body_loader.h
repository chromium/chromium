// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_H_

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class ResponseBodyLoader;

// See ResponseBodyLoader for details. This is a virtual interface to expose
// only Drain functions.
class PLATFORM_EXPORT ResponseBodyLoaderDrainableInterface
    : public GarbageCollected<ResponseBodyLoaderDrainableInterface> {
 public:
  virtual ~ResponseBodyLoaderDrainableInterface() = default;

  // Drains the response body and returns it. This function must not be called
  // when the load has already been started or aborted, or the body has already
  // been drained. This function may return an invalid handle when it is
  // unable to convert the body to a data pipe, even when the body itself is
  // valid. In that case, this function is no-op. If this function returns a
  // valid handle, the caller is responsible for reading the body and providing
  // the information to the client this function provides.
  // Note that the notification from the client is *synchronously* propergated
  // to the original client ResponseBodyLoader owns,  e.g., when the caller
  // calls ResponseBodyLoaderClient::DidCancelLoadingBody, it synchronously
  // cancels the resource loading (if |this| is associated with
  // blink::ResourceLoader). A user of this function should ensure that calling
  // the client's method doesn't lead to a reentrant problem.
  virtual mojo::ScopedDataPipeConsumerHandle DrainAsDataPipe(
      ResponseBodyLoaderClient** client) = 0;

  // Drains the response body and returns it. This function must not be called
  // when the load has already been started or aborted, or the body has already
  // been drained. Unlike DrainAsDataPipe, this function always succeeds.
  // This ResponseBodyLoader will still monitor the loading signals, and report
  // them back to the associated client asynchronously.
  virtual BytesConsumer& DrainAsBytesConsumer() = 0;

  virtual void Trace(Visitor*) {}
};

// ResponseBodyLoader reads the response body and reports the contents to the
// associated client. There are two ways:
//  - By calling Start(), ResponseBodyLoader reads the response body. and
//    reports the contents to the client. Abort() aborts reading.
//  - By calling DrainAsDataPipe, a user can "drain" the contents from
//    ResponseBodyLoader. The caller is responsible for reading the body and
//    providing the information to the client this function provides.
//  - By calling DrainBytesConsumer, a user can "drain" the contents from
//    ResponseBodyLoader.
// A ResponseBodyLoader is bound to the thread on which it is created.
class PLATFORM_EXPORT ResponseBodyLoader final
    : public ResponseBodyLoaderDrainableInterface,
      private ResponseBodyLoaderClient,
      private BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ResponseBodyLoader);

 public:
  ResponseBodyLoader(BytesConsumer&,
                     ResponseBodyLoaderClient&,
                     scoped_refptr<base::SingleThreadTaskRunner>);

  // ResponseBodyLoaderDrainableInterface implementation.
  mojo::ScopedDataPipeConsumerHandle DrainAsDataPipe(
      ResponseBodyLoaderClient**) override;
  BytesConsumer& DrainAsBytesConsumer() override;

  // Starts loading.
  void Start();

  // Aborts loading. This is expected to be called from the client's side, and
  // does not report the failure to the client. This doesn't affect a
  // drained data pipe. This function cannot be called when suspended.
  void Abort();

  // Suspendes loading.
  void Suspend();

  // Resumes loading.
  void Resume();

  bool IsAborted() const { return aborted_; }
  bool IsSuspended() const { return suspended_; }
  bool IsDrained() const { return drained_; }

  void Trace(Visitor*) override;

  // The maximal number of bytes consumed in a task. When there are more bytes
  // in the data pipe, they will be consumed in following tasks. Setting a too
  // small number will generate ton of tasks but setting a too large number will
  // lead to thread janks. Also, some clients cannot handle too large chunks
  // (512k for example).
  static constexpr size_t kMaxNumConsumedBytesInTask = 64 * 1024;

 private:
  class DelegatingBytesConsumer;

  // ResponseBodyLoaderClient implementation.
  void DidReceiveData(base::span<const char> data) override;
  void DidFinishLoadingBody() override;
  void DidFailLoadingBody() override;
  void DidCancelLoadingBody() override;

  // BytesConsumer::Client implementation.
  void OnStateChange() override;
  String DebugName() const override { return "ResponseBodyLoader"; }

  Member<BytesConsumer> bytes_consumer_;
  Member<DelegatingBytesConsumer> delegating_bytes_consumer_;
  const Member<ResponseBodyLoaderClient> client_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool started_ = false;
  bool aborted_ = false;
  bool suspended_ = false;
  bool drained_ = false;
  bool finish_signal_is_pending_ = false;
  bool fail_signal_is_pending_ = false;
  bool cancel_signal_is_pending_ = false;
  bool in_two_phase_read_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESPONSE_BODY_LOADER_H_
