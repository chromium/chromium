// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_activation_service.h"

#include "base/functional/bind.h"
#include "extensions/browser/extension_user_activation_service_factory.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"

namespace extensions {

ExtensionUserActivationService::ExtensionUserActivationService() = default;

ExtensionUserActivationService::~ExtensionUserActivationService() = default;

// static
ExtensionUserActivationService* ExtensionUserActivationService::Get(
    content::BrowserContext* context) {
  return ExtensionUserActivationServiceFactory::GetForBrowserContext(context);
}

void ExtensionUserActivationService::NotifyUserActivation(
    const ExtensionId& extension_id) {
  user_activation_timers_[extension_id].Start(
      FROM_HERE, blink::kActivationLifespan,
      base::BindOnce(&ExtensionUserActivationService::RemoveActivation,
                     base::Unretained(this), extension_id));
}

bool ExtensionUserActivationService::HasTransientActivation(
    const ExtensionId& extension_id) const {
  return user_activation_timers_.contains(extension_id);
}

void ExtensionUserActivationService::RemoveActivation(
    const ExtensionId& extension_id) {
  user_activation_timers_.erase(extension_id);
}

}  // namespace extensions
