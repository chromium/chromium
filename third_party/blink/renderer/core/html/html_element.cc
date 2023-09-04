/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2008, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_element.h"

#include "base/containers/enum_set.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_stringlegacynulltoemptystring_trustedscript.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_recalc_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/toggle_event.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/anchor_element_observer.h"
#include "third_party/blink/renderer/core/html/custom/custom_element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_listbox_element.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml_names.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

using AttributeChangedFunction =
    void (HTMLElement::*)(const Element::AttributeModificationParams& params);

struct AttributeTriggers {
  const QualifiedName& attribute;
  WebFeature web_feature;
  const AtomicString& event;
  AttributeChangedFunction function;
};

namespace {

// https://html.spec.whatwg.org/multipage/interaction.html#editing-host
// An editing host is either an HTML element with its contenteditable attribute
// in the true state, or a child HTML element of a Document whose design mode
// enabled is true.
// https://w3c.github.io/editing/execCommand.html#editable
// Something is editable if it is a node; it is not an editing host; it does not
// have a contenteditable attribute set to the false state; its parent is an
// editing host or editable; and either it is an HTML element, or it is an svg
// or math element, or it is not an Element and its parent is an HTML element.
bool IsEditableOrEditingHost(const Node& node) {
  auto* html_element = DynamicTo<HTMLElement>(node);
  if (html_element) {
    ContentEditableType content_editable =
        html_element->contentEditableNormalized();
    if (content_editable == ContentEditableType::kContentEditable ||
        content_editable == ContentEditableType::kPlaintextOnly)
      return true;
    if (html_element->GetDocument().InDesignMode() &&
        html_element->isConnected()) {
      return true;
    }
    if (content_editable == ContentEditableType::kNotContentEditable)
      return false;
  }
  if (!node.parentNode())
    return false;
  if (!IsEditableOrEditingHost(*node.parentNode()))
    return false;
  if (html_element)
    return true;
  if (IsA<SVGSVGElement>(node))
    return true;
  if (auto* mathml_element = DynamicTo<MathMLElement>(node))
    return mathml_element->HasTagName(mathml_names::kMathTag);
  return !IsA<Element>(node) && node.parentNode()->IsHTMLElement();
}

const WebFeature kNoWebFeature = static_cast<WebFeature>(0);

HTMLElement* GetParentForDirectionality(const HTMLElement& element,
                                        bool& needs_slot_assignment_recalc) {
  if (element.IsPseudoElement())
    return DynamicTo<HTMLElement>(element.ParentOrShadowHostNode());

  if (element.IsChildOfShadowHost()) {
    ShadowRoot* root = element.ShadowRootOfParent();
    if (!root || !root->HasSlotAssignment())
      return nullptr;

    if (root->NeedsSlotAssignmentRecalc()) {
      needs_slot_assignment_recalc = true;
      return nullptr;
    }
  }
  if (auto* parent_slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(
          element.parentElement())) {
    ShadowRoot* root = parent_slot->ContainingShadowRoot();
    if (root->NeedsSlotAssignmentRecalc()) {
      needs_slot_assignment_recalc = true;
      return nullptr;
    }
  }

  // We should take care of all cases that would trigger a slot assignment
  // recalc, and delay the check for later for a performance reason.
  SlotAssignmentRecalcForbiddenScope forbid_slot_recalc(element.GetDocument());
  return DynamicTo<HTMLElement>(FlatTreeTraversal::ParentElement(element));
}

void CheckSoftNavigationHeuristicsTracking(const Document& document,
                                           const Node* insertion_point) {
  DCHECK(insertion_point);
  if (document.IsTrackingSoftNavigationHeuristics()) {
    LocalDOMWindow* window = document.domWindow();
    if (!window) {
      return;
    }
    LocalFrame* frame = window->GetFrame();
    if (!frame || !frame->IsMainFrame()) {
      return;
    }
    ScriptState* script_state = ToScriptStateForMainWorld(frame);
    if (!script_state) {
      return;
    }

    SoftNavigationHeuristics* heuristics =
        SoftNavigationHeuristics::From(*window);
    DCHECK(heuristics);
    heuristics->ModifiedDOM(script_state);
  }
}

class PopoverCloseWatcherEventListener : public NativeEventListener {
 public:
  explicit PopoverCloseWatcherEventListener(HTMLElement* popover)
      : popover_(popover) {}

  void Invoke(ExecutionContext*, Event* event) override {
    if (!popover_) {
      return;
    }
    // Don't do anything in response to cancel events, as per the HTML spec
    if (event->type() == event_type_names::kClose) {
      popover_->HidePopoverInternal(
          HidePopoverFocusBehavior::kFocusPreviousElement,
          HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
          /*exception_state=*/nullptr);
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(popover_);
    NativeEventListener::Trace(visitor);
  }

 private:
  WeakMember<HTMLElement> popover_;
};

}  // anonymous namespace

String HTMLElement::nodeName() const {
  // FIXME: Would be nice to have an atomicstring lookup based off uppercase
  // chars that does not have to copy the string on a hit in the hash.
  // FIXME: We should have a way to detect XHTML elements and replace the
  // hasPrefix() check with it.
  if (IsA<HTMLDocument>(GetDocument())) {
    if (!TagQName().HasPrefix())
      return TagQName().LocalNameUpper();
    return Element::nodeName().UpperASCII();
  }
  return Element::nodeName();
}

bool HTMLElement::ShouldSerializeEndTag() const {
  // See https://www.w3.org/TR/DOM-Parsing/
  if (HasTagName(html_names::kAreaTag) || HasTagName(html_names::kBaseTag) ||
      HasTagName(html_names::kBasefontTag) ||
      HasTagName(html_names::kBgsoundTag) || HasTagName(html_names::kBrTag) ||
      HasTagName(html_names::kColTag) || HasTagName(html_names::kEmbedTag) ||
      HasTagName(html_names::kFrameTag) || HasTagName(html_names::kHrTag) ||
      HasTagName(html_names::kImgTag) || HasTagName(html_names::kInputTag) ||
      HasTagName(html_names::kKeygenTag) || HasTagName(html_names::kLinkTag) ||
      HasTagName(html_names::kMetaTag) || HasTagName(html_names::kParamTag) ||
      HasTagName(html_names::kSourceTag) || HasTagName(html_names::kTrackTag) ||
      HasTagName(html_names::kWbrTag))
    return false;
  return true;
}

static inline CSSValueID UnicodeBidiAttributeForDirAuto(HTMLElement* element) {
  if (element->HasTagName(html_names::kPreTag) ||
      element->HasTagName(html_names::kTextareaTag))
    return CSSValueID::kPlaintext;
  // FIXME: For bdo element, dir="auto" should result in "bidi-override isolate"
  // but we don't support having multiple values in unicode-bidi yet.
  // See https://bugs.webkit.org/show_bug.cgi?id=73164.
  return CSSValueID::kIsolate;
}

unsigned HTMLElement::ParseBorderWidthAttribute(
    const AtomicString& value) const {
  unsigned border_width = 0;
  if (value.empty() || !ParseHTMLNonNegativeInteger(value, border_width)) {
    if (HasTagName(html_names::kTableTag) && !value.IsNull())
      return 1;
  }
  return border_width;
}

void HTMLElement::ApplyBorderAttributeToStyle(
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  unsigned width = ParseBorderWidthAttribute(value);
  for (CSSPropertyID property_id :
       {CSSPropertyID::kBorderTopWidth, CSSPropertyID::kBorderBottomWidth,
        CSSPropertyID::kBorderLeftWidth, CSSPropertyID::kBorderRightWidth}) {
    AddPropertyToPresentationAttributeStyle(
        style, property_id, width, CSSPrimitiveValue::UnitType::kPixels);
  }
  for (CSSPropertyID property_id :
       {CSSPropertyID::kBorderTopStyle, CSSPropertyID::kBorderBottomStyle,
        CSSPropertyID::kBorderLeftStyle, CSSPropertyID::kBorderRightStyle}) {
    AddPropertyToPresentationAttributeStyle(style, property_id,
                                            CSSValueID::kSolid);
  }
}

bool HTMLElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kAlignAttr ||
      name == html_names::kContenteditableAttr ||
      name == html_names::kHiddenAttr || name == html_names::kLangAttr ||
      name.Matches(xml_names::kLangAttr) ||
      name == html_names::kDraggableAttr || name == html_names::kDirAttr ||
      name == html_names::kInertAttr)
    return true;
  return Element::IsPresentationAttribute(name);
}

static inline bool IsValidDirAttribute(const AtomicString& value) {
  return EqualIgnoringASCIICase(value, "auto") ||
         EqualIgnoringASCIICase(value, "ltr") ||
         EqualIgnoringASCIICase(value, "rtl");
}

void HTMLElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kAlignAttr) {
    if (EqualIgnoringASCIICase(value, "middle")) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              CSSValueID::kCenter);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kTextAlign,
                                              value);
    }
  } else if (name == html_names::kContenteditableAttr) {
    AtomicString lower_value = value.LowerASCII();
    if (lower_value.empty() || lower_value == keywords::kTrue) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify, CSSValueID::kReadWrite);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kBreakWord);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitLineBreak, CSSValueID::kAfterWhiteSpace);
      UseCounter::Count(GetDocument(), WebFeature::kContentEditableTrue);
      if (HasTagName(html_names::kHTMLTag)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kContentEditableTrueOnHTML);
      }
    } else if (lower_value == keywords::kPlaintextOnly) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify,
          CSSValueID::kReadWritePlaintextOnly);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kOverflowWrap, CSSValueID::kBreakWord);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitLineBreak, CSSValueID::kAfterWhiteSpace);
      UseCounter::Count(GetDocument(),
                        WebFeature::kContentEditablePlainTextOnly);
    } else if (lower_value == keywords::kFalse) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserModify, CSSValueID::kReadOnly);
    }
  } else if (name == html_names::kHiddenAttr) {
    if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
            GetExecutionContext()) &&
        EqualIgnoringASCIICase(value, "until-found")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kContentVisibility, CSSValueID::kHidden);
      UseCounter::Count(GetDocument(), WebFeature::kHiddenUntilFoundAttribute);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kDisplay,
                                              CSSValueID::kNone);
      UseCounter::Count(GetDocument(), WebFeature::kHiddenAttribute);
    }
  } else if (name == html_names::kDraggableAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kDraggableAttribute);
    if (EqualIgnoringASCIICase(value, keywords::kTrue)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserDrag, CSSValueID::kElement);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kUserSelect,
                                              CSSValueID::kNone);
    } else if (EqualIgnoringASCIICase(value, keywords::kFalse)) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kWebkitUserDrag, CSSValueID::kNone);
    }
  } else if (name == html_names::kDirAttr) {
    if (EqualIgnoringASCIICase(value, "auto")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyID::kUnicodeBidi,
          UnicodeBidiAttributeForDirAuto(this));
    } else {
      if (IsValidDirAttribute(value)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kDirection, value);
      } else if (IsA<HTMLBodyElement>(*this)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kDirection, "ltr");
      }
      if (!HasTagName(html_names::kBdiTag) &&
          !HasTagName(html_names::kBdoTag) &&
          !HasTagName(html_names::kOutputTag)) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyID::kUnicodeBidi, CSSValueID::kIsolate);
      }
    }
  } else if (name.Matches(xml_names::kLangAttr)) {
    MapLanguageAttributeToLocale(value, style);
  } else if (name == html_names::kLangAttr) {
    // xml:lang has a higher priority than lang.
    if (!FastHasAttribute(xml_names::kLangAttr))
      MapLanguageAttributeToLocale(value, style);
  } else {
    Element::CollectStyleForPresentationAttribute(name, value, style);
  }
}

