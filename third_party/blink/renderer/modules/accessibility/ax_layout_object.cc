/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"

#include <algorithm>
#include <memory>
#include <string>

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/event_util.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/list/list_marker.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_mock_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/accessibility/ax_role_properties.h"

namespace blink {

AXLayoutObject::AXLayoutObject(LayoutObject* layout_object,
                               AXObjectCacheImpl& ax_object_cache)
    : AXNodeObject(layout_object->GetNode(), ax_object_cache),
      layout_object_(layout_object) {
// TODO(aleventhal) Get correct current state of autofill.
#if DCHECK_IS_ON()
  DCHECK(layout_object_);
  layout_object_->SetHasAXObject(true);
#endif
}

AXLayoutObject::~AXLayoutObject() {
  DCHECK(IsDetached());
}

void AXLayoutObject::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  AXNodeObject::Trace(visitor);
}

LayoutObject* AXLayoutObject::GetLayoutObject() const {
  return layout_object_.Get();
}

void AXLayoutObject::Detach() {
  AXNodeObject::Detach();

#if DCHECK_IS_ON()
  if (layout_object_)
    layout_object_->SetHasAXObject(false);
#endif
  layout_object_ = nullptr;
}

bool AXLayoutObject::IsAXLayoutObject() const {
  return true;
}

//
// Whether objects are ignored, i.e. not included in the tree.
//

//
// Properties of static elements.
//

//
// Properties of interactive elements.
//

String AXLayoutObject::TextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources) const {
  if (layout_object_) {
    std::optional<String> text_alternative = GetCSSAltText(GetElement());
    bool found_text_alternative = false;
    if (text_alternative) {
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = ax::mojom::blink::NameFrom::kAttribute;
        name_sources->back().text = text_alternative.value();
      }
      return text_alternative.value();
    }
    if (layout_object_->IsBR()) {
      text_alternative = String("\n");
      found_text_alternative = true;
    } else if (layout_object_->IsText() &&
               (!recursive || !layout_object_->IsCounter())) {
      auto* layout_text = To<LayoutText>(layout_object_.Get());
      String visible_text = layout_text->PlainText();  // Actual rendered text.
      // If no text boxes we assume this is unrendered end-of-line whitespace.
      // TODO find robust way to deterministically detect end-of-line space.
      if (visible_text.empty()) {
        // No visible rendered text -- must be whitespace.
        // Either it is useful whitespace for separating words or not.
        if (layout_text->IsAllCollapsibleWhitespace()) {
          if (LastKnownIsIgnoredValue())
            return "";
          // If no textboxes, this was whitespace at the line's end.
          text_alternative = " ";
        } else {
          text_alternative = layout_text->TransformedText();
        }
      } else {
        text_alternative = visible_text;
      }
      found_text_alternative = true;
    } else if (!recursive) {
      if (ListMarker* marker = ListMarker::Get(layout_object_)) {
        text_alternative = marker->TextAlternative(*layout_object_);
        found_text_alternative = true;
      }
    }

    if (found_text_alternative) {
      name_from = ax::mojom::blink::NameFrom::kContents;
      if (name_sources) {
        name_sources->push_back(NameSource(false));
        name_sources->back().type = name_from;
        name_sources->back().text = text_alternative.value();
      }
      // Ensure that text nodes count toward
      // kMaxDescendantsForTextAlternativeComputation when calculating the name
      // for their direct parent (see AXNodeObject::TextFromDescendants).
      visited.insert(this);
      return text_alternative.value();
    }
  }

  return AXNodeObject::TextAlternative(
      recursive, aria_label_or_description_root, visited, name_from,
      related_objects, name_sources);
}

//
// Hit testing.
//


//
// DOM and layout tree access.
//

Document* AXLayoutObject::GetDocument() const {
  if (!GetLayoutObject())
    return nullptr;
  return &GetLayoutObject()->GetDocument();
}

void AXLayoutObject::HandleAutofillSuggestionAvailabilityChanged(
    WebAXAutofillSuggestionAvailability suggestion_availability) {
  // Autofill suggestion availability is stored in AXObjectCache.
  AXObjectCache().SetAutofillSuggestionAvailability(AXObjectID(),
                                                    suggestion_availability);
}

void AXLayoutObject::GetWordBoundaries(Vector<int>& word_starts,
                                       Vector<int>& word_ends) const {
  if (!layout_object_ || !layout_object_->IsListMarker()) {
    return;
  }

  String text_alternative;
  if (ListMarker* marker = ListMarker::Get(layout_object_)) {
    text_alternative = marker->TextAlternative(*layout_object_);
  }
  if (text_alternative.ContainsOnlyWhitespaceOrEmpty())
    return;

  Vector<AbstractInlineTextBox::WordBoundaries> boundaries;
  AbstractInlineTextBox::GetWordBoundariesForText(boundaries, text_alternative);
  word_starts.reserve(boundaries.size());
  word_ends.reserve(boundaries.size());
  for (const auto& boundary : boundaries) {
    word_starts.push_back(boundary.start_index);
    word_ends.push_back(boundary.end_index);
  }
}

}  // namespace blink
