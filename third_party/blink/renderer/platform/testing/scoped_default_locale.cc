// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_default_locale.h"

#include <string>

#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/text/layout_locale.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

ScopedDefaultLocale::ScopedDefaultLocale(std::string_view locale)
    : scoped_icu_locale_(std::string(locale)) {
  OverrideUserPreferredLanguagesForTesting({AtomicString::FromUtf8(locale)});
  LayoutLocale::ClearForTesting();
}

ScopedDefaultLocale::~ScopedDefaultLocale() {
  OverrideUserPreferredLanguagesForTesting({});
  LayoutLocale::ClearForTesting();
}

}  // namespace blink
