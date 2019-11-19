// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_ELEMENT_SOURCE_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_ELEMENT_SOURCE_UTILS_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class WebMediaPlayerSource;
class WebMediaStream;

// Obtains a WebMediaStream from a WebMediaPlayerSource. If the
// WebMediaPlayerSource does not contain a WebMediaStream, a null
// WebMediaStream is returned.
BLINK_PLATFORM_EXPORT WebMediaStream
GetWebMediaStreamFromWebMediaPlayerSource(const WebMediaPlayerSource& source);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_ELEMENT_SOURCE_UTILS_H_
