// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CONSTANTS_H_
#define MEDIA_REMOTING_REMOTING_CONSTANTS_H_

namespace media {
namespace remoting {

// The src attribute for remoting media should use the URL with this scheme.
// The URL format is "media-remoting:<id>", e.g. "media-remoting:test".
constexpr char kRemotingScheme[] = "media-remoting";

// The minimum media element duration that is allowed for media remoting.
// Frequent switching into and out of media remoting for short-duration media
// can feel "janky" to the user.
constexpr double kMinRemotingMediaDurationInSec = 60;

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CONSTANTS_H_
