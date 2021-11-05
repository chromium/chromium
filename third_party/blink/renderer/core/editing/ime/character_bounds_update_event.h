// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_CHARACTER_BOUNDS_UPDATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_CHARACTER_BOUNDS_UPDATE_EVENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class CharacterBoundsUpdateEventInit;
class CharacterBoundsUpdateEvent;

// Spec draft:
// https://w3c.github.io/editing/docs/EditContext/index.html#characterboundsupdateevent
class CORE_EXPORT CharacterBoundsUpdateEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CharacterBoundsUpdateEvent(
      const CharacterBoundsUpdateEventInit* dict);
  CharacterBoundsUpdateEvent(uint32_t range_start, uint32_t range_end);
  static CharacterBoundsUpdateEvent* Create(
      const CharacterBoundsUpdateEventInit* dict);
  ~CharacterBoundsUpdateEvent() override;

  uint32_t rangeStart() const;
  uint32_t rangeEnd() const;

  const AtomicString& InterfaceName() const override;

 private:
  uint32_t range_start_ = 0;
  uint32_t range_end_ = 0;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_IME_CHARACTER_BOUNDS_UPDATE_EVENT_H_
