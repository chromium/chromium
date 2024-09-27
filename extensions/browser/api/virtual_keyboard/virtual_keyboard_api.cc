// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard/virtual_keyboard_api.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/common/api/virtual_keyboard.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/base/ime/ash/input_method_manager.h"
#endif

namespace extensions {

void VirtualKeyboardRestrictFeaturesFunction::OnRestrictFeatures(
    api::virtual_keyboard::FeatureRestrictions update) {
  Respond(WithArguments(update.ToValue()));
}

ExtensionFunction::ResponseAction
VirtualKeyboardRestrictFeaturesFunction::Run() {
  std::optional<api::virtual_keyboard::RestrictFeatures::Params> params =
      api::virtual_keyboard::RestrictFeatures::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
#if BUILDFLAG(IS_CHROMEOS)
  using ::ash::input_method::InputMethodManager;
  InputMethodManager* input_method_manager = InputMethodManager::Get();
  if (input_method_manager) {
    if (params->restrictions.handwriting_enabled) {
      input_method_manager->SetImeMenuFeatureEnabled(
          InputMethodManager::FEATURE_HANDWRITING,
          *params->restrictions.handwriting_enabled);
    }
    if (params->restrictions.voice_input_enabled) {
      input_method_manager->SetImeMenuFeatureEnabled(
          InputMethodManager::FEATURE_VOICE,
          *params->restrictions.voice_input_enabled);
    }
  }
#endif

  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(browser_context());
  api->delegate()->RestrictFeatures(
      *params,
      base::BindOnce(
          &VirtualKeyboardRestrictFeaturesFunction::OnRestrictFeatures, this));

  return RespondLater();
}

}  // namespace extensions
