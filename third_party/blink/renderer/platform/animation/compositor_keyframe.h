// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class TimingFunction;
}

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorKeyframe {
 public:
  virtual ~CompositorKeyframe() = default;

  virtual double Time() const = 0;

  scoped_refptr<TimingFunction> GetTimingFunctionForTesting() const;

 private:
  virtual const cc::TimingFunction* CcTimingFunction() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_H_
