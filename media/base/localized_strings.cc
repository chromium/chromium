// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/localized_strings.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace media {

static LocalizedStringProvider g_localized_string_provider = nullptr;

void SetLocalizedStringProvider(LocalizedStringProvider func) {
  g_localized_string_provider = func;
}

std::string GetLocalizedStringUTF8(MessageId message_id) {
  return base::UTF16ToUTF8(GetLocalizedStringUTF16(message_id));
}

std::u16string GetLocalizedStringUTF16(MessageId message_id) {
  return g_localized_string_provider ? g_localized_string_provider(message_id)
                                     : std::u16string();
}

}  // namespace media
