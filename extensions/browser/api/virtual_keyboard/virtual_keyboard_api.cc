// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard/virtual_keyboard_api.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/common/api/virtual_keyboard.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ime/ash/input_method_manager.h"
#endif

namespace extensions {

VirtualKeyboardRestrictFeaturesFunction::
    VirtualKeyboardRestrictFeaturesFunction() {}

ExtensionFunction::ResponseAction
VirtualKeyboardRestrictFeaturesFunction::Run() {
  std::unique_ptr<api::virtual_keyboard::RestrictFeatures::Params> params =
      api::virtual_keyboard::RestrictFeatures::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  api::virtual_keyboard::FeatureRestrictions update =
      api->delegate()->RestrictFeatures(*params);

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(update.ToValue())));
}

}  // namespace extensions
