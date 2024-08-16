// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_REMOTING_CONSTANTS_H_
#define MEDIA_BASE_REMOTING_CONSTANTS_H_

namespace media::remoting {

// The src attribute for remoting media should use the URL with this scheme.
// The URL format is "media-remoting:<id>", e.g. "media-remoting:test".
inline constexpr char kRemotingScheme[] = "media-remoting";

// The minimum media element duration that is allowed for media remoting.
inline constexpr double kMinRemotingMediaDurationInSec = 15;

// The minimum media element duration that is allowed for switching from
// mirroring to media remoting by bringing the video to fullscreen mode.
// Frequent switching into and out of media remoting for short-duration media
// can feel "janky" to the user.
inline constexpr double kMinMediaDurationForSwitchingToRemotingInSec = 60;

// The duration to wait and calculate the pixel rate of the media element and to
// ensure that all preconditions are held stable before starting media remoting.
// Note that RendererController needs at least `kPixelRateCalInSec` to calculate
// the pixel rate before remoting can start.
inline constexpr double kPixelRateCalInSec = 5;

}  // namespace media::remoting

#endif  // MEDIA_BASE_REMOTING_CONSTANTS_H_
