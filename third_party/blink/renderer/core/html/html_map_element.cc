/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_map_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLMapElement::HTMLMapElement(Document& document)
    : HTMLElement(html_names::kMapTag, document) {
  UseCounter::Count(document, WebFeature::kMapElement);
}

HTMLMapElement::~HTMLMapElement() = default;

HTMLAreaElement* HTMLMapElement::AreaForPoint(
    const PhysicalOffset& location,
    const LayoutObject* container_object) {
  HTMLAreaElement* default_area = nullptr;
  for (HTMLAreaElement& area :
       Traversal<HTMLAreaElement>::DescendantsOf(*this)) {
    if (area.IsDefault() && !default_area)
      default_area = &area;
    else if (area.PointInArea(location, container_object))
      return &area;
  }

  return default_area;
}

HTMLImageElement* HTMLMapElement::ImageElement() {
  HTMLCollection* images = GetDocument().images();
  for (unsigned i = 0; Element* curr = images->item(i); ++i) {
    // The HTMLImageElement's useMap() value includes the '#' symbol at the
    // beginning, which has to be stripped off.
    auto& image_element = To<HTMLImageElement>(*curr);
    String use_map_name =
        image_element.FastGetAttribute(html_names::kUsemapAttr)
            .GetString()
            .Substring(1);
    if (use_map_name == name_)
      return &image_element;
  }

  return nullptr;
}

void HTMLMapElement::ParseAttribute(const AttributeModificationParams& params) {
  // FIXME: This logic seems wrong for XML documents.
  // Either the id or name will be used depending on the order the attributes
  // are parsed.

  if (params.name == html_names::kIdAttr ||
      params.name == html_names::kNameAttr) {
    if (params.name == html_names::kIdAttr) {
      // Call base class so that hasID bit gets set.
      HTMLElement::ParseAttribute(params);
      if (IsA<HTMLDocument>(GetDocument()))
        return;
    }
    if (isConnected())
      GetTreeScope().RemoveImageMap(*this);
    String map_name = params.new_value;
    if (map_name[0] == '#')
      map_name = map_name.Substring(1);
    name_ = AtomicString(map_name);
    if (isConnected())
      GetTreeScope().AddImageMap(*this);

    return;
  }

  HTMLElement::ParseAttribute(params);
}

HTMLCollection* HTMLMapElement::areas() {
  return EnsureCachedCollection<HTMLCollection>(kMapAreas);
}

Node::InsertionNotificationRequest HTMLMapElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected())
    GetTreeScope().AddImageMap(*this);
  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLMapElement::RemovedFrom(ContainerNode& insertion_point) {
  if (insertion_point.isConnected())
    GetTreeScope().RemoveImageMap(*this);
  HTMLElement::RemovedFrom(insertion_point);
}

}  // namespace blink