// static
AttributeTriggers* HTMLElement::TriggersForAttributeName(
    const QualifiedName& attr_name) {
  const AtomicString& kNoEvent = g_null_atom;
  static AttributeTriggers attribute_triggers[] = {
      {html_names::kDirAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnDirAttrChanged},
      {html_names::kFormAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnFormAttrChanged},
      {html_names::kLangAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnLangAttrChanged},
      {html_names::kNonceAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnNonceAttrChanged},
      {html_names::kPopoverAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::OnPopoverChanged},

      // Attributes handled by base class
      {html_names::kFocusgroupAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::ReparseAttribute},
      {html_names::kTabindexAttr, kNoWebFeature, kNoEvent,
       &HTMLElement::ReparseAttribute},

      {html_names::kOnabortAttr, kNoWebFeature, event_type_names::kAbort,
       nullptr},
      {html_names::kOnanimationendAttr, kNoWebFeature,
       event_type_names::kAnimationend, nullptr},
      {html_names::kOnanimationiterationAttr, kNoWebFeature,
       event_type_names::kAnimationiteration, nullptr},
      {html_names::kOnanimationstartAttr, kNoWebFeature,
       event_type_names::kAnimationstart, nullptr},
      {html_names::kOnauxclickAttr, kNoWebFeature, event_type_names::kAuxclick,
       nullptr},
      {html_names::kOnbeforecopyAttr, kNoWebFeature,
       event_type_names::kBeforecopy, nullptr},
      {html_names::kOnbeforecutAttr, kNoWebFeature,
       event_type_names::kBeforecut, nullptr},
      {html_names::kOnbeforeinputAttr, kNoWebFeature,
       event_type_names::kBeforeinput, nullptr},
      {html_names::kOnbeforepasteAttr, kNoWebFeature,
       event_type_names::kBeforepaste, nullptr},
      {html_names::kOnbeforetoggleAttr, kNoWebFeature,
       event_type_names::kBeforetoggle, nullptr},
      {html_names::kOnblurAttr, kNoWebFeature, event_type_names::kBlur,
       nullptr},
      {html_names::kOncancelAttr, kNoWebFeature, event_type_names::kCancel,
       nullptr},
      {html_names::kOncanplayAttr, kNoWebFeature, event_type_names::kCanplay,
       nullptr},
      {html_names::kOncanplaythroughAttr, kNoWebFeature,
       event_type_names::kCanplaythrough, nullptr},
      {html_names::kOnchangeAttr, kNoWebFeature, event_type_names::kChange,
       nullptr},
      {html_names::kOnclickAttr, kNoWebFeature, event_type_names::kClick,
       nullptr},
      {html_names::kOncloseAttr, kNoWebFeature, event_type_names::kClose,
       nullptr},
      {html_names::kOncontextlostAttr, kNoWebFeature,
       event_type_names::kContextlost, nullptr},
      {html_names::kOncontextmenuAttr, kNoWebFeature,
       event_type_names::kContextmenu, nullptr},
      {html_names::kOncontextrestoredAttr, kNoWebFeature,
       event_type_names::kContextrestored, nullptr},
      {html_names::kOncopyAttr, kNoWebFeature, event_type_names::kCopy,
       nullptr},
      {html_names::kOncuechangeAttr, kNoWebFeature,
       event_type_names::kCuechange, nullptr},
      {html_names::kOncutAttr, kNoWebFeature, event_type_names::kCut, nullptr},
      {html_names::kOndblclickAttr, kNoWebFeature, event_type_names::kDblclick,
       nullptr},
      {html_names::kOndragAttr, kNoWebFeature, event_type_names::kDrag,
       nullptr},
      {html_names::kOndragendAttr, kNoWebFeature, event_type_names::kDragend,
       nullptr},
      {html_names::kOndragenterAttr, kNoWebFeature,
       event_type_names::kDragenter, nullptr},
      {html_names::kOndragleaveAttr, kNoWebFeature,
       event_type_names::kDragleave, nullptr},
      {html_names::kOndragoverAttr, kNoWebFeature, event_type_names::kDragover,
       nullptr},
      {html_names::kOndragstartAttr, kNoWebFeature,
       event_type_names::kDragstart, nullptr},
      {html_names::kOndropAttr, kNoWebFeature, event_type_names::kDrop,
       nullptr},
      {html_names::kOndurationchangeAttr, kNoWebFeature,
       event_type_names::kDurationchange, nullptr},
      {html_names::kOnemptiedAttr, kNoWebFeature, event_type_names::kEmptied,
       nullptr},
      {html_names::kOnendedAttr, kNoWebFeature, event_type_names::kEnded,
       nullptr},
      {html_names::kOnerrorAttr, kNoWebFeature, event_type_names::kError,
       nullptr},
      {html_names::kOnfocusAttr, kNoWebFeature, event_type_names::kFocus,
       nullptr},
      {html_names::kOnfocusinAttr, kNoWebFeature, event_type_names::kFocusin,
       nullptr},
      {html_names::kOnfocusoutAttr, kNoWebFeature, event_type_names::kFocusout,
       nullptr},
      {html_names::kOnformdataAttr, kNoWebFeature, event_type_names::kFormdata,
       nullptr},
      {html_names::kOngotpointercaptureAttr, kNoWebFeature,
       event_type_names::kGotpointercapture, nullptr},
      {html_names::kOninputAttr, kNoWebFeature, event_type_names::kInput,
       nullptr},
      {html_names::kOninvalidAttr, kNoWebFeature, event_type_names::kInvalid,
       nullptr},
      {html_names::kOnkeydownAttr, kNoWebFeature, event_type_names::kKeydown,
       nullptr},
      {html_names::kOnkeypressAttr, kNoWebFeature, event_type_names::kKeypress,
       nullptr},
      {html_names::kOnkeyupAttr, kNoWebFeature, event_type_names::kKeyup,
       nullptr},
      {html_names::kOnloadAttr, kNoWebFeature, event_type_names::kLoad,
       nullptr},
      {html_names::kOnloadeddataAttr, kNoWebFeature,
       event_type_names::kLoadeddata, nullptr},
      {html_names::kOnloadedmetadataAttr, kNoWebFeature,
       event_type_names::kLoadedmetadata, nullptr},
      {html_names::kOnloadstartAttr, kNoWebFeature,
       event_type_names::kLoadstart, nullptr},
      {html_names::kOnlostpointercaptureAttr, kNoWebFeature,
       event_type_names::kLostpointercapture, nullptr},
      {html_names::kOnmousedownAttr, kNoWebFeature,
       event_type_names::kMousedown, nullptr},
      {html_names::kOnmouseenterAttr, kNoWebFeature,
       event_type_names::kMouseenter, nullptr},
      {html_names::kOnmouseleaveAttr, kNoWebFeature,
       event_type_names::kMouseleave, nullptr},
      {html_names::kOnmousemoveAttr, kNoWebFeature,
       event_type_names::kMousemove, nullptr},
      {html_names::kOnmouseoutAttr, kNoWebFeature, event_type_names::kMouseout,
       nullptr},
      {html_names::kOnmouseoverAttr, kNoWebFeature,
       event_type_names::kMouseover, nullptr},
      {html_names::kOnmouseupAttr, kNoWebFeature, event_type_names::kMouseup,
       nullptr},
      {html_names::kOnmousewheelAttr, kNoWebFeature,
       event_type_names::kMousewheel, nullptr},
      {html_names::kOnoverscrollAttr, kNoWebFeature,
       event_type_names::kOverscroll, nullptr},
      {html_names::kOnpasteAttr, kNoWebFeature, event_type_names::kPaste,
       nullptr},
      {html_names::kOnpauseAttr, kNoWebFeature, event_type_names::kPause,
       nullptr},
      {html_names::kOnplayAttr, kNoWebFeature, event_type_names::kPlay,
       nullptr},
      {html_names::kOnplayingAttr, kNoWebFeature, event_type_names::kPlaying,
       nullptr},
      {html_names::kOnpointercancelAttr, kNoWebFeature,
       event_type_names::kPointercancel, nullptr},
      {html_names::kOnpointerdownAttr, kNoWebFeature,
       event_type_names::kPointerdown, nullptr},
      {html_names::kOnpointerenterAttr, kNoWebFeature,
       event_type_names::kPointerenter, nullptr},
      {html_names::kOnpointerleaveAttr, kNoWebFeature,
       event_type_names::kPointerleave, nullptr},
      {html_names::kOnpointermoveAttr, kNoWebFeature,
       event_type_names::kPointermove, nullptr},
      {html_names::kOnpointeroutAttr, kNoWebFeature,
       event_type_names::kPointerout, nullptr},
      {html_names::kOnpointeroverAttr, kNoWebFeature,
       event_type_names::kPointerover, nullptr},
      {html_names::kOnpointerrawupdateAttr, kNoWebFeature,
       event_type_names::kPointerrawupdate, nullptr},
      {html_names::kOnpointerupAttr, kNoWebFeature,
       event_type_names::kPointerup, nullptr},
      {html_names::kOnprogressAttr, kNoWebFeature, event_type_names::kProgress,
       nullptr},
      {html_names::kOnratechangeAttr, kNoWebFeature,
       event_type_names::kRatechange, nullptr},
      {html_names::kOnresetAttr, kNoWebFeature, event_type_names::kReset,
       nullptr},
      {html_names::kOnresizeAttr, kNoWebFeature, event_type_names::kResize,
       nullptr},
      {html_names::kOnscrollAttr, kNoWebFeature, event_type_names::kScroll,
       nullptr},
      {html_names::kOnscrollendAttr, kNoWebFeature,
       event_type_names::kScrollend, nullptr},
      {html_names::kOnseekedAttr, kNoWebFeature, event_type_names::kSeeked,
       nullptr},
      {html_names::kOnseekingAttr, kNoWebFeature, event_type_names::kSeeking,
       nullptr},
      {html_names::kOnsecuritypolicyviolationAttr, kNoWebFeature,
       event_type_names::kSecuritypolicyviolation, nullptr},
      {html_names::kOnselectAttr, kNoWebFeature, event_type_names::kSelect,
       nullptr},
      {html_names::kOnselectstartAttr, kNoWebFeature,
       event_type_names::kSelectstart, nullptr},
      {html_names::kOnslotchangeAttr, kNoWebFeature,
       event_type_names::kSlotchange, nullptr},
      {html_names::kOnstalledAttr, kNoWebFeature, event_type_names::kStalled,
       nullptr},
      {html_names::kOnsubmitAttr, kNoWebFeature, event_type_names::kSubmit,
       nullptr},
      {html_names::kOnsuspendAttr, kNoWebFeature, event_type_names::kSuspend,
       nullptr},
      {html_names::kOntimeupdateAttr, kNoWebFeature,
       event_type_names::kTimeupdate, nullptr},
      {html_names::kOntoggleAttr, kNoWebFeature, event_type_names::kToggle,
       nullptr},
      {html_names::kOntouchcancelAttr, kNoWebFeature,
       event_type_names::kTouchcancel, nullptr},
      {html_names::kOntouchendAttr, kNoWebFeature, event_type_names::kTouchend,
       nullptr},
      {html_names::kOntouchmoveAttr, kNoWebFeature,
       event_type_names::kTouchmove, nullptr},
      {html_names::kOntouchstartAttr, kNoWebFeature,
       event_type_names::kTouchstart, nullptr},
      {html_names::kOntransitionendAttr, kNoWebFeature,
       event_type_names::kWebkitTransitionEnd, nullptr},
      {html_names::kOnvolumechangeAttr, kNoWebFeature,
       event_type_names::kVolumechange, nullptr},
      {html_names::kOnwaitingAttr, kNoWebFeature, event_type_names::kWaiting,
       nullptr},
      {html_names::kOnwebkitanimationendAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationEnd, nullptr},
      {html_names::kOnwebkitanimationiterationAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationIteration, nullptr},
      {html_names::kOnwebkitanimationstartAttr, kNoWebFeature,
       event_type_names::kWebkitAnimationStart, nullptr},
      {html_names::kOnwebkitfullscreenchangeAttr, kNoWebFeature,
       event_type_names::kWebkitfullscreenchange, nullptr},
      {html_names::kOnwebkitfullscreenerrorAttr, kNoWebFeature,
       event_type_names::kWebkitfullscreenerror, nullptr},
      {html_names::kOnwebkittransitionendAttr, kNoWebFeature,
       event_type_names::kWebkitTransitionEnd, nullptr},
      {html_names::kOnwheelAttr, kNoWebFeature, event_type_names::kWheel,
       nullptr},

      // Begin ARIA attributes.
      {html_names::kAriaActivedescendantAttr,
       WebFeature::kARIAActiveDescendantAttribute, kNoEvent, nullptr},
      {html_names::kAriaAtomicAttr, WebFeature::kARIAAtomicAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaAutocompleteAttr,
       WebFeature::kARIAAutocompleteAttribute, kNoEvent, nullptr},
      {html_names::kAriaBusyAttr, WebFeature::kARIABusyAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaCheckedAttr, WebFeature::kARIACheckedAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaColcountAttr, WebFeature::kARIAColCountAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaColindexAttr, WebFeature::kARIAColIndexAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaColspanAttr, WebFeature::kARIAColSpanAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaControlsAttr, WebFeature::kARIAControlsAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaCurrentAttr, WebFeature::kARIACurrentAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaDescribedbyAttr, WebFeature::kARIADescribedByAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaDescriptionAttr, WebFeature::kARIADescriptionAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaDetailsAttr, WebFeature::kARIADetailsAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaDisabledAttr, WebFeature::kARIADisabledAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaDropeffectAttr, WebFeature::kARIADropEffectAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaErrormessageAttr,
       WebFeature::kARIAErrorMessageAttribute, kNoEvent, nullptr},
      {html_names::kAriaExpandedAttr, WebFeature::kARIAExpandedAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaFlowtoAttr, WebFeature::kARIAFlowToAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaGrabbedAttr, WebFeature::kARIAGrabbedAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaHaspopupAttr, WebFeature::kARIAHasPopupAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaHiddenAttr, WebFeature::kARIAHiddenAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaInvalidAttr, WebFeature::kARIAInvalidAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaKeyshortcutsAttr,
       WebFeature::kARIAKeyShortcutsAttribute, kNoEvent, nullptr},
      {html_names::kAriaLabelAttr, WebFeature::kARIALabelAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaLabeledbyAttr, WebFeature::kARIALabeledByAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaLabelledbyAttr, WebFeature::kARIALabelledByAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaLevelAttr, WebFeature::kARIALevelAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaLiveAttr, WebFeature::kARIALiveAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaModalAttr, WebFeature::kARIAModalAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaMultilineAttr, WebFeature::kARIAMultilineAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaMultiselectableAttr,
       WebFeature::kARIAMultiselectableAttribute, kNoEvent, nullptr},
      {html_names::kAriaOrientationAttr, WebFeature::kARIAOrientationAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaOwnsAttr, WebFeature::kARIAOwnsAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaPlaceholderAttr, WebFeature::kARIAPlaceholderAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaPosinsetAttr, WebFeature::kARIAPosInSetAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaPressedAttr, WebFeature::kARIAPressedAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaReadonlyAttr, WebFeature::kARIAReadOnlyAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaRelevantAttr, WebFeature::kARIARelevantAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaRequiredAttr, WebFeature::kARIARequiredAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaRoledescriptionAttr,
       WebFeature::kARIARoleDescriptionAttribute, kNoEvent, nullptr},
      {html_names::kAriaRowcountAttr, WebFeature::kARIARowCountAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaRowindexAttr, WebFeature::kARIARowIndexAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaRowspanAttr, WebFeature::kARIARowSpanAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaSelectedAttr, WebFeature::kARIASelectedAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaSetsizeAttr, WebFeature::kARIASetSizeAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaSortAttr, WebFeature::kARIASortAttribute, kNoEvent,
       nullptr},
      {html_names::kAriaValuemaxAttr, WebFeature::kARIAValueMaxAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaValueminAttr, WebFeature::kARIAValueMinAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaValuenowAttr, WebFeature::kARIAValueNowAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaValuetextAttr, WebFeature::kARIAValueTextAttribute,
       kNoEvent, nullptr},
      {html_names::kAriaVirtualcontentAttr,
       WebFeature::kARIAVirtualcontentAttribute, kNoEvent, nullptr},
      // End ARIA attributes.

      {html_names::kAutocapitalizeAttr, WebFeature::kAutocapitalizeAttribute,
       kNoEvent, nullptr},
  };

  using AttributeToTriggerIndexMap = HashMap<QualifiedName, uint32_t>;
  DEFINE_STATIC_LOCAL(AttributeToTriggerIndexMap,
                      attribute_to_trigger_index_map, ());
  if (!attribute_to_trigger_index_map.size()) {
    for (uint32_t i = 0; i < std::size(attribute_triggers); ++i) {
      DCHECK(attribute_triggers[i].attribute.NamespaceURI().IsNull())
          << "Lookup table does not work for namespaced attributes because "
             "they would not match for different prefixes";
      attribute_to_trigger_index_map.insert(attribute_triggers[i].attribute, i);
    }
  }

  auto iter = attribute_to_trigger_index_map.find(attr_name);
  if (iter != attribute_to_trigger_index_map.end())
    return &attribute_triggers[iter->value];
  return nullptr;
}

// static
const AtomicString& HTMLElement::EventNameForAttributeName(
    const QualifiedName& attr_name) {
  AttributeTriggers* triggers = TriggersForAttributeName(attr_name);
  if (triggers)
    return triggers->event;
  return g_null_atom;
}

void HTMLElement::AttributeChanged(const AttributeModificationParams& params) {
  Element::AttributeChanged(params);
  if (params.name == html_names::kDisabledAttr &&
      IsFormAssociatedCustomElement() &&
      params.old_value.IsNull() != params.new_value.IsNull()) {
    EnsureElementInternals().DisabledAttributeChanged();
    if (params.reason == AttributeModificationReason::kDirectly &&
        IsDisabledFormControl() && AdjustedFocusedElementInTreeScope() == this)
      blur();
    return;
  }
  if (params.name == html_names::kReadonlyAttr &&
      IsFormAssociatedCustomElement() &&
      params.old_value.IsNull() != params.new_value.IsNull()) {
    EnsureElementInternals().ReadonlyAttributeChanged();
    return;
  }
  if (params.name == html_names::kAnchorAttr &&
      RuntimeEnabledFeatures::CSSAnchorPositioningEnabled()) {
    EnsureAnchorElementObserver().Notify();
    return;
  }

  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  // adjustedFocusedElementInTreeScope() is not trivial. We should check
  // attribute names, then call adjustedFocusedElementInTreeScope().
  if (params.name == html_names::kHiddenAttr && !params.new_value.IsNull()) {
    if (AdjustedFocusedElementInTreeScope() == this)
      blur();
  } else if (params.name == html_names::kSpellcheckAttr) {
    if (GetDocument().GetFrame()) {
      GetDocument().GetFrame()->GetSpellChecker().RespondToChangedEnablement(
          *this, IsSpellCheckingEnabled());
    }
  } else if (params.name == html_names::kContenteditableAttr) {
    if (GetDocument().GetFrame()) {
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .RemoveSpellingAndGrammarMarkers(
              *this, SpellChecker::ElementsType::kOnlyNonEditable);
    }
    if (AdjustedFocusedElementInTreeScope() != this)
      return;
    // The attribute change may cause IsFocusable() to return false
    // for the element which had focus.
    //
    // TODO(tkent): We should avoid updating style.  We'd like to check only
    // DOM-level focusability here.
    GetDocument().UpdateStyleAndLayoutTreeForNode(this,
                                                  DocumentUpdateReason::kFocus);
    if (!IsFocusable()) {
      blur();
    }
  }
}

