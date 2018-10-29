// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/web_media_element_source_utils.h"

#include "third_party/blink/public/platform/web_media_player_source.h"
#include "third_party/blink/public/platform/web_media_stream.h"

namespace content {

blink::WebMediaStream GetWebMediaStreamFromWebMediaPlayerSource(
    const blink::WebMediaPlayerSource& source) {
  if (source.IsMediaStream())
    return source.GetAsMediaStream();

  return blink::WebMediaStream();
}

}  // namespace content
