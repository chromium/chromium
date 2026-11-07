// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_modifiability_observer.h"

#include "extensions/browser/pref_transformer_interface.h"

namespace extensions {

PrefModifiabilityObserver::PrefModifiabilityObserver(
    PrefTransformerInterface* transformer,
    content::BrowserContext* context)
    : transformer_(transformer),
      context_(context),
      was_modifiable_(transformer_->IsExtensionModifiable(context_)) {}

PrefModifiabilityObserver::~PrefModifiabilityObserver() = default;

void PrefModifiabilityObserver::MaybeNotifyModifiabilityChanged() {
  bool is_modifiable = transformer_->IsExtensionModifiable(context_);
  if (is_modifiable == was_modifiable_) {
    return;
  }
  was_modifiable_ = is_modifiable;
  transformer_->OnModifiabilityChanged(context_, browser_pref_);
}

}  // namespace extensions
