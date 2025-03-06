// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_UNIQUE_FONT_SELECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_UNIQUE_FONT_SELECTOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Font;
class FontSelector;
class FontSelectorClient;

// A wrapper of blink::FontSelector.
// This class maintains a cache that returns unique blink::Font instances from
// equivalent blink::FontDescription instances.
class CORE_EXPORT UniqueFontSelector
    : public GarbageCollected<UniqueFontSelector> {
 public:
  explicit UniqueFontSelector(FontSelector& base_selector);
  void Trace(Visitor* visitor) const;

  const Font* FindOrCreateFont(const FontDescription& description);
  void DidSwitchFrame();

  FontSelector* BaseFontSelector() const { return base_selector_; }
  void RegisterForInvalidationCallbacks(FontSelectorClient* client);

 private:
  Member<FontSelector> base_selector_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_UNIQUE_FONT_SELECTOR_H_
