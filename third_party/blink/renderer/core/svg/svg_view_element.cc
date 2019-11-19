/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_view_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SVGViewElement::SVGViewElement(Document& document)
    : SVGElement(svg_names::kViewTag, document), SVGFitToViewBox(this) {
  UseCounter::Count(document, WebFeature::kSVGViewElement);
}

void SVGViewElement::Trace(blink::Visitor* visitor) {
  SVGElement::Trace(visitor);
  SVGFitToViewBox::Trace(visitor);
}

void SVGViewElement::ParseAttribute(const AttributeModificationParams& params) {
  if (SVGZoomAndPan::ParseAttribute(params.name, params.new_value))
    return;

  SVGElement::ParseAttribute(params);
}

}  // namespace blink