void HTMLElement::ParseAttribute(const AttributeModificationParams& params) {
  AttributeTriggers* triggers = TriggersForAttributeName(params.name);
  if (!triggers) {
    if (!params.name.NamespaceURI().IsNull()) {
      // AttributeTriggers lookup table does not support namespaced attributes.
      // Fall back to Element implementation for attributes like xml:lang.
      Element::ParseAttribute(params);
    }
    return;
  }

  if (triggers->event != g_null_atom) {
    SetAttributeEventListener(
        triggers->event,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), params.name, params.new_value));
  }

  if (triggers->web_feature != kNoWebFeature) {
    // Count usage of attributes but ignore attributes in user agent shadow DOM.
    if (!IsInUserAgentShadowRoot())
      UseCounter::Count(GetDocument(), triggers->web_feature);
  }
  if (triggers->function)
    ((*this).*(triggers->function))(params);
}

DocumentFragment* HTMLElement::TextToFragment(const String& text,
                                              ExceptionState& exception_state) {
  DocumentFragment* fragment = DocumentFragment::Create(GetDocument());
  unsigned i, length = text.length();
  UChar c = 0;
  for (unsigned start = 0; start < length;) {
    // Find next line break.
    for (i = start; i < length; i++) {
      c = text[i];
      if (c == '\r' || c == '\n')
        break;
    }

    if (i > start) {
      fragment->AppendChild(
          Text::Create(GetDocument(), text.Substring(start, i - start)),
          exception_state);
      if (exception_state.HadException())
        return nullptr;
    }

    if (i == length)
      break;

    fragment->AppendChild(MakeGarbageCollected<HTMLBRElement>(GetDocument()),
                          exception_state);
    if (exception_state.HadException())
      return nullptr;

    // Make sure \r\n doesn't result in two line breaks.
    if (c == '\r' && i + 1 < length && text[i + 1] == '\n')
      i++;

    start = i + 1;  // Character after line break.
  }

  return fragment;
}

V8UnionStringLegacyNullToEmptyStringOrTrustedScript*
HTMLElement::innerTextForBinding() {
  return MakeGarbageCollected<
      V8UnionStringLegacyNullToEmptyStringOrTrustedScript>(innerText());
}

void HTMLElement::setInnerTextForBinding(
    const V8UnionStringLegacyNullToEmptyStringOrTrustedScript*
        string_or_trusted_script,
    ExceptionState& exception_state) {
  String value;
  switch (string_or_trusted_script->GetContentType()) {
    case V8UnionStringLegacyNullToEmptyStringOrTrustedScript::ContentType::
        kStringLegacyNullToEmptyString:
      value = string_or_trusted_script->GetAsStringLegacyNullToEmptyString();
      break;
    case V8UnionStringLegacyNullToEmptyStringOrTrustedScript::ContentType::
        kTrustedScript:
      value = string_or_trusted_script->GetAsTrustedScript()->toString();
      break;
  }
  setInnerText(value);
}

String HTMLElement::innerText() {
  return Element::innerText();
}

void HTMLElement::setInnerText(const String& text) {
  // FIXME: This doesn't take whitespace collapsing into account at all.

  // The usage of ASSERT_NO_EXCEPTION in this function is subject to mutation
  // events being fired while removing elements. By delaying them to the end of
  // the function, we can guarantee that no exceptions will be thrown.
  EventQueueScope delay_mutation_events;

  if (!text.Contains('\n') && !text.Contains('\r')) {
    if (text.empty()) {
      RemoveChildren();
      return;
    }
    ReplaceChildrenWithText(this, text, ASSERT_NO_EXCEPTION);
    return;
  }

  // Add text nodes and <br> elements.
  DocumentFragment* fragment = TextToFragment(text, ASSERT_NO_EXCEPTION);
  ReplaceChildrenWithFragment(this, fragment, ASSERT_NO_EXCEPTION);
}

void HTMLElement::setOuterText(const String& text,
                               ExceptionState& exception_state) {
  ContainerNode* parent = parentNode();
  if (!parent) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNoModificationAllowedError,
        "The element has no parent.");
    return;
  }

  Node* prev = previousSibling();
  Node* next = nextSibling();
  Node* new_child = nullptr;

  // Convert text to fragment with <br> tags instead of linebreaks if needed.
  if (text.Contains('\r') || text.Contains('\n'))
    new_child = TextToFragment(text, exception_state);
  else
    new_child = Text::Create(GetDocument(), text);

  if (exception_state.HadException())
    return;

  parent->ReplaceChild(new_child, this, exception_state);

  Node* node = next ? next->previousSibling() : nullptr;
  auto* next_text_node = DynamicTo<Text>(node);
  if (!exception_state.HadException() && next_text_node)
    MergeWithNextTextNode(next_text_node, exception_state);

  auto* prev_text_node = DynamicTo<Text>(prev);
  if (!exception_state.HadException() && prev && prev->IsTextNode())
    MergeWithNextTextNode(prev_text_node, exception_state);
}

void HTMLElement::ApplyAspectRatioToStyle(const AtomicString& width,
                                          const AtomicString& height,
                                          MutableCSSPropertyValueSet* style) {
  HTMLDimension width_dim;
  if (!ParseDimensionValue(width, width_dim) || !width_dim.IsAbsolute())
    return;
  HTMLDimension height_dim;
  if (!ParseDimensionValue(height, height_dim) || !height_dim.IsAbsolute())
    return;
  ApplyAspectRatioToStyle(width_dim.Value(), height_dim.Value(), style);
}

void HTMLElement::ApplyIntegerAspectRatioToStyle(
    const AtomicString& width,
    const AtomicString& height,
    MutableCSSPropertyValueSet* style) {
  unsigned width_val = 0;
  if (!ParseHTMLNonNegativeInteger(width, width_val))
    return;
  unsigned height_val = 0;
  if (!ParseHTMLNonNegativeInteger(height, height_val))
    return;
  ApplyAspectRatioToStyle(width_val, height_val, style);
}

void HTMLElement::ApplyAspectRatioToStyle(double width,
                                          double height,
                                          MutableCSSPropertyValueSet* style) {
  auto* width_val = CSSNumericLiteralValue::Create(
      width, CSSPrimitiveValue::UnitType::kNumber);
  auto* height_val = CSSNumericLiteralValue::Create(
      height, CSSPrimitiveValue::UnitType::kNumber);
  auto* ratio_value =
      MakeGarbageCollected<cssvalue::CSSRatioValue>(*width_val, *height_val);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueID::kAuto));
  list->Append(*ratio_value);

  style->SetProperty(CSSPropertyID::kAspectRatio, *list);
}

void HTMLElement::ApplyAlignmentAttributeToStyle(
    const AtomicString& alignment,
    MutableCSSPropertyValueSet* style) {
  // Vertical alignment with respect to the current baseline of the text
  // right or left means floating images.
  CSSValueID float_value = CSSValueID::kInvalid;
  CSSValueID vertical_align_value = CSSValueID::kInvalid;

  if (EqualIgnoringASCIICase(alignment, "absmiddle") ||
      EqualIgnoringASCIICase(alignment, "abscenter")) {
    vertical_align_value = CSSValueID::kMiddle;
  } else if (EqualIgnoringASCIICase(alignment, "absbottom")) {
    vertical_align_value = CSSValueID::kBottom;
  } else if (EqualIgnoringASCIICase(alignment, "left")) {
    float_value = CSSValueID::kLeft;
    vertical_align_value = CSSValueID::kTop;
  } else if (EqualIgnoringASCIICase(alignment, "right")) {
    float_value = CSSValueID::kRight;
    vertical_align_value = CSSValueID::kTop;
  } else if (EqualIgnoringASCIICase(alignment, "top")) {
    vertical_align_value = CSSValueID::kTop;
  } else if (EqualIgnoringASCIICase(alignment, "middle")) {
    vertical_align_value = CSSValueID::kWebkitBaselineMiddle;
  } else if (EqualIgnoringASCIICase(alignment, "center")) {
    vertical_align_value = CSSValueID::kMiddle;
  } else if (EqualIgnoringASCIICase(alignment, "bottom")) {
    vertical_align_value = CSSValueID::kBaseline;
  } else if (EqualIgnoringASCIICase(alignment, "texttop")) {
    vertical_align_value = CSSValueID::kTextTop;
  }

  if (IsValidCSSValueID(float_value)) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kFloat,
                                            float_value);
  }

  if (IsValidCSSValueID(vertical_align_value)) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kVerticalAlign, vertical_align_value);
  }
}

bool HTMLElement::HasCustomFocusLogic() const {
  return false;
}

ContentEditableType HTMLElement::contentEditableNormalized() const {
  AtomicString value =
      FastGetAttribute(html_names::kContenteditableAttr).LowerASCII();

  if (value.IsNull())
    return ContentEditableType::kInherit;
  if (value.empty() || value == keywords::kTrue) {
    return ContentEditableType::kContentEditable;
  }
  if (value == keywords::kFalse) {
    return ContentEditableType::kNotContentEditable;
  }
  if (value == keywords::kPlaintextOnly) {
    return ContentEditableType::kPlaintextOnly;
  }

  return ContentEditableType::kInherit;
}

String HTMLElement::contentEditable() const {
  switch (contentEditableNormalized()) {
    case ContentEditableType::kInherit:
      return keywords::kInherit;
    case ContentEditableType::kContentEditable:
      return keywords::kTrue;
    case ContentEditableType::kNotContentEditable:
      return keywords::kFalse;
    case ContentEditableType::kPlaintextOnly:
      return keywords::kPlaintextOnly;
  }
}

void HTMLElement::setContentEditable(const String& enabled,
                                     ExceptionState& exception_state) {
  String lower_value = enabled.LowerASCII();
  if (lower_value == keywords::kTrue) {
    setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  } else if (lower_value == keywords::kFalse) {
    setAttribute(html_names::kContenteditableAttr, keywords::kFalse);
  } else if (lower_value == keywords::kPlaintextOnly) {
    setAttribute(html_names::kContenteditableAttr, keywords::kPlaintextOnly);
  } else if (lower_value == keywords::kInherit) {
    removeAttribute(html_names::kContenteditableAttr);
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The value provided ('" + enabled +
                                          "') is not one of 'true', 'false', "
                                          "'plaintext-only', or 'inherit'.");
  }
}

V8UnionBooleanOrStringOrUnrestrictedDouble* HTMLElement::hidden() const {
  const AtomicString& attribute = FastGetAttribute(html_names::kHiddenAttr);

  if (!RuntimeEnabledFeatures::BeforeMatchEventEnabled(GetExecutionContext())) {
    return MakeGarbageCollected<V8UnionBooleanOrStringOrUnrestrictedDouble>(
        attribute != g_null_atom);
  }

  if (attribute == g_null_atom) {
    return MakeGarbageCollected<V8UnionBooleanOrStringOrUnrestrictedDouble>(
        false);
  }
  if (attribute == "until-found") {
    return MakeGarbageCollected<V8UnionBooleanOrStringOrUnrestrictedDouble>(
        String("until-found"));
  }
  return MakeGarbageCollected<V8UnionBooleanOrStringOrUnrestrictedDouble>(true);
}

void HTMLElement::setHidden(
    const V8UnionBooleanOrStringOrUnrestrictedDouble* value) {
  if (!value) {
    removeAttribute(html_names::kHiddenAttr);
    return;
  }
  switch (value->GetContentType()) {
    case V8UnionBooleanOrStringOrUnrestrictedDouble::ContentType::kBoolean:
      if (value->GetAsBoolean()) {
        setAttribute(html_names::kHiddenAttr, g_empty_atom);
      } else {
        removeAttribute(html_names::kHiddenAttr);
      }
      break;
    case V8UnionBooleanOrStringOrUnrestrictedDouble::ContentType::kString:
      if (RuntimeEnabledFeatures::BeforeMatchEventEnabled(
              GetExecutionContext()) &&
          EqualIgnoringASCIICase(value->GetAsString(), "until-found")) {
        setAttribute(html_names::kHiddenAttr, AtomicString("until-found"));
      } else if (value->GetAsString() == "") {
        removeAttribute(html_names::kHiddenAttr);
      } else {
        setAttribute(html_names::kHiddenAttr, g_empty_atom);
      }
      break;
    case V8UnionBooleanOrStringOrUnrestrictedDouble::ContentType::
        kUnrestrictedDouble:
      double double_value = value->GetAsUnrestrictedDouble();
      if (double_value && !std::isnan(double_value)) {
        setAttribute(html_names::kHiddenAttr, g_empty_atom);
      } else {
        removeAttribute(html_names::kHiddenAttr);
      }
      break;
  }
}

namespace {

PopoverValueType GetPopoverTypeFromAttributeValue(const AtomicString& value) {
  AtomicString lower_value = value.LowerASCII();
  if (lower_value == keywords::kAuto || (!value.IsNull() && value.empty())) {
    return PopoverValueType::kAuto;
  } else if (lower_value == keywords::kHint &&
             RuntimeEnabledFeatures::HTMLPopoverHintEnabled()) {
    return PopoverValueType::kHint;
  } else if (lower_value == keywords::kManual) {
    return PopoverValueType::kManual;
  } else if (!value.IsNull()) {
    // Invalid values default to popover=manual.
    return PopoverValueType::kManual;
  }
  return PopoverValueType::kNone;
}
}  // namespace

void HTMLElement::UpdatePopoverAttribute(const AtomicString& value) {
  if (HTMLListboxElement::IsSelectlistAssociated(this)) {
    CHECK(RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
    // Selectlist listboxes manage their own popover state.
    return;
  }

  PopoverValueType type = GetPopoverTypeFromAttributeValue(value);
  if (type == PopoverValueType::kManual &&
      !EqualIgnoringASCIICase(value, keywords::kManual)) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "Found a 'popover' attribute with an invalid value."));
    UseCounter::Count(GetDocument(), WebFeature::kPopoverTypeInvalid);
  }
  if (HasPopoverAttribute()) {
    if (PopoverType() == type)
      return;
    String original_type = FastGetAttribute(html_names::kPopoverAttr);
    // If the popover type is changing, hide it.
    if (popoverOpen()) {
      HidePopoverInternal(
          HidePopoverFocusBehavior::kFocusPreviousElement,
          HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
          /*exception_state=*/nullptr);
      // Event handlers could have changed the popover, including by removing
      // the popover attribute, or changing its value. If that happened, we need
      // to make sure that PopoverData's copy of the popover attribute stays in
      // sync.
      type = GetPopoverTypeFromAttributeValue(
          FastGetAttribute(html_names::kPopoverAttr));
    }
  }
  if (type == PopoverValueType::kNone) {
    if (HasPopoverAttribute()) {
      // If the popover attribute is being removed, remove the PopoverData.
      RemovePopoverData();
    }
    return;
  }
  UseCounter::Count(GetDocument(), WebFeature::kValidPopoverAttribute);
  switch (type) {
    case PopoverValueType::kAuto:
      UseCounter::Count(GetDocument(), WebFeature::kPopoverTypeAuto);
      break;
    case PopoverValueType::kHint:
      UseCounter::Count(GetDocument(), WebFeature::kPopoverTypeHint);
      break;
    case PopoverValueType::kManual:
      UseCounter::Count(GetDocument(), WebFeature::kPopoverTypeManual);
      break;
    case PopoverValueType::kNone:
      NOTREACHED();
  }
  CHECK_EQ(type, GetPopoverTypeFromAttributeValue(
                     FastGetAttribute(html_names::kPopoverAttr)));
  EnsurePopoverData()->setType(type);
}

