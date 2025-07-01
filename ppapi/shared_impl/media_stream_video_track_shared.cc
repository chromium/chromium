// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/media_stream_video_track_shared.h"


namespace {

const int32_t kMaxWidth = 4096;
const int32_t kMaxHeight = 4096;

}  // namespace

namespace ppapi {

// static
bool MediaStreamVideoTrackShared::VerifyAttributes(
    const Attributes& attributes) {
  if (attributes.buffers < 0)
    return false;
  if (attributes.format < PP_VIDEOFRAME_FORMAT_UNKNOWN ||
      attributes.format > PP_VIDEOFRAME_FORMAT_LAST) {
    return false;
  }
  if (attributes.width < 0 ||
      attributes.width > kMaxWidth ||
      attributes.width & 0x3) {
    return false;
  }
  if (attributes.height < 0 ||
      attributes.height > kMaxHeight ||
      attributes.height & 0x3) {
    return false;
  }
  return true;
}

}  // namespace ppapi
