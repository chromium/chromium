// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtp_contributing_source_cache.h"

#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

HeapVector<Member<RTCRtpSynchronizationSource>>
RTCRtpSynchronizationSourcesFromRTCRtpSources(
    ScriptState* script_state,
    const RtpContributingSourceCache::RTCRtpSources* rtp_sources) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentLoadTiming& time_converter =
      window->GetFrame()->Loader().GetDocumentLoader()->GetTiming();

  HeapVector<Member<RTCRtpSynchronizationSource>> synchronization_sources;
  if (!rtp_sources)
    return synchronization_sources;
  for (const auto& rtp_source : *rtp_sources) {
    if (rtp_source->SourceType() != RTCRtpSource::Type::kSSRC)
      continue;
    RTCRtpSynchronizationSource* synchronization_source =
        MakeGarbageCollected<RTCRtpSynchronizationSource>();
    synchronization_source->setTimestamp(
        time_converter.MonotonicTimeToPseudoWallTime(rtp_source->Timestamp())
            .InMilliseconds());
    synchronization_source->setSource(rtp_source->Source());
    if (rtp_source->AudioLevel().has_value()) {
      synchronization_source->setAudioLevel(rtp_source->AudioLevel().value());
    }
    if (rtp_source->CaptureTimestamp().has_value()) {
      synchronization_source->setCaptureTimestamp(
          rtp_source->CaptureTimestamp().value());
    }
    if (rtp_source->SenderCaptureTimeOffset().has_value()) {
      synchronization_source->setSenderCaptureTimeOffset(
          rtp_source->SenderCaptureTimeOffset().value());
    }
    synchronization_source->setRtpTimestamp(rtp_source->RtpTimestamp());
    synchronization_sources.push_back(synchronization_source);
  }
  return synchronization_sources;
}

HeapVector<Member<RTCRtpContributingSource>>
RTCRtpContributingSourcesFromRTCRtpSources(
    ScriptState* script_state,
    const RtpContributingSourceCache::RTCRtpSources* rtp_sources) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  DocumentLoadTiming& time_converter =
      window->GetFrame()->Loader().GetDocumentLoader()->GetTiming();

  HeapVector<Member<RTCRtpContributingSource>> contributing_sources;
  if (!rtp_sources)
    return contributing_sources;
  for (const auto& rtp_source : *rtp_sources) {
    if (rtp_source->SourceType() != RTCRtpSource::Type::kCSRC)
      continue;
    RTCRtpContributingSource* contributing_source =
        MakeGarbageCollected<RTCRtpContributingSource>();
    contributing_source->setTimestamp(
        time_converter.MonotonicTimeToPseudoWallTime(rtp_source->Timestamp())
            .InMilliseconds());
    contributing_source->setSource(rtp_source->Source());
    if (rtp_source->AudioLevel().has_value()) {
      contributing_source->setAudioLevel(rtp_source->AudioLevel().value());
    }
    if (rtp_source->CaptureTimestamp().has_value()) {
      contributing_source->setCaptureTimestamp(
          rtp_source->CaptureTimestamp().value());
    }
    if (rtp_source->SenderCaptureTimeOffset().has_value()) {
      contributing_source->setSenderCaptureTimeOffset(
          rtp_source->SenderCaptureTimeOffset().value());
    }
    contributing_source->setRtpTimestamp(rtp_source->RtpTimestamp());
    contributing_sources.push_back(contributing_source);
  }
  return contributing_sources;
}

}  // namespace

RtpContributingSourceCache::RtpContributingSourceCache(
    RTCPeerConnection* pc,
    scoped_refptr<base::SingleThreadTaskRunner> worker_thread_runner)
    : pc_(pc), worker_thread_runner_(worker_thread_runner) {
  DCHECK(pc_);
  DCHECK(worker_thread_runner_);
}

void RtpContributingSourceCache::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
}

HeapVector<Member<RTCRtpSynchronizationSource>>
RtpContributingSourceCache::getSynchronizationSources(
    ScriptState* script_state,
    ExceptionState& exception_state,
    RTCRtpReceiver* receiver) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached");
    return HeapVector<Member<RTCRtpSynchronizationSource>>();
  }
  MaybeUpdateRtpSources(script_state, receiver);
  return RTCRtpSynchronizationSourcesFromRTCRtpSources(script_state,
                                                       GetRtpSources(receiver));
}

