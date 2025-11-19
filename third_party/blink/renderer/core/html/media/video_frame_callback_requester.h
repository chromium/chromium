// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_FRAME_CALLBACK_REQUESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_FRAME_CALLBACK_REQUESTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class HTMLVideoElement;

// Interface defining what the HTMLVideoElement should be aware
// of in order to support the <video>.requestVideoFrameCallback() API.
class CORE_EXPORT VideoFrameCallbackRequester
    : public GarbageCollected<VideoFrameCallbackRequester> {
 public:
  VideoFrameCallbackRequester(const VideoFrameCallbackRequester&) = delete;
  VideoFrameCallbackRequester& operator=(const VideoFrameCallbackRequester&) =
      delete;
  virtual ~VideoFrameCallbackRequester() = default;

  virtual void Trace(Visitor*) const;

  virtual void OnWebMediaPlayerCreated() = 0;
  virtual void OnWebMediaPlayerCleared() = 0;
  virtual void OnRequestVideoFrameCallback() = 0;

 protected:
  explicit VideoFrameCallbackRequester(HTMLVideoElement&);

  Member<HTMLVideoElement> element_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_FRAME_CALLBACK_REQUESTER_H_
