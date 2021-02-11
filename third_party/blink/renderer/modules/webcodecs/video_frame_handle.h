// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_logger.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/skia/include/core/SkRefCnt.h"

// Note: Don't include "media/base/video_frame.h" here without good reason,
// since it includes a lot of non-blink types which can pollute the namespace.

class SkImage;

namespace media {
class VideoFrame;
}

namespace blink {

class ExecutionContext;

// Wrapper class that allows sharing a single |frame_| reference across
// multiple VideoFrames, which can be invalidated for all frames at once.
//
// If Invalidate() is not called before the handle's destructor runs, this means
// that none of the VideoFrames sharing this handle were closed, and they were
// all GC'ed instead. This can lead to stalls, since frames are not released
// fast enough through the GC to keep a pipeline running smoothly. In that case
// report an unclosed frame through |close_auditor_|.
class MODULES_EXPORT VideoFrameHandle
    : public WTF::ThreadSafeRefCounted<VideoFrameHandle> {
 public:
  VideoFrameHandle(scoped_refptr<media::VideoFrame>, ExecutionContext*);
  VideoFrameHandle(scoped_refptr<media::VideoFrame>,
                   sk_sp<SkImage> sk_image,
                   ExecutionContext*);
  VideoFrameHandle(scoped_refptr<media::VideoFrame>,
                   sk_sp<SkImage> sk_image,
                   scoped_refptr<VideoFrameLogger::VideoFrameCloseAuditor>);

  // Returns a copy of |frame_|, which should be re-used throughout the scope
  // of a function call, instead of calling frame() multiple times. Otherwise
  // the frame could be destroyed between calls.
  scoped_refptr<media::VideoFrame> frame();

  // Returns a copy of |sk_image_| which may be nullptr if this isn't an SkImage
  // backed VideoFrame.
  sk_sp<SkImage> sk_image();

  // Releases the underlying media::VideoFrame reference, affecting all
  // blink::VideoFrames and blink::Planes that hold a reference to |this|.
  void Invalidate();

  // Clones this VideoFrameHandle into a new VideoFrameHandle object.
  scoped_refptr<VideoFrameHandle> Clone();

 private:
  friend class WTF::ThreadSafeRefCounted<VideoFrameHandle>;
  ~VideoFrameHandle();

  WTF::Mutex mutex_;
  sk_sp<SkImage> sk_image_;
  scoped_refptr<media::VideoFrame> frame_;
  scoped_refptr<VideoFrameLogger::VideoFrameCloseAuditor> close_auditor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_
