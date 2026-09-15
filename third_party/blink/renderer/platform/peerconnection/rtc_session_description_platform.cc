// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

RTCSessionDescriptionPlatform::RTCSessionDescriptionPlatform(const String& type,
                                                             const String& sdp)
    : type_(type), sdp_(sdp) {}

}  // namespace blink
