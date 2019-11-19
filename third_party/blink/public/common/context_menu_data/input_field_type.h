// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_INPUT_FIELD_TYPE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_INPUT_FIELD_TYPE_H_

namespace blink {

enum class ContextMenuDataInputFieldType {
  // Not an input field.
  kNone,
  // type = text, search, email, url
  kPlainText,
  // type = password
  kPassword,
  // type = number
  kNumber,
  // type = tel
  kTelephone,
  // type = <etc.>
  kOther,
  kLast = kOther
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_CONTEXT_MENU_DATA_INPUT_FIELD_TYPE_H_
