/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_sender.h"

#include <memory>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_dtmf_tone_change_event.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_dtmf_sender_handler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

static const int kMinToneDurationMs = 40;
static const int kDefaultToneDurationMs = 100;
static const int kMaxToneDurationMs = 6000;
static const int kMinInterToneGapMs = 30;
static const int kMaxInterToneGapMs = 6000;
static const int kDefaultInterToneGapMs = 70;

RTCDTMFSender* RTCDTMFSender::Create(
    ExecutionContext* context,
    std::unique_ptr<RtcDtmfSenderHandler> dtmf_sender_handler) {
  DCHECK(dtmf_sender_handler);
  return MakeGarbageCollected<RTCDTMFSender>(context,
                                             std::move(dtmf_sender_handler));
}

RTCDTMFSender::RTCDTMFSender(ExecutionContext* context,
                             std::unique_ptr<RtcDtmfSenderHandler> handler)
    : ExecutionContextLifecycleObserver(context),
      handler_(std::move(handler)),
      stopped_(false) {
  handler_->SetClient(this);
}

RTCDTMFSender::~RTCDTMFSender() = default;

void RTCDTMFSender::Dispose() {
  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  handler_->SetClient(nullptr);
  handler_.reset();
}

bool RTCDTMFSender::canInsertDTMF() const {
  return handler_->CanInsertDTMF();
}

String RTCDTMFSender::toneBuffer() const {
  return tone_buffer_;
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               ExceptionState& exception_state) {
  insertDTMF(tones, kDefaultToneDurationMs, kDefaultInterToneGapMs,
             exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               ExceptionState& exception_state) {
  insertDTMF(tones, duration, kDefaultInterToneGapMs, exception_state);
}

void RTCDTMFSender::insertDTMF(const String& tones,
                               int duration,
                               int inter_tone_gap,
                               ExceptionState& exception_state) {
  // https://w3c.github.io/webrtc-pc/#dom-rtcdtmfsender-insertdtmf
  if (!canInsertDTMF()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The 'canInsertDTMF' attribute is false: "
                                      "this sender cannot send DTMF.");
    return;
  }
  // Spec: Throw on illegal characters
  if (strspn(tones.Ascii().c_str(), "0123456789abcdABCD#*,") !=
      tones.length()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "Illegal characters in InsertDTMF tone argument");
    return;
  }

  // Spec: Clamp the duration to between 40 and 6000 ms
  duration_ = std::max(duration, kMinToneDurationMs);
  duration_ = std::min(duration_, kMaxToneDurationMs);
  // Spec: Clamp the inter-tone gap to between 30 and 6000 ms
  inter_tone_gap_ = std::max(inter_tone_gap, kMinInterToneGapMs);
  inter_tone_gap_ = std::min(inter_tone_gap_, kMaxInterToneGapMs);

  // Spec: a-d should be represented in the tone buffer as A-D
  tone_buffer_ = tones.UpperASCII();

  if (tone_buffer_.empty()) {
    return;
  }
  if (!playout_task_is_scheduled_) {
    playout_task_is_scheduled_ = true;
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE, WTF::BindOnce(&RTCDTMFSender::PlayoutTask,
                                            WrapPersistent(this)));
  }
}

void RTCDTMFSender::PlayoutTask() {
  playout_task_is_scheduled_ = false;
  // TODO(crbug.com/891638): Add check on transceiver's "stopped"
  // and "currentDirection" attributes as per spec.
  if (tone_buffer_.empty()) {
    Member<Event> event = MakeGarbageCollected<RTCDTMFToneChangeEvent>("");
    DispatchEvent(*event.Release());
    return;
  }
  String this_tone = tone_buffer_.Substring(0, 1);
  tone_buffer_ = tone_buffer_.Substring(1, tone_buffer_.length() - 1);
  // InsertDTMF handles both tones and ",", and calls DidPlayTone after
  // the specified delay.
  if (!handler_->InsertDTMF(this_tone, duration_, inter_tone_gap_)) {
    LOG(ERROR) << "DTMF: Could not send provided tone, '" << this_tone.Ascii()
               << "'.";
    return;
  }
  playout_task_is_scheduled_ = true;
  Member<Event> event = MakeGarbageCollected<RTCDTMFToneChangeEvent>(this_tone);
  DispatchEvent(*event.Release());
}

void RTCDTMFSender::DidPlayTone(const String& tone) {
  // We're using the DidPlayTone with an empty buffer to signal the
  // end of the tone.
  if (tone.empty()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kNetworking)
        ->PostDelayedTask(
            FROM_HERE,
            WTF::BindOnce(&RTCDTMFSender::PlayoutTask, WrapPersistent(this)),
            base::Milliseconds(inter_tone_gap_));
  }
}

const AtomicString& RTCDTMFSender::InterfaceName() const {
  return event_target_names::kRTCDTMFSender;
}

ExecutionContext* RTCDTMFSender::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void RTCDTMFSender::ContextDestroyed() {
  stopped_ = true;
  handler_->SetClient(nullptr);
}

void RTCDTMFSender::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  RtcDtmfSenderHandler::Client::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
