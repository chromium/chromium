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

DOMMimeTypeArray::DOMMimeTypeArray(LocalDOMWindow* window) {
  if (window) {
    dom_mime_types_ = NavigatorPlugins::plugins(*window->navigator())
                          ->GetFixedMimeTypeArray();
  }
}

void DOMMimeTypeArray::Trace(Visitor* visitor) const {
  visitor->Trace(dom_mime_types_);
  ScriptWrappable::Trace(visitor);
}

unsigned DOMMimeTypeArray::length() const {
  return dom_mime_types_.size();
}

DOMMimeType* DOMMimeTypeArray::item(unsigned index) {
  if (index >= dom_mime_types_.size()) {
    return nullptr;
  }
  return dom_mime_types_[index].Get();
}

DOMMimeType* DOMMimeTypeArray::namedItem(const AtomicString& property_name) {
  for (const auto& mimetype : dom_mime_types_) {
    if (mimetype->type() == property_name) {
      return mimetype.Get();
    }
  }
  return nullptr;
}

void DOMMimeTypeArray::NamedPropertyEnumerator(Vector<String>& property_names,
                                               ExceptionState&) const {
  property_names.ReserveInitialCapacity(dom_mime_types_.size());
  for (const auto& mimetype : dom_mime_types_) {
    property_names.UncheckedAppend(mimetype->type());
  }
}

bool DOMMimeTypeArray::NamedPropertyQuery(const AtomicString& property_name,
                                          ExceptionState&) const {
  return base::Contains(dom_mime_types_, property_name, &DOMMimeType::type);
}

}  // namespace blink
