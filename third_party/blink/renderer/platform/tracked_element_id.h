// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRACKED_ELEMENT_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRACKED_ELEMENT_ID_H_

#include <optional>  // For std::optional

#include "base/token.h"
#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/rect.h"  // For gfx::RectF

namespace blink {

// TrackedElementId is a strong alias for base::Token, used to uniquely
// identify tracked elements.
using TrackedElementId =
    base::StrongAlias<class TrackedElementIdTag, base::Token>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TRACKED_ELEMENT_ID_H_
