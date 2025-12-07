/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
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

#include "third_party/blink/renderer/core/svg/svg_element.h"

#include "base/auto_reset.h"
#include "base/strings/string_view_util.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/svg/animation/element_smil_animations.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

SVGElement::SVGElement(const QualifiedName& tag_name,
                       Document& document,
                       ConstructionType construction_type)
    : Element(tag_name, &document, construction_type),
      svg_rare_data_(nullptr),
      class_name_(
          MakeGarbageCollected<SVGAnimatedString>(this,
                                                  html_names::kClassAttr)) {
  if (!RuntimeEnabledFeatures::Svg2CascadeEnabled()) {
    SetHasCustomStyleCallbacks();
  }
}

void SVGElement::DetachLayoutTree(bool performing_reattach) {
  Element::DetachLayoutTree(performing_reattach);
  // To avoid a noncollectable Blink GC reference cycle, we must clear the
  // ComputedStyle here. See http://crbug.com/878032#c11
  if (HasSVGRareData())
    SvgRareData()->ClearOverriddenComputedStyle();
}

void SVGElement::WillRecalcStyle(const StyleRecalcChange change) {
  if (!HasSVGRareData())
    return;
  // If the style changes because of a regular property change (not induced by
  // SMIL animations themselves) reset the "computed style without SMIL style
  // properties", so the base value change gets reflected.
  if (change.ShouldRecalcStyleFor(*this))
    SvgRareData()->SetNeedsOverrideComputedStyleUpdate();
}

SVGElementRareData* SVGElement::EnsureSVGRareData() {
  if (!svg_rare_data_)
    svg_rare_data_ = MakeGarbageCollected<SVGElementRareData>();
  return svg_rare_data_.Get();
}

bool SVGElement::IsOutermostSVGSVGElement() const {
  if (!IsA<SVGSVGElement>(*this))
    return false;

  // Element may not be in the document, pretend we're outermost for viewport(),
  // getCTM(), etc.
  if (!parentNode())
    return true;

  // We act like an outermost SVG element, if we're a direct child of a
  // <foreignObject> element.
  if (IsA<SVGForeignObjectElement>(*parentNode()))
    return true;

  // If we're living in a shadow tree, we're a <svg> element that got created as
  // replacement for a <symbol> element or a cloned <svg> element in the
  // referenced tree. In that case we're always an inner <svg> element.
  if (InUseShadowTree() && ParentOrShadowHostElement() &&
      ParentOrShadowHostElement()->IsSVGElement())
    return false;

  // This is true whenever this is the outermost SVG, even if there are HTML
  // elements outside it
  return !parentNode()->IsSVGElement();
}

void SVGElement::ReportAttributeParsingError(SVGParsingError error,
                                             const QualifiedName& name,
                                             const AtomicString& value) {
  if (error == SVGParseStatus::kNoError)
    return;
  // Don't report any errors on attribute removal.
  if (value.IsNull())
    return;
  GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kRendering,
      mojom::ConsoleMessageLevel::kError,
      StrCat({"Error: ", error.Format(tagName(), name, value)})));
}

String SVGElement::title() const {
  // According to spec, we should not return titles when hovering over root
  // <svg> elements imported as a standalone document(those <title> elements
  // are the title of the document, not a tooltip) so we instantly return.
  if (IsA<SVGSVGElement>(*this) && this == GetDocument().documentElement())
    return String();

  if (InUseShadowTree()) {
    String use_title(OwnerShadowHost()->title());
    if (!use_title.empty())
      return use_title;
  }

  // If we aren't an instance in a <use> or the <use> title was not found, then
  // find the first <title> child of this element.
  // If a title child was found, return the text contents.
  if (Element* title_element = Traversal<SVGTitleElement>::FirstChild(*this))
    return title_element->innerText();

  // Otherwise return a null/empty string.
  return String();
}

template <typename T>
static void ForSelfAndInstances(SVGElement* element, T callback) {
  callback(element);
  for (SVGElement* instance : element->InstancesForElement())
    callback(instance);
}

ElementSMILAnimations* SVGElement::GetSMILAnimations() const {
  if (!HasSVGRareData())
    return nullptr;
  return SvgRareData()->GetSMILAnimations();
}

ElementSMILAnimations& SVGElement::EnsureSMILAnimations() {
  return EnsureSVGRareData()->EnsureSMILAnimations();
}

void SVGElement::SetAnimatedAttribute(const QualifiedName& attribute,
                                      SVGPropertyBase* value) {
  // When animating the 'class' attribute we need to have our own
  // unique element data since we'll be altering the active class
  // names for the element.
  if (attribute == html_names::kClassAttr)
    EnsureUniqueElementData();

  ForSelfAndInstances(this, [&attribute, &value](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute)) {
      animated_property->SetAnimatedValue(value);
      element->SvgAttributeChanged({*animated_property, attribute,
                                    AttributeModificationReason::kDirectly});
    }
  });
}

void SVGElement::ClearAnimatedAttribute(const QualifiedName& attribute) {
  ForSelfAndInstances(this, [&attribute](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute)) {
      animated_property->SetAnimatedValue(nullptr);
      element->SvgAttributeChanged({*animated_property, attribute,
                                    AttributeModificationReason::kDirectly});
    }
  });
}