bool HTMLElement::HasPopoverAttribute() const {
  return GetPopoverData();
}

AtomicString HTMLElement::popover() const {
  auto attribute_value =
      FastGetAttribute(html_names::kPopoverAttr).LowerASCII();
  if (attribute_value.IsNull()) {
    return attribute_value;  // Nullable
  } else if (attribute_value.empty()) {
    return keywords::kAuto;  // ReflectEmpty = "auto"
  } else if (attribute_value == keywords::kAuto ||
             attribute_value == keywords::kManual) {
    return attribute_value;  // ReflectOnly
  } else if (attribute_value == keywords::kHint &&
             RuntimeEnabledFeatures::HTMLPopoverHintEnabled()) {
    return attribute_value;  // ReflectOnly (with HTMLPopoverHint enabled)
  } else {
    return keywords::kManual;  // ReflectInvalid = "manual"
  }
}
void HTMLElement::setPopover(const AtomicString& value) {
  setAttribute(html_names::kPopoverAttr, value);
}

PopoverValueType HTMLElement::PopoverType() const {
  return GetPopoverData() ? GetPopoverData()->type() : PopoverValueType::kNone;
}

// This should be true when `:popover-open` should match.
bool HTMLElement::popoverOpen() const {
  if (auto* popover_data = GetPopoverData())
    return popover_data->visibilityState() == PopoverVisibilityState::kShowing;
  return false;
}

bool HTMLElement::IsPopoverReady(PopoverTriggerAction action,
                                 ExceptionState* exception_state,
                                 bool include_event_handler_text,
                                 Document* expected_document) const {
  CHECK_NE(action, PopoverTriggerAction::kNone);

  auto maybe_throw_exception = [&exception_state, &include_event_handler_text](
                                   DOMExceptionCode code, const char* msg) {
    if (exception_state) {
      String error_message =
          String(msg) +
          (include_event_handler_text
               ? " This might have been the result of the \"beforetoggle\" "
                 "event handler changing the state of this popover."
               : "");
      exception_state->ThrowDOMException(code, error_message);
    }
  };

  if (!HasPopoverAttribute() &&
      !HTMLListboxElement::IsSelectlistAssociated(this)) {
    maybe_throw_exception(DOMExceptionCode::kNotSupportedError,
                          "Not supported on elements that do not have a valid "
                          "value for the 'popover' attribute.");
    return false;
  }
  if (action == PopoverTriggerAction::kShow &&
      GetPopoverData()->visibilityState() != PopoverVisibilityState::kHidden) {
    if (!RuntimeEnabledFeatures::PopoverDialogDontThrowEnabled()) {
      maybe_throw_exception(DOMExceptionCode::kInvalidStateError,
                            "Invalid on popover elements which aren't hidden.");
    }
    return false;
  }
  if (action == PopoverTriggerAction::kHide &&
      GetPopoverData()->visibilityState() != PopoverVisibilityState::kShowing) {
    // Important to check that visibility is not kShowing (rather than
    // popoverOpen()), because a hide transition might have been started on this
    // popover already, and we don't want to allow a double-hide.
    if (!RuntimeEnabledFeatures::PopoverDialogDontThrowEnabled()) {
      maybe_throw_exception(
          DOMExceptionCode::kInvalidStateError,
          "Invalid on popover elements that aren't already showing.");
    }
    return false;
  }
  if (!isConnected()) {
    maybe_throw_exception(DOMExceptionCode::kInvalidStateError,
                          "Invalid on disconnected popover elements.");
    return false;
  }
  if (expected_document && &GetDocument() != expected_document) {
    maybe_throw_exception(DOMExceptionCode::kInvalidStateError,
                          "Invalid when the document changes while showing or "
                          "hiding a popover element.");
    return false;
  }
  if (auto* dialog = DynamicTo<HTMLDialogElement>(this)) {
    if (action == PopoverTriggerAction::kShow && dialog->IsModal()) {
      maybe_throw_exception(DOMExceptionCode::kInvalidStateError,
                            "The dialog is already open as a dialog, and "
                            "therefore cannot be opened as a popover.");
      return false;
    }
  }
  if (action == PopoverTriggerAction::kShow &&
      Fullscreen::IsFullscreenElement(*this)) {
    maybe_throw_exception(
        DOMExceptionCode::kInvalidStateError,
        "This element is already in fullscreen mode, and therefore cannot be "
        "opened as a popover.");
    return false;
  }
  return true;
}

namespace {
// We have to mark *all* invokers for the given popover dirty in the
// ax tree, since they all should now have an updated expanded state.
void MarkPopoverInvokersDirty(const HTMLElement& popover) {
  CHECK(popover.HasPopoverAttribute());
  auto& document = popover.GetDocument();
  AXObjectCache* cache = document.ExistingAXObjectCache();
  if (!cache) {
    return;
  }
  for (auto* invoker_candidate :
       *popover.GetTreeScope().RootNode().PopoverInvokers()) {
    auto* invoker = To<HTMLFormControlElement>(invoker_candidate);
    if (popover == invoker->popoverTargetElement().popover) {
      cache->MarkElementDirty(invoker);
    }
  }
}
}  // namespace

bool HTMLElement::togglePopover(ExceptionState& exception_state) {
  if (popoverOpen()) {
    hidePopover(exception_state);
  } else {
    ShowPopoverInternal(/*invoker*/ nullptr, &exception_state);
  }

  if (GetPopoverData()) {
    return GetPopoverData()->visibilityState() ==
           PopoverVisibilityState::kShowing;
  }
  return false;
}

bool HTMLElement::togglePopover(bool force, ExceptionState& exception_state) {
  if (!force && popoverOpen()) {
    hidePopover(exception_state);
  } else if (force && !popoverOpen()) {
    ShowPopoverInternal(/*invoker*/ nullptr, &exception_state);
  } else {
    // Throw an exception if this element is disconnected or doesn't have a
    // popover attribute.
    IsPopoverReady(PopoverTriggerAction::kToggle, &exception_state,
                   /*include_event_handler_text=*/false,
                   /*document=*/nullptr);
  }

  if (GetPopoverData()) {
    return GetPopoverData()->visibilityState() ==
           PopoverVisibilityState::kShowing;
  }
  return false;
}

void HTMLElement::showPopover(ExceptionState& exception_state) {
  ShowPopoverInternal(/*invoker*/ nullptr, &exception_state);
}

void HTMLElement::ShowPopoverInternal(Element* invoker,
                                      ExceptionState* exception_state) {
  if (!IsPopoverReady(PopoverTriggerAction::kShow, exception_state,
                      /*include_event_handler_text=*/false, /*document=*/nullptr)) {
    CHECK(exception_state)
        << " Callers which aren't supposed to throw exceptions should not call "
           "ShowPopoverInternal when the Popover isn't in a valid state to be "
           "shown.";
    return;
  }

  CHECK(!GetPopoverData() || !GetPopoverData()->invoker());

  // Fire events by default, unless we're recursively showing this popover.
  PopoverData::ScopedStartShowingOrHiding scoped_was_showing_or_hiding(*this);
  auto transition_behavior =
      scoped_was_showing_or_hiding
          ? HidePopoverTransitionBehavior::kNoEventsNoWaiting
          : HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions;

  auto& original_document = GetDocument();

  // Fire the "opening" beforetoggle event.
  auto* event = ToggleEvent::Create(
      event_type_names::kBeforetoggle, Event::Cancelable::kYes,
      /*old_state*/ "closed", /*new_state*/ "open");
  CHECK(!event->bubbles());
  CHECK(event->cancelable());
  CHECK_EQ(event->oldState(), "closed");
  CHECK_EQ(event->newState(), "open");
  event->SetTarget(this);
  if (DispatchEvent(*event) != DispatchEventResult::kNotCanceled)
    return;

  // The 'beforetoggle' event handler could have changed this popover, e.g. by
  // changing its type, removing it from the document, moving it to another
  // document, or calling showPopover().
  if (!IsPopoverReady(PopoverTriggerAction::kShow, exception_state,
                      /*include_event_handler_text=*/true,
                      &original_document)) {
    return;
  }

  bool should_restore_focus = false;
  auto original_type = PopoverType();
  if (original_type == PopoverValueType::kAuto ||
      original_type == PopoverValueType::kHint) {
    const auto* ancestor = FindTopmostPopoverAncestor(*this, invoker);
    if (original_type == PopoverValueType::kHint) {
      CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
      // If the new popover is popover=hint, hide other hints first.
      if (original_document.PopoverHintShowing()) {
        original_document.PopoverHintShowing()->HidePopoverInternal(
            HidePopoverFocusBehavior::kNone, transition_behavior,
            exception_state);
      }
      // Then hide open popovers that aren't ancestors of this hint.
      if (ancestor) {
        HideAllPopoversUntil(
            ancestor, original_document, HidePopoverFocusBehavior::kNone,
            transition_behavior, HidePopoverIndependence::kHideUnrelated);
      }
    } else {
      // If the new popover is a popover=auto, hide any popover above this in
      // the stack, if any.
      HideAllPopoversUntil(ancestor, original_document,
                           HidePopoverFocusBehavior::kNone, transition_behavior,
                           HidePopoverIndependence::kHideUnrelated);
    }

    // The 'beforetoggle' event handlers could have changed this popover, e.g.
    // by changing its type, removing it from the document, moving it to
    // another document, or calling showPopover().
    if (PopoverType() != original_type) {
      if (exception_state) {
        exception_state->ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "The value of the popover attribute was changed while hiding the "
            "popover.");
      }
      return;
    }
    if (!IsPopoverReady(PopoverTriggerAction::kShow, exception_state,
                        /*include_event_handler_text=*/true,
                        &original_document)) {
      return;
    }

    // We only restore focus for popover=auto/hint, and only for the first
    // popover in the stack. If there's nothing showing, restore focus.
    should_restore_focus = !original_document.TopmostPopoverOrHint();

    // Add this popover to the popover stack.
    if (original_type == PopoverValueType::kAuto) {
      auto& stack = original_document.PopoverStack();
      CHECK(!stack.Contains(this));
      stack.push_back(this);
    } else {
      CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
      original_document.SetPopoverHintShowing(this);
    }

    if (RuntimeEnabledFeatures::CloseWatcherEnabled()) {
      auto* close_watcher = CloseWatcher::Create(
          GetDocument().domWindow(), /*dialog_for_use_counters=*/nullptr);
      if (close_watcher) {
        auto* event_listener =
            MakeGarbageCollected<PopoverCloseWatcherEventListener>(this);
        close_watcher->addEventListener(event_type_names::kClose,
                                        event_listener);
        close_watcher->addEventListener(event_type_names::kCancel,
                                        event_listener);
      }
      GetPopoverData()->setCloseWatcher(close_watcher);
    }
  }

  MarkPopoverInvokersDirty(*this);
  GetPopoverData()->setPreviouslyFocusedElement(nullptr);
  Element* originally_focused_element = original_document.FocusedElement();
  original_document.AddToTopLayer(this);
  // Make the popover match `:popover-open` and remove `display:none` styling:
  GetPopoverData()->setVisibilityState(PopoverVisibilityState::kShowing);
  GetPopoverData()->setInvoker(invoker);
  PseudoStateChanged(CSSSelector::kPseudoPopoverOpen);
  CHECK(!original_document.AllOpenPopovers().Contains(this));
  original_document.AllOpenPopovers().insert(this);

  // Queue a delayed hide event, if necessary.
  if (RuntimeEnabledFeatures::HTMLPopoverHintEnabled()) {
    if (!GetDocument().HoverElement() ||
        !IsNodePopoverDescendant(*GetDocument().HoverElement())) {
      MaybeQueuePopoverHideEvent();
    }
  }

  SetPopoverFocusOnShow();

  // Store the element to focus when this popover closes.
  if (should_restore_focus && HasPopoverAttribute()) {
    GetPopoverData()->setPreviouslyFocusedElement(originally_focused_element);
  }

  // Queue the "opening" toggle event.
  String old_state = "closed";
  ToggleEvent* after_event;
  if (GetPopoverData()->hasPendingToggleEventTask()) {
    // There's already a queued 'toggle' event. Cancel it and fire a new one
    // keeping the original value for old_state.
    old_state =
        GetPopoverData()->pendingToggleEventStartedClosed() ? "closed" : "open";
    GetPopoverData()->cancelPendingToggleEventTask();
  } else {
    GetPopoverData()->setPendingToggleEventStartedClosed(true);
  }
  after_event = ToggleEvent::Create(event_type_names::kToggle,
                                    Event::Cancelable::kNo, old_state,
                                    /*new_state*/ "open");
  CHECK_EQ(after_event->newState(), "open");
  CHECK_EQ(after_event->oldState(), old_state);
  CHECK(!after_event->bubbles());
  CHECK(!after_event->cancelable());
  after_event->SetTarget(this);
  GetPopoverData()->setPendingToggleEventTask(PostCancellableTask(
      *original_document.GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      WTF::BindOnce(
          [](HTMLElement* element, ToggleEvent* event) {
            CHECK(element);
            CHECK(event);
            element->DispatchEvent(*event);
          },
          WrapPersistent(this), WrapPersistent(after_event))));
}

