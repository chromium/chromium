// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WIDGET_CREATION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WIDGET_CREATION_OBSERVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {
class CORE_EXPORT WidgetCreationObserver : public GarbageCollectedMixin {
 public:
  virtual void OnLocalRootWidgetCreated() = 0;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_WIDGET_CREATION_OBSERVER_H_
