// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_encoded_source.h"

#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_encoded_source_event.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/webrtc/api/encoded_video_frame_injector_interface.h"

namespace blink {

namespace {

void InitializeVideoSinkAndFireEvent(
    CrossThreadHandle<RTCRtpSenderEncodedSource> source_handle,
    scoped_refptr<webrtc::EncodedVideoFrameInjectorInterface> injector,
    CrossThreadHandle<ScriptPromiseResolver<IDLUndefined>> resolver_handle,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner) {
  auto* source =
      MakeUnwrappingCrossThreadHandle(source_handle).GetOnCreationThread();
  CHECK(source);

  source->InitializeVideoSink(std::move(injector));

  auto* global_scope =
      To<DedicatedWorkerGlobalScope>(source->GetExecutionContext());
  auto* event = MakeGarbageCollected<RTCRtpSenderEncodedSourceEvent>(source);
  global_scope->DispatchEvent(*event);

  // resolve promise on main thread
  PostCrossThreadTask(
      *main_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](CrossThreadHandle<ScriptPromiseResolver<IDLUndefined>>
                 resolver_handle) {
            auto* resolver = MakeUnwrappingCrossThreadHandle(resolver_handle)
                                 .GetOnCreationThread();
            if (resolver) {
              resolver->Resolve();
            }
          },
          std::move(resolver_handle)));
}

void SetVideoFrameInjector(
    CrossThreadWeakHandle<RTCRtpSender> weak_sender,
    webrtc::KeyFrameCallback keyframe_callback,
    webrtc::BitrateInfoCallback bitrate_callback,
    scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner,
    CrossThreadHandle<RTCRtpSenderEncodedSource> source_handle,
    CrossThreadHandle<ScriptPromiseResolver<IDLUndefined>> resolver_handle) {
  auto* rtp_sender =
      MakeUnwrappingCrossThreadWeakHandle(weak_sender).GetOnCreationThread();
  auto* resolver =
      MakeUnwrappingCrossThreadHandle(resolver_handle).GetOnCreationThread();
  if (!rtp_sender) {
    if (resolver) {
      resolver->Reject(
          DOMException::Create("Sender destroyed", "InvalidStateError"));
    }
    return;
  }

  scoped_refptr<webrtc::EncodedVideoFrameInjectorInterface> injector =
      rtp_sender->CreateEncodedVideoFrameInjector(std::move(keyframe_callback),
                                                  std::move(bitrate_callback));
  if (!injector) {
    if (resolver) {
      resolver->Reject(
          DOMException::Create("Failed to create injector", "OperationError"));
    }
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner =
      rtp_sender->GetExecutionContext()->GetTaskRunner(
          TaskType::kInternalMediaRealTime);

  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(&InitializeVideoSinkAndFireEvent,
                          std::move(source_handle), std::move(injector),
                          std::move(resolver_handle),
                          std::move(main_task_runner)));
}

}  // namespace

Event* RTCRtpSenderEncodedSource::CreateVideoEncodedSource(
    CrossThreadWeakHandle<RTCRtpSender> weak_sender,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    CrossThreadHandle<ScriptPromiseResolver<IDLUndefined>> resolver_handle,
    ScriptState* worker_script_state,
    CustomEventMessage data) {
  CHECK(ExecutionContext::From(worker_script_state)->IsContextThread());
  auto* source = MakeGarbageCollected<RTCRtpSenderEncodedSource>(
      worker_script_state, "video");

  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner =
      ExecutionContext::From(worker_script_state)
          ->GetTaskRunner(TaskType::kInternalMediaRealTime);

  webrtc::KeyFrameCallback keyframe_callback =
      [worker_task_runner,
       source_handle = MakeCrossThreadWeakHandle(source)]() {
        PostCrossThreadTask(
            *worker_task_runner, FROM_HERE,
            CrossThreadBindOnce(
                [](CrossThreadWeakHandle<RTCRtpSenderEncodedSource> handle) {
                  if (auto* source = MakeUnwrappingCrossThreadWeakHandle(handle)
                                         .GetOnCreationThread()) {
                    source->HandleKeyFrameRequest();
                  }
                },
                source_handle));
      };

  webrtc::BitrateInfoCallback bitrate_callback =
      [worker_task_runner, source_handle = MakeCrossThreadWeakHandle(source)](
          int32_t allocated_bitrate, int32_t available_outgoing_bitrate) {
        PostCrossThreadTask(
            *worker_task_runner, FROM_HERE,
            CrossThreadBindOnce(
                [](CrossThreadWeakHandle<RTCRtpSenderEncodedSource> handle,
                   int32_t allocated_bitrate,
                   int32_t available_outgoing_bitrate) {
                  if (auto* source = MakeUnwrappingCrossThreadWeakHandle(handle)
                                         .GetOnCreationThread()) {
                    source->HandleBitrateInfoChange(allocated_bitrate,
                                                    available_outgoing_bitrate);
                  }
                },
                source_handle, allocated_bitrate, available_outgoing_bitrate));
      };

  // set frame injector on main thread
  PostCrossThreadTask(
      *main_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &SetVideoFrameInjector, std::move(weak_sender),
          std::move(keyframe_callback), std::move(bitrate_callback),
          std::move(worker_task_runner), MakeCrossThreadHandle(source),
          std::move(resolver_handle)));

  // RTCRtpSenderEncodedSourceEvent will be fired asynchronously after the
  // frame injector is successfully set.
  return nullptr;
}

RTCRtpSenderEncodedSource::RTCRtpSenderEncodedSource(ScriptState* script_state,
                                                     const String& kind)
    : execution_context_(ExecutionContext::From(script_state)) {}

void RTCRtpSenderEncodedSource::InitializeVideoSink(
    scoped_refptr<webrtc::EncodedVideoFrameInjectorInterface> injector) {
  auto* global_scope = To<DedicatedWorkerGlobalScope>(execution_context_.Get());
  ScriptState* script_state =
      global_scope->ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  auto* underlying_sink = MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
      script_state, std::move(injector), this,
      /*detach_frame_data_on_write=*/false);

  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, /*high_water_mark*/ 1);
}

void RTCRtpSenderEncodedSource::Trace(Visitor* visitor) const {
  visitor->Trace(writable_);
  visitor->Trace(execution_context_);
  EventTarget::Trace(visitor);
}

void RTCRtpSenderEncodedSource::HandleBitrateInfoChange(
    int32_t allocated_bitrate,
    int32_t available_outgoing_bitrate) {
  if (allocated_bitrate_ == allocated_bitrate &&
      available_outgoing_bitrate_ == available_outgoing_bitrate) {
    return;
  }
  allocated_bitrate_ = allocated_bitrate;
  available_outgoing_bitrate_ = available_outgoing_bitrate;
  DispatchEvent(*Event::Create(event_type_names::kBitrateinfochange));
}

void RTCRtpSenderEncodedSource::HandleKeyFrameRequest() {
  DispatchEvent(*Event::Create(event_type_names::kKeyframerequest));
}

}  // namespace blink
