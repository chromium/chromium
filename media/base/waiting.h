// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WAITING_H_
#define MEDIA_BASE_WAITING_H_

#include "base/functional/callback_forward.h"

namespace media {

// Here "waiting" refers to the state that media pipeline stalls waiting because
// of some reason, e.g. no decryption key. It could cause Javascript events like
// "waitingforkey" [1], but not necessarily.
// Note: this generally does not cause the "waiting" event on HTML5 media
// elements [2], which is tightly related to the buffering state change (see
// buffering_state.h).
// [1] https://www.w3.org/TR/encrypted-media/#dom-evt-waitingforkey
// [2]
// https://www.w3.org/TR/html5/semantics-embedded-content.html#eventdef-media-waiting

enum class WaitingReason {
  // The playback cannot start because "Media Data May Contain Encrypted Blocks"
  // and no CDM is available. The playback will start after a CDM is set. See
  // https://www.w3.org/TR/encrypted-media/#media-may-contain-encrypted-blocks
  kNoCdm,

  // The playback cannot proceed because some decryption key is not available.
  // This could happen when the license exchange is delayed or failed. The
  // playback will resume after the decryption key becomes available.
  // See https://www.w3.org/TR/encrypted-media/#encrypted-block-encountered
  kNoDecryptionKey,

  // The playback cannot proceed because the decoder has lost its state, e.g.
  // information about reference frames. Usually this only happens to hardware
  // decoders. To recover from this state, reset the decoder and start decoding
  // from a key frame, which can typically be accomplished by a pipeline seek.
  kDecoderStateLost,

  // The playback cannot proceed because the secure output surface is gone. This
  // can happen when user backgrounds the page when it's playing secure content.
  kSecureSurfaceLost,

  // Must be assigned with the last enum value above.
  kMaxValue = kSecureSurfaceLost,
};

// Callback to notify waiting state and the reason.
using WaitingCB = base::RepeatingCallback<void(WaitingReason)>;

}  // namespace media

#endif  // MEDIA_BASE_WAITING_H_
