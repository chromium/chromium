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

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DOMPluginArray::DOMPluginArray(LocalDOMWindow* window,
                               bool should_return_fixed_plugin_data)
    : ExecutionContextLifecycleObserver(window),
      PluginsChangedObserver(window ? window->GetFrame()->GetPage() : nullptr),
      should_return_fixed_plugin_data_(should_return_fixed_plugin_data) {
  UpdatePluginData();
}

void DOMPluginArray::Trace(Visitor* visitor) const {
  visitor->Trace(dom_plugins_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  PluginsChangedObserver::Trace(visitor);
}

unsigned DOMPluginArray::length() const {
  return dom_plugins_.size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  if (index >= dom_plugins_.size())
    return nullptr;

  if (!dom_plugins_[index]) {
    if (should_return_fixed_plugin_data_)
      return nullptr;
    dom_plugins_[index] = MakeGarbageCollected<DOMPlugin>(
        DomWindow(), *GetPluginData()->Plugins()[index]);
  }

  return dom_plugins_[index].Get();
}

DOMPlugin* DOMPluginArray::namedItem(const AtomicString& property_name) {
  if (should_return_fixed_plugin_data_) {
    for (const auto& plugin : dom_plugins_) {
      if (plugin->name() == property_name)
        return plugin.Get();
    }
    return nullptr;
  }
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
  if (should_return_fixed_plugin_data_) {
    property_names.ReserveInitialCapacity(dom_plugins_.size());
    for (const auto& plugin : dom_plugins_)
      property_names.UncheckedAppend(plugin->name());
    return;
  }
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
  if (!DomWindow())
    return;

  PluginData::RefreshBrowserSidePluginCache();
  if (PluginData* data = GetPluginData())
    data->ResetPluginData();

  for (Frame* frame = DomWindow()->GetFrame()->GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    Navigator& navigator = *local_frame->DomWindow()->navigator();
    NavigatorPlugins::plugins(navigator)->UpdatePluginData();
    NavigatorPlugins::mimeTypes(navigator)->UpdatePluginData();
  }

  if (reload)
    DomWindow()->GetFrame()->Reload(WebFrameLoadType::kReload);
}

PluginData* DOMPluginArray::GetPluginData() const {
  return DomWindow() ? DomWindow()->GetFrame()->GetPluginData() : nullptr;
}

namespace {
DOMPlugin* MakeFakePlugin(String plugin_name, LocalDOMWindow* window) {
  String description = "Portable Document Format";
  String filename = "internal-pdf-viewer";
  auto* plugin_info =
      MakeGarbageCollected<PluginInfo>(plugin_name, filename, description,
                                       /*background_color=*/Color::kTransparent,
                                       /*may_use_external_handler=*/false);
  Vector<String> extensions{"pdf"};
  for (const char* mime_type : {"application/pdf", "text/pdf"}) {
    auto* mime_info = MakeGarbageCollected<MimeClassInfo>(
        mime_type, description, *plugin_info, extensions);
    plugin_info->AddMimeType(mime_info);
  }
  return MakeGarbageCollected<DOMPlugin>(window, *plugin_info);
}
}  // namespace

HeapVector<Member<DOMMimeType>> DOMPluginArray::GetFixedMimeTypeArray() {
  DCHECK(should_return_fixed_plugin_data_);
  HeapVector<Member<DOMMimeType>> mimetypes;
  if (dom_plugins_.empty())
    return mimetypes;
  DCHECK_EQ(dom_plugins_[0]->length(), 2u);
  mimetypes.push_back(dom_plugins_[0]->item(0));
  mimetypes.push_back(dom_plugins_[0]->item(1));
  return mimetypes;
}

bool DOMPluginArray::IsPdfViewerAvailable() {
  auto* data = GetPluginData();
  if (!data)
    return false;
  for (const Member<MimeClassInfo>& mime_info : data->Mimes()) {
    if (mime_info->Type() == "application/pdf")
      return true;
  }
  return false;
}

void DOMPluginArray::UpdatePluginData() {
  if (should_return_fixed_plugin_data_) {
    dom_plugins_.clear();
    if (IsPdfViewerAvailable()) {
      // See crbug.com/1164635 and https://github.com/whatwg/html/pull/6738.
      // To reduce fingerprinting and make plugins/mimetypes more
      // interoperable, this is the spec'd, hard-coded list of plugins:
      Vector<String> plugins{"PDF Viewer", "Chrome PDF Viewer",
                             "Chromium PDF Viewer", "Microsoft Edge PDF Viewer",
                             "WebKit built-in PDF"};
      for (auto name : plugins)
        dom_plugins_.push_back(MakeFakePlugin(name, DomWindow()));
    }
    return;
  }
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

void DOMPluginArray::ContextDestroyed() {
  dom_plugins_.clear();
}

void DOMPluginArray::PluginsChanged() {
  UpdatePluginData();
}

}  // namespace blink
