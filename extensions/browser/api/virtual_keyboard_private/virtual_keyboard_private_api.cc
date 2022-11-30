// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
const char kSetWindowBoundsInScreenFailed[] =
    "Setting bounds of the virtual keyboard failed";
const char kUnknownError[] = "Unknown error.";
const char kPasteClipboardItemFailed[] = "Pasting the clipboard item failed";
const char kDeleteClipboardItemFailed[] = "Deleting the clipboard item failed";

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
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  std::u16string text = base::UTF8ToUTF16(args()[0].GetString());
  if (!delegate()->InsertText(text))
    return RespondNow(Error(kUnknownError));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSendKeyEventFunction::Run() {
  std::unique_ptr<keyboard::SendKeyEvent::Params> params(
      keyboard::SendKeyEvent::Params::Create(args()));
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
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool enable = args()[0].GetBool();
  delegate()->SetHotrodKeyboard(enable);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateLockKeyboardFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_bool());
  bool lock = args()[0].GetBool();
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
  delegate()->GetKeyboardConfig(base::BindOnce(
      &VirtualKeyboardPrivateGetKeyboardConfigFunction::OnKeyboardConfig,
      this));
  return RespondLater();
}

void VirtualKeyboardPrivateGetKeyboardConfigFunction::OnKeyboardConfig(
    absl::optional<base::Value::Dict> results) {
  Respond(results ? OneArgument(base::Value(std::move(*results)))
                  : Error(kUnknownError));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateOpenSettingsFunction::Run() {
  if (!delegate()->IsSettingsEnabled() || !delegate()->ShowLanguageSettings()) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateOpenSuggestionSettingsFunction::Run() {
  if (!delegate()->IsSettingsEnabled() ||
      !delegate()->ShowSuggestionSettings()) {
    return RespondNow(Error(kUnknownError));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetContainerBehaviorFunction::Run() {
  std::unique_ptr<keyboard::SetContainerBehavior::Params> params =
      keyboard::SetContainerBehavior::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  gfx::Rect target_bounds = KeyboardBoundsToRect(params->options.bounds);
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
  Respond(OneArgument(base::Value(success)));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetDraggableAreaFunction::Run() {
  std::unique_ptr<keyboard::SetDraggableArea::Params> params =
      keyboard::SetDraggableArea::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetDraggableArea(params->bounds))
    return RespondNow(Error(kSetDraggableAreaFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetKeyboardStateFunction::Run() {
  std::unique_ptr<keyboard::SetKeyboardState::Params> params =
      keyboard::SetKeyboardState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetRequestedKeyboardState(params->state))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetOccludedBoundsFunction::Run() {
  std::unique_ptr<keyboard::SetOccludedBounds::Params> params =
      keyboard::SetOccludedBounds::Params::Create(args());
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
      keyboard::SetHitTestBounds::Params::Create(args());
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
      keyboard::SetAreaToRemainOnScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetAreaToRemainOnScreen(bounds))
    return RespondNow(Error(kSetAreaToRemainOnScreenFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetWindowBoundsInScreenFunction::Run() {
  std::unique_ptr<keyboard::SetWindowBoundsInScreen::Params> params =
      keyboard::SetWindowBoundsInScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds_in_screen = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetWindowBoundsInScreen(bounds_in_screen))
    return RespondNow(Error(kSetWindowBoundsInScreenFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateSetWindowBoundsInScreenFunction ::
    ~VirtualKeyboardPrivateSetWindowBoundsInScreenFunction() = default;

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateGetClipboardHistoryFunction::Run() {
  std::unique_ptr<keyboard::GetClipboardHistory::Params> params =
      keyboard::GetClipboardHistory::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  std::set<std::string> item_id_filter;
  if (params->options.item_ids) {
    for (const auto& id : *(params->options.item_ids)) {
      item_id_filter.insert(id);
    }
  }

  delegate()->GetClipboardHistory(
      item_id_filter,
      base::BindOnce(&VirtualKeyboardPrivateGetClipboardHistoryFunction::
                         OnGetClipboardHistory,
                     this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void VirtualKeyboardPrivateGetClipboardHistoryFunction::OnGetClipboardHistory(
    base::Value results) {
  Respond(OneArgument(std::move(results)));
}

VirtualKeyboardPrivateGetClipboardHistoryFunction ::
    ~VirtualKeyboardPrivateGetClipboardHistoryFunction() = default;

ExtensionFunction::ResponseAction
VirtualKeyboardPrivatePasteClipboardItemFunction::Run() {
  std::unique_ptr<keyboard::PasteClipboardItem::Params> params =
      keyboard::PasteClipboardItem::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!delegate()->PasteClipboardItem(params->item_id))
    return RespondNow(Error(kPasteClipboardItemFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivatePasteClipboardItemFunction ::
    ~VirtualKeyboardPrivatePasteClipboardItemFunction() = default;

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateDeleteClipboardItemFunction::Run() {
  std::unique_ptr<keyboard::DeleteClipboardItem::Params> params =
      keyboard::DeleteClipboardItem::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!delegate()->DeleteClipboardItem(params->item_id))
    return RespondNow(Error(kDeleteClipboardItemFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateDeleteClipboardItemFunction ::
    ~VirtualKeyboardPrivateDeleteClipboardItemFunction() = default;

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