void SVGElement::SetAnimatedMotionTransform(
    const AffineTransform& motion_transform) {
  ForSelfAndInstances(this, [&motion_transform](SVGElement* element) {
    AffineTransform* transform = element->AnimateMotionTransform();
    DCHECK(transform);
    *transform = motion_transform;
    if (LayoutObject* layout_object = element->GetLayoutObject()) {
      layout_object->SetNeedsTransformUpdate();
      // The transform paint property relies on the SVG transform value.
      layout_object->SetNeedsPaintPropertyUpdate();
      MarkForLayoutAndParentResourceInvalidation(*layout_object);
    }
  });
}

void SVGElement::ClearAnimatedMotionTransform() {
  SetAnimatedMotionTransform(AffineTransform());
}

bool SVGElement::HasSMILAnimations() const {
  return GetSMILAnimations() && GetSMILAnimations()->HasAnimations();
}

AffineTransform SVGElement::LocalCoordinateSpaceTransform(CTMScope) const {
  // To be overridden by SVGTransformableElement (or as special case
  // SVGTextElement and SVGPatternElement)
  return AffineTransform();
}

bool SVGElement::HasTransform(
    ApplyMotionTransformTag apply_motion_transform) const {
  return (GetLayoutObject() && GetLayoutObject()->HasTransform()) ||
         (apply_motion_transform == kIncludeMotionTransform &&
          HasMotionTransform());
}

AffineTransform SVGElement::CalculateTransform(
    ApplyMotionTransformTag apply_motion_transform) const {
  const LayoutObject* layout_object = GetLayoutObject();

  AffineTransform matrix;
  if (layout_object && layout_object->HasTransform()) {
    const gfx::RectF reference_box =
        TransformHelper::ComputeReferenceBox(*layout_object);
    matrix = TransformHelper::ComputeTransform(
        GetDocument(), layout_object->StyleRef(), reference_box,
        ComputedStyle::kIncludeTransformOrigin);
  }

  // Apply any "motion transform" contribution if requested (and existing.)
  if (apply_motion_transform == kIncludeMotionTransform) {
    ApplyMotionTransform(matrix);
  }
  return matrix;
}

void SVGElement::ApplyMotionTransform(AffineTransform& matrix) const {
  if (HasSVGRareData()) {
    matrix.PostConcat(*SvgRareData()->AnimateMotionTransform());
  }
}

Node::InsertionNotificationRequest SVGElement::InsertedInto(
    ContainerNode& root_parent) {
  Element::InsertedInto(root_parent);
  HideNonce();
  return kInsertionDone;
}

void SVGElement::RemovedFrom(ContainerNode& root_parent) {
  Element::RemovedFrom(root_parent);

  if (root_parent.isConnected()) {
    if (SVGElement* corresponding_element =
            HasSVGRareData() ? SvgRareData()->CorrespondingElement()
                             : nullptr) {
      corresponding_element->RemoveInstance(this);
      SvgRareData()->SetCorrespondingElement(nullptr);
    }
    RemoveAllIncomingReferences();
  }

  InvalidateInstances();
}

void SVGElement::ChildrenChanged(const ChildrenChange& change) {
  Element::ChildrenChanged(change);

  // Invalidate all instances associated with us.
  InvalidateInstances();
}

CSSPropertyID SVGElement::CssPropertyIdForSVGAttributeName(
    const ExecutionContext* execution_context,
    const QualifiedName& attr_name) {
  if (!attr_name.NamespaceURI().IsNull())
    return CSSPropertyID::kInvalid;

  static HashMap<StringImpl*, CSSPropertyID>* property_name_to_id_map = nullptr;
  if (!property_name_to_id_map) {
    property_name_to_id_map = new HashMap<StringImpl*, CSSPropertyID>;
    // This is a list of all base CSS and SVG CSS properties which are exposed
    // as SVG XML attributes
    const auto attr_names = std::to_array<const QualifiedName*>({
        &svg_names::kAlignmentBaselineAttr,
        &svg_names::kBaselineShiftAttr,
        &svg_names::kBufferedRenderingAttr,
        &svg_names::kClipAttr,
        &svg_names::kClipPathAttr,
        &svg_names::kClipRuleAttr,
        &svg_names::kColorAttr,
        &svg_names::kColorInterpolationAttr,
        &svg_names::kColorInterpolationFiltersAttr,
        &svg_names::kColorRenderingAttr,
        &svg_names::kCursorAttr,
        &svg_names::kDirectionAttr,
        &svg_names::kDisplayAttr,
        &svg_names::kDominantBaselineAttr,
        &svg_names::kFillAttr,
        &svg_names::kFillOpacityAttr,
        &svg_names::kFillRuleAttr,
        &svg_names::kFilterAttr,
        &svg_names::kFloodColorAttr,
        &svg_names::kFloodOpacityAttr,
        &svg_names::kFontFamilyAttr,
        &svg_names::kFontSizeAttr,
        &svg_names::kFontStretchAttr,
        &svg_names::kFontStyleAttr,
        &svg_names::kFontVariantAttr,
        &svg_names::kFontWeightAttr,
        &svg_names::kImageRenderingAttr,
        &svg_names::kLetterSpacingAttr,
        &svg_names::kLightingColorAttr,
        &svg_names::kMarkerEndAttr,
        &svg_names::kMarkerMidAttr,
        &svg_names::kMarkerStartAttr,
        &svg_names::kMaskAttr,
        &svg_names::kMaskTypeAttr,
        &svg_names::kOpacityAttr,
        &svg_names::kOverflowAttr,
        &svg_names::kPaintOrderAttr,
        &svg_names::kPointerEventsAttr,
        &svg_names::kShapeRenderingAttr,
        &svg_names::kStopColorAttr,
        &svg_names::kStopOpacityAttr,
        &svg_names::kStrokeAttr,
        &svg_names::kStrokeDasharrayAttr,
        &svg_names::kStrokeDashoffsetAttr,
        &svg_names::kStrokeLinecapAttr,
        &svg_names::kStrokeLinejoinAttr,
        &svg_names::kStrokeMiterlimitAttr,
        &svg_names::kStrokeOpacityAttr,
        &svg_names::kStrokeWidthAttr,
        &svg_names::kTextAnchorAttr,
        &svg_names::kTextDecorationAttr,
        &svg_names::kTextRenderingAttr,
        &svg_names::kTransformOriginAttr,
        &svg_names::kUnicodeBidiAttr,
        &svg_names::kVectorEffectAttr,
        &svg_names::kVisibilityAttr,
        &svg_names::kWordSpacingAttr,
        &svg_names::kWritingModeAttr,
    });
    for (const auto* qname : attr_names) {
      CSSPropertyID property_id =
          CssPropertyID(execution_context, qname->LocalName());
      DCHECK_GT(property_id, CSSPropertyID::kInvalid);
      property_name_to_id_map->Set(qname->LocalName().Impl(), property_id);
    }
  }

  auto it = property_name_to_id_map->find(attr_name.LocalName().Impl());
  if (it == property_name_to_id_map->end())
    return CSSPropertyID::kInvalid;
  return it->value;
}

