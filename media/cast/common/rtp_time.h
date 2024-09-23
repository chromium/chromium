// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_RTP_TIME_H_
#define MEDIA_CAST_COMMON_RTP_TIME_H_

#include "third_party/openscreen/src/cast/streaming/rtp_time.h"

namespace media::cast {

// TODO(crbug.com/40231271): this typedef should be removed and
// the openscreen type used directly.
using RtpTimeDelta = openscreen::cast::RtpTimeDelta;
using RtpTimeTicks = openscreen::cast::RtpTimeTicks;

}  // namespace media::cast

#endif  // MEDIA_CAST_COMMON_RTP_TIME_H_