HeapVector<Member<RTCRtpContributingSource>>
RtpContributingSourceCache::getContributingSources(
    ScriptState* script_state,
    ExceptionState& exception_state,
    RTCRtpReceiver* receiver) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Window is detached");
    return HeapVector<Member<RTCRtpContributingSource>>();
  }
  MaybeUpdateRtpSources(script_state, receiver);
  return RTCRtpContributingSourcesFromRTCRtpSources(script_state,
                                                    GetRtpSources(receiver));
}

void RtpContributingSourceCache::MaybeUpdateRtpSources(
    ScriptState* script_state,
    RTCRtpReceiver* requesting_receiver) {
  if (!pc_) {
    return;
  }
  HashMap<RTCRtpReceiverPlatform*, RTCRtpSources>* cached_sources_by_receiver;
  switch (requesting_receiver->kind()) {
    case RTCRtpReceiver::MediaKind::kAudio:
      cached_sources_by_receiver = &cached_sources_by_audio_receiver_;
      break;
    case RTCRtpReceiver::MediaKind::kVideo:
      cached_sources_by_receiver = &cached_sources_by_video_receiver_;
      break;
  }
  if (cached_sources_by_receiver->find(
          requesting_receiver->platform_receiver()) !=
      cached_sources_by_receiver->end()) {
    // The sources are already cached for this receiver, no action needed.
    return;
  }

  // Receivers whose cache to update.
  Vector<RTCRtpReceiverPlatform*> receivers;
  if (cached_sources_by_receiver->empty()) {
    // If the cache is empty then we only update the cache for this one
    // receiver. This avoids updating the cache for all receivers in cases where
    // the app is only interested in a single receiver per kind.
    receivers.push_back(requesting_receiver->platform_receiver());
  } else {
    // If the cache is not empty, the app is interested in multiple
    // RTCRtpReceiver objects. In this case, pay the cost up-front to update the
    // cache for all receivers of this kind under the assumption that the app
    // will be interested in all receivers of this kind. This heuristic limits
    // the number of block-invoke in common use cases, but may increase overhead
    // in edge cases where a subset of receivers are polled per microtask.
    for (const Member<RTCRtpReceiver>& receiver : pc_->getReceivers()) {
      if (receiver->kind() != requesting_receiver->kind())
        continue;
      receivers.push_back(receiver->platform_receiver());
    }
  }
  base::WaitableEvent event;
  // Unretained is safe because we're waiting for the operation to complete.
  PostCrossThreadTask(
      *worker_thread_runner_, FROM_HERE,
      WTF::CrossThreadBindOnce(
          &RtpContributingSourceCache::UpdateRtpSourcesOnWorkerThread,
          WTF::CrossThreadUnretained(this),
          WTF::CrossThreadUnretained(&receivers),
          WTF::CrossThreadUnretained(cached_sources_by_receiver),
          WTF::CrossThreadUnretained(&event)));
  event.Wait();

  ExecutionContext::From(script_state)
      ->GetAgent()
      ->event_loop()
      ->EnqueueMicrotask(WTF::BindOnce(&RtpContributingSourceCache::ClearCache,
                                       weak_factory_.GetWeakPtr()));
}

void RtpContributingSourceCache::UpdateRtpSourcesOnWorkerThread(
    Vector<RTCRtpReceiverPlatform*>* receivers,
    HashMap<RTCRtpReceiverPlatform*, RTCRtpSources>* cached_sources_by_receiver,
    base::WaitableEvent* event) {
  // Calling GetSources() while on the worker thread avoids a per-receiver
  // block-invoke inside the webrtc::RtpReceiverInterface PROXY.
  for (RTCRtpReceiverPlatform* receiver : *receivers) {
    if (cached_sources_by_receiver->find(receiver) ==
        cached_sources_by_receiver->end()) {
      cached_sources_by_receiver->insert(receiver, receiver->GetSources());
    }
  }
  event->Signal();
}

void RtpContributingSourceCache::ClearCache() {
  cached_sources_by_audio_receiver_.clear();
  cached_sources_by_video_receiver_.clear();
}

const RtpContributingSourceCache::RTCRtpSources*
RtpContributingSourceCache::GetRtpSources(RTCRtpReceiver* receiver) const {
  const HashMap<RTCRtpReceiverPlatform*, RTCRtpSources>*
      cached_sources_by_receiver =
          receiver->kind() == RTCRtpReceiver::MediaKind::kAudio
              ? &cached_sources_by_audio_receiver_
              : &cached_sources_by_video_receiver_;
  auto it = cached_sources_by_receiver->find(receiver->platform_receiver());
  if (it == cached_sources_by_receiver->end())
    return nullptr;
  return &it->value;
}

}  // namespace blink
