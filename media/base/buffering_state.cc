// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffering_state.h"

#include <string>
#include <vector>

#include "base/check.h"

namespace media {

std::string BufferingStateToString(BufferingState state,
                                   BufferingStateChangeReason reason) {
  DCHECK(state == BUFFERING_HAVE_NOTHING || state == BUFFERING_HAVE_ENOUGH);
  DCHECK(reason == BUFFERING_CHANGE_REASON_UNKNOWN ||
         reason == DEMUXER_UNDERFLOW || reason == DECODER_UNDERFLOW ||
         reason == REMOTING_NETWORK_CONGESTION);

  std::string state_string = state == BUFFERING_HAVE_NOTHING
                                 ? "BUFFERING_HAVE_NOTHING"
                                 : "BUFFERING_HAVE_ENOUGH";

  std::vector<std::string> flag_strings;
  if (reason == DEMUXER_UNDERFLOW)
    state_string += " (DEMUXER_UNDERFLOW)";
  else if (reason == DECODER_UNDERFLOW)
    state_string += " (DECODER_UNDERFLOW)";
  else if (reason == REMOTING_NETWORK_CONGESTION)
    state_string += " (REMOTING_NETWORK_CONGESTION)";

  return state_string;
}

}  // namespace media
