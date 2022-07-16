// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CONSTANTS_H_
#define REMOTING_BASE_CONSTANTS_H_

#include "base/time/time.h"

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

// The default interval for processes to send resource usage to network process.
constexpr base::TimeDelta kDefaultProcessStatsInterval =
    base::Milliseconds(2000);

// The video frame rate.
constexpr int kTargetFrameRate = 30;

}  // namespace remoting

#endif  // REMOTING_BASE_CONSTANTS_H_
