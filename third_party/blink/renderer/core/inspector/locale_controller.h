// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LocaleController {
 public:
  static LocaleController& instance();

  LocaleController(const LocaleController&) = delete;
  LocaleController& operator=(const LocaleController&) = delete;

  String SetLocaleOverride(const String& locale, bool is_claiming_override);

 private:
  LocaleController();
  ~LocaleController() = default;

  void UpdateLocale(const String& locale);

  base::Lock lock_;

  String embedder_locale_;
  String locale_override_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_LOCALE_CONTROLLER_H_
