// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/pref_transformer_interface.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "extensions/browser/pref_modifiability_observer.h"

namespace extensions {

PrefTransformerInterface::PrefTransformerInterface() = default;

PrefTransformerInterface::~PrefTransformerInterface() = default;

bool PrefTransformerInterface::IsExtensionModifiable(
    content::BrowserContext* context) const {
  return true;
}

void PrefTransformerInterface::AddObserver(content::BrowserContext* context,
                                           const std::string& browser_pref,
                                           Observer* observer) {
  if (auto it = modifiability_observers_.find(context);
      it == modifiability_observers_.end()) {
    std::unique_ptr<PrefModifiabilityObserver> modifiability_observer =
        CreateModifiabilityObserver(context);
    if (!modifiability_observer) {
      return;
    }
    modifiability_observer->browser_pref_ = browser_pref;
    modifiability_observers_.emplace(context,
                                     std::move(modifiability_observer));
  } else {
    CHECK_EQ(it->second->browser_pref_, browser_pref);
  }
  observers_[context].AddObserver(observer);
}

void PrefTransformerInterface::RemoveObserver(content::BrowserContext* context,
                                              Observer* observer) {
  auto it = observers_.find(context);
  if (it == observers_.end()) {
    return;
  }
  it->second.RemoveObserver(observer);
  if (it->second.empty() && notification_depth_ == 0) {
    modifiability_observers_.erase(context);
    observers_.erase(it);
  }
}

std::unique_ptr<PrefModifiabilityObserver>
PrefTransformerInterface::CreateModifiabilityObserver(
    content::BrowserContext* context) {
  return nullptr;
}

void PrefTransformerInterface::OnModifiabilityChanged(
    content::BrowserContext* context,
    const std::string& browser_pref) {
  auto it = observers_.find(context);
  if (it == observers_.end()) {
    return;
  }
  {
    base::AutoReset<size_t> reset(&notification_depth_,
                                  notification_depth_ + 1);
    for (Observer& observer : it->second) {
      observer.OnPrefModifiabilityChanged(browser_pref);
    }
  }
  if (notification_depth_ == 0) {
    std::erase_if(observers_, [this](const auto& entry) {
      if (entry.second.empty()) {
        modifiability_observers_.erase(entry.first);
        return true;
      }
      return false;
    });
  }
}

}  // namespace extensions
