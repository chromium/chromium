// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_TIMELINE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "cc/animation/animation_timeline.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CompositorAnimationClient;

// A compositor representation for cc::AnimationTimeline.
class PLATFORM_EXPORT CompositorAnimationTimeline {
  USING_FAST_MALLOC(CompositorAnimationTimeline);

 public:
  CompositorAnimationTimeline();
  explicit CompositorAnimationTimeline(scoped_refptr<cc::AnimationTimeline>);
  ~CompositorAnimationTimeline();

  cc::AnimationTimeline* GetAnimationTimeline() const;
  void UpdateCompositorTimeline(base::Optional<CompositorElementId> pending_id,
                                const std::vector<double> scroll_offsets);

  void AnimationAttached(const CompositorAnimationClient&);
  void AnimationDestroyed(const CompositorAnimationClient&);

 private:
  scoped_refptr<cc::AnimationTimeline> animation_timeline_;

  DISALLOW_COPY_AND_ASSIGN(CompositorAnimationTimeline);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_TIMELINE_H_
