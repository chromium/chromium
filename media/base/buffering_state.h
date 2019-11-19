// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BUFFERING_STATE_H_
#define MEDIA_BASE_BUFFERING_STATE_H_

#include "base/callback_forward.h"

namespace media {

enum BufferingState {
  // Indicates that there is no data buffered.
  //
  // Typical reason is data underflow and hence playback should be paused.
  BUFFERING_HAVE_NOTHING,

  // Indicates that enough data has been buffered.
  //
  // Typical reason is enough data has been prerolled to start playback.
  BUFFERING_HAVE_ENOUGH,

  BUFFERING_STATE_MAX = BUFFERING_HAVE_ENOUGH,
};

enum BufferingStateChangeReason {
  // The reason for the change is not known. This is a valid value for both
  // HAVE_NOTHING and HAVE_ENOUGH states. Notably, it is used with all
  // HAVE_ENOUGH events. The real cause of have HAVE_ENOUGH events is either
  // completion of initial pre-roll, or a resolution of the previous underflow's
  // cause. Interested observers can determine this by checking the most recent
  // state change events. This reason may also be provided for some HAVE_NOTHING
  // events where it is architecturally difficult to determine the cause.
  BUFFERING_CHANGE_REASON_UNKNOWN,

  // Renderer ran out of decoded frames because of delay getting more encoded
  // data from the demuxer. For src=, this indicates network slowness. For MSE
  // it means the data wasn't appended in time (probably also network slowness).
  DEMUXER_UNDERFLOW,

  // Renderer ran out of decoded frames because decoder couldn't keep up.
  DECODER_UNDERFLOW,

  // The local demuxer has the data, but the remote renderer (e.g. cast) hasn't
  // received it yet. Only possible during media "remoting".
  REMOTING_NETWORK_CONGESTION,

  BUFFERING_STATE_CHANGE_REASON_MAX = REMOTING_NETWORK_CONGESTION,
};

// Used to indicate changes in buffering state;
typedef base::Callback<void(BufferingState, BufferingStateChangeReason)>
    BufferingStateCB;

}  // namespace media

#endif  // MEDIA_BASE_BUFFERING_STATE_H_
