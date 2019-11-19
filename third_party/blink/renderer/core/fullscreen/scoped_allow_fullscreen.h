// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT ScopedAllowFullscreen {
  STACK_ALLOCATED();

 public:
  enum Reason { kOrientationChange };

  static base::Optional<Reason> FullscreenAllowedReason();
  explicit ScopedAllowFullscreen(Reason);
  ~ScopedAllowFullscreen();

 private:
  static base::Optional<Reason> reason_;
  base::Optional<Reason> previous_reason_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowFullscreen);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FULLSCREEN_SCOPED_ALLOW_FULLSCREEN_H_
