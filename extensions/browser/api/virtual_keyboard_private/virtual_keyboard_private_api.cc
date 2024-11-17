// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/common/api/virtual_keyboard_private.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/clipboard/clipboard_history_item.h"
#include "base/barrier_closure.h"
#include "base/task/thread_pool.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
const char kGetClipboardHistoryFailed[] =
    "Getting the clipboard history failed";
const char kPasteClipboardItemFailed[] = "Pasting the clipboard item failed";
const char kDeleteClipboardItemFailed[] = "Deleting the clipboard item failed";
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace keyboard = api::virtual_keyboard_private;

gfx::Rect KeyboardBoundsToRect(const keyboard::Bounds& bounds) {
  return {bounds.left, bounds.top, bounds.width, bounds.height};
}

#if BUILDFLAG(IS_CHROMEOS)
using extensions::api::virtual_keyboard_private::ClipboardItem;
using extensions::api::virtual_keyboard_private::DisplayFormat;

// Appends a new item based on `history_item` to the list that `item_ptr` points
// to. If the conversion requires a bitmap to be encoded, some of the work will
// happen asynchronously; regardless, `barrier_callback` will be signaled when
// the conversion finishes.
void ConvertClipboardHistoryItemToClipboardItem(
    base::OnceClosure barrier_callback,
    const ui::ColorProvider& color_provider,
    const ash::ClipboardHistoryItem& history_item,
    std::vector<ClipboardItem>* items_ptr) {
  // Populate all `ClipboardItem` fields except `image_data`.
  ClipboardItem item;
  item.id = history_item.id().ToString();
  item.time_copied =
      history_item.time_copied().InMillisecondsFSinceUnixEpochIgnoringNull();

  switch (history_item.display_format()) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kUnknown:
      NOTREACHED();
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      item.text_data = base::UTF16ToUTF8(history_item.display_text());
      item.display_format = DisplayFormat::kText;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      item.display_format = DisplayFormat::kPng;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      item.display_format = DisplayFormat::kHtml;
      break;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      DCHECK(!item.image_data.has_value());

      const auto& icon = history_item.icon();
      DCHECK(icon.has_value());

      item.image_data =
          webui::GetBitmapDataUrl(*icon->Rasterize(&color_provider).bitmap());
      item.text_data = base::UTF16ToUTF8(history_item.display_text());
      item.display_format = DisplayFormat::kFile;
      break;
  }

  items_ptr->push_back(std::move(item));

  // Getting a data URL for a bitmap can be time-intensive. Populate
  // `image_data` asynchronously if `history_item` has image data.
  if (const auto& maybe_image = history_item.display_image()) {
    const auto* const bitmap =
        maybe_image->IsVectorIcon()
            ? maybe_image->Rasterize(&color_provider).bitmap()
            : maybe_image->GetImage().ToSkBitmap();
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure barrier_callback, ClipboardItem& item,
               const SkBitmap& bitmap) {
              item.image_data = webui::GetBitmapDataUrl(bitmap);
              std::move(barrier_callback).Run();
            },
            std::move(barrier_callback), std::ref(items_ptr->back()), *bitmap));
  } else {
    // Signal that the (non-existent) asynchronous conversion work is done.
    std::move(barrier_callback).Run();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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
  std::optional<keyboard::SendKeyEvent::Params> params =
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
    std::optional<base::Value::Dict> results) {
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
  std::optional<keyboard::SetContainerBehavior::Params> params =
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
  std::optional<keyboard::SetDraggableArea::Params> params =
      keyboard::SetDraggableArea::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetDraggableArea(params->bounds))
    return RespondNow(Error(kSetDraggableAreaFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetKeyboardStateFunction::Run() {
  std::optional<keyboard::SetKeyboardState::Params> params =
      keyboard::SetKeyboardState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!delegate()->SetRequestedKeyboardState(params->state))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetOccludedBoundsFunction::Run() {
  std::optional<keyboard::SetOccludedBounds::Params> params =
      keyboard::SetOccludedBounds::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> occluded_bounds;
  occluded_bounds.reserve(params->bounds_list.size());
  for (const auto& bounds : params->bounds_list) {
    occluded_bounds.push_back(KeyboardBoundsToRect(bounds));
  }

  if (!delegate()->SetOccludedBounds(occluded_bounds))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetHitTestBoundsFunction::Run() {
  std::optional<keyboard::SetHitTestBounds::Params> params =
      keyboard::SetHitTestBounds::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<gfx::Rect> hit_test_bounds;
  hit_test_bounds.reserve(params->bounds_list.size());
  for (const auto& bounds : params->bounds_list) {
    hit_test_bounds.push_back(KeyboardBoundsToRect(bounds));
  }

  if (!delegate()->SetHitTestBounds(hit_test_bounds))
    return RespondNow(Error(kVirtualKeyboardNotEnabled));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetAreaToRemainOnScreenFunction::Run() {
  std::optional<keyboard::SetAreaToRemainOnScreen::Params> params =
      keyboard::SetAreaToRemainOnScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetAreaToRemainOnScreen(bounds))
    return RespondNow(Error(kSetAreaToRemainOnScreenFailed));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
VirtualKeyboardPrivateSetWindowBoundsInScreenFunction::Run() {
  std::optional<keyboard::SetWindowBoundsInScreen::Params> params =
      keyboard::SetWindowBoundsInScreen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const gfx::Rect bounds_in_screen = KeyboardBoundsToRect(params->bounds);
  if (!delegate()->SetWindowBoundsInScreen(bounds_in_screen))
    return RespondNow(Error(kSetWindowBoundsInScreenFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateSetWindowBoundsInScreenFunction ::
    ~VirtualKeyboardPrivateSetWindowBoundsInScreenFunction() = default;

#if BUILDFLAG(IS_CHROMEOS)
ExtensionFunction::ResponseAction
VirtualKeyboardPrivateGetClipboardHistoryFunction::Run() {
  std::optional<keyboard::GetClipboardHistory::Params> params =
      keyboard::GetClipboardHistory::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  delegate()->GetClipboardHistory(base::BindOnce(
      &VirtualKeyboardPrivateGetClipboardHistoryFunction::OnGetClipboardHistory,
      this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void VirtualKeyboardPrivateGetClipboardHistoryFunction::OnGetClipboardHistory(
    std::vector<ash::ClipboardHistoryItem> history_items) {
  const auto* web_contents = GetSenderWebContents();
  if (!web_contents) {
    Respond(Error(kGetClipboardHistoryFailed));
    return;
  }

  // Create a container for converted clipboard items.
  // NOTE: Reserving space for all items is necessary to ensure that each item
  // stays at its original memory address while `items` is being populated.
  auto items = std::make_unique<ClipboardItems>();
  items->reserve(history_items.size());
  auto* items_ptr = items.get();

  // Post back to this sequence once all items have been fully converted.
  base::RepeatingClosure barrier = base::BarrierClosure(
      history_items.size(),
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&VirtualKeyboardPrivateGetClipboardHistoryFunction::
                             OnClipboardHistoryItemsConverted,
                         this, std::move(items))));

  // Convert each `ClipboardHistoryItem` into a `ClipboardItem`. A response with
  // serialized items will be sent in `OnClipboardHistoryItemsConverted()`.
  for (const auto& history_item : history_items) {
    ConvertClipboardHistoryItemToClipboardItem(
        barrier, web_contents->GetColorProvider(), history_item, items_ptr);
  }
}

void VirtualKeyboardPrivateGetClipboardHistoryFunction::
    OnClipboardHistoryItemsConverted(std::unique_ptr<ClipboardItems> items) {
  base::Value::List results;
  for (const auto& item : *items) {
    results.Append(item.ToValue());
  }
  Respond(WithArguments(std::move(results)));
}

VirtualKeyboardPrivateGetClipboardHistoryFunction ::
    ~VirtualKeyboardPrivateGetClipboardHistoryFunction() = default;

ExtensionFunction::ResponseAction
VirtualKeyboardPrivatePasteClipboardItemFunction::Run() {
  std::optional<keyboard::PasteClipboardItem::Params> params =
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
  std::optional<keyboard::DeleteClipboardItem::Params> params =
      keyboard::DeleteClipboardItem::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!delegate()->DeleteClipboardItem(params->item_id))
    return RespondNow(Error(kDeleteClipboardItemFailed));
  return RespondNow(NoArguments());
}

VirtualKeyboardPrivateDeleteClipboardItemFunction ::
    ~VirtualKeyboardPrivateDeleteClipboardItemFunction() = default;
#endif  // BUILDFLAG(IS_CHROMEOS)

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
