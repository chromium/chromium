// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/crash_keys.h"

#include "base/strings/string_number_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"

namespace extensions::crash_keys {

void SetActiveExtensions(const std::set<ExtensionId>& extensions) {
  static crash_reporter::CrashKeyString<4> num_extensions("num-extensions");
  num_extensions.Set(base::NumberToString(extensions.size()));

  using ExtensionIDKey = crash_reporter::CrashKeyString<64>;
  static ExtensionIDKey extension_ids[] = {
      {"extension-1", ExtensionIDKey::Tag::kArray},
      {"extension-2", ExtensionIDKey::Tag::kArray},
      {"extension-3", ExtensionIDKey::Tag::kArray},
      {"extension-4", ExtensionIDKey::Tag::kArray},
      {"extension-5", ExtensionIDKey::Tag::kArray},
      {"extension-6", ExtensionIDKey::Tag::kArray},
      {"extension-7", ExtensionIDKey::Tag::kArray},
      {"extension-8", ExtensionIDKey::Tag::kArray},
      {"extension-9", ExtensionIDKey::Tag::kArray},
      {"extension-10", ExtensionIDKey::Tag::kArray},
  };

  auto it = extensions.begin();
  for (ExtensionIDKey& crash_key : extension_ids) {
    if (it == extensions.end()) {
      crash_key.Clear();
    } else {
      crash_key.Set(*it);
      ++it;
    }
  }
}

}  // namespace extensions::crash_keys
