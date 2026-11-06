// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_SCOPED_DEFAULT_LOCALE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_SCOPED_DEFAULT_LOCALE_H_

#include <string_view>

#include "base/test/icu_test_util.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Overrides the default Blink and ICU locales for the duration of a test scope
// and resets cached `LayoutLocale` state on construction and destruction.
class ScopedDefaultLocale {
  STACK_ALLOCATED();

 public:
  explicit ScopedDefaultLocale(std::string_view locale);
  ScopedDefaultLocale(const ScopedDefaultLocale&) = delete;
  ScopedDefaultLocale& operator=(const ScopedDefaultLocale&) = delete;
  ~ScopedDefaultLocale();

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_icu_locale_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_SCOPED_DEFAULT_LOCALE_H_
