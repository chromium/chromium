// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "ui/events/event.h"

namespace extensions {

namespace {

const char kNotYetImplementedError[] =
    "API is not implemented on this platform.";
const char kVirtualKeyboardNotEnabled[] =
    "The virtual keyboard is not enabled.";
const char kSetDraggableAreaFailed[] =
    "Setting draggable area of virtual keyboard failed.";
const char kSetAreaToRemainOnScreenFailed[] =
    "Setting area to remain on screen of virtual keyboard failed.";
const char kUnknownError[] = "Unknown error.";

namespace keyboard = api::virtual_keyboard_private;

gfx::Rect KeyboardBoundsToRect(const keyboard::Bounds& bounds) {
  return {bounds.left, bounds.top, bounds.width, bounds.height};
}

}  // namespace

bool VirtualKeyboardPrivateFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;

  VirtualKeyboardAPI* api =
      BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>::Get(browser_context());
  DCHECK(api);
  delegate_ = api->delegate();
  if (!delegate_) {
    *error = kNotYetImplementedError;
    return false;
  }
  return true;
}

VirtualKeyboardPrivateFunction::~VirtualKeyboardPrivateFunction() {}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateInsertTextFunction::Run() {
  base::string16 text;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &text));
  if (!delegate()->InsertText(text))
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSendKeyEventFunction::Run() {
  std::unique_ptr<keyboard::SendKeyEvent::Params> params(
      keyboard::SendKeyEvent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(params->key_event.modifiers);
  const keyboard::VirtualKeyboardEvent& event = params->key_event;
  if (!delegate()->SendKeyEvent(keyboard::ToString(event.type),
                                event.char_value, event.key_code,
                                event.key_name, *event.modifiers)) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateHideKeyboardFunction::Run() {
  if (!delegate()->HideKeyboard())
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetHotrodKeyboardFunction::Run() {
  bool enable = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &enable));
  delegate()->SetHotrodKeyboard(enable);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateLockKeyboardFunction::Run() {
  bool lock = false;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(0, &lock));
  if (!delegate()->LockKeyboard(lock))
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateKeyboardLoadedFunction::Run() {
  if (!delegate()->OnKeyboardLoaded())
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateGetKeyboardConfigFunction::Run() {
  delegate()->GetKeyboardConfig(base::Bind(
      &VirtualKeyboardPrivateGetKeyboardConfigFunction::OnKeyboardConfig,
      this));
  return RespondLater();
}

void VirtualKeyboardPrivateGetKeyboardConfigFunction::OnKeyboardConfig(
    std::unique_ptr<base::DictionaryValue> results) {
  Respond(results ? OneArgument(std::move(results)) : Error(kUnknownError));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateOpenSettingsFunction::Run() {
  if (!delegate()->IsLanguageSettingsEnabled() ||
      !delegate()->ShowLanguageSettings()) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetContainerBehaviorFunction::Run() {
  std::unique_ptr<keyboard::SetContainerBehavior::Params> params =
      keyboard::SetContainerBehavior::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Optional<gfx::Rect> target_bounds(base::nullopt);
  if (params->options.bounds)
    target_bounds = KeyboardBoundsToRect(*params->options.bounds);

  if (!delegate()->SetVirtualKeyboardMode(
          params->options.mode, std::move(target_bounds),
          base::BindOnce(&VirtualKeyboardPrivateSetContainerBehaviorFunction::
                             OnSetContainerBehavior,
                         this)))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondLater();
}

void VirtualKeyboardPrivateSetContainerBehaviorFunction::OnSetContainerBehavior(
    bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetDraggableAreaFunction::Run() {
  std::unique_ptr<keyboard::SetDraggableArea::Params> params =
      keyboard::SetDraggableArea::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetDraggableArea(params->bounds))
    return RespondNow(Error(kSetDraggableAreaFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetKeyboardStateFunction::Run() {
  std::unique_ptr<keyboard::SetKeyboardState::Params> params =
      keyboard::SetKeyboardState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetRequestedKeyboardState(params->state))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetOccludedBoundsFunction::Run() {
  std::unique_ptr<keyboard::SetOccludedBounds::Params> params =
      keyboard::SetOccludedBounds::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> occluded_bounds;
  occluded_bounds.reserve(params->bounds_list.size());
  for (const auto& bounds : params->bounds_list)
    occluded_bounds.push_back(KeyboardBoundsToRect(bounds));

  if (!delegate()->SetOccludedBounds(occluded_bounds))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetHitTestBoundsFunction::Run() {
  std::unique_ptr<keyboard::SetHitTestBounds::Params> params =
      keyboard::SetHitTestBounds::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> hit_test_bounds;
  hit_test_bounds.reserve(params->bounds_list.size());
  for (const auto& bounds : params->bounds_list)
    hit_test_bounds.push_back(KeyboardBoundsToRect(bounds));

  if (!delegate()->SetHitTestBounds(hit_test_bounds))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetAreaToRemainOnScreenFunction::Run() {
  std::unique_ptr<keyboard::SetAreaToRemainOnScreen::Params> params =
      keyboard::SetAreaToRemainOnScreen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetAreaToRemainOnScreen(bounds))
    return RespondNow(Error(kSetAreaToRemainOnScreenFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardAPI::VirtualKeyboardAPI(content::BrowserContext* context) {
  delegate_ =
      ExtensionsAPIClient::Get()->CreateVirtualKeyboardDelegate(context);
}

VirtualKeyboardAPI::~VirtualKeyboardAPI() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>*
VirtualKeyboardAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
