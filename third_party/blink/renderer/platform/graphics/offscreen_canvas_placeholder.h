// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/common/resources/resource_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkFilterQuality.h"

namespace blink {

class CanvasResource;
class CanvasResourceDispatcher;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
  DISALLOW_NEW();

 public:
  ~OffscreenCanvasPlaceholder();

  virtual void SetOffscreenCanvasResource(scoped_refptr<CanvasResource>,
                                          viz::ResourceId resource_id);
  void SetOffscreenCanvasDispatcher(
      base::WeakPtr<CanvasResourceDispatcher>,
      scoped_refptr<base::SingleThreadTaskRunner>);

  void ReleaseOffscreenCanvasFrame();

  void SetSuspendOffscreenCanvasAnimation(bool);

  static OffscreenCanvasPlaceholder* GetPlaceholderCanvasById(
      unsigned placeholder_id);

  void RegisterPlaceholderCanvas(unsigned placeholder_id);
  void UnregisterPlaceholderCanvas();
  const scoped_refptr<CanvasResource>& OffscreenCanvasFrame() const {
    return placeholder_frame_;
  }

  bool IsOffscreenCanvasRegistered() const {
    return placeholder_id_ != kNoPlaceholderId;
  }

  void UpdateOffscreenCanvasFilterQuality(SkFilterQuality filter_quality);

 private:
  bool PostSetSuspendAnimationToOffscreenCanvasThread(bool suspend);

  // Information about the Offscreen Canvas:
  scoped_refptr<CanvasResource> placeholder_frame_;
  base::WeakPtr<CanvasResourceDispatcher> frame_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> frame_dispatcher_task_runner_;
  viz::ResourceId placeholder_frame_resource_id_ = 0;

  enum {
    kNoPlaceholderId = -1,
  };
  int placeholder_id_ = kNoPlaceholderId;

  enum AnimationState {
    kActiveAnimation,
    kSuspendedAnimation,
    kShouldSuspendAnimation,
    kShouldActivateAnimation,
  };
  AnimationState animation_state_ = kActiveAnimation;
  base::Optional<SkFilterQuality> filter_quality_ = base::nullopt;
};

}  // namespace blink

#endif
