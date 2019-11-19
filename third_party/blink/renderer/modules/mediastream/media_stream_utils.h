// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class MediaStreamComponent;
class WebMediaStreamTrack;

class MediaStreamUtils {
  STATIC_ONLY(MediaStreamUtils);

 public:
  static void CreateNativeAudioMediaStreamTrack(
      const WebMediaStreamTrack&,
      scoped_refptr<base::SingleThreadTaskRunner>);

  static void DidCreateMediaStreamTrack(MediaStreamComponent*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_UTILS_H_
