/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_dev_tools_host.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/context_menu_data/menu_item_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/inspector/dev_tools_host.h"
#include "third_party/blink/renderer/core/inspector/inspector_frontend_client.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void V8DevToolsHost::PlatformMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
#if BUILDFLAG(IS_MAC)
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "mac"));
#elif BUILDFLAG(IS_WIN)
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "windows"));
#else  // Unix-like systems
  V8SetReturnValue(info, V8AtomicString(info.GetIsolate(), "linux"));
#endif
}

static bool PopulateContextMenuItems(v8::Isolate* isolate,
                                     const v8::Local<v8::Array>& item_array,
                                     std::vector<MenuItemInfo>& items) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  for (uint32_t i = 0; i < item_array->Length(); ++i) {
    v8::Local<v8::Value> item_value =
        item_array->Get(context, i).ToLocalChecked();
    if (!item_value->IsObject()) {
      return false;
    }
    v8::Local<v8::Object> item = item_value.As<v8::Object>();

    v8::Local<v8::Value> type;
    v8::Local<v8::Value> id;
    v8::Local<v8::Value> label;
    v8::Local<v8::Value> enabled;
    v8::Local<v8::Value> checked;
    v8::Local<v8::Value> sub_items;
    if (!item->Get(context, V8AtomicString(isolate, "type")).ToLocal(&type) ||
        !item->Get(context, V8AtomicString(isolate, "id")).ToLocal(&id) ||
        !item->Get(context, V8AtomicString(isolate, "label")).ToLocal(&label) ||
        !item->Get(context, V8AtomicString(isolate, "enabled"))
             .ToLocal(&enabled) ||
        !item->Get(context, V8AtomicString(isolate, "checked"))
             .ToLocal(&checked) ||
        !item->Get(context, V8AtomicString(isolate, "subItems"))
             .ToLocal(&sub_items))
      return false;
    if (!type->IsString())
      continue;
    String type_string = ToCoreStringWithNullCheck(type.As<v8::String>());
    items.reserve(items.size() + 1);
    items.emplace_back();
    MenuItemInfo& item_info = items[items.size() - 1];
    if (type_string == "separator") {
      item_info.type = MenuItemInfo::kSeparator;
      item_info.enabled = true;
      item_info.action = DevToolsHost::kMaxContextMenuAction;
    } else if (type_string == "subMenu" && sub_items->IsArray()) {
      item_info.type = MenuItemInfo::kSubMenu;
      item_info.enabled = true;
      item_info.action = DevToolsHost::kMaxContextMenuAction;
      v8::Local<v8::Array> sub_items_array =
          v8::Local<v8::Array>::Cast(sub_items);
      if (!PopulateContextMenuItems(isolate, sub_items_array,
                                    item_info.sub_menu_items))
        return false;
      TOSTRING_DEFAULT(V8StringResource<kTreatNullAsNullString>, label_string,
                       label, false);
      item_info.label = base::UTF8ToUTF16(String(label_string).Utf8());
    } else {
      int32_t int32_id;
      if (!id->Int32Value(context).To(&int32_id) || int32_id < 0 ||
          int32_id >= static_cast<int>(DevToolsHost::kMaxContextMenuAction))
        return false;
      TOSTRING_DEFAULT(V8StringResource<kTreatNullAsNullString>, label_string,
                       label, false);
      if (type_string == "checkbox")
        item_info.type = MenuItemInfo::kCheckableOption;
      else
        item_info.type = MenuItemInfo::kOption;
      item_info.label = base::UTF8ToUTF16(String(label_string).Utf8());
      item_info.enabled = true;
      item_info.action = int32_id;
      if (checked->IsBoolean())
        item_info.checked = checked.As<v8::Boolean>()->Value();
      if (enabled->IsBoolean())
        item_info.enabled = enabled.As<v8::Boolean>()->Value();
    }
  }
  return true;
}

void V8DevToolsHost::ShowContextMenuAtPointMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 3)
    return;

  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "DevToolsHost", "showContextMenuAtPoint");
  v8::Isolate* isolate = info.GetIsolate();

  float x = ToRestrictedFloat(isolate, info[0], exception_state);
  if (exception_state.HadException())
    return;
  float y = ToRestrictedFloat(isolate, info[1], exception_state);
  if (exception_state.HadException())
    return;

  v8::Local<v8::Value> array = info[2];
  if (!array->IsArray())
    return;
  std::vector<MenuItemInfo> items;
  if (!PopulateContextMenuItems(isolate, v8::Local<v8::Array>::Cast(array),
                                items))
    return;

  Document* document = nullptr;
  if (info.Length() >= 4 && info[3]->IsObject()) {
    document = V8HTMLDocument::ToWrappable(isolate, info[3]);
  } else {
    LocalDOMWindow* window = EnteredDOMWindow(isolate);
    document = window ? window->document() : nullptr;
  }
  if (!document || !document->GetFrame())
    return;

  DevToolsHost* devtools_host =
      V8DevToolsHost::ToWrappableUnsafe(info.Holder());
  devtools_host->ShowContextMenu(document->GetFrame(), x, y, std::move(items));
}

}  // namespace blink