SVGSVGElement* SVGElement::ownerSVGElement() const {
  if (IsOutermostSVGSVGElement()) {
    return nullptr;
  }
  ContainerNode* n = ParentOrShadowHostNode();
  while (n) {
    if (auto* svg_svg_element = DynamicTo<SVGSVGElement>(n))
      return svg_svg_element;

    n = n->ParentOrShadowHostNode();
  }

  return nullptr;
}

SVGElement* SVGElement::viewportElement() const {
  // This function needs shadow tree support - as LayoutSVGContainer uses this
  // function to determine the "overflow" property. <use> on <symbol> wouldn't
  // work otherwhise.
  ContainerNode* n = ParentOrShadowHostNode();
  while (n) {
    if (IsA<SVGSVGElement>(*n) || IsA<SVGImageElement>(*n) ||
        IsA<SVGSymbolElement>(*n))
      return To<SVGElement>(n);

    n = n->ParentOrShadowHostNode();
  }

  return nullptr;
}

void SVGElement::AddInstance(SVGElement* instance) {
  DCHECK(instance);
  DCHECK(instance->InUseShadowTree());

  HeapHashSet<WeakMember<SVGElement>>& instances =
      EnsureSVGRareData()->ElementInstances();
  DCHECK(!instances.Contains(instance));

  instances.insert(instance);
}

void SVGElement::RemoveInstance(SVGElement* instance) {
  DCHECK(instance);
  // Called during instance->RemovedFrom() after removal from shadow tree
  DCHECK(!instance->isConnected());

  HeapHashSet<WeakMember<SVGElement>>& instances =
      SvgRareData()->ElementInstances();

  instances.erase(instance);
}

static HeapHashSet<WeakMember<SVGElement>>& EmptyInstances() {
  using EmptyInstanceHolder =
      DisallowNewWrapper<HeapHashSet<WeakMember<SVGElement>>>;
  DEFINE_STATIC_LOCAL(Persistent<EmptyInstanceHolder>, empty_instances,
                      (MakeGarbageCollected<EmptyInstanceHolder>()));
  return empty_instances->Value();
}

const HeapHashSet<WeakMember<SVGElement>>& SVGElement::InstancesForElement()
    const {
  if (!HasSVGRareData())
    return EmptyInstances();
  return SvgRareData()->ElementInstances();
}

SVGElement* SVGElement::CorrespondingElement() const {
  DCHECK(!HasSVGRareData() || !SvgRareData()->CorrespondingElement() ||
         ContainingShadowRoot());
  return HasSVGRareData() ? SvgRareData()->CorrespondingElement() : nullptr;
}

SVGUseElement* SVGElement::GeneratingUseElement() const {
  if (ShadowRoot* root = ContainingShadowRoot()) {
    return DynamicTo<SVGUseElement>(root->host());
  }
  return nullptr;
}

SVGResourceTarget& SVGElement::EnsureResourceTarget() {
  return EnsureSVGRareData()->EnsureResourceTarget(*this);
}

bool SVGElement::IsResourceTarget() const {
  return HasSVGRareData() && SvgRareData()->HasResourceTarget();
}

void SVGElement::SetCorrespondingElement(SVGElement* corresponding_element) {
  EnsureSVGRareData()->SetCorrespondingElement(corresponding_element);
}