// static
// All popovers up to, but not including, |endpoint|, will be hidden. If there
// are "unrelated" popovers open, such as a stack of popover=auto popovers and
// |endpoint| is a popover=hint, then the popover_independence argument controls
// whether those unrelated popover=auto popovers are hidden.
void HTMLElement::HideAllPopoversUntil(
    const HTMLElement* endpoint,
    Document& document,
    HidePopoverFocusBehavior focus_behavior,
    HidePopoverTransitionBehavior transition_behavior,
    HidePopoverIndependence popover_independence) {
  CHECK(!endpoint || endpoint->HasPopoverAttribute());
  CHECK(endpoint ||
        popover_independence == HidePopoverIndependence::kHideUnrelated);
  // We never throw exceptions from HideAllPopoversUntil, since it is always
  // used to close other popovers that are already showing.
  ExceptionState* exception_state = nullptr;
  auto close_all_open_popovers = [&document, &focus_behavior,
                                  &transition_behavior, &exception_state]() {
    while (auto* popover = document.TopmostPopoverOrHint()) {
      popover->HidePopoverInternal(focus_behavior, transition_behavior,
                                   exception_state);
    }
  };

  if (!endpoint)
    return close_all_open_popovers();

  auto& stack = document.PopoverStack();
  if (endpoint->PopoverType() == PopoverValueType::kHint) {
    CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
    if (popover_independence == HidePopoverIndependence::kHideUnrelated) {
      if (document.PopoverHintShowing() &&
          document.PopoverHintShowing() != endpoint) {
        document.PopoverHintShowing()->HidePopoverInternal(
            focus_behavior, transition_behavior, exception_state);
      }
      // Close all auto popovers.
      while (!stack.empty()) {
        stack.back()->HidePopoverInternal(focus_behavior, transition_behavior,
                                          exception_state);
      }
    }
  } else {
    CHECK_EQ(endpoint->PopoverType(), PopoverValueType::kAuto);

    bool repeating_hide = false;
    do {
      const Element* hint_ancestor = nullptr;
      if (document.PopoverHintShowing()) {
        // If there is a popover=hint showing that is a descendant of something
        // on the popover=auto stack, then the hint should be hidden before that
        // ancestor is hidden, regardless of popover_independence.
        hint_ancestor = FindTopmostPopoverAncestor(
            *document.PopoverHintShowing(),
            document.PopoverHintShowing()->GetPopoverData()->invoker());
        if (!hint_ancestor &&
            popover_independence == HidePopoverIndependence::kHideUnrelated) {
          document.PopoverHintShowing()->HidePopoverInternal(
              focus_behavior, transition_behavior, exception_state);
        }
      }
      // Then hide everything in the popover=auto stack until the last_to_hide
      // popover is closed, or the stack is empty.
      const HTMLElement* last_to_hide = nullptr;
      bool found_endpoint = false;
      for (auto popover : stack) {
        if (popover == endpoint) {
          found_endpoint = true;
        } else if (found_endpoint) {
          last_to_hide = popover;
          break;
        }
      }
      if (!found_endpoint) {
        return close_all_open_popovers();
      }
      if (!last_to_hide && document.PopoverHintShowing() &&
          hint_ancestor == endpoint) {
        CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
        // endpoint is the top of the popover stack, and there's a nested hint.
        document.PopoverHintShowing()->HidePopoverInternal(
            focus_behavior, transition_behavior, exception_state);
      }
      while (last_to_hide && last_to_hide->popoverOpen() && !stack.empty()) {
        // We never throw exceptions from HideAllPopoversUntil, since it is
        // always used to close other popovers that are already showing.
        if (stack.back() == hint_ancestor) {
          CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
          document.PopoverHintShowing()->HidePopoverInternal(
              focus_behavior, transition_behavior, exception_state);
        }
        stack.back()->HidePopoverInternal(focus_behavior, transition_behavior,
                                          exception_state);
      }

      // Now check if we're left with endpoint at the top of the stack.
      repeating_hide = stack.Contains(endpoint) && stack.back() != endpoint;
      if (repeating_hide) {
        // No longer fire events.
        transition_behavior = HidePopoverTransitionBehavior::kNoEventsNoWaiting;
        document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "The `beforetoggle` event handler for a popover triggered another "
            "popover to be shown. This is not recommended."));
      }
    } while (repeating_hide);
  }
}

void HTMLElement::hidePopover(ExceptionState& exception_state) {
  HidePopoverInternal(
      HidePopoverFocusBehavior::kFocusPreviousElement,
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
      &exception_state);
}

void HTMLElement::HidePopoverInternal(
    HidePopoverFocusBehavior focus_behavior,
    HidePopoverTransitionBehavior transition_behavior,
    ExceptionState* exception_state) {
  if (!IsPopoverReady(PopoverTriggerAction::kHide, exception_state,
                      /*include_event_handler_text=*/true, /*document=*/nullptr)) {
    return;
  }

  auto& document = GetDocument();
  bool show_warning =
      transition_behavior != HidePopoverTransitionBehavior::kNoEventsNoWaiting;
  PopoverData::ScopedStartShowingOrHiding scoped_was_showing_or_hiding(
      *this, show_warning);
  if (scoped_was_showing_or_hiding) {
    // We're in a loop, so stop firing events.
    transition_behavior = HidePopoverTransitionBehavior::kNoEventsNoWaiting;
  }

  if (PopoverType() == PopoverValueType::kAuto ||
      PopoverType() == PopoverValueType::kHint) {
    // Hide any popovers above us in the stack.
    HideAllPopoversUntil(this, document, focus_behavior, transition_behavior,
                         HidePopoverIndependence::kLeaveUnrelated);
    // The 'beforetoggle' event handlers could have changed this popover, e.g.
    // by changing its type, removing it from the document, or calling
    // hidePopover().
    if (!IsPopoverReady(PopoverTriggerAction::kHide, exception_state,
                        /*include_event_handler_text=*/true, &document)) {
      return;
    }
  }

  MarkPopoverInvokersDirty(*this);
  GetPopoverData()->setInvoker(nullptr);
  // Events are only fired in the case that the popover is not being removed
  // from the document.
  if (transition_behavior ==
      HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions) {
    // Fire the "closing" beforetoggle event.
    auto* event = ToggleEvent::Create(
        event_type_names::kBeforetoggle, Event::Cancelable::kNo,
        /*old_state*/ "open", /*new_state*/ "closed");
    CHECK(!event->bubbles());
    CHECK(!event->cancelable());
    CHECK_EQ(event->oldState(), "open");
    CHECK_EQ(event->newState(), "closed");
    event->SetTarget(this);
    auto result = DispatchEvent(*event);
    if (result != DispatchEventResult::kNotCanceled) {
      // The event can be cancelled before dispatch, if the target or execution
      // context no longer exists, etc. See crbug.com/1445329.
      CHECK_EQ(result, DispatchEventResult::kCanceledBeforeDispatch);
      return;
    }

    // The 'beforetoggle' event handler could have changed this popover, e.g. by
    // changing its type, removing it from the document, or calling
    // showPopover().
    if (!IsPopoverReady(PopoverTriggerAction::kHide, exception_state,
                        /*include_event_handler_text=*/true, &document)) {
      return;
    }

    // Queue the "closing" toggle event.
    String old_state = "open";
    ToggleEvent* after_event;
    if (GetPopoverData()->hasPendingToggleEventTask()) {
      // There's already a queued 'toggle' event. Cancel it and fire a new one
      // keeping the original value for old_state.
      old_state = GetPopoverData()->pendingToggleEventStartedClosed() ? "closed"
                                                                      : "open";
      GetPopoverData()->cancelPendingToggleEventTask();
    } else {
      GetPopoverData()->setPendingToggleEventStartedClosed(false);
    }
    after_event = ToggleEvent::Create(event_type_names::kToggle,
                                      Event::Cancelable::kNo, old_state,
                                      /*new_state*/ "closed");
    CHECK_EQ(after_event->newState(), "closed");
    CHECK_EQ(after_event->oldState(), old_state);
    CHECK(!after_event->bubbles());
    CHECK(!after_event->cancelable());
    after_event->SetTarget(this);
    GetPopoverData()->setPendingToggleEventTask(PostCancellableTask(
        *document.GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
        WTF::BindOnce(
            [](HTMLElement* element, ToggleEvent* event) {
              CHECK(element);
              CHECK(event);
              element->DispatchEvent(*event);
            },
            WrapPersistent(this), WrapPersistent(after_event))));

    document.ScheduleForTopLayerRemoval(this,
                                        Document::TopLayerReason::kPopover);
  } else {
    document.RemoveFromTopLayerImmediately(this);
  }

  // Remove this popover from the stack.
  if (PopoverType() == PopoverValueType::kAuto) {
    auto& stack = document.PopoverStack();
    CHECK(!stack.empty());
    CHECK_EQ(stack.back(), this);
    stack.pop_back();
  } else if (PopoverType() == PopoverValueType::kHint) {
    CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
    CHECK_EQ(document.TopmostPopoverOrHint(), this);
    document.SetPopoverHintShowing(nullptr);
  }

  // Re-apply display:none, and stop matching `:popover-open`.
  GetPopoverData()->setVisibilityState(PopoverVisibilityState::kHidden);
  PseudoStateChanged(CSSSelector::kPseudoPopoverOpen);
  document.AllOpenPopovers().erase(this);

  Element* previously_focused_element =
      GetPopoverData()->previouslyFocusedElement();
  if (previously_focused_element) {
    GetPopoverData()->setPreviouslyFocusedElement(nullptr);
    if (focus_behavior == HidePopoverFocusBehavior::kFocusPreviousElement &&
        contains(document.AdjustedFocusedElement())) {
      FocusOptions* focus_options = FocusOptions::Create();
      focus_options->setPreventScroll(true);
      previously_focused_element->Focus(FocusParams(
          SelectionBehaviorOnFocus::kRestore, mojom::blink::FocusType::kScript,
          /*capabilities=*/nullptr, focus_options));
    }
  }

  if (auto* selectlist = popoverOwnerSelectListElement()) {
    // popoverOwnerSelectListElement() is set on both the <selectlist> listbox
    // and the <selectlist> autofill preview popover.
    if (selectlist->ListBoxPart() == this) {
      selectlist->ListboxWasClosed();
    }
  }

  if (auto* close_watcher = GetPopoverData()->closeWatcher()) {
    close_watcher->destroy();
    GetPopoverData()->setCloseWatcher(nullptr);
  }
}

void HTMLElement::SetPopoverFocusOnShow() {
  // The layout must be updated here because we call Element::isFocusable,
  // which requires an up-to-date layout.
  GetDocument().UpdateStyleAndLayoutTreeForNode(this,
                                                DocumentUpdateReason::kPopover);

  if (auto* dialog = DynamicTo<HTMLDialogElement>(this)) {
    if (RuntimeEnabledFeatures::DialogNewFocusBehaviorEnabled()) {
      dialog->SetFocusForDialog();
    } else {
      HTMLDialogElement::SetFocusForDialogLegacy(dialog);
    }
    return;
  }

  Element* control = IsAutofocusable() ? this : GetAutofocusDelegate();

  // If the popover does not use autofocus, then the focus should remain on the
  // currently active element.
  // https://open-ui.org/components/popover.research.explainer#focus-management
  if (!control)
    return;

  // 3. Run the focusing steps for control.
  control->Focus();

  // 4. Let topDocument be the active document of control's node document's
  // browsing context's top-level browsing context.
  // 5. If control's node document's origin is not the same as the origin of
  // topDocument, then return.
  Document& doc = control->GetDocument();
  if (!doc.IsActive())
    return;
  if (!doc.IsInMainFrame() &&
      !doc.TopFrameOrigin()->CanAccess(
          doc.GetExecutionContext()->GetSecurityOrigin())) {
    return;
  }

  // 6. Empty topDocument's autofocus candidates.
  // 7. Set topDocument's autofocus processed flag to true.
  doc.TopDocument().FinalizeAutofocus();
}

namespace {

// Remember to keep kMinValue and kMaxValue in sync.
enum class PopoverAncestorOptions {
  kExclusive,
  kIncludeManualPopovers,

  // For `PopoverAncestorOptionsSet`.
  kMinValue = kExclusive,
  kMaxValue = kIncludeManualPopovers,
};
using PopoverAncestorOptionsSet =
    base::EnumSet<PopoverAncestorOptions,
                  PopoverAncestorOptions::kMinValue,
                  PopoverAncestorOptions::kMaxValue>;

template <typename UnaryPredicate>
const HTMLElement* NearestMatchingAncestor(
    const Node* node,
    const PopoverAncestorOptionsSet ancestor_options,
    const UnaryPredicate get_candidate_popover) {
  if (ancestor_options.Has(PopoverAncestorOptions::kExclusive) && node) {
    node = FlatTreeTraversal::Parent(*node);
  }
  for (; node; node = FlatTreeTraversal::Parent(*node)) {
    auto* candidate_popover = get_candidate_popover(node);
    if (!candidate_popover || !candidate_popover->popoverOpen()) {
      continue;
    }
    if (!ancestor_options.Has(PopoverAncestorOptions::kIncludeManualPopovers) &&
        candidate_popover->PopoverType() == PopoverValueType::kManual) {
      continue;
    }
    return candidate_popover;
  }
  return nullptr;
}

const HTMLElement* NearestOpenPopover(
    const Node* node,
    const PopoverAncestorOptionsSet ancestor_options =
        PopoverAncestorOptionsSet()) {
  return NearestMatchingAncestor(
      node, ancestor_options,
      [](const Node* test_node) { return DynamicTo<HTMLElement>(test_node); });
}

const HTMLElement* NearestTargetPopoverForInvoker(
    const Node* node,
    const PopoverAncestorOptionsSet ancestor_options =
        PopoverAncestorOptionsSet()) {
  return NearestMatchingAncestor(
      node, ancestor_options, [](const Node* test_node) {
        auto* form_element = DynamicTo<HTMLFormControlElement>(test_node);
        return form_element ? const_cast<HTMLFormControlElement*>(form_element)
                                  ->popoverTargetElement()
                                  .popover.Get()
                            : nullptr;
      });
}

}  // namespace

// static
// This function will return the topmost (highest in the popover stack)
// ancestral popover for the provided popover. Popovers can be related to each
// other in several ways, creating a tree of popovers. There are three paths
// through which one popover (call it the "child" popover) can have an ancestor
// popover (call it the "parent" popover):
//  1. the popovers are nested within each other in the DOM tree. In this case,
//     the descendant popover is the "child" and its ancestor popover is the
//     "parent".
//  2. a popover has an `anchor` attribute pointing to another element in the
//     DOM. In this case, the popover is the "child", and the DOM-contained
//     popover of its anchor element is the "parent". If the anchor doesn't
//     point to an element, or that element isn't contained within a popover, no
//     such relationship exists.
//  3. an invoking element (e.g. a <button>) has one of the invoking attributes
//     (e.g. popovertoggletarget) pointing to a popover. In this case, the
//     popover is the "child", and the DOM-contained popover of the invoking
//     element is the "parent". As with anchor, the invoker must be in a popover
//     and reference an open popover.
// In each of the relationships formed above, the parent popover must be
// strictly lower in the popover stack than the child popover, or it does not
// form a valid ancestral relationship. This eliminates non-showing popovers and
// self-pointers (e.g. a popover with an anchor attribute that points back to
// the same popover), and it allows for the construction of a well-formed tree
// from the (possibly cyclic) graph of connections. For example, if two popovers
// have anchors pointing to each other, the only valid relationship is that the
// first one to open is the "parent" and the second is the "child". Only
// popover=auto popovers can *be* ancestors, and only popover=auto/hint popovers
// can *have* ancestors.
const HTMLElement* HTMLElement::FindTopmostPopoverAncestor(
    HTMLElement& new_popover,
    Element* new_popovers_invoker) {
  CHECK(new_popover.HasPopoverAttribute());
  CHECK_NE(new_popover.PopoverType(), PopoverValueType::kManual);
  auto& document = new_popover.GetDocument();

  // Build a map from each open popover to its position in the stack.
  HeapHashMap<Member<const HTMLElement>, int> popover_positions;
  int indx = 0;
  for (auto popover : document.PopoverStack()) {
    popover_positions.Set(popover, indx++);
  }
  if (auto* popover_hint = document.PopoverHintShowing()) {
    popover_positions.Set(popover_hint, indx++);
  }
  popover_positions.Set(&new_popover, indx++);

  const HTMLElement* topmost_popover_ancestor = nullptr;
  auto check_ancestor = [&topmost_popover_ancestor,
                         &popover_positions](const Element* to_check) {
    auto* candidate_ancestor = NearestOpenPopover(to_check);
    if (!candidate_ancestor ||
        candidate_ancestor->PopoverType() != PopoverValueType::kAuto) {
      return;
    }
    int candidate_position = popover_positions.at(candidate_ancestor);
    if (!topmost_popover_ancestor ||
        popover_positions.at(topmost_popover_ancestor) < candidate_position) {
      topmost_popover_ancestor = candidate_ancestor;
    }
  };
  // Add the three types of ancestor relationships to the map:
  // 1. DOM tree ancestor.
  check_ancestor(FlatTreeTraversal::ParentElement(new_popover));
  // 2. Anchor attribute.
  check_ancestor(new_popover.anchorElement());
  // 3. Invoker to popover
  check_ancestor(new_popovers_invoker);
  return topmost_popover_ancestor;
}

