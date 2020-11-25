// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_LOGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_LOGGER_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// This class is used to distribute a VideoFrameDestructionAuditor flag to
// VideoFrameHandles. If a handle's destructor is run without having received a
// call to Invalidate(), it will set |destruction_auditor_|. The
// VideoFrameLogger periodically checks whether or not the flag is set, and
// outputs an error message to the JS console, reminding developers to call
// destroy() on their VideoFrames.
//
// This class lets us avoid making VideoFrames ExecutionLifeCycleObservers,
// which could add 1000s of observers per second. It also avoids the use of
// a pre-finzalizer on VideoFrames, which could have a GC performance impact.
class MODULES_EXPORT VideoFrameLogger
    : public GarbageCollected<VideoFrameLogger>,
      public Supplement<ExecutionContext> {
 public:
  // Class that reports when blink::VideoFrames have been garbage collected
  // without having destroy() called on them. This is a web page application
  // error which can cause a web page to stall.
  class VideoFrameDestructionAuditor
      : public WTF::ThreadSafeRefCounted<VideoFrameDestructionAuditor> {
   public:
    void ReportUndestroyedFrame();
    void Clear();

    bool were_frames_not_destroyed() { return were_frames_not_destroyed_; }

   private:
    friend class WTF::ThreadSafeRefCounted<VideoFrameDestructionAuditor>;
    ~VideoFrameDestructionAuditor() = default;

    bool were_frames_not_destroyed_ = false;
  };

  static const char kSupplementName[];

  static VideoFrameLogger& From(ExecutionContext&);

  explicit VideoFrameLogger(ExecutionContext&);
  virtual ~VideoFrameLogger() = default;

  // Disallow copy and assign.
  VideoFrameLogger& operator=(const VideoFrameLogger&) = delete;
  VideoFrameLogger(const VideoFrameLogger&) = delete;

  // Returns |destruction_auditor_| and starts |timer_| if needed.
  scoped_refptr<VideoFrameDestructionAuditor> GetDestructionAuditor();

  void Trace(Visitor*) const override;

 private:
  void LogDestructionErrors(TimerBase*);

  base::TimeTicks last_auditor_access_;

  std::unique_ptr<TimerBase> timer_;

  scoped_refptr<VideoFrameDestructionAuditor> destruction_auditor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_LOGGER_H_