bool SVGElement::InUseShadowTree() const {
  return GeneratingUseElement();
}

void SVGElement::ParseAttribute(const AttributeModificationParams& params) {
  // SVGElement and HTMLElement are handling "nonce" the same way.
  if (params.name == html_names::kNonceAttr) {
    if (params.new_value != g_empty_atom)
      setNonce(params.new_value);
  } else if (params.name == svg_names::kLangAttr) {
    LangAttributeChanged();
  }

  const AtomicString& event_name =
      HTMLElement::EventNameForAttributeName(params.name);
  if (!event_name.IsNull()) {
    SetAttributeEventListener(
        event_name, JSEventHandlerForContentAttribute::Create(
                        GetExecutionContext(), params.name, params.new_value));
    return;
  }

  Element::ParseAttribute(params);
}

SVGAnimatedPropertyBase* SVGElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == html_names::kClassAttr) {
    return class_name_.Get();
  } else {
    return nullptr;
  }
}

bool SVGElement::IsPresentationAttribute(const QualifiedName& name) const {
  return name.Matches(xml_names::kLangAttr) || name == svg_names::kLangAttr;
}

namespace {

bool ProbablyUrlFunction(const AtomicString& value) {
  return value.length() > 5 && value.Is8Bit() &&
         base::as_string_view(value.Span8()).starts_with("url(");
}

bool UseCSSURIValueCacheForProperty(CSSPropertyID property_id) {
  return property_id == CSSPropertyID::kFill ||
         property_id == CSSPropertyID::kClipPath;
}

}  // namespace

void SVGElement::AddPropertyToPresentationAttributeStyleWithCache(
    HeapVector<CSSPropertyValue, 8>& style,
    CSSPropertyID property_id,
    const AtomicString& value) {
  if (UseCSSURIValueCacheForProperty(property_id) &&
      ProbablyUrlFunction(value)) {
    // Cache CSSURIValue objects for a given attribute value string. If other
    // presentation attributes change repeatedly while the fill or clip-path
    // stay the same, we still recreate the presentation attribute style for
    // the mentioned attributes/properties. Cache them to avoid expensive url
    // parsing and resolution.
    StyleEngine& engine = GetDocument().GetStyleEngine();
    if (const CSSValue* cached_value =
            engine.GetCachedFillOrClipPathURIValue(value)) {
      AddPropertyToPresentationAttributeStyle(style, property_id,
                                              *cached_value);
    } else {
      AddPropertyToPresentationAttributeStyle(style, property_id, value);
      if (unsigned count = style.size()) {
        // Cache the value if it was added.
        const CSSPropertyValue& last_decl = style[--count];
        if (last_decl.PropertyID() == property_id) {
          engine.AddCachedFillOrClipPathURIValue(value, last_decl.Value());
        }
      }
    }
  } else {
    AddPropertyToPresentationAttributeStyle(style, property_id, value);
  }
}

void SVGElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    HeapVector<CSSPropertyValue, 8>& style) {
  CSSPropertyID property_id =
      CssPropertyIdForSVGAttributeName(GetExecutionContext(), name);
  if (property_id > CSSPropertyID::kInvalid) {
    AddPropertyToPresentationAttributeStyleWithCache(style, property_id, value);
    return;
  }
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property && property->HasPresentationAttributeMapping()) {
    if (const CSSValue* css_value = property->CssValue()) {
      AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                              *css_value);
    }
    return;
  }
  if (name.Matches(xml_names::kLangAttr)) {
    MapLanguageAttributeToLocale(value, style);
  } else if (name == svg_names::kLangAttr) {
    if (!FastHasAttribute(xml_names::kLangAttr)) {
      MapLanguageAttributeToLocale(value, style);
    }
  }
}

bool SVGElement::HaveLoadedRequiredResources() {
  for (SVGElement* child = Traversal<SVGElement>::FirstChild(*this); child;
       child = Traversal<SVGElement>::NextSibling(*child)) {
    if (!child->HaveLoadedRequiredResources())
      return false;
  }
  return true;
}

static inline void CollectInstancesForSVGElement(
    SVGElement* element,
    HeapHashSet<WeakMember<SVGElement>>& instances) {
  DCHECK(element);
  if (element->ContainingShadowRoot())
    return;

  instances = element->InstancesForElement();
}

void SVGElement::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  // Add event listener to regular DOM element
  Node::AddedEventListener(event_type, registered_listener);

  // Add event listener to all shadow tree DOM element instances
  HeapHashSet<WeakMember<SVGElement>> instances;
  CollectInstancesForSVGElement(this, instances);
  AddEventListenerOptionsResolved* options = registered_listener.Options();
  EventListener* listener = registered_listener.Callback();
  for (SVGElement* element : instances) {
    bool result =
        element->Node::AddEventListenerInternal(event_type, listener, options);
    DCHECK(result);
  }
}

void SVGElement::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  Node::RemovedEventListener(event_type, registered_listener);

  // Remove event listener from all shadow tree DOM element instances
  HeapHashSet<WeakMember<SVGElement>> instances;
  CollectInstancesForSVGElement(this, instances);
  EventListenerOptions* options = registered_listener.Options();
  const EventListener* listener = registered_listener.Callback();
  for (SVGElement* shadow_tree_element : instances) {
    DCHECK(shadow_tree_element);

    shadow_tree_element->Node::RemoveEventListenerInternal(event_type, listener,
                                                           options);
  }
}