namespace {
// For light dismiss, we need to find the closest popover that the user has
// clicked. For hover triggering, we need to find the closest popover that is
// related to a hovered node. In both cases, this is the nearest DOM ancestor
// that is either a popover or the invoking element for a popover. It is
// possible both exist, in which case the topmost one (highest on the popover
// stack) is returned.
const HTMLElement* FindTopmostRelatedPopover(
    const Node& node,
    const PopoverAncestorOptionsSet& ancestor_options =
        PopoverAncestorOptionsSet()) {
  auto& document = node.GetDocument();
  // Check if we're in an invoking element or a popover, and choose
  // the higher popover on the stack.
  auto* direct_popover_ancestor = NearestOpenPopover(&node, ancestor_options);
  auto* invoker_popover_ancestor =
      NearestTargetPopoverForInvoker(&node, ancestor_options);
  auto get_stack_position = [&document](const HTMLElement* popover) {
    if (popover && popover == document.PopoverHintShowing()) {
      return document.PopoverStack().size() + 1;
    }
    auto pos = document.PopoverStack().Find(popover);
    return pos == kNotFound ? 0 : (pos + 1);
  };
  if (!invoker_popover_ancestor ||
      get_stack_position(direct_popover_ancestor) >
          get_stack_position(invoker_popover_ancestor)) {
    return direct_popover_ancestor;
  }
  return invoker_popover_ancestor;
}
}  // namespace

// static
void HTMLElement::HandlePopoverLightDismiss(const Event& event,
                                            const Node& target_node) {
  CHECK(event.isTrusted());
  auto& document = target_node.GetDocument();
  if (!document.TopmostPopoverOrHint()) {
    return;
  }

  const AtomicString& event_type = event.type();
  if (IsA<PointerEvent>(event)) {
    // PointerEventManager will call this function before actually dispatching
    // the event.
    CHECK(!event.HasEventPath());
    CHECK_EQ(Event::PhaseType::kNone, event.eventPhase());

    if (event_type == event_type_names::kPointerdown) {
      document.SetPopoverPointerdownTarget(
          FindTopmostRelatedPopover(target_node));
    } else if (event_type == event_type_names::kPointerup) {
      // Hide everything up to the clicked element. We do this on pointerup,
      // rather than pointerdown or click, primarily for accessibility concerns.
      // See
      // https://www.w3.org/WAI/WCAG21/Understanding/pointer-cancellation.html
      // for more information on why it is better to perform potentially
      // destructive actions (including hiding a popover) on pointer-up rather
      // than pointer-down. To properly handle the use case where a user starts
      // a pointer-drag on a popover, and finishes off the popover (to highlight
      // text), the ancestral popover is stored in pointerdown and compared
      // here.
      auto* ancestor_popover = FindTopmostRelatedPopover(target_node);
      bool same_target =
          ancestor_popover == document.PopoverPointerdownTarget();
      document.SetPopoverPointerdownTarget(nullptr);
      if (same_target) {
        HideAllPopoversUntil(
            ancestor_popover, document, HidePopoverFocusBehavior::kNone,
            HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
            HidePopoverIndependence::kHideUnrelated);
      }
    }
  } else if (event_type == event_type_names::kKeydown) {
    // TODO(http://crbug.com/1171318): Remove this keydown branch once
    // CloseWatcher has been fully enabled. CloseWatcher will handle escape key
    // presses instead of this.
    DCHECK(!RuntimeEnabledFeatures::CloseWatcherEnabled());
    const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event);
    if (key_event && key_event->key() == "Escape") {
      CHECK(!event.GetEventPath().IsEmpty());
      CHECK_EQ(Event::PhaseType::kNone, event.eventPhase());
      // Escape key just pops the topmost popover=auto/hint off the stack.
      document.TopmostPopoverOrHint()->HidePopoverInternal(
          HidePopoverFocusBehavior::kFocusPreviousElement,
          HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
          /*exception_state=*/nullptr);
    }
  }
}

void HTMLElement::InvokePopover(Element* invoker) {
  CHECK(invoker);
  CHECK(HasPopoverAttribute());
  ShowPopoverInternal(invoker, /*exception_state=*/nullptr);
}

Element* HTMLElement::anchorElement() {
  // TODO(crbug.com/1425215): Fix GetElementAttribute() for out-of-tree-scope
  // elements, so that we can remove the hack below.
  if (!IsInTreeScope()) {
    return nullptr;
  }
  Element* element = GetElementAttribute(html_names::kAnchorAttr);
  return element;
}

void HTMLElement::setAnchorElement(Element* new_element) {
  SetElementAttribute(html_names::kAnchorAttr, new_element);
  EnsureAnchorElementObserver().Notify();
}

// Must be called on an Element that is a popover. Returns true if |node| is a
// descendant of this popover. This includes the case where |node| is contained
// within another popover, and the container popover is a descendant of this
// popover. This also includes "indirect" relationships that the popover API
// provides, such as through invoking elements or via the anchor attribute.
// Note that in the special case of popover=manual popovers, which do not
// usually have ancestral relationships, this function *will* check for invoker
// and anchor relationships to form descendant edges. This is important for the
// `popover-hide-delay` CSS property, which works for all popover types, and
// needs to keep popovers open when a descendant is hovered.
bool HTMLElement::IsNodePopoverDescendant(const Node& node) const {
  CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
  CHECK(HasPopoverAttribute());
  const HTMLElement* ancestor = FindTopmostRelatedPopover(
      node, {PopoverAncestorOptions::kIncludeManualPopovers});
  while (ancestor) {
    if (ancestor == this) {
      return true;
    }
    ancestor = FindTopmostRelatedPopover(
        *ancestor, PopoverAncestorOptionsSet{
                       PopoverAncestorOptions::kExclusive,
                       PopoverAncestorOptions::kIncludeManualPopovers});
  }
  return false;
}

void HTMLElement::MaybeQueuePopoverHideEvent() {
  CHECK(RuntimeEnabledFeatures::HTMLPopoverHintEnabled());
  CHECK(HasPopoverAttribute());
  // If the popover isn't showing, or it has an infinite PopoverHideDelay, do
  // nothing.
  if (GetPopoverData()->visibilityState() == PopoverVisibilityState::kHidden) {
    return;
  }
  if (!GetComputedStyle()) {
    return;
  }
  float hide_delay_seconds = GetComputedStyle()->PopoverHideDelay();
  // If the value is infinite or NaN, don't hide the popover.
  if (!std::isfinite(hide_delay_seconds)) {
    return;
  }
  // Queue the task to hide this popover.
  GetPopoverData()->setHoverHideTask(PostDelayedCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault),
      FROM_HERE,
      WTF::BindOnce(
          [](HTMLElement* popover) {
            if (!popover->HasPopoverAttribute()) {
              return;
            }
            // We're hover-hiding this popover, so remove *all* hover show
            // tasks.
            popover->GetPopoverData()->hoverShowTasks().clear();
            if (!popover->popoverOpen()) {
              return;
            }
            popover->HidePopoverInternal(
                HidePopoverFocusBehavior::kFocusPreviousElement,
                HidePopoverTransitionBehavior::kFireEventsAndWaitForTransitions,
                /*exception_state=*/nullptr);
          },
          WrapWeakPersistent(this)),
      base::Seconds(hide_delay_seconds)));
}

// static
void HTMLElement::HoveredElementChanged(Element* old_element,
                                        Element* new_element) {
  if (!RuntimeEnabledFeatures::HTMLPopoverHintEnabled()) {
    return;
  }
  if (old_element) {
    // For the previously-hovered element: loop through all showing popovers
    // (including popover=manual) and see if the element that just lost focus
    // was an ancestor. If so, queue a task to hide it after a delay.
    for (auto& popover : old_element->GetDocument().AllOpenPopovers()) {
      if (popover->IsNodePopoverDescendant(*old_element)) {
        popover->MaybeQueuePopoverHideEvent();
      }
    }
  }
  // It is possible that both old_element and new_element are descendants of
  // the same open popover, in which case we'll queue a hide task and then
  // immediately cancel it, resulting in no change.
  if (new_element) {
    // For the newly-hovered element: loop through all showing popovers and see
    // if the newly-focused element is an ancestor. If so, cancel that popover's
    // hide-after-delay task.
    for (auto& popover : new_element->GetDocument().AllOpenPopovers()) {
      if (popover->IsNodePopoverDescendant(*new_element)) {
        popover->GetPopoverData()->setHoverHideTask(TaskHandle());
      }
    }
  }
}

void HTMLElement::SetPopoverOwnerSelectListElement(
    HTMLSelectListElement* element) {
  CHECK(RuntimeEnabledFeatures::HTMLSelectListElementEnabled());
  CHECK(HasPopoverAttribute());
  GetPopoverData()->setOwnerSelectListElement(element);
}

HTMLSelectListElement* HTMLElement::popoverOwnerSelectListElement() const {
  return GetPopoverData() ? GetPopoverData()->ownerSelectListElement()
                          : nullptr;
}

bool HTMLElement::DispatchFocusEvent(
    Element* old_focused_element,
    mojom::blink::FocusType type,
    InputDeviceCapabilities* source_capabilities) {
  return Element::DispatchFocusEvent(old_focused_element, type,
                                     source_capabilities);
}

const AtomicString& HTMLElement::autocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, kOff, ("off"));
  DEFINE_STATIC_LOCAL(const AtomicString, kNone, ("none"));
  DEFINE_STATIC_LOCAL(const AtomicString, kCharacters, ("characters"));
  DEFINE_STATIC_LOCAL(const AtomicString, kWords, ("words"));
  DEFINE_STATIC_LOCAL(const AtomicString, kSentences, ("sentences"));

  const AtomicString& value = FastGetAttribute(html_names::kAutocapitalizeAttr);
  if (value.empty())
    return g_empty_atom;

  if (EqualIgnoringASCIICase(value, kNone) ||
      EqualIgnoringASCIICase(value, kOff))
    return kNone;
  if (EqualIgnoringASCIICase(value, kCharacters))
    return kCharacters;
  if (EqualIgnoringASCIICase(value, kWords))
    return kWords;
  // "sentences", "on", or an invalid value
  return kSentences;
}

void HTMLElement::setAutocapitalize(const AtomicString& value) {
  setAttribute(html_names::kAutocapitalizeAttr, value);
}

bool HTMLElement::isContentEditableForBinding() const {
  return IsEditableOrEditingHost(*this);
}

bool HTMLElement::draggable() const {
  return EqualIgnoringASCIICase(FastGetAttribute(html_names::kDraggableAttr),
                                "true");
}

void HTMLElement::setDraggable(bool value) {
  setAttribute(html_names::kDraggableAttr,
               value ? keywords::kTrue : keywords::kFalse);
}

bool HTMLElement::spellcheck() const {
  return IsSpellCheckingEnabled();
}

void HTMLElement::setSpellcheck(bool enable) {
  setAttribute(html_names::kSpellcheckAttr,
               enable ? keywords::kTrue : keywords::kFalse);
}

void HTMLElement::click() {
  DispatchSimulatedClick(nullptr, SimulatedClickCreationScope::kFromScript);
  if (IsA<HTMLInputElement>(this)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLInputElementSimulatedClick);
  }
}

void HTMLElement::AccessKeyAction(SimulatedClickCreationScope creation_scope) {
  DispatchSimulatedClick(nullptr, creation_scope);
}

String HTMLElement::title() const {
  return FastGetAttribute(html_names::kTitleAttr);
}

TranslateAttributeMode HTMLElement::GetTranslateAttributeMode() const {
  const AtomicString& value = FastGetAttribute(html_names::kTranslateAttr);

  if (value == g_null_atom)
    return kTranslateAttributeInherit;
  if (EqualIgnoringASCIICase(value, "yes") || EqualIgnoringASCIICase(value, ""))
    return kTranslateAttributeYes;
  if (EqualIgnoringASCIICase(value, "no"))
    return kTranslateAttributeNo;

  return kTranslateAttributeInherit;
}

bool HTMLElement::translate() const {
  for (const HTMLElement* element = this; element;
       element = Traversal<HTMLElement>::FirstAncestor(*element)) {
    TranslateAttributeMode mode = element->GetTranslateAttributeMode();
    if (mode != kTranslateAttributeInherit) {
      DCHECK(mode == kTranslateAttributeYes || mode == kTranslateAttributeNo);
      return mode == kTranslateAttributeYes;
    }
  }

  // Default on the root element is translate=yes.
  return true;
}

void HTMLElement::setTranslate(bool enable) {
  setAttribute(html_names::kTranslateAttr, AtomicString(enable ? "yes" : "no"));
}

// Returns the conforming 'dir' value associated with the state the attribute is
// in (in its canonical case), if any, or the empty string if the attribute is
// in a state that has no associated keyword value or if the attribute is not in
// a defined state (e.g. the attribute is missing and there is no missing value
// default).
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#limited-to-only-known-values
static inline const AtomicString& ToValidDirValue(const AtomicString& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, ltr_value, ("ltr"));
  DEFINE_STATIC_LOCAL(const AtomicString, rtl_value, ("rtl"));
  DEFINE_STATIC_LOCAL(const AtomicString, auto_value, ("auto"));

  if (EqualIgnoringASCIICase(value, ltr_value))
    return ltr_value;
  if (EqualIgnoringASCIICase(value, rtl_value))
    return rtl_value;
  if (EqualIgnoringASCIICase(value, auto_value))
    return auto_value;
  return g_null_atom;
}

const AtomicString& HTMLElement::dir() {
  return ToValidDirValue(FastGetAttribute(html_names::kDirAttr));
}

void HTMLElement::setDir(const AtomicString& value) {
  setAttribute(html_names::kDirAttr, value);
}

HTMLFormElement* HTMLElement::formOwner() const {
  if (const auto* internals = GetElementInternals())
    return internals->Form();
  return nullptr;
}

HTMLFormElement* HTMLElement::FindFormAncestor() const {
  return Traversal<HTMLFormElement>::FirstAncestor(*this);
}

static inline bool ElementAffectsDirectionality(const Node* node) {
  auto* html_element = DynamicTo<HTMLElement>(node);
  return html_element && (IsA<HTMLBDIElement>(*html_element) ||
                          IsValidDirAttribute(html_element->FastGetAttribute(
                              html_names::kDirAttr)));
}

void HTMLElement::ChildrenChanged(const ChildrenChange& change) {
  Element::ChildrenChanged(change);

  if (GetDocument().HasDirAttribute()) {
    AdjustDirectionalityIfNeededAfterChildrenChanged(change);

    if (change.IsChildInsertion() && !SelfOrAncestorHasDirAutoAttribute()) {
      auto* element = DynamicTo<HTMLElement>(change.sibling_changed);
      if (element && !element->NeedsInheritDirectionalityFromParent() &&
          !ElementAffectsDirectionality(element))
        element->UpdateDirectionalityAndDescendant(CachedDirectionality());
    }
  }
  if (change.IsChildInsertion()) {
    CheckSoftNavigationHeuristicsTracking(GetDocument(),
                                          change.sibling_changed);
  }
}

bool HTMLElement::HasDirectionAuto() const {
  // <bdi> defaults to dir="auto"
  // https://html.spec.whatwg.org/C/#the-bdi-element
  const AtomicString& direction = FastGetAttribute(html_names::kDirAttr);
  return (IsA<HTMLBDIElement>(*this) && direction == g_null_atom) ||
         EqualIgnoringASCIICase(direction, "auto");
}

