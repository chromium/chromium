// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/mediastream/web_media_element_source_utils.h"

#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_media_stream.h"

namespace blink {

WebMediaStream GetWebMediaStreamFromWebMediaPlayerSource(
    const WebMediaPlayerSource& source) {
  if (source.IsMediaStream())
    return source.GetAsMediaStream();

  return WebMediaStream();
}

}  // namespace blink
