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

#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"

#include "base/containers/contains.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"
#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DOMMimeTypeArray::DOMMimeTypeArray(LocalDOMWindow* window,
                                   bool should_return_fixed_plugin_data)
    : ExecutionContextLifecycleObserver(window),
      PluginsChangedObserver(window ? window->GetFrame()->GetPage() : nullptr),
      should_return_fixed_plugin_data_(should_return_fixed_plugin_data) {
  UpdatePluginData();
}

void DOMMimeTypeArray::Trace(Visitor* visitor) const {
  visitor->Trace(dom_mime_types_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

unsigned DOMMimeTypeArray::length() const {
  return dom_mime_types_.size();
}

DOMMimeType* DOMMimeTypeArray::item(unsigned index) {
  if (index >= dom_mime_types_.size())
    return nullptr;
  if (!dom_mime_types_[index]) {
    dom_mime_types_[index] = MakeGarbageCollected<DOMMimeType>(
        DomWindow(), *GetPluginData()->Mimes()[index]);
  }

  return dom_mime_types_[index].Get();
}

DOMMimeType* DOMMimeTypeArray::namedItem(const AtomicString& property_name) {
  if (should_return_fixed_plugin_data_) {
    for (const auto& mimetype : dom_mime_types_) {
      if (mimetype->type() == property_name)
        return mimetype.Get();
    }
    return nullptr;
  }
  PluginData* data = GetPluginData();
  if (!data)
    return nullptr;

  for (const Member<MimeClassInfo>& mime : data->Mimes()) {
    if (mime->Type() == property_name) {
      unsigned index = static_cast<unsigned>(&mime - &data->Mimes()[0]);
      return item(index);
    }
  }
  return nullptr;
}

void DOMMimeTypeArray::NamedPropertyEnumerator(Vector<String>& property_names,
                                               ExceptionState&) const {
  if (should_return_fixed_plugin_data_) {
    property_names.ReserveInitialCapacity(dom_mime_types_.size());
    for (const auto& mimetype : dom_mime_types_)
      property_names.UncheckedAppend(mimetype->type());
    return;
  }
  PluginData* data = GetPluginData();
  if (!data)
    return;
  property_names.ReserveInitialCapacity(data->Mimes().size());
  for (const MimeClassInfo* mime_info : data->Mimes()) {
    property_names.UncheckedAppend(mime_info->Type());
  }
}

bool DOMMimeTypeArray::NamedPropertyQuery(const AtomicString& property_name,
                                          ExceptionState&) const {
  if (should_return_fixed_plugin_data_) {
    return base::Contains(dom_mime_types_, property_name, &DOMMimeType::type);
  }
  PluginData* data = GetPluginData();
  if (!data)
    return false;
  return data->SupportsMimeType(property_name);
}

PluginData* DOMMimeTypeArray::GetPluginData() const {
  if (!DomWindow())
    return nullptr;
  return DomWindow()->GetFrame()->GetPluginData();
}

void DOMMimeTypeArray::UpdatePluginData() {
  dom_mime_types_.clear();
  if (should_return_fixed_plugin_data_) {
    if (DomWindow()) {
      dom_mime_types_ = NavigatorPlugins::plugins(*DomWindow()->navigator())
                            ->GetFixedMimeTypeArray();
    }
    return;
  }
  PluginData* data = GetPluginData();
  if (!data)
    return;

  HeapVector<Member<DOMMimeType>> old_dom_mime_types(
      std::move(dom_mime_types_));
  dom_mime_types_.resize(data->Mimes().size());

  for (Member<DOMMimeType>& mime : old_dom_mime_types) {
    if (mime) {
      for (const Member<MimeClassInfo>& mime_info : data->Mimes()) {
        if (mime->type() == mime_info->Type()) {
          unsigned index =
              static_cast<unsigned>(&mime_info - &data->Mimes()[0]);
          dom_mime_types_[index] = mime;
        }
      }
    }
  }
}

void DOMMimeTypeArray::ContextDestroyed() {
  dom_mime_types_.clear();
}

void DOMMimeTypeArray::PluginsChanged() {
  UpdatePluginData();
}

}  // namespace blink
