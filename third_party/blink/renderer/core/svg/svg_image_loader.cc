/*
 * Copyright (C) 2005, 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_image_loader.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"

namespace blink {

SVGImageLoader::SVGImageLoader(SVGImageElement* node) : ImageLoader(node) {}

void SVGImageLoader::DispatchLoadEvent() {
  if (GetContent()->ErrorOccurred()) {
    DispatchErrorEvent();
    return;
  }

  auto* image_element = To<SVGImageElement>(GetElement());
  image_element->SendSVGLoadEventToSelfAndAncestorChainIfPossible();
}

void SVGImageLoader::DispatchErrorEvent() {
  GetElement()->DispatchEvent(*Event::Create(event_type_names::kError));
}

}  // namespace blink