static bool HasLoadListener(Element* element) {
  if (element->HasEventListeners(event_type_names::kLoad))
    return true;

  for (element = element->ParentOrShadowHostElement(); element;
       element = element->ParentOrShadowHostElement()) {
    EventListenerVector* entry =
        element->GetEventListeners(event_type_names::kLoad);
    if (!entry)
      continue;
    for (auto& registered_event_listener : *entry) {
      if (registered_event_listener->Capture()) {
        return true;
      }
    }
  }

  return false;
}

bool SVGElement::SendSVGLoadEventIfPossible() {
  if (!HaveLoadedRequiredResources())
    return false;
  if ((IsStructurallyExternal() || IsA<SVGSVGElement>(*this)) &&
      HasLoadListener(this))
    DispatchEvent(*Event::Create(event_type_names::kLoad));
  return true;
}

void SVGElement::SendSVGLoadEventToSelfAndAncestorChainIfPossible() {
  // Let Document::implicitClose() dispatch the 'load' to the outermost SVG
  // root.
  if (IsOutermostSVGSVGElement())
    return;

  // Save the next parent to dispatch to in case dispatching the event mutates
  // the tree.
  Element* parent = ParentOrShadowHostElement();
  if (!SendSVGLoadEventIfPossible())
    return;

  // If document/window 'load' has been sent already, then only deliver to
  // the element in question.
  if (GetDocument().LoadEventFinished())
    return;

  auto* svg_element = DynamicTo<SVGElement>(parent);
  if (!svg_element)
    return;

  svg_element->SendSVGLoadEventToSelfAndAncestorChainIfPossible();
}

void SVGElement::AttributeChanged(const AttributeModificationParams& params) {
  // Note about the 'class' attribute:
  // The "special storage" (SVGAnimatedString) for the 'class' attribute (and
  // the 'className' property) is updated by the following block (`class_name_`
  // returned by PropertyFromAttribute().). SvgAttributeChanged then triggers
  // the resulting style updates (as well as Element::AttributeChanged()).
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(params.name);
  if (property) {
    SVGParsingError parse_error = property->AttributeChanged(params.new_value);
    ReportAttributeParsingError(parse_error, params.name, params.new_value);
  }

  Element::AttributeChanged(params);

  if (property) {
    SvgAttributeChanged({*property, params.name, params.reason});
    SynchronizeAttributeInShadowInstances(params.name, params.new_value);
    return;
  }

  if (!RuntimeEnabledFeatures::Svg2CascadeEnabled()) {
    if (params.name == html_names::kIdAttr) {
      // TODO(crbug.com/40550039): Id attributes also need to be synchronized
      // instead of rebuilding the tree as animations/transitions would
      // otherwise not work correctly.
      InvalidateInstances();
      return;
    }

    // Changes to the style attribute are processed lazily (see
    // Element::getAttribute() and related methods), so we don't want changes to
    // the style attribute to result in extra work here.
    if (params.name == html_names::kStyleAttr) {
      return;
    }
  }

  CSSPropertyID prop_id =
      CssPropertyIdForSVGAttributeName(GetExecutionContext(), params.name);
  if (prop_id > CSSPropertyID::kInvalid) {
    UpdatePresentationAttributeStyle(prop_id, params.name, params.new_value);
  }
  if (prop_id > CSSPropertyID::kInvalid ||
      RuntimeEnabledFeatures::Svg2CascadeEnabled()) {
    SynchronizeAttributeInShadowInstances(params.name, params.new_value);
  }
}

void SVGElement::SvgAttributeChanged(const SvgAttributeChangedParams& params) {
  if (class_name_ == &params.property) {
    ClassAttributeChanged(AtomicString(class_name_->CurrentValue()->Value()));
    return;
  }
}

void SVGElement::BaseValueChanged(const SVGAnimatedPropertyBase& property) {
  EnsureUniqueElementData().SetSvgAttributesAreDirty(true);
  SvgAttributeChanged({property, property.AttributeName(),
                       AttributeModificationReason::kDirectly});
  if (class_name_ == &property) {
    UpdateClassList(g_null_atom,
                    AtomicString(class_name_->BaseValue()->Value()));
  }
  InvalidateInstances();
}

void SVGElement::SynchronizeSVGAttribute(const QualifiedName& name) const {
  DCHECK(HasElementData());
  DCHECK(GetElementData()->svg_attributes_are_dirty());
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property && property->NeedsSynchronizeAttribute()) {
    property->SynchronizeAttribute();
  }
}

void SVGElement::SynchronizeAllSVGAttributes() const {
  DCHECK(HasElementData());
  DCHECK(GetElementData()->svg_attributes_are_dirty());
  if (class_name_->NeedsSynchronizeAttribute()) {
    class_name_->SynchronizeAttribute();
  }
  GetElementData()->SetSvgAttributesAreDirty(false);
}

