/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_INFO_H_

#include <string>
#include <vector>

#include "base/i18n/rtl.h"

namespace blink {

struct MenuItemInfo {
  enum Type { kOption, kCheckableOption, kGroup, kSeparator, kSubMenu };

  MenuItemInfo()
      : type(kOption),
        action(0),
        text_direction(base::i18n::TextDirection::UNKNOWN_DIRECTION),
        has_text_direction_override(false),
        enabled(false),
        checked(false) {}

  std::u16string label;
  bool is_experimental_feature = false;
  std::u16string tool_tip;
  Type type;
  unsigned action;
  base::i18n::TextDirection text_direction;
  std::vector<MenuItemInfo> sub_menu_items;
  bool has_text_direction_override;
  bool enabled;
  bool checked;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_MENU_ITEM_INFO_H_
