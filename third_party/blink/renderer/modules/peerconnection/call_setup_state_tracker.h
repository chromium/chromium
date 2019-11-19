// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_CALL_SETUP_STATE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_CALL_SETUP_STATE_TRACKER_H_

#include <utility>

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

// Represents the different states that an offerer can go through during call
// setup, where later steps involve SDP exchange.
//
// Valid transitions are from an operation's "pending" to "resolved" or
// "rejected" state. "Rejected" can transition to "resolved" if another attempt
// is made (without going to "pending" in-between). Only "resolved" can
// transition to the next operation's "pending" state. Transition between
// operations are only valid in the defined order.
//
// The model we are using is one that measures how close we get to establishing
// a connection. In reality, the peer connection may make multiple tries, and
// follow multiple paths towards reaching a connected state, but we're only
// interested in seeing how far it got on its most successful attempt.
enum class OffererState {
  kNotStarted = 0,
  // createOffer()
  kCreateOfferPending = 1,
  kCreateOfferRejected = 2,
  kCreateOfferResolved = 3,
  // setLocalDescription(offer)
  kSetLocalOfferPending = 4,
  kSetLocalOfferRejected = 5,
  kSetLocalOfferResolved = 6,
  // setRemoteDescription(answer)
  kSetRemoteAnswerPending = 7,
  kSetRemoteAnswerRejected = 8,
  kSetRemoteAnswerResolved = 9,

  kMaxValue = kSetRemoteAnswerResolved,
};

// Represents the different states that an answerer can go through during call
// setup, where initial steps involve SDP exchange. The transition graph for
// this enum follows the same logic as OffererState, see above.
enum class AnswererState {
  kNotStarted = 0,
  // setRemoteDescription(offer)
  kSetRemoteOfferPending = 1,
  kSetRemoteOfferRejected = 2,
  kSetRemoteOfferResolved = 3,
  // createAnswer()
  kCreateAnswerPending = 4,
  kCreateAnswerRejected = 5,
  kCreateAnswerResolved = 6,
  // setLocalDescription(answer)
  kSetLocalAnswerPending = 7,
  kSetLocalAnswerRejected = 8,
  kSetLocalAnswerResolved = 9,

  kMaxValue = kSetLocalAnswerResolved,
};

// A metric reflecting the most successful attempt towards reaching a connected
// state. It's a simplified view based on a CallSetupStateTracker's OffererState
// and AnswererState. Transition graph:
//
//   kNotStarted
//      v
//   kStarted ----+
//      v         |
//   kFailed      |
//      v         |
//   kSucceded <--+
//
enum class CallSetupState {
  // OffererState and AnswererState are both in kNotStarted.
  kNotStarted = 0,
  // OffererState or AnswererState have had a value other than kNotStarted, but
  // the conditions for any of the other states have not been reached.
  kStarted = 1,
  // OffererState or AnswererState have or have had one of the "rejected"
  // states, and the condition for kSucceeded has not been reached.
  kFailed = 2,
  // OffererState or AnswererState is in the final "resolved" state -
  // OffererState::kSetRemoteAnswerResolved or
  // AnswererState::kSetLocalAnswerResolved.
  kSucceeded = 3,

  kMaxValue = kSucceeded,
};

class MODULES_EXPORT CallSetupStateTracker {
  DISALLOW_NEW();

 public:
  CallSetupStateTracker();

  OffererState offerer_state() const;
  AnswererState answerer_state() const;
  CallSetupState GetCallSetupState() const;
  bool document_uses_media() const;

  // The |event| describes what event just happened, see enum definitions.
  // The |document_uses_media| parameter is meant to reflect whether or not the
  // document is using media, such as if getUserMedia() has ever been called
  // (this is up to the caller). The tracker remembers if this has ever been
  // true with document_uses_media() which allows the tracker to know about this
  // even if it outlives the document. If it has ever been true,
  // document_uses_media() stays true even if subsequent calls are made with the
  // value false.
  bool NoteOffererStateEvent(OffererState event, bool document_uses_media);
  bool NoteAnswererStateEvent(AnswererState event, bool document_uses_media);

 private:
  const HashSet<std::pair<OffererState, OffererState>>
      valid_offerer_transitions_;
  const HashSet<std::pair<AnswererState, AnswererState>>
      valid_answerer_transitions_;

  OffererState offerer_state_;
  AnswererState answerer_state_;
  // If the tracker has ever been in any of the "rejected" states. This remains
  // true even if the peer connection recovers to a non-"rejected" state.
  bool has_ever_failed_;
  // If the document has ever been noted to use media based on
  // Note[Offerer/Answerer]StateEvent() calls. Once true, this value will never
  // be false again.
  bool document_uses_media_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_CALL_SETUP_STATE_TRACKER_H_
