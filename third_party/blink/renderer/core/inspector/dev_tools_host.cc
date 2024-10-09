/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/dev_tools_host.h"

#include <utility>

#include "base/json/json_reader.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_show_context_menu_item.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/inspector/inspector_frontend_client.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/context_menu_controller.h"
#include "third_party/blink/renderer/core/page/context_menu_provider.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

class FrontendMenuProvider final : public ContextMenuProvider {
 public:
  FrontendMenuProvider(DevToolsHost* devtools_host,
                       WebVector<MenuItemInfo> items)
      : devtools_host_(devtools_host), items_(std::move(items)) {}
  ~FrontendMenuProvider() override {
    // Verify that this menu provider has been detached.
    DCHECK(!devtools_host_);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(devtools_host_);
    ContextMenuProvider::Trace(visitor);
  }

  void Disconnect() { devtools_host_ = nullptr; }

  void ContextMenuCleared() override {
    if (devtools_host_) {
      devtools_host_->EvaluateScript("DevToolsAPI.contextMenuCleared()");
      devtools_host_->ClearMenuProvider();
      devtools_host_ = nullptr;
    }
    items_.clear();
  }

  WebVector<MenuItemInfo> PopulateContextMenu() override {
    return std::move(items_);
  }

  void ContextMenuItemSelected(unsigned action) override {
    if (!devtools_host_ || action >= DevToolsHost::kMaxContextMenuAction)
      return;
    devtools_host_->EvaluateScript("DevToolsAPI.contextMenuItemSelected(" +
                                   String::Number(action) + ")");
  }

 private:
  Member<DevToolsHost> devtools_host_;
  WebVector<MenuItemInfo> items_;
};

DevToolsHost::DevToolsHost(InspectorFrontendClient* client,
                           LocalFrame* frontend_frame)
    : client_(client),
      frontend_frame_(frontend_frame),
      menu_provider_(nullptr) {}

DevToolsHost::~DevToolsHost() = default;

void DevToolsHost::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  visitor->Trace(frontend_frame_);
  visitor->Trace(menu_provider_);
  ScriptWrappable::Trace(visitor);
}

void DevToolsHost::EvaluateScript(const String& expression) {
  if (ScriptForbiddenScope::IsScriptForbidden())
    return;
  if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
    CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  } else {
    DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  }
  ClassicScript::CreateUnspecifiedScript(expression,
                                         ScriptSourceLocationType::kInternal)
      ->RunScriptOnScriptState(ToScriptStateForMainWorld(frontend_frame_));
}

void DevToolsHost::DisconnectClient() {
  client_ = nullptr;
  if (menu_provider_) {
    menu_provider_->Disconnect();
    menu_provider_ = nullptr;
  }
  frontend_frame_ = nullptr;
}

float DevToolsHost::zoomFactor() {
  if (!frontend_frame_)
    return 1;
  float zoom_factor = frontend_frame_->LayoutZoomFactor();
  // Cancel the device scale factor applied to the zoom factor.
  const ChromeClient* client =
      frontend_frame_->View()->GetChromeClient();
  float window_to_viewport_ratio =
      client->WindowToViewportScalar(frontend_frame_, 1.0f);
  return zoom_factor / window_to_viewport_ratio;
}

void DevToolsHost::copyText(const String& text) {
  frontend_frame_->GetSystemClipboard()->WritePlainText(text);
  frontend_frame_->GetSystemClipboard()->CommitWrite();
}

String DevToolsHost::platform() const {
#if BUILDFLAG(IS_MAC)
  return "mac";
#elif BUILDFLAG(IS_WIN)
  return "windows";
#else  // Unix-like systems
  return "linux";
#endif
}

void DevToolsHost::sendMessageToEmbedder(const String& message) {
  if (client_) {
    // Strictly convert, as we expect message to be serialized JSON.
    auto value = base::JSONReader::Read(
        message.Utf8(WTF::UTF8ConversionMode::kStrictUTF8Conversion));
    if (!value || !value->is_dict()) {
      ScriptState* script_state = ToScriptStateForMainWorld(frontend_frame_);
      if (!script_state)
        return;
      V8ThrowException::ThrowTypeError(
          script_state->GetIsolate(),
          value ? "Message to embedder must deserialize to a dictionary value"
                : "Message to embedder couldn't be JSON-deserialized");
      return;
    }
    client_->SendMessageToEmbedder(std::move(*value).TakeDict());
  }
}

void DevToolsHost::sendMessageToEmbedder(base::Value::Dict message) {
  if (client_)
    client_->SendMessageToEmbedder(std::move(message));
}

static std::vector<MenuItemInfo> PopulateContextMenuItems(
    const HeapVector<Member<ShowContextMenuItem>>& item_array) {
  std::vector<MenuItemInfo> items;
  for (auto& item : item_array) {
    MenuItemInfo& item_info = items.emplace_back();

    if (item->type() == "separator") {
      item_info.type = MenuItemInfo::kSeparator;
      item_info.enabled = true;
      item_info.action = DevToolsHost::kMaxContextMenuAction;
    } else if (item->type() == "subMenu" && item->hasSubItems()) {
      item_info.type = MenuItemInfo::kSubMenu;
      item_info.enabled = true;
      item_info.action = DevToolsHost::kMaxContextMenuAction;
      item_info.sub_menu_items = PopulateContextMenuItems(item->subItems());
      String label = item->getLabelOr(String());
      label.Ensure16Bit();
      item_info.label = std::u16string(label.Characters16(), label.length());
    } else {
      if (!item->hasId() || item->id() >= DevToolsHost::kMaxContextMenuAction) {
        return std::vector<MenuItemInfo>();
      }

      if (item->type() == "checkbox") {
        item_info.type = MenuItemInfo::kCheckableOption;
      } else {
        item_info.type = MenuItemInfo::kOption;
      }
      String label = item->getLabelOr(String());
      label.Ensure16Bit();
      item_info.label = std::u16string(label.Characters16(), label.length());
      item_info.is_experimental_feature = item->isExperimentalFeature();
      item_info.enabled = item->enabled();
      item_info.action = item->id();
      item_info.checked = item->checked();
    }
  }
  return items;
}

void DevToolsHost::showContextMenuAtPoint(
    v8::Isolate* isolate,
    float x,
    float y,
    const HeapVector<Member<ShowContextMenuItem>>& items,
    Document* document) {
  DCHECK(frontend_frame_);

  LocalFrame* target_frame = nullptr;
  if (document) {
    target_frame = document->GetFrame();
  } else if (LocalDOMWindow* window = EnteredDOMWindow(isolate)) {
    target_frame = window->GetFrame();
  }
  if (!target_frame) {
    return;
  }

  std::vector<MenuItemInfo> menu_items = PopulateContextMenuItems(items);
  auto* menu_provider =
      MakeGarbageCollected<FrontendMenuProvider>(this, std::move(menu_items));
  menu_provider_ = menu_provider;
  float zoom = target_frame->LayoutZoomFactor();
  {
    ContextMenuAllowedScope scope;
    target_frame->GetPage()->GetContextMenuController().ClearContextMenu();
    target_frame->GetPage()->GetContextMenuController().ShowContextMenuAtPoint(
        target_frame, x * zoom, y * zoom, menu_provider);
  }
}

bool DevToolsHost::isHostedMode() {
  return false;
}

}  // namespace blink
