// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/target_frame_cache.h"

#import "base/containers/contains.h"

TargetFrameCache::TargetFrameCache() {}

TargetFrameCache::~TargetFrameCache() {}

void TargetFrameCache::AddFrame(UIView* view, CGRect frame) {
  targetFrames_[view] = frame;
}

void TargetFrameCache::RemoveFrame(UIView* view) {
  targetFrames_.erase(view);
}

CGRect TargetFrameCache::GetFrame(UIView* view) {
  std::map<UIView*, CGRect>::iterator it = targetFrames_.find(view);
  if (it != targetFrames_.end())
    return it->second;

  return CGRectZero;
}

bool TargetFrameCache::HasFrame(UIView* view) {
  return base::Contains(targetFrames_, view);
}