MutableCSSPropertyValueSet*
SVGElement::GetPresentationAttributeStyleForDirectUpdate() {
  // If the element is not attached to the layout tree, then just mark dirty.
  if (!GetLayoutObject()) {
    return nullptr;
  }
  auto* element_data = GetElementData();
  if (!element_data) {
    return nullptr;
  }
  // If _something_ has already marked our presentation attribute style as
  // dirty, just roll with that and let the normal update via
  // CollectStyleForPresentationAttribute() handle it.
  if (element_data->presentation_attribute_style_is_dirty()) {
    return nullptr;
  }
  // Ditto if no property value set has been created yet.
  if (!element_data->PresentationAttributeStyle()) {
    return nullptr;
  }
  if (!element_data->presentation_attribute_style_->IsMutable()) {
    element_data = &EnsureUniqueElementData();
    element_data->presentation_attribute_style_ =
        element_data->presentation_attribute_style_->MutableCopy();
  }
  return To<MutableCSSPropertyValueSet>(
      element_data->presentation_attribute_style_.Get());
}

void SVGElement::UpdatePresentationAttributeStyle(
    const SVGAnimatedPropertyBase& property) {
  DCHECK(property.HasPresentationAttributeMapping());
  if (auto* mutable_style = GetPresentationAttributeStyleForDirectUpdate()) {
    const CSSPropertyID property_id = property.CssPropertyId();
    if (property.IsSpecified()) {
      if (const CSSValue* value = property.CssValue()) {
        mutable_style->SetProperty(property_id, *value);
      } else {
        mutable_style->RemoveProperty(property_id);
      }
    } else {
      mutable_style->RemoveProperty(property_id);
    }
  } else {
    InvalidateSVGPresentationAttributeStyle();
  }
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::FromAttribute(property.AttributeName()));
}

void SVGElement::UpdatePresentationAttributeStyle(
    CSSPropertyID property_id,
    const QualifiedName& attr_name,
    const AtomicString& value) {
  auto set_result = MutableCSSPropertyValueSet::kModifiedExisting;
  if (auto* mutable_style = GetPresentationAttributeStyleForDirectUpdate()) {
    auto* execution_context = GetExecutionContext();
    set_result = mutable_style->ParseAndSetProperty(
        property_id, value, false,
        execution_context ? execution_context->GetSecureContextMode()
                          : SecureContextMode::kInsecureContext,
        GetDocument().ElementSheet().Contents());
    // We want "replace" semantics, so if parsing failed, then make sure any
    // existing value is removed.
    if (set_result == MutableCSSPropertyValueSet::kParseError) {
      if (mutable_style->RemoveProperty(property_id)) {
        set_result = MutableCSSPropertyValueSet::kChangedPropertySet;
      }
    }
  } else {
    InvalidateSVGPresentationAttributeStyle();
  }
  if (set_result >= MutableCSSPropertyValueSet::kModifiedExisting) {
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
  }
}

void SVGElement::AddAnimatedPropertyToPresentationAttributeStyle(
    const SVGAnimatedPropertyBase& property,
    HeapVector<CSSPropertyValue, 8>& style) {
  DCHECK(property.HasPresentationAttributeMapping());
  // Apply values from animating attributes that are also presentation
  // attributes, but do not have a corresponding content attribute.
  if (property.HasContentAttribute() || !property.IsAnimating()) {
    return;
  }
  const CSSValue* value = property.CssValue();
  if (!value) {
    return;
  }
  AddPropertyToPresentationAttributeStyle(style, property.CssPropertyId(),
                                          *value);
}

const ComputedStyle* SVGElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  SVGElement* corresponding_element = CorrespondingElement();
  if (!corresponding_element || RuntimeEnabledFeatures::Svg2CascadeEnabled()) {
    return GetDocument().GetStyleResolver().ResolveStyle(this,
                                                         style_recalc_context);
  }

  const ComputedStyle* style = nullptr;
  if (Element* parent = ParentOrShadowHostElement())
    style = parent->GetComputedStyle();

  StyleRequest style_request;
  style_request.parent_override = style;
  style_request.layout_parent_override = style;
  style_request.styled_element = this;
  StyleRecalcContext corresponding_recalc_context(style_recalc_context);
  corresponding_recalc_context.old_style =
      PostStyleUpdateScope::GetOldStyle(*this);
  return GetDocument().GetStyleResolver().ResolveStyle(
      corresponding_element, corresponding_recalc_context, style_request);
}

bool SVGElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  return IsValid() && HasSVGParent() && Element::LayoutObjectIsNeeded(style);
}

bool SVGElement::HasSVGParent() const {
  Element* parent = FlatTreeTraversal::ParentElement(*this);
  return parent && parent->IsSVGElement();
}

MutableCSSPropertyValueSet* SVGElement::AnimatedSMILStyleProperties() const {
  if (HasSVGRareData())
    return SvgRareData()->AnimatedSMILStyleProperties();
  return nullptr;
}

MutableCSSPropertyValueSet* SVGElement::EnsureAnimatedSMILStyleProperties() {
  return EnsureSVGRareData()->EnsureAnimatedSMILStyleProperties();
}

const ComputedStyle* SVGElement::BaseComputedStyleForSMIL() {
  if (!HasSVGRareData())
    return EnsureComputedStyle();
  const ComputedStyle* parent_style = nullptr;
  if (Element* parent = LayoutTreeBuilderTraversal::ParentElement(*this)) {
    parent_style = parent->EnsureComputedStyle();
  }
  return SvgRareData()->OverrideComputedStyle(this, parent_style);
}

