// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CG_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CG_CONVERSIONS_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/platform_export.h"

#if defined(OS_MAC)

namespace gfx {
class Point;
}

typedef struct CGPoint CGPoint;

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

namespace blink {

PLATFORM_EXPORT gfx::Point CGPointToPoint(const CGPoint&);
PLATFORM_EXPORT CGPoint PointToCGPoint(const gfx::Point&);

}  // namespace blink

#endif  // defined(OS_MAC)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GEOMETRY_CG_CONVERSIONS_H_
