// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"

#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// static
VideoFrameMonitor& VideoFrameMonitor::Instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(VideoFrameMonitor, instance, ());
  return instance;
}

void VideoFrameMonitor::OnOpenFrame(const std::string& source_id,
                                    media::VideoFrame::ID frame_id) {
  base::AutoLock locker(GetLock());
  OnOpenFrameLocked(source_id, frame_id);
}

void VideoFrameMonitor::OnCloseFrame(const std::string& source_id,
                                     media::VideoFrame::ID frame_id) {
  base::AutoLock locker(GetLock());
  OnCloseFrameLocked(source_id, frame_id);
}

wtf_size_t VideoFrameMonitor::NumFrames(const std::string& source_id) {
  base::AutoLock locker(GetLock());
  return NumFramesLocked(source_id);
}

int VideoFrameMonitor::NumRefs(const std::string& source_id,
                               media::VideoFrame::ID frame_id) {
  base::AutoLock locker(GetLock());
  return NumRefsLocked(source_id, frame_id);
}

bool VideoFrameMonitor::IsEmpty() {
  base::AutoLock locker(GetLock());
  return map_.empty();
}

void VideoFrameMonitor::OnOpenFrameLocked(const std::string& source_id,
                                          media::VideoFrame::ID frame_id) {
  DCHECK(!source_id.empty());
  lock_.AssertAcquired();
  FrameMap& frame_map = map_[source_id];
  auto it_frame = frame_map.find(frame_id);
  if (it_frame == frame_map.end()) {
    frame_map.insert(frame_id, 1);
  } else {
    DCHECK_GT(it_frame->value, 0);
    ++it_frame->value;
  }
}

void VideoFrameMonitor::OnCloseFrameLocked(const std::string& source_id,
                                           media::VideoFrame::ID frame_id) {
  DCHECK(!source_id.empty());
  lock_.AssertAcquired();
  auto it_source = map_.find(source_id);
  CHECK(it_source != map_.end(), base::NotFatalUntil::M130);
  FrameMap& frame_map = it_source->second;
  auto it_frame = frame_map.find(frame_id);
  CHECK(it_frame != frame_map.end(), base::NotFatalUntil::M130);
  DCHECK_GT(it_frame->value, 0);
  if (--it_frame->value == 0) {
    frame_map.erase(it_frame);
    if (frame_map.empty())
      map_.erase(it_source);
  }
}

wtf_size_t VideoFrameMonitor::NumFramesLocked(const std::string& source_id) {
  DCHECK(!source_id.empty());
  lock_.AssertAcquired();
  auto it = map_.find(source_id);
  return it == map_.end() ? 0u : it->second.size();
}

int VideoFrameMonitor::NumRefsLocked(const std::string& source_id,
                                     media::VideoFrame::ID frame_id) {
  DCHECK(!source_id.empty());
  lock_.AssertAcquired();
  auto it = map_.find(source_id);
  if (it == map_.end())
    return 0u;

  FrameMap& frame_map = it->second;
  auto it_frame = frame_map.find(frame_id);
  return it_frame == frame_map.end() ? 0 : it_frame->value;
}

}  // namespace blink