bool SVGElement::HasFocusEventListeners() const {
  return HasEventListeners(event_type_names::kFocusin) ||
         HasEventListeners(event_type_names::kFocusout) ||
         HasEventListeners(event_type_names::kFocus) ||
         HasEventListeners(event_type_names::kBlur);
}

void SVGElement::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject& layout_object) {
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, true);
}

void SVGElement::NotifyResourceClients() const {
  LocalSVGResource* resource =
      GetTreeScope().EnsureSVGTreeScopedResources().ExistingResourceForId(
          GetIdAttribute());
  if (!resource || resource->Target() != this) {
    return;
  }
  resource->NotifyContentChanged();
}

void SVGElement::InvalidateStyleAttribute(
    bool only_changed_independent_properties) {
  Element::InvalidateStyleAttribute(only_changed_independent_properties);
  if (RuntimeEnabledFeatures::Svg2CascadeEnabled() &&
      !InstancesForElement().empty()) {
    SynchronizeAttributeInShadowInstances(html_names::kStyleAttr,
                                          getAttribute(html_names::kStyleAttr));
  }
}

void SVGElement::InvalidateInstances() {
  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.empty())
    return;

  // Mark all use elements referencing 'element' for rebuilding
  for (SVGElement* instance : set) {
    instance->SetCorrespondingElement(nullptr);

    if (SVGUseElement* element = instance->GeneratingUseElement()) {
      DCHECK(element->isConnected());
      element->InvalidateShadowTree();
    }
  }

  SvgRareData()->ElementInstances().clear();
}

void SVGElement::SetNeedsStyleRecalcForInstances(
    StyleChangeType change_type,
    const StyleChangeReasonForTracing& reason) {
  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.empty())
    return;

  for (SVGElement* instance : set)
    instance->SetNeedsStyleRecalc(change_type, reason);
}

#if DCHECK_IS_ON()
bool SVGElement::IsAnimatableAttribute(const QualifiedName& name) const {
  // This static is atomically initialized to dodge a warning about
  // a race when dumping debug data for a layer.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(HashSet<QualifiedName>, animatable_attributes,
                                  ({
                                      svg_names::kAmplitudeAttr,
                                      svg_names::kAzimuthAttr,
                                      svg_names::kBaseFrequencyAttr,
                                      svg_names::kBiasAttr,
                                      svg_names::kClipPathUnitsAttr,
                                      svg_names::kCxAttr,
                                      svg_names::kCyAttr,
                                      svg_names::kDiffuseConstantAttr,
                                      svg_names::kDivisorAttr,
                                      svg_names::kDxAttr,
                                      svg_names::kDyAttr,
                                      svg_names::kEdgeModeAttr,
                                      svg_names::kElevationAttr,
                                      svg_names::kExponentAttr,
                                      svg_names::kFilterUnitsAttr,
                                      svg_names::kFxAttr,
                                      svg_names::kFyAttr,
                                      svg_names::kGradientTransformAttr,
                                      svg_names::kGradientUnitsAttr,
                                      svg_names::kHeightAttr,
                                      svg_names::kHrefAttr,
                                      svg_names::kIn2Attr,
                                      svg_names::kInAttr,
                                      svg_names::kInterceptAttr,
                                      svg_names::kK1Attr,
                                      svg_names::kK2Attr,
                                      svg_names::kK3Attr,
                                      svg_names::kK4Attr,
                                      svg_names::kKernelMatrixAttr,
                                      svg_names::kKernelUnitLengthAttr,
                                      svg_names::kLengthAdjustAttr,
                                      svg_names::kLimitingConeAngleAttr,
                                      svg_names::kMarkerHeightAttr,
                                      svg_names::kMarkerUnitsAttr,
                                      svg_names::kMarkerWidthAttr,
                                      svg_names::kMaskContentUnitsAttr,
                                      svg_names::kMaskUnitsAttr,
                                      svg_names::kMethodAttr,
                                      svg_names::kModeAttr,
                                      svg_names::kNumOctavesAttr,
                                      svg_names::kOffsetAttr,
                                      svg_names::kOperatorAttr,
                                      svg_names::kOrderAttr,
                                      svg_names::kOrientAttr,
                                      svg_names::kPathLengthAttr,
                                      svg_names::kPatternContentUnitsAttr,
                                      svg_names::kPatternTransformAttr,
                                      svg_names::kPatternUnitsAttr,
                                      svg_names::kPointsAtXAttr,
                                      svg_names::kPointsAtYAttr,
                                      svg_names::kPointsAtZAttr,
                                      svg_names::kPreserveAlphaAttr,
                                      svg_names::kPreserveAspectRatioAttr,
                                      svg_names::kPrimitiveUnitsAttr,
                                      svg_names::kRadiusAttr,
                                      svg_names::kRAttr,
                                      svg_names::kRefXAttr,
                                      svg_names::kRefYAttr,
                                      svg_names::kResultAttr,
                                      svg_names::kRotateAttr,
                                      svg_names::kRxAttr,
                                      svg_names::kRyAttr,
                                      svg_names::kScaleAttr,
                                      svg_names::kSeedAttr,
                                      svg_names::kSlopeAttr,
                                      svg_names::kSpacingAttr,
                                      svg_names::kSpecularConstantAttr,
                                      svg_names::kSpecularExponentAttr,
                                      svg_names::kSpreadMethodAttr,
                                      svg_names::kStartOffsetAttr,
                                      svg_names::kStdDeviationAttr,
                                      svg_names::kStitchTilesAttr,
                                      svg_names::kSurfaceScaleAttr,
                                      svg_names::kTableValuesAttr,
                                      svg_names::kTargetAttr,
                                      svg_names::kTargetXAttr,
                                      svg_names::kTargetYAttr,
                                      svg_names::kTransformAttr,
                                      svg_names::kTypeAttr,
                                      svg_names::kValuesAttr,
                                      svg_names::kViewBoxAttr,
                                      svg_names::kWidthAttr,
                                      svg_names::kX1Attr,
                                      svg_names::kX2Attr,
                                      svg_names::kXAttr,
                                      svg_names::kXChannelSelectorAttr,
                                      svg_names::kY1Attr,
                                      svg_names::kY2Attr,
                                      svg_names::kYAttr,
                                      svg_names::kYChannelSelectorAttr,
                                      svg_names::kZAttr,
                                  }));

  if (name == html_names::kClassAttr)
    return true;

  return animatable_attributes.Contains(name);
}
#endif  // DCHECK_IS_ON()

