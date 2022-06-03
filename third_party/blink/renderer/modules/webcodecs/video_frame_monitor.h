// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_MONITOR_H_

#include <map>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// This class helps monitor usage of media::VideoFrame objects. This class does
// not directly use VideoFrames, but references them using the ID returned by
// media::VideoFrame::unique_id.
// This class is an alternative to using media::VideoFrame destruction observers
// for cases that require monitoring to stop well before the media::VideoFrame
// is destroyed. For example, monitoring blink::VideoFrames backed by
// media::VideoFrames, where the JS-exposed blink::VideoFrames can be closed
// but the backing media::VideoFrames can continue to live outside the
// blink::VideoFrames.
// This class is a per-renderer-process singleton and relies on the fact
// that VideoFrame IDs are unique per process. That means that the same ID
// identifies the same frame regardless of the execution context.
// The motivating use case for this class is keeping track of frames coming
// from sources that have global limits in the number of in-flight frames
// allowed. Thus, frames are monitored per source. Sources are identified with
// a nonempty std::string, so any way to group frames can be used as long as a
// source ID is given. std::string is chosen over the Blink-standard WTF::String
// because:
//   1. source IDs often come from outside Blink (e.g., camera and screen
//      device IDs)
//   2. std::string is easier to use in a multithreaded environment.
// All the methods of this class can be called from any thread.
class MODULES_EXPORT VideoFrameMonitor {
 public:
  // Returns the singleton VideoFrameMonitor.
  static VideoFrameMonitor& Instance();
  VideoFrameMonitor(const VideoFrameMonitor&) = delete;
  VideoFrameMonitor& operator=(const VideoFrameMonitor&) = delete;

  // Report that a new frame with ID |frame_id| associated with the source with
  // ID |source_id| is being monitored.
  void OnOpenFrame(const std::string& source_id, int frame_id);
  // Report that a new frame with ID |frame_id| associated with the source with
  // ID |source_id| is being monitored.
  void OnCloseFrame(const std::string& source_id, int frame_id);
  // Reports the number of distinct monitored frames associated with
  // |source_id|.
  wtf_size_t NumFrames(const std::string& source_id);
  // Reports the reference count for the frame with ID |frame_id| associated
  // with the source with ID |source_id|.
  int NumRefs(const std::string& source_id, int frame_id);

  // Reports true if nothing is being monitored, false otherwise.
  bool IsEmpty();

  // This function returns a mutex that can be used to lock the
  // VideoFrameMonitor so that multiple invocations to the methods below
  // (suffixed with "Locked" can be done atomically, as a single update to
  // the monitor. Locking the VideoFrameMonitor using GetMutex() must be done
  // very carefully, as any invocation to any non-Locked method while the mutex
  // is acquired will result in deadlock.
  // For example, blink::VideoFrame objects may be automatically monitored, so
  // they should not be created, cloned or closed while the mutex is acquired.
  Mutex& GetMutex() { return mutex_; }

  // The methods below can be called only when the mutex returned by GetMutex()
  // has been acquired. Other than that, they are equivalent to their
  // corresponding non-locked version.
  void OnOpenFrameLocked(const std::string& source_id, int frame_id);
  void OnCloseFrameLocked(const std::string& source_id, int frame_id);
  wtf_size_t NumFramesLocked(const std::string& source_id);
  int NumRefsLocked(const std::string& source_id, int frame_id);

 private:
  VideoFrameMonitor() = default;

  // key: unique ID of a frame.
  // value: reference count for the frame (among objects explicitly tracking
  //        the frame with VideoFrameMonitor).
  using FrameMap = HashMap<int,
                           int,
                           WTF::IntHash<int>,
                           WTF::UnsignedWithZeroKeyHashTraits<int>>;

  // key: ID of the source of the frames.
  // value: References to frames associated to that source.
  // Using std::map because HashMap does not directly support std::string.
  using SourceMap = std::map<std::string, FrameMap>;

  Mutex mutex_;
  // Contains all data for VideoFrameMonitor.
  SourceMap map_ GUARDED_BY(mutex_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_MONITOR_H_
