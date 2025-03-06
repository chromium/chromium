// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/unique_font_selector.h"

#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

UniqueFontSelector::UniqueFontSelector(FontSelector& base_selector)
    : base_selector_(base_selector) {}

void UniqueFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(base_selector_);
}

const Font* UniqueFontSelector::FindOrCreateFont(
    const FontDescription& description) {
  if (!RuntimeEnabledFeatures::CanvasTextNgEnabled()) {
    return MakeGarbageCollected<Font>(description, base_selector_);
  }

  // TODO(crbug.com/389726691): Implement a cache.
  return MakeGarbageCollected<Font>(description, base_selector_);
}

void UniqueFontSelector::DidSwitchFrame() {}

void UniqueFontSelector::RegisterForInvalidationCallbacks(
    FontSelectorClient* client) {
  base_selector_->RegisterForInvalidationCallbacks(client);
}

}  // namespace blink
