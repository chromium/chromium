// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtp_contributing_source_cache.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
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
  MaybeUpdateRtpSources(receiver->kind());
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
  MaybeUpdateRtpSources(receiver->kind());
  return RTCRtpContributingSourcesFromRTCRtpSources(script_state,
                                                    GetRtpSources(receiver));
}

void RtpContributingSourceCache::MaybeUpdateRtpSources(
    RTCRtpReceiver::MediaKind kind) {
  bool* cache_is_obsolete;
  HashMap<RTCRtpReceiverPlatform*, RTCRtpSources>* cached_sources_by_receiver;
  switch (kind) {
    case RTCRtpReceiver::MediaKind::kAudio:
      cache_is_obsolete = &audio_cache_is_obsolete_;
      cached_sources_by_receiver = &cached_sources_by_audio_receiver_;
      break;
    case RTCRtpReceiver::MediaKind::kVideo:
      cache_is_obsolete = &video_cache_is_obsolete_;
      cached_sources_by_receiver = &cached_sources_by_video_receiver_;
      break;
  }
  if (!*cache_is_obsolete || !pc_) {
    return;
  }

  // Clear and refresh the cache for all RTCRtpReceiver objects in a single
  // block-invoke to the WebRTC worker thread. This increases the overhead of a
  // single getSynchronizationSources/getContributingSources call, but we expect
  // an application that calls these methods on one RTCRtpReceiver to do it on
  // every RTCRtpReceiver of the same kind. In that case, this heuristic brings
  // down the number of block-invokes from 1 per receiver to 1 per kind.
  cached_sources_by_receiver->clear();
  Vector<RTCRtpReceiverPlatform*> receivers;
  for (const Member<RTCRtpReceiver>& receiver : pc_->getReceivers()) {
    if (receiver->kind() != kind)
      continue;
    receivers.push_back(receiver->platform_receiver());
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

  *cache_is_obsolete = false;
  Microtask::EnqueueMicrotask(
      WTF::Bind(&RtpContributingSourceCache::MakeCacheObsolete,
                weak_factory_.GetWeakPtr()));
}

void RtpContributingSourceCache::UpdateRtpSourcesOnWorkerThread(
    Vector<RTCRtpReceiverPlatform*>* receivers,
    HashMap<RTCRtpReceiverPlatform*, RTCRtpSources>* cached_sources_by_receiver,
    base::WaitableEvent* event) {
  // Calling GetSources() while on the worker thread avoids a per-receiver
  // block-invoke inside the webrtc::RtpReceiverInterface PROXY.
  for (RTCRtpReceiverPlatform* receiver : *receivers) {
    cached_sources_by_receiver->insert(receiver, receiver->GetSources());
  }
  event->Signal();
}

void RtpContributingSourceCache::MakeCacheObsolete() {
  audio_cache_is_obsolete_ = true;
  video_cache_is_obsolete_ = true;
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
