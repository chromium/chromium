// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CONSTANTS_H_
#define REMOTING_BASE_CONSTANTS_H_

namespace remoting {

// Namespace used for chromoting XMPP stanzas.
extern const char kChromotingXmlNamespace[];

// Channel names.
extern const char kAudioChannelName[];
extern const char kControlChannelName[];
extern const char kEventChannelName[];
extern const char kVideoChannelName[];
extern const char kVideoStatsChannelNamePrefix[];

// MIME types for the clipboard.
extern const char kMimeTypeTextUtf8[];

const int kDefaultDpi = 96;

// The video frame rate.
constexpr int kTargetFrameRate = 30;

}  // namespace remoting

#endif  // REMOTING_BASE_CONSTANTS_H_
