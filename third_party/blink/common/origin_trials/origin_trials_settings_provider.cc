// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/origin_trials_settings_provider.h"

namespace blink {

OriginTrialsSettingsProvider::OriginTrialsSettingsProvider() = default;
OriginTrialsSettingsProvider::~OriginTrialsSettingsProvider() = default;

// static
OriginTrialsSettingsProvider* OriginTrialsSettingsProvider::Get() {
  static base::NoDestructor<OriginTrialsSettingsProvider> instance;
  return instance.get();
}

void OriginTrialsSettingsProvider::SetSettings(
    blink::mojom::OriginTrialsSettingsPtr settings) {
  base::AutoLock auto_lock(settings_lock_);
  settings_ = std::move(settings);
}

blink::mojom::OriginTrialsSettingsPtr
OriginTrialsSettingsProvider::GetSettings() {
  return settings_.Clone();
}

}  // namespace blink