// TODO(https://crbug.com/576815): Once the CSSPseudoDir flag is
// removed, this function no longer needs to be templatized over
// Traversal since it can always use NodeTraversal.
template <typename Traversal>
absl::optional<TextDirection> HTMLElement::ResolveAutoDirectionality(
    bool& is_deferred,
    Node* stay_within) const {
  is_deferred = false;
  if (auto* input_element = DynamicTo<HTMLInputElement>(*this)) {
    return BidiParagraph::BaseDirectionForStringOrLtr(input_element->Value());
  }

  // For <textarea>, the heuristic is applied on a per-paragraph level, and
  // we should traverse the flat tree.
  Node* node = (IsA<HTMLTextAreaElement>(*this) || IsA<HTMLSlotElement>(*this))
                   ? FlatTreeTraversal::FirstChild(*this)
                   : Traversal::FirstChild(*this);
  while (node) {
    // Skip bdi, script, style and text form controls.
    auto* element = DynamicTo<Element>(node);
    if (EqualIgnoringASCIICase(node->nodeName(), "bdi") ||
        IsA<HTMLScriptElement>(*node) || IsA<HTMLStyleElement>(*node) ||
        (element && element->IsTextControl()) ||
        (element && element->ShadowPseudoId() ==
                        shadow_element_names::kPseudoInputPlaceholder)) {
      node = Traversal::NextSkippingChildren(*node, stay_within);
      continue;
    }

    auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node);
    if (slot && !RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
      ShadowRoot* root = slot->ContainingShadowRoot();
      // Defer to adjust the directionality to avoid recalcuating slot
      // assignment in FlatTreeTraversal when updating slot.
      // ResolveAutoDirectionality will be adjusted after recalculating its
      // children.
      if (root->NeedsSlotAssignmentRecalc()) {
        is_deferred = true;
        return TextDirection::kLtr;
      }
    }

    // Skip elements with valid dir attribute
    if (auto* element_node = DynamicTo<Element>(node)) {
      AtomicString dir_attribute_value =
          element_node->FastGetAttribute(html_names::kDirAttr);
      if (IsValidDirAttribute(dir_attribute_value)) {
        node = Traversal::NextSkippingChildren(*node, stay_within);
        continue;
      }
    }

    // TODO(https://crbug.com/576815): Once we have final spec text for
    // https://github.com/whatwg/html/issues/3699 we should recheck the
    // relative order of this check and the "Skip elements with valid
    // dir attribute" check above, and add tests for the case that
    // exercises both.  (Note that if the order is switched, this test
    // needs to consider the dir attribute on the slot element rather
    // than just jumping to its shadow host.)
    if (slot && RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
      ShadowRoot* root = slot->ContainingShadowRoot();
      return root->host().CachedDirectionality();
    }

    if (node->IsTextNode()) {
      if (const absl::optional<TextDirection> text_direction =
              BidiParagraph::BaseDirectionForString(node->textContent(true))) {
        return *text_direction;
      }
    }

    if (slot) {
      absl::optional<TextDirection> text_direction =
          slot->ResolveAutoDirectionality<FlatTreeTraversal>(is_deferred,
                                                             stay_within);
      if (text_direction.has_value())
        return text_direction;
    }

    node = Traversal::Next(*node, stay_within);
  }
  return absl::nullopt;
}

void HTMLElement::AdjustDirectionalityIfNeededAfterChildAttributeChanged(
    Element* child) {
  DCHECK(SelfOrAncestorHasDirAutoAttribute());
  bool is_deferred;
  TextDirection text_direction =
      ResolveAutoDirectionality<NodeTraversal>(is_deferred, this)
          .value_or(TextDirection::kLtr);
  if (CachedDirectionality() != text_direction && !is_deferred) {
    SetCachedDirectionality(text_direction);

    for (Element* element_to_adjust = this; element_to_adjust;
         element_to_adjust =
             FlatTreeTraversal::ParentElement(*element_to_adjust)) {
      if (ElementAffectsDirectionality(element_to_adjust)) {
        DynamicTo<HTMLElement>(element_to_adjust)
            ->UpdateDirectionalityAndDescendant(text_direction);

        const ComputedStyle* style = GetComputedStyle();
        if (style && style->Direction() != text_direction) {
          element_to_adjust->SetNeedsStyleRecalc(
              kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                     style_change_reason::kWritingModeChange));
        }
        return;
      }
    }
  }
}

bool HTMLElement::CalculateAndAdjustAutoDirectionality(Node* stay_within) {
  bool is_deferred = false;
  TextDirection text_direction =
      ResolveAutoDirectionality<NodeTraversal>(is_deferred, stay_within)
          .value_or(TextDirection::kLtr);
  if (CachedDirectionality() != text_direction && !is_deferred) {
    UpdateDirectionalityAndDescendant(text_direction);

    const ComputedStyle* style = GetComputedStyle();
    if (style && style->Direction() != text_direction) {
      SetNeedsStyleRecalc(kLocalStyleChange,
                          StyleChangeReasonForTracing::Create(
                              style_change_reason::kWritingModeChange));
      return true;
    }
  }

  return false;
}

void HTMLElement::AdjustDirectionalityIfNeededAfterChildrenChanged(
    const ChildrenChange& change) {
  if (!SelfOrAncestorHasDirAutoAttribute())
    return;

  Node* stay_within = nullptr;
  if (change.type == ChildrenChangeType::kTextChanged) {
    CHECK(change.old_text);
    TextDirection old_text_direction =
        BidiParagraph::BaseDirectionForStringOrLtr(*change.old_text);
    auto* character_data = DynamicTo<CharacterData>(change.sibling_changed);
    DCHECK(character_data);
    TextDirection new_text_direction =
        BidiParagraph::BaseDirectionForStringOrLtr(character_data->data());
    if (old_text_direction == new_text_direction)
      return;
    stay_within = change.sibling_changed;
  } else if (change.IsChildInsertion()) {
    if (change.sibling_changed->IsTextNode()) {
      const absl::optional<TextDirection> new_text_direction =
          BidiParagraph::BaseDirectionForString(
              change.sibling_changed->textContent(true));
      if (!new_text_direction ||
          *new_text_direction == CachedDirectionality()) {
        return;
      }
    }
    stay_within = change.sibling_changed;
  }

  UpdateDescendantHasDirAutoAttribute(true /* has_dir_auto */);

  for (Element* element_to_adjust = this; element_to_adjust;
       element_to_adjust =
           FlatTreeTraversal::ParentElement(*element_to_adjust)) {
    if (ElementAffectsDirectionality(element_to_adjust)) {
      if (To<HTMLElement>(element_to_adjust)
              ->CalculateAndAdjustAutoDirectionality(
                  stay_within ? stay_within : element_to_adjust)) {
        SetNeedsStyleRecalc(kLocalStyleChange,
                            StyleChangeReasonForTracing::Create(
                                style_change_reason::kPseudoClass));
      }
      if (RuntimeEnabledFeatures::CSSPseudoDirEnabled())
        element_to_adjust->PseudoStateChanged(CSSSelector::kPseudoDir);
      return;
    }
  }
}

void HTMLElement::AdjustDirectionalityIfNeededAfterShadowRootChanged() {
  DCHECK(IsShadowHost(this));
  if (SelfOrAncestorHasDirAutoAttribute()) {
    for (auto* element_to_adjust = this; element_to_adjust;
         element_to_adjust = DynamicTo<HTMLElement>(
             FlatTreeTraversal::ParentElement(*element_to_adjust))) {
      if (ElementAffectsDirectionality(element_to_adjust)) {
        element_to_adjust->CalculateAndAdjustAutoDirectionality(
            element_to_adjust);
        return;
      }
    }
  } else if (!NeedsInheritDirectionalityFromParent()) {
    UpdateDescendantDirectionality(CachedDirectionality());
  }
}

void HTMLElement::AdjustCandidateDirectionalityForSlot(
    HeapHashSet<Member<Node>> candidate_set) {
  HeapHashSet<Member<HTMLElement>> directionality_set;
  // Transfer a candidate directionality set to |directionality_set| to avoid
  // the tree walk to the duplicated parent node for the directionality.
  for (auto& node : candidate_set) {
    Node* node_to_adjust = node.Get();
    if (!node->SelfOrAncestorHasDirAutoAttribute()) {
      if (ElementAffectsDirectionality(node))
        continue;
      auto* slot = node->AssignedSlot();
      if (slot && slot->SelfOrAncestorHasDirAutoAttribute()) {
        node_to_adjust = slot;
      } else {
        if (slot && !slot->NeedsInheritDirectionalityFromParent()) {
          node->SetCachedDirectionality(slot->CachedDirectionality());
        }
        continue;
      }
    }

    bool needs_slot_assignment_recalc = false;
    for (auto* element_to_adjust = DynamicTo<HTMLElement>(node_to_adjust);
         element_to_adjust;
         element_to_adjust = GetParentForDirectionality(
             *element_to_adjust, needs_slot_assignment_recalc)) {
      if (ElementAffectsDirectionality(element_to_adjust)) {
        directionality_set.insert(element_to_adjust);
        continue;
      }
    }
  }

  for (auto& element : directionality_set) {
    if (element->CalculateAndAdjustAutoDirectionality(element) &&
        RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
      element->SetNeedsStyleRecalc(kLocalStyleChange,
                                   StyleChangeReasonForTracing::Create(
                                       style_change_reason::kPseudoClass));
    }
  }
}

Node::InsertionNotificationRequest HTMLElement::InsertedInto(
    ContainerNode& insertion_point) {
  // Process the superclass first to ensure that `InActiveDocument()` is
  // updated.
  Element::InsertedInto(insertion_point);
  HideNonce();

  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().InsertedInto(insertion_point);

  if (AnchorElementObserver* observer = GetAnchorElementObserver()) {
    observer->Notify();
  }

  return kInsertionDone;
}

void HTMLElement::RemovedFrom(ContainerNode& insertion_point) {
  if (HasPopoverAttribute()) {
    // If a popover is removed from the document, make sure it gets
    // removed from the popover element stack and the top layer.
    bool was_in_document = insertion_point.isConnected();
    if (was_in_document) {
      // We can't run focus event handlers while removing elements.
      HidePopoverInternal(HidePopoverFocusBehavior::kNone,
                          HidePopoverTransitionBehavior::kNoEventsNoWaiting,
                          /*exception_state=*/nullptr);
    }
  }

  Element::RemovedFrom(insertion_point);
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().RemovedFrom(insertion_point);

  if (AnchorElementObserver* observer = GetAnchorElementObserver()) {
    observer->Notify();
  }
}

void HTMLElement::DidMoveToNewDocument(Document& old_document) {
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().DidMoveToNewDocument(old_document);
  Element::DidMoveToNewDocument(old_document);
}

void HTMLElement::AddHTMLLengthToStyle(MutableCSSPropertyValueSet* style,
                                       CSSPropertyID property_id,
                                       const String& value,
                                       AllowPercentage allow_percentage,
                                       AllowZero allow_zero) {
  HTMLDimension dimension;
  if (!ParseDimensionValue(value, dimension))
    return;
  if (property_id == CSSPropertyID::kWidth &&
      (dimension.IsPercentage() || dimension.IsRelative())) {
    UseCounter::Count(GetDocument(), WebFeature::kHTMLElementDeprecatedWidth);
  }
  if (dimension.IsRelative())
    return;
  if (dimension.IsPercentage() &&
      allow_percentage == kDontAllowPercentageValues)
    return;
  if (dimension.Value() == 0 && allow_zero == kDontAllowZeroValues)
    return;
  CSSPrimitiveValue::UnitType unit =
      dimension.IsPercentage() ? CSSPrimitiveValue::UnitType::kPercentage
                               : CSSPrimitiveValue::UnitType::kPixels;
  AddPropertyToPresentationAttributeStyle(style, property_id, dimension.Value(),
                                          unit);
}

static Color ParseColorStringWithCrazyLegacyRules(const String& color_string) {
  // Per spec, only look at the first 128 digits of the string.
  const size_t kMaxColorLength = 128;
  // We'll pad the buffer with two extra 0s later, so reserve two more than the
  // max.
  Vector<char, kMaxColorLength + 2> digit_buffer;

  wtf_size_t i = 0;
  // Skip a leading #.
  if (color_string[0] == '#')
    i = 1;

  // Grab the first 128 characters, replacing non-hex characters with 0.
  // Non-BMP characters are replaced with "00" due to them appearing as two
  // "characters" in the String.
  for (; i < color_string.length() && digit_buffer.size() < kMaxColorLength;
       i++) {
    if (!IsASCIIHexDigit(color_string[i]))
      digit_buffer.push_back('0');
    else
      digit_buffer.push_back(color_string[i]);
  }

  if (!digit_buffer.size())
    return Color::kBlack;

  // Pad the buffer out to at least the next multiple of three in size.
  digit_buffer.push_back('0');
  digit_buffer.push_back('0');

  if (digit_buffer.size() < 6) {
    return Color::FromRGB(ToASCIIHexValue(digit_buffer[0]),
                          ToASCIIHexValue(digit_buffer[1]),
                          ToASCIIHexValue(digit_buffer[2]));
  }

  // Split the digits into three components, then search the last 8 digits of
  // each component.
  DCHECK_GE(digit_buffer.size(), 6u);
  wtf_size_t component_length = digit_buffer.size() / 3;
  wtf_size_t component_search_window_length =
      std::min<wtf_size_t>(component_length, 8);
  wtf_size_t red_index = component_length - component_search_window_length;
  wtf_size_t green_index =
      component_length * 2 - component_search_window_length;
  wtf_size_t blue_index = component_length * 3 - component_search_window_length;
  // Skip digits until one of them is non-zero, or we've only got two digits
  // left in the component.
  while (digit_buffer[red_index] == '0' && digit_buffer[green_index] == '0' &&
         digit_buffer[blue_index] == '0' &&
         (component_length - red_index) > 2) {
    red_index++;
    green_index++;
    blue_index++;
  }
  DCHECK_LT(red_index + 1, component_length);
  DCHECK_GE(green_index, component_length);
  DCHECK_LT(green_index + 1, component_length * 2);
  DCHECK_GE(blue_index, component_length * 2);
  SECURITY_DCHECK(blue_index + 1 < digit_buffer.size());

  int red_value =
      ToASCIIHexValue(digit_buffer[red_index], digit_buffer[red_index + 1]);
  int green_value =
      ToASCIIHexValue(digit_buffer[green_index], digit_buffer[green_index + 1]);
  int blue_value =
      ToASCIIHexValue(digit_buffer[blue_index], digit_buffer[blue_index + 1]);
  return Color::FromRGB(red_value, green_value, blue_value);
}

// Color parsing that matches HTML's "rules for parsing a legacy color value"
bool HTMLElement::ParseColorWithLegacyRules(const String& attribute_value,
                                            Color& parsed_color) {
  // An empty string doesn't apply a color. (One containing only whitespace
  // does, which is why this check occurs before stripping.)
  if (attribute_value.empty())
    return false;

  String color_string = attribute_value.StripWhiteSpace();

  // "transparent" doesn't apply a color either.
  if (EqualIgnoringASCIICase(color_string, "transparent"))
    return false;

  // If the string is a 3/6-digit hex color or a named CSS color, use that.
  // Apply legacy rules otherwise. Note color.setFromString() accepts 4/8-digit
  // hex color, so restrict its use with length checks here to support legacy
  // HTML attributes.

  bool success = false;
  if ((color_string.length() == 4 || color_string.length() == 7) &&
      color_string[0] == '#')
    success = parsed_color.SetFromString(color_string);
  if (!success)
    success = parsed_color.SetNamedColor(color_string);
  if (!success) {
    parsed_color = ParseColorStringWithCrazyLegacyRules(color_string);
    success = true;
  }

  return success;
}