SVGElementSet* SVGElement::SetOfIncomingReferences() const {
  if (!HasSVGRareData())
    return nullptr;
  return &SvgRareData()->IncomingReferences();
}

void SVGElement::AddReferenceTo(SVGElement* target_element) {
  DCHECK(target_element);

  EnsureSVGRareData()->OutgoingReferences().insert(target_element);
  target_element->EnsureSVGRareData()->IncomingReferences().insert(this);
}

SVGElementSet& SVGElement::GetDependencyTraversalVisitedSet() {
  // This strong reference is safe, as it is guaranteed that this set will be
  // emptied at the end of recursion in NotifyIncomingReferences.
  using SVGElementSetHolder = DisallowNewWrapper<SVGElementSet>;
  DEFINE_STATIC_LOCAL(Persistent<SVGElementSetHolder>,
                      invalidating_dependencies,
                      (MakeGarbageCollected<SVGElementSetHolder>()));
  return invalidating_dependencies->Value();
}

void SVGElement::RemoveAllIncomingReferences() {
  if (!HasSVGRareData())
    return;

  SVGElementSet& incoming_references = SvgRareData()->IncomingReferences();
  for (SVGElement* source_element : incoming_references) {
    DCHECK(source_element->HasSVGRareData());
    source_element->EnsureSVGRareData()->OutgoingReferences().erase(this);
  }
  incoming_references.clear();
}

void SVGElement::RemoveAllOutgoingReferences() {
  if (!HasSVGRareData())
    return;

  SVGElementSet& outgoing_references = SvgRareData()->OutgoingReferences();
  for (SVGElement* target_element : outgoing_references) {
    DCHECK(target_element->HasSVGRareData());
    target_element->EnsureSVGRareData()->IncomingReferences().erase(this);
  }
  outgoing_references.clear();
}

SVGElementResourceClient* SVGElement::GetSVGResourceClient() {
  if (!HasSVGRareData())
    return nullptr;
  return SvgRareData()->GetSVGResourceClient();
}

SVGElementResourceClient& SVGElement::EnsureSVGResourceClient() {
  return EnsureSVGRareData()->EnsureSVGResourceClient(this);
}

void SVGElement::Trace(Visitor* visitor) const {
  visitor->Trace(svg_rare_data_);
  visitor->Trace(class_name_);
  Element::Trace(visitor);
}

void SVGElement::AccessKeyAction(SimulatedClickCreationScope creation_scope) {
  DispatchSimulatedClick(nullptr, creation_scope);
}

void SVGElement::SynchronizeListOfSVGAttributes(
    const base::span<SVGAnimatedPropertyBase*> attributes) {
  for (SVGAnimatedPropertyBase* attr : attributes) {
    if (attr->NeedsSynchronizeAttribute()) {
      attr->SynchronizeAttribute();
    }
  }
}

void SVGElement::AddAnimatedPropertiesToPresentationAttributeStyle(
    const base::span<const SVGAnimatedPropertyBase*> properties,
    HeapVector<CSSPropertyValue, 8>& style) {
  for (const SVGAnimatedPropertyBase* property : properties) {
    AddAnimatedPropertyToPresentationAttributeStyle(*property, style);
  }
}

void SVGElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);

  if (!context.performing_reattach && GetLayoutObject() &&
      GetSMILAnimations()) {
    GetTimeContainer()->DidAttachLayoutObject();
  }
}

SMILTimeContainer* SVGElement::GetTimeContainer() const {
  if (auto* svg_root = DynamicTo<SVGSVGElement>(*this)) {
    return svg_root->TimeContainer();
  }

  return ownerSVGElement()->TimeContainer();
}

void SVGElement::SynchronizeAttributeInShadowInstances(
    const QualifiedName& name,
    const AtomicString& value) {
  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  for (SVGElement* instance : set) {
    instance->SetAttributeWithoutValidation(name, value);
  }
}

}  // namespace blink
