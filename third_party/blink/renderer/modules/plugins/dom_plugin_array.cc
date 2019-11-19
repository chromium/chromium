/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DOMPluginArray::DOMPluginArray(LocalFrame* frame)
    : ContextLifecycleObserver(frame ? frame->GetDocument() : nullptr),
      PluginsChangedObserver(frame ? frame->GetPage() : nullptr) {
  UpdatePluginData();
}

void DOMPluginArray::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_plugins_);
  ScriptWrappable::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  PluginsChangedObserver::Trace(visitor);
}

unsigned DOMPluginArray::length() const {
  return dom_plugins_.size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  if (index >= dom_plugins_.size())
    return nullptr;

  if (!dom_plugins_[index]) {
    dom_plugins_[index] =
        DOMPlugin::Create(GetFrame(), *GetPluginData()->Plugins()[index]);
  }

  return dom_plugins_[index];
}

DOMPlugin* DOMPluginArray::namedItem(const AtomicString& property_name) {
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;

  for (const Member<PluginInfo>& plugin_info : data->Plugins()) {
    if (plugin_info->Name() == property_name) {
      unsigned index =
          static_cast<unsigned>(&plugin_info - &data->Plugins()[0]);
      return item(index);
    }
  }
  return nullptr;
}

void DOMPluginArray::NamedPropertyEnumerator(Vector<String>& property_names,
                                             ExceptionState&) const {
  PluginData* data = GetPluginData();
  if (!data)
    return;
  property_names.ReserveInitialCapacity(data->Plugins().size());
  for (const PluginInfo* plugin_info : data->Plugins()) {
    property_names.UncheckedAppend(plugin_info->Name());
  }
}

bool DOMPluginArray::NamedPropertyQuery(const AtomicString& property_name,
                                        ExceptionState& exception_state) const {
  Vector<String> properties;
  NamedPropertyEnumerator(properties, exception_state);
  return properties.Contains(property_name);
}

void DOMPluginArray::refresh(bool reload) {
  if (!GetFrame())
    return;

  PluginData::RefreshBrowserSidePluginCache();
  if (PluginData* data = GetPluginData())
    data->ResetPluginData();

  for (Frame* frame = GetFrame()->GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    Navigator& navigator = *local_frame->DomWindow()->navigator();
    NavigatorPlugins::plugins(navigator)->UpdatePluginData();
    NavigatorPlugins::mimeTypes(navigator)->UpdatePluginData();
  }

  if (reload)
    GetFrame()->Reload(WebFrameLoadType::kReload);
}

PluginData* DOMPluginArray::GetPluginData() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetPluginData();
}

void DOMPluginArray::UpdatePluginData() {
  PluginData* data = GetPluginData();
  if (!data) {
    dom_plugins_.clear();
    return;
  }

  HeapVector<Member<DOMPlugin>> old_dom_plugins(std::move(dom_plugins_));
  dom_plugins_.clear();
  dom_plugins_.resize(data->Plugins().size());

  for (Member<DOMPlugin>& plugin : old_dom_plugins) {
    if (plugin) {
      for (const Member<PluginInfo>& plugin_info : data->Plugins()) {
        if (plugin->name() == plugin_info->Name()) {
          unsigned index =
              static_cast<unsigned>(&plugin_info - &data->Plugins()[0]);
          dom_plugins_[index] = plugin;
        }
      }
    }
  }
}

void DOMPluginArray::ContextDestroyed(ExecutionContext*) {
  dom_plugins_.clear();
}

void DOMPluginArray::PluginsChanged() {
  UpdatePluginData();
}

}  // namespace blink
