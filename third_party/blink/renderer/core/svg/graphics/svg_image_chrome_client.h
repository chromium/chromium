/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_CHROME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_CHROME_CLIENT_H_

#include "base/gtest_prod_util.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class SVGImage;

class CORE_EXPORT SVGImageChromeClient final : public EmptyChromeClient {
 public:
  explicit SVGImageChromeClient(SVGImage*);

  void InitAnimationTimer(
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner);

  bool IsSVGImageChromeClient() const override;

  SVGImage* GetImage() const { return image_; }

  void SuspendAnimation();
  void ResumeAnimation();
  void RestoreAnimationIfNeeded();

  bool IsSuspended() const { return timeline_state_ >= kSuspended; }

  void Trace(Visitor*) const final;

 private:
  void ChromeDestroyed() override;
  void InvalidateContainer() override;
  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta = base::TimeDelta()) override;

  void SetTimerForTesting(
      DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>*);
  TimerBase& GetTimerForTesting() const { return animation_timer_->Value(); }
  void AnimationTimerFired(TimerBase*);

  SVGImage* image_;
  Member<DisallowNewWrapper<HeapTaskRunnerTimer<SVGImageChromeClient>>>
      animation_timer_;
  enum {
    kRunning,
    kSuspended,
    kSuspendedWithAnimationPending,
  } timeline_state_;

  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, TimelineSuspendAndResume);
  FRIEND_TEST_ALL_PREFIXES(SVGImageTest, ResetAnimation);
  FRIEND_TEST_ALL_PREFIXES(SVGImageSimTest, PageVisibilityHiddenToVisible);
  FRIEND_TEST_ALL_PREFIXES(SVGImageSimTest,
                           AnimationsPausedWhenImageScrolledOutOfView);
};

template <>
struct DowncastTraits<SVGImageChromeClient> {
  static bool AllowFrom(const ChromeClient& client) {
    return client.IsSVGImageChromeClient();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_GRAPHICS_SVG_IMAGE_CHROME_CLIENT_H_