void HTMLElement::AddHTMLColorToStyle(MutableCSSPropertyValueSet* style,
                                      CSSPropertyID property_id,
                                      const String& attribute_value) {
  Color parsed_color;
  if (!ParseColorWithLegacyRules(attribute_value, parsed_color))
    return;

  style->SetProperty(property_id, *cssvalue::CSSColor::Create(parsed_color));
}

LabelsNodeList* HTMLElement::labels() {
  if (!IsLabelable())
    return nullptr;
  return EnsureCachedCollection<LabelsNodeList>(kLabelsNodeListType);
}

bool HTMLElement::IsInteractiveContent() const {
  return false;
}

void HTMLElement::DefaultEventHandler(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (event.type() == event_type_names::kKeypress && keyboard_event) {
    HandleKeypressEvent(*keyboard_event);
    if (event.DefaultHandled())
      return;
  }

  Element::DefaultEventHandler(event);
}

bool HTMLElement::HandleKeyboardActivation(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  if (keyboard_event) {
    if (event.type() == event_type_names::kKeydown &&
        keyboard_event->key() == " ") {
      SetActive(true);
      // No setDefaultHandled() - IE dispatches a keypress in this case.
      return true;
    }
    if (event.type() == event_type_names::kKeypress) {
      switch (keyboard_event->charCode()) {
        case '\r':
          DispatchSimulatedClick(&event);
          event.SetDefaultHandled();
          return true;
        case ' ':
          // Prevent scrolling down the page.
          event.SetDefaultHandled();
          return true;
      }
    }
    if (event.type() == event_type_names::kKeyup &&
        keyboard_event->key() == " ") {
      if (IsActive())
        DispatchSimulatedClick(&event);
      event.SetDefaultHandled();
      return true;
    }
  }
  return false;
}

bool HTMLElement::MatchesReadOnlyPseudoClass() const {
  return !MatchesReadWritePseudoClass();
}

// https://html.spec.whatwg.org/multipage/semantics-other.html#selector-read-write
// The :read-write pseudo-class must match ... elements that are editing hosts
// or editable and are neither input elements nor textarea elements
bool HTMLElement::MatchesReadWritePseudoClass() const {
  return IsEditableOrEditingHost(*this);
}

void HTMLElement::HandleKeypressEvent(KeyboardEvent& event) {
  if (!IsSpatialNavigationEnabled(GetDocument().GetFrame()) || !SupportsFocus())
    return;
  if (RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return;
  GetDocument().UpdateStyleAndLayoutTree();
  // if the element is a text form control (like <input type=text> or
  // <textarea>) or has contentEditable attribute on, we should enter a space or
  // newline even in spatial navigation mode instead of handling it as a "click"
  // action.
  if (IsTextControl() || IsEditable(*this))
    return;
  int char_code = event.charCode();
  if (char_code == '\r' || char_code == ' ') {
    DispatchSimulatedClick(&event);
    event.SetDefaultHandled();
  }
}

int HTMLElement::AdjustedOffsetForZoom(LayoutUnit offset) {
  const auto* layout_object = GetLayoutObject();
  DCHECK(layout_object);
  return AdjustForAbsoluteZoom::AdjustLayoutUnit(offset,
                                                 layout_object->StyleRef())
      .Round();
}

int HTMLElement::OffsetTopOrLeft(bool top) {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  const auto* layout_object = GetLayoutBoxModelObject();
  if (!layout_object)
    return 0;

  HeapHashSet<Member<TreeScope>> ancestor_tree_scopes = GetAncestorTreeScopes();
  LayoutUnit offset;
  Element* offset_parent = this;
  bool new_spec_behavior =
      RuntimeEnabledFeatures::OffsetParentNewSpecBehaviorEnabled();
  // This loop adds up all of the offsetTop/offsetLeft values for this and
  // parent shadow-hidden offsetParents up the flat tree. If
  // |ancestor_tree_scopes| doesn't contain the next |offset_parent|'s
  // TreeScope, then we know that |offset_parent| is shadow-hidden from |this|.
  do {
    // offset_parent->OffsetParent() may update style and layout:
    Element* next_offset_parent = offset_parent->OffsetParent();
    if (const auto* offset_parent_layout_object =
            offset_parent->GetLayoutBoxModelObject()) {
      if (top) {
        offset += offset_parent_layout_object->OffsetTop(next_offset_parent);
      } else {
        offset += offset_parent_layout_object->OffsetLeft(next_offset_parent);
      }
    }
    offset_parent = next_offset_parent;
  } while (new_spec_behavior && offset_parent &&
           !ancestor_tree_scopes.Contains(&offset_parent->GetTreeScope()));

  return AdjustedOffsetForZoom(offset);
}

int HTMLElement::offsetLeftForBinding() {
  return OffsetTopOrLeft(/*top=*/false);
}

int HTMLElement::offsetTopForBinding() {
  return OffsetTopOrLeft(/*top=*/true);
}

int HTMLElement::offsetWidthForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  int result = 0;
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    result = AdjustedOffsetForZoom(layout_object->OffsetWidth());
    RecordScrollbarSizeForStudy(result, /* is_width= */ true,
                                /* is_offset= */ true);
  }
  return result;
}

DISABLE_CFI_PERF
int HTMLElement::offsetHeightForBinding() {
  GetDocument().EnsurePaintLocationDataValidForNode(
      this, DocumentUpdateReason::kJavaScript);
  int result = 0;
  if (const auto* layout_object = GetLayoutBoxModelObject()) {
    result = AdjustedOffsetForZoom(layout_object->OffsetHeight());
    RecordScrollbarSizeForStudy(result, /* is_width= */ false,
                                /* is_offset= */ true);
  }
  return result;
}

Element* HTMLElement::unclosedOffsetParent() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return nullptr;

  return layout_object->OffsetParent(this);
}

void HTMLElement::UpdateDescendantHasDirAutoAttribute(bool has_dir_auto) {
  Node* node = FlatTreeTraversal::FirstChild(*this);
  while (node) {
    if (auto* element = DynamicTo<Element>(node)) {
      AtomicString dir_attribute_value =
          element->FastGetAttribute(html_names::kDirAttr);
      if (IsValidDirAttribute(dir_attribute_value)) {
        node = FlatTreeTraversal::NextSkippingChildren(*node, this);
        continue;
      }

      if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
        ShadowRoot* root = slot->ContainingShadowRoot();
        // Defer to adjust the directionality to avoid recalcuating slot
        // assignment in FlatTreeTraversal when updating slot.
        // Slot and its children will be updated after recalculating children.
        if (root->NeedsSlotAssignmentRecalc()) {
          root->SetNeedsDirAutoAttributeUpdate(true);
          node = FlatTreeTraversal::NextSkippingChildren(*node, this);
          continue;
        }
      }

      if (!has_dir_auto) {
        if (!element->SelfOrAncestorHasDirAutoAttribute()) {
          node = FlatTreeTraversal::NextSkippingChildren(*node, this);
          continue;
        }
        element->ClearSelfOrAncestorHasDirAutoAttribute();
      } else {
        if (element->SelfOrAncestorHasDirAutoAttribute()) {
          node = FlatTreeTraversal::NextSkippingChildren(*node, this);
          continue;
        }
        element->SetSelfOrAncestorHasDirAutoAttribute();
      }
    }
    node = FlatTreeTraversal::Next(*node, this);
  }
}

void HTMLElement::UpdateDirectionalityAndDescendant(TextDirection direction) {
  SetCachedDirectionality(direction);
  UpdateDescendantDirectionality(direction);
}

void HTMLElement::UpdateDescendantDirectionality(TextDirection direction) {
  Node* node = FlatTreeTraversal::FirstChild(*this);
  while (node) {
    if (IsA<HTMLElement>(node)) {
      if (ElementAffectsDirectionality(node) ||
          node->CachedDirectionality() == direction) {
        node = FlatTreeTraversal::NextSkippingChildren(*node, this);
        continue;
      }

      node->SetCachedDirectionality(direction);
      if (auto* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node)) {
        ShadowRoot* root = slot->ContainingShadowRoot();
        // Defer to update the directionality of slot's descendant to avoid
        // recalcuating slot assignment in FlatTreeTraversal when updating slot.
        if (root->NeedsSlotAssignmentRecalc()) {
          node = FlatTreeTraversal::NextSkippingChildren(*node, this);
          continue;
        }
      }
    }
    node = FlatTreeTraversal::Next(*node, this);
  }
}

void HTMLElement::OnDirAttrChanged(const AttributeModificationParams& params) {
  // If an ancestor has dir=auto, and this node has the first character,
  // changes to dir attribute may affect the ancestor.
  if (!IsValidDirAttribute(params.old_value) &&
      !IsValidDirAttribute(params.new_value))
    return;

  GetDocument().SetHasDirAttribute();

  bool is_old_auto = SelfOrAncestorHasDirAutoAttribute();
  bool is_new_auto = HasDirectionAuto();

  if (is_new_auto) {
    if (auto* input_element = DynamicTo<HTMLInputElement>(*this)) {
      input_element->EnsureShadowSubtree();
    }
  }

  bool needs_slot_assignment_recalc = false;
  auto* parent =
      GetParentForDirectionality(*this, needs_slot_assignment_recalc);
  if (!is_old_auto || !is_new_auto) {
    if (parent && parent->SelfOrAncestorHasDirAutoAttribute()) {
      parent->AdjustDirectionalityIfNeededAfterChildAttributeChanged(this);
    }
  }

  if (is_old_auto && !is_new_auto) {
    ClearSelfOrAncestorHasDirAutoAttribute();
    UpdateDescendantHasDirAutoAttribute(false /* has_dir_auto */);
  } else if (!is_old_auto && is_new_auto) {
    SetSelfOrAncestorHasDirAutoAttribute();
    UpdateDescendantHasDirAutoAttribute(true /* has_dir_auto */);
  }

  if (is_new_auto) {
    CalculateAndAdjustAutoDirectionality(this);
  } else {
    absl::optional<TextDirection> text_direction;
    if (EqualIgnoringASCIICase(params.new_value, "ltr")) {
      text_direction = TextDirection::kLtr;
    } else if (EqualIgnoringASCIICase(params.new_value, "rtl")) {
      text_direction = TextDirection::kRtl;
    }

    if (!text_direction.has_value()) {
      if (parent) {
        text_direction = parent->CachedDirectionality();
      } else {
        text_direction = TextDirection::kLtr;
      }
    }

    if (needs_slot_assignment_recalc) {
      SetNeedsInheritDirectionalityFromParent();
    } else {
      UpdateDirectionalityAndDescendant(*text_direction);
    }
  }

  if (RuntimeEnabledFeatures::CSSPseudoDirEnabled()) {
    SetNeedsStyleRecalc(
        kSubtreeStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kPseudoClass));
    PseudoStateChanged(CSSSelector::kPseudoDir);
  }
}

void HTMLElement::OnPopoverChanged(const AttributeModificationParams& params) {
  UpdatePopoverAttribute(params.new_value);
}

void HTMLElement::ReparseAttribute(const AttributeModificationParams& params) {
  Element::ParseAttribute(params);
}

void HTMLElement::OnFormAttrChanged(const AttributeModificationParams& params) {
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().FormAttributeChanged();
}

void HTMLElement::OnLangAttrChanged(const AttributeModificationParams& params) {
  LangAttributeChanged();
}

void HTMLElement::OnNonceAttrChanged(
    const AttributeModificationParams& params) {
  if (params.new_value != g_empty_atom)
    setNonce(params.new_value);
}

ElementInternals* HTMLElement::attachInternals(
    ExceptionState& exception_state) {
  // 1. If this's is value is not null, then throw a "NotSupportedError"
  // DOMException.
  if (IsValue()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Unable to attach ElementInternals to a customized built-in element.");
    return nullptr;
  }

  // 2. Let definition be the result of looking up a custom element definition
  // given this's node document, its namespace, its local name, and null as the
  // is value.
  CustomElementRegistry* registry = CustomElement::Registry(*this);
  auto* definition =
      registry ? registry->DefinitionForName(localName()) : nullptr;

  // 3. If definition is null, then throw an "NotSupportedError" DOMException.
  if (!definition) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Unable to attach ElementInternals to non-custom elements.");
    return nullptr;
  }

  // 4. If definition's disable internals is true, then throw a
  // "NotSupportedError" DOMException.
  if (definition->DisableInternals()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ElementInternals is disabled by disabledFeature static field.");
    return nullptr;
  }

  // 5. If this's attached internals is true, then throw an "NotSupportedError"
  // DOMException.
  if (DidAttachInternals()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "ElementInternals for the specified element was already attached.");
    return nullptr;
  }

  // 6. If this's custom element state is not "precustomized" or "custom", then
  // throw a "NotSupportedError" DOMException.
  if (GetCustomElementState() != CustomElementState::kCustom &&
      GetCustomElementState() != CustomElementState::kPreCustomized) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The attachInternals() function cannot be called prior to the "
        "execution of the custom element constructor.");
    return nullptr;
  }

  // 7. Set this's attached internals to true.
  SetDidAttachInternals();
  // 8. Return a new ElementInternals instance whose target element is this.
  UseCounter::Count(GetDocument(), WebFeature::kElementAttachInternals);
  return &EnsureElementInternals();
}

bool HTMLElement::IsFormAssociatedCustomElement() const {
  return GetCustomElementState() == CustomElementState::kCustom &&
         GetCustomElementDefinition()->IsFormAssociated();
}

bool HTMLElement::SupportsFocus() const {
  return Element::SupportsFocus() && !IsDisabledFormControl();
}

bool HTMLElement::IsDisabledFormControl() const {
  if (!IsFormAssociatedCustomElement())
    return false;
  return const_cast<HTMLElement*>(this)
      ->EnsureElementInternals()
      .IsActuallyDisabled();
}

bool HTMLElement::MatchesEnabledPseudoClass() const {
  return IsFormAssociatedCustomElement() && !const_cast<HTMLElement*>(this)
                                                 ->EnsureElementInternals()
                                                 .IsActuallyDisabled();
}

bool HTMLElement::MatchesValidityPseudoClasses() const {
  return IsFormAssociatedCustomElement();
}

bool HTMLElement::willValidate() const {
  return IsFormAssociatedCustomElement() && const_cast<HTMLElement*>(this)
                                                ->EnsureElementInternals()
                                                .WillValidate();
}

bool HTMLElement::IsValidElement() {
  return IsFormAssociatedCustomElement() &&
         EnsureElementInternals().IsValidElement();
}

bool HTMLElement::IsLabelable() const {
  return IsFormAssociatedCustomElement();
}

void HTMLElement::FinishParsingChildren() {
  Element::FinishParsingChildren();
  if (IsFormAssociatedCustomElement())
    EnsureElementInternals().TakeStateAndRestore();
}

void HTMLElement::ParserDidSetAttributes() {
  Element::ParserDidSetAttributes();

  if (GetDocument().HasDirAttribute() && !HasDirectionAuto() &&
      !ElementAffectsDirectionality(this)) {
    bool needs_slot_assignment_recalc = false;
    auto* parent =
        GetParentForDirectionality(*this, needs_slot_assignment_recalc);
    if (needs_slot_assignment_recalc)
      SetNeedsInheritDirectionalityFromParent();
    else if (parent)
      SetCachedDirectionality(parent->CachedDirectionality());
  }
}

}  // namespace blink

#ifndef NDEBUG

// For use in the debugger
void dumpInnerHTML(blink::HTMLElement*);

void dumpInnerHTML(blink::HTMLElement* element) {
  printf("%s\n", element->innerHTML().Ascii().c_str());
}

#endif
