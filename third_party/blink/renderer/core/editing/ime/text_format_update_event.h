// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_TEXT_FORMAT_UPDATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_TEXT_FORMAT_UPDATE_EVENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class TextFormatUpdateEventInit;
class TextFormatUpdateEvent;

// The textformatupdate event is fired when the input method desires a specific
// region to be styled in a certain fashion, limited to the style properties
// that correspond with the properties that are exposed on TextFormatUpdateEvent
// (e.g. backgroundColor, textDecoration, etc.). The consumer of the EditContext
// should update their view accordingly to provide the user with visual feedback
// as prescribed by the software keyboard. Note that this may have accessibility
// implications, as the IME may not be aware of the color scheme of the editable
// contents (i.e. may be requesting blue highlight on text that was already
// blue).
class CORE_EXPORT TextFormatUpdateEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  TextFormatUpdateEvent(const TextFormatUpdateEventInit* dict);
  TextFormatUpdateEvent(uint32_t format_range_start,
                        uint32_t format_range_end,
                        const String& underline_color,
                        const String& background_color,
                        const String& text_decoration_color,
                        const String& text_underline_style);
  static TextFormatUpdateEvent* Create(const TextFormatUpdateEventInit* dict);
  ~TextFormatUpdateEvent() override;

  uint32_t formatRangeStart() const;
  uint32_t formatRangeEnd() const;
  String underlineColor() const;
  String backgroundColor() const;
  String textDecorationColor() const;
  String textUnderlineStyle() const;

  const AtomicString& InterfaceName() const override;
  // member variables to keep track of the event parameters
 private:
  uint32_t format_range_start_ = 0;
  uint32_t format_range_end_ = 0;
  String underline_color_;
  String background_color_;
  String text_decoration_color_;
  String text_underline_style_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_TEXT_FORMAT_UPDATE_EVENT_H_
