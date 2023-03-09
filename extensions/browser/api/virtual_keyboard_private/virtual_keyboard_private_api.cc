// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard_private.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/clipboard/clipboard_history_item.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kGetClipboardHistoryFailed[] =
    "Getting the clipboard history failed";
const char kPasteClipboardItemFailed[] = "Pasting the clipboard item failed";
const char kDeleteClipboardItemFailed[] = "Deleting the clipboard item failed";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace keyboard = api::virtual_keyboard_private;

gfx::Rect KeyboardBoundsToRect(const keyboard::Bounds& bounds) {
  return {bounds.left, bounds.top, bounds.width, bounds.height};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
base::Value::Dict SerializeClipboardHistoryItem(
    const ui::ColorProvider& color_provider,
    const ash::ClipboardHistoryItem& item) {
  using extensions::api::virtual_keyboard_private::DisplayFormat;

  extensions::api::virtual_keyboard_private::ClipboardItem clipboard_item;
  clipboard_item.id = item.id().ToString();
  clipboard_item.time_copied = item.time_copied().ToJsTimeIgnoringNull();
  if (const auto& maybe_image = item.display_image()) {
    clipboard_item.image_data =
        webui::GetBitmapDataUrl(*maybe_image->GetImage().ToSkBitmap());
  }

  switch (item.display_format()) {
    case ash::ClipboardHistoryItem::DisplayFormat::kText:
      clipboard_item.text_data = base::UTF16ToUTF8(item.display_text());
      clipboard_item.display_format = DisplayFormat::DISPLAY_FORMAT_TEXT;
      break;
    case ash::ClipboardHistoryItem::DisplayFormat::kPng:
      clipboard_item.display_format = DisplayFormat::DISPLAY_FORMAT_PNG;
      break;
    case ash::ClipboardHistoryItem::DisplayFormat::kHtml:
      clipboard_item.display_format = DisplayFormat::DISPLAY_FORMAT_HTML;
      break;
    case ash::ClipboardHistoryItem::DisplayFormat::kFile:
      DCHECK(!clipboard_item.image_data.has_value());

      const auto& icon = item.icon();
      DCHECK(icon.has_value());

      clipboard_item.image_data =
          webui::GetBitmapDataUrl(*icon->Rasterize(&color_provider).bitmap());
      clipboard_item.text_data = base::UTF16ToUTF8(item.display_text());
      clipboard_item.display_format = DisplayFormat::DISPLAY_FORMAT_FILE;
      break;
  }

  return clipboard_item.ToValue();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  absl::optional<keyboard::SendKeyEvent::Params> params =
      keyboard::SendKeyEvent::Params::Create(args());
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
  Respond(results ? WithArguments(std::move(*results)) : Error(kUnknownError));
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
  absl::optional<keyboard::SetContainerBehavior::Params> params =
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
  Respond(WithArguments(success));
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetDraggableAreaFunction::Run() {
  absl::optional<keyboard::SetDraggableArea::Params> params =
      keyboard::SetDraggableArea::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetDraggableArea(params->bounds))
    return RespondNow(Error(kSetDraggableAreaFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetKeyboardStateFunction::Run() {
  absl::optional<keyboard::SetKeyboardState::Params> params =
      keyboard::SetKeyboardState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetRequestedKeyboardState(params->state))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetOccludedBoundsFunction::Run() {
  absl::optional<keyboard::SetOccludedBounds::Params> params =
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
  absl::optional<keyboard::SetHitTestBounds::Params> params =
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
  absl::optional<keyboard::SetAreaToRemainOnScreen::Params> params =
      keyboard::SetAreaToRemainOnScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetAreaToRemainOnScreen(bounds))
    return RespondNow(Error(kSetAreaToRemainOnScreenFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetWindowBoundsInScreenFunction::Run() {
  absl::optional<keyboard::SetWindowBoundsInScreen::Params> params =
      keyboard::SetWindowBoundsInScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds_in_screen = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetWindowBoundsInScreen(bounds_in_screen))
    return RespondNow(Error(kSetWindowBoundsInScreenFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateSetWindowBoundsInScreenFunction ::
    ~VirtualKeyboardPrivateSetWindowBoundsInScreenFunction() = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
ExtensionFunction::ResponseAction
VirtualKeyboardPrivateGetClipboardHistoryFunction::Run() {
  absl::optional<keyboard::GetClipboardHistory::Params> params =
      keyboard::GetClipboardHistory::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  delegate()->GetClipboardHistory(base::BindOnce(
      &VirtualKeyboardPrivateGetClipboardHistoryFunction::OnGetClipboardHistory,
      this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void VirtualKeyboardPrivateGetClipboardHistoryFunction::OnGetClipboardHistory(
    std::vector<ash::ClipboardHistoryItem> items) {
  const auto* web_contents = GetSenderWebContents();
  if (!web_contents) {
    Respond(Error(kGetClipboardHistoryFailed));
    return;
  }

  base::Value::List results;
  for (const auto& item : items) {
    results.Append(
        SerializeClipboardHistoryItem(web_contents->GetColorProvider(), item));
  }

  Respond(WithArguments(std::move(results)));
}

VirtualKeyboardPrivateGetClipboardHistoryFunction ::
    ~VirtualKeyboardPrivateGetClipboardHistoryFunction() = default;

ExtensionFunction::ResponseAction
VirtualKeyboardPrivatePasteClipboardItemFunction::Run() {
  absl::optional<keyboard::PasteClipboardItem::Params> params =
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
  absl::optional<keyboard::DeleteClipboardItem::Params> params =
      keyboard::DeleteClipboardItem::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!delegate()->DeleteClipboardItem(params->item_id))
    return RespondNow(Error(kDeleteClipboardItemFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateDeleteClipboardItemFunction ::
    ~VirtualKeyboardPrivateDeleteClipboardItemFunction() = default;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

VirtualKeyboardAPI::VirtualKeyboardAPI(content::BrowserContext* context) {
  delegate_ =
      ExtensionsAPIClient::Get()->CreateVirtualKeyboardDelegate(context);
}

VirtualKeyboardAPI::~VirtualKeyboardAPI() = default;

static base::LazyInstance<BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VirtualKeyboardAPI>*
VirtualKeyboardAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
