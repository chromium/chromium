/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/embedded_object_painter.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

LayoutEmbeddedObject::LayoutEmbeddedObject(HTMLFrameOwnerElement* element)
    : LayoutEmbeddedContent(element) {
  View()->GetFrameView()->SetIsVisuallyNonEmpty();
}

LayoutEmbeddedObject::~LayoutEmbeddedObject() = default;

static String LocalizedUnavailablePluginReplacementText(
    Node* node,
    LayoutEmbeddedObject::PluginAvailability availability) {
  Locale& locale =
      node ? To<Element>(node)->GetLocale() : Locale::DefaultLocale();
  switch (availability) {
    case LayoutEmbeddedObject::kPluginAvailable:
      break;
    case LayoutEmbeddedObject::kPluginMissing:
      return locale.QueryString(IDS_PLUGIN_INITIALIZATION_ERROR);
    case LayoutEmbeddedObject::kPluginBlockedByContentSecurityPolicy:
      return String();  // There is no matched resource_id for
                        // kPluginBlockedByContentSecurityPolicy yet. Return an
                        // empty String(). See crbug.com/302130 for more
                        // details.
  }
  NOTREACHED_IN_MIGRATION();
  return String();
}

void LayoutEmbeddedObject::SetPluginAvailability(
    PluginAvailability availability) {
  NOT_DESTROYED();
  DCHECK_EQ(kPluginAvailable, plugin_availability_);
  plugin_availability_ = availability;

  unavailable_plugin_replacement_text_ =
      LocalizedUnavailablePluginReplacementText(GetNode(), availability);

  // node() is nullptr when LayoutEmbeddedContent is being destroyed.
  if (GetNode())
    SetShouldDoFullPaintInvalidation();
}

bool LayoutEmbeddedObject::ShowsUnavailablePluginIndicator() const {
  NOT_DESTROYED();
  return plugin_availability_ != kPluginAvailable;
}

void LayoutEmbeddedObject::PaintReplaced(
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  EmbeddedObjectPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutEmbeddedObject::UpdateAfterLayout() {
  NOT_DESTROYED();
  LayoutEmbeddedContent::UpdateAfterLayout();
  if (!GetEmbeddedContentView() && GetFrameView())
    GetFrameView()->AddPartToUpdate(*this);
}

void LayoutEmbeddedObject::ComputeIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  NOT_DESTROYED();
  DCHECK(!ShouldApplySizeContainment());
  FrameView* frame_view = ChildFrameView();
  if (frame_view && frame_view->GetIntrinsicSizingInfo(intrinsic_sizing_info)) {
    // Scale based on our zoom as the embedded document doesn't have that info.
    intrinsic_sizing_info.size.Scale(StyleRef().EffectiveZoom());
    return;
  }

  LayoutEmbeddedContent::ComputeIntrinsicSizingInfo(intrinsic_sizing_info);
}

}  // namespace blink
