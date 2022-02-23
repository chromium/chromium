/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/modules/plugins/dom_plugin.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

DOMPlugin::DOMPlugin(LocalDOMWindow* window, const PluginInfo& plugin_info)
    : ExecutionContextClient(window), plugin_info_(&plugin_info) {}

void DOMPlugin::Trace(Visitor* visitor) const {
  visitor->Trace(plugin_info_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

String DOMPlugin::name() const {
  return plugin_info_->Name();
}

String DOMPlugin::filename() const {
  return plugin_info_->Filename();
}

String DOMPlugin::description() const {
  return plugin_info_->Description();
}

unsigned DOMPlugin::length() const {
  return plugin_info_->GetMimeClassInfoSize();
}

DOMMimeType* DOMPlugin::item(unsigned index) {
  const MimeClassInfo* mime = plugin_info_->GetMimeClassInfo(index);

  if (!mime)
    return nullptr;

  return MakeGarbageCollected<DOMMimeType>(DomWindow(), *mime);
}

DOMMimeType* DOMPlugin::namedItem(const AtomicString& property_name) {
  const MimeClassInfo* mime = plugin_info_->GetMimeClassInfo(property_name);

  if (!mime)
    return nullptr;

  return MakeGarbageCollected<DOMMimeType>(DomWindow(), *mime);
}

void DOMPlugin::NamedPropertyEnumerator(Vector<String>& property_names,
                                        ExceptionState&) const {
  property_names.ReserveInitialCapacity(plugin_info_->GetMimeClassInfoSize());
  for (const MimeClassInfo* mime_info : plugin_info_->Mimes()) {
    property_names.UncheckedAppend(mime_info->Type());
  }
}

bool DOMPlugin::NamedPropertyQuery(const AtomicString& property_name,
                                   ExceptionState&) const {
  return plugin_info_->GetMimeClassInfo(property_name);
}

}  // namespace blink
