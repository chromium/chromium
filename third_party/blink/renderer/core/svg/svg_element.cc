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
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/effect_stack.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/svg_interpolation_environment.h"
#include "third_party/blink/renderer/core/animation/svg_interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_property_id_templates.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

using namespace HTMLNames;
using namespace svg_names;

SVGElement::SVGElement(const QualifiedName& tag_name,
                       Document& document,
                       ConstructionType construction_type)
    : Element(tag_name, &document, construction_type),
      svg_rare_data_(nullptr),
      class_name_(SVGAnimatedString::Create(this, HTMLNames::classAttr)) {
  AddToPropertyMap(class_name_);
  SetHasCustomStyleCallbacks();
}

SVGElement::~SVGElement() {
  DCHECK(isConnected() || !HasRelativeLengths());
}

void SVGElement::DetachLayoutTree(const AttachContext& context) {
  Element::DetachLayoutTree(context);
  if (SVGElement* element = CorrespondingElement())
    element->RemoveInstanceMapping(this);
  // To avoid a noncollectable Blink GC reference cycle, we must clear the
  // ComputedStyle here. See http://crbug.com/878032#c11
  if (HasSVGRareData())
    SvgRareData()->ClearOverriddenComputedStyle();
}

void SVGElement::AttachLayoutTree(AttachContext& context) {
  Element::AttachLayoutTree(context);
  if (SVGElement* element = CorrespondingElement())
    element->MapInstanceToElement(this);
}

TreeScope& SVGElement::TreeScopeForIdResolution() const {
  const SVGElement* tree_scope_element = this;
  if (const SVGElement* element = CorrespondingUseElement())
    tree_scope_element = element;
  return tree_scope_element->GetTreeScope();
}

int SVGElement::tabIndex() const {
  if (SupportsFocus())
    return Element::tabIndex();
  return -1;
}

void SVGElement::WillRecalcStyle(StyleRecalcChange change) {
  if (!HasSVGRareData())
    return;
  // If the style changes because of a regular property change (not induced by
  // SMIL animations themselves) reset the "computed style without SMIL style
  // properties", so the base value change gets reflected.
  if (change > kNoChange || NeedsStyleRecalc())
    SvgRareData()->SetNeedsOverrideComputedStyleUpdate();
}

SVGElementRareData* SVGElement::EnsureSVGRareData() {
  if (!svg_rare_data_)
    svg_rare_data_ = new SVGElementRareData();
  return svg_rare_data_.Get();
}

bool SVGElement::IsOutermostSVGSVGElement() const {
  if (!IsSVGSVGElement(*this))
    return false;

  // Element may not be in the document, pretend we're outermost for viewport(),
  // getCTM(), etc.
  if (!parentNode())
    return true;

  // We act like an outermost SVG element, if we're a direct child of a
  // <foreignObject> element.
  if (IsSVGForeignObjectElement(*parentNode()))
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
  GetDocument().AddConsoleMessage(
      ConsoleMessage::Create(kRenderingMessageSource, kErrorMessageLevel,
                             "Error: " + error.Format(tagName(), name, value)));
}

String SVGElement::title() const {
  // According to spec, we should not return titles when hovering over root
  // <svg> elements imported as a standalone document(those <title> elements
  // are the title of the document, not a tooltip) so we instantly return.
  if (IsSVGSVGElement(*this) && this == GetDocument().documentElement())
    return String();

  if (InUseShadowTree()) {
    String use_title(OwnerShadowHost()->title());
    if (!use_title.IsEmpty())
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

bool SVGElement::InstanceUpdatesBlocked() const {
  return HasSVGRareData() && SvgRareData()->InstanceUpdatesBlocked();
}

void SVGElement::SetInstanceUpdatesBlocked(bool value) {
  if (HasSVGRareData())
    SvgRareData()->SetInstanceUpdatesBlocked(value);
}

void SVGElement::SetWebAnimationsPending() {
  GetDocument().AccessSVGExtensions().AddWebAnimationsPendingSVGElement(*this);
  EnsureSVGRareData()->SetWebAnimatedAttributesDirty(true);
  EnsureUniqueElementData().animated_svg_attributes_are_dirty_ = true;
}

static bool IsSVGAttributeHandle(const PropertyHandle& property_handle) {
  return property_handle.IsSVGAttribute();
}

void SVGElement::ApplyActiveWebAnimations() {
  ActiveInterpolationsMap active_interpolations_map =
      EffectStack::ActiveInterpolations(
          &GetElementAnimations()->GetEffectStack(), nullptr, nullptr,
          KeyframeEffect::kDefaultPriority, IsSVGAttributeHandle);
  for (auto& entry : active_interpolations_map) {
    const QualifiedName& attribute = entry.key.SvgAttribute();
    SVGInterpolationTypesMap map;
    SVGInterpolationEnvironment environment(
        map, *this, PropertyFromAttribute(attribute)->BaseValueBase());
    InvalidatableInterpolation::ApplyStack(entry.value, environment);
  }
  if (!HasSVGRareData())
    return;
  SvgRareData()->SetWebAnimatedAttributesDirty(false);
}

static inline void NotifyAnimValChanged(SVGElement* target_element,
                                        const QualifiedName& attribute_name) {
  target_element->InvalidateSVGAttributes();
  target_element->SvgAttributeChanged(attribute_name);
}

template <typename T>
static void ForSelfAndInstances(SVGElement* element, T callback) {
  SVGElement::InstanceUpdateBlocker blocker(element);
  callback(element);
  for (SVGElement* instance : element->InstancesForElement())
    callback(instance);
}

void SVGElement::SetWebAnimatedAttribute(const QualifiedName& attribute,
                                         SVGPropertyBase* value) {
  ForSelfAndInstances(this, [&attribute, &value](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute)) {
      animated_property->SetAnimatedValue(value);
      NotifyAnimValChanged(element, attribute);
    }
  });
  EnsureSVGRareData()->WebAnimatedAttributes().insert(&attribute);
}

void SVGElement::ClearWebAnimatedAttributes() {
  if (!HasSVGRareData())
    return;
  for (const QualifiedName* attribute :
       SvgRareData()->WebAnimatedAttributes()) {
    ForSelfAndInstances(this, [&attribute](SVGElement* element) {
      if (SVGAnimatedPropertyBase* animated_property =
              element->PropertyFromAttribute(*attribute)) {
        animated_property->AnimationEnded();
        NotifyAnimValChanged(element, *attribute);
      }
    });
  }
  SvgRareData()->WebAnimatedAttributes().clear();
}

void SVGElement::SetAnimatedAttribute(const QualifiedName& attribute,
                                      SVGPropertyBase* value) {
  ForSelfAndInstances(this, [&attribute, &value](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute))
      animated_property->SetAnimatedValue(value);
  });
}

void SVGElement::InvalidateAnimatedAttribute(const QualifiedName& attribute) {
  ForSelfAndInstances(this, [&attribute](SVGElement* element) {
    NotifyAnimValChanged(element, attribute);
  });
}

void SVGElement::ClearAnimatedAttribute(const QualifiedName& attribute) {
  ForSelfAndInstances(this, [&attribute](SVGElement* element) {
    if (SVGAnimatedPropertyBase* animated_property =
            element->PropertyFromAttribute(attribute))
      animated_property->AnimationEnded();
  });
}

AffineTransform SVGElement::LocalCoordinateSpaceTransform(CTMScope) const {
  // To be overriden by SVGGraphicsElement (or as special case SVGTextElement
  // and SVGPatternElement)
  return AffineTransform();
}

bool SVGElement::HasTransform(
    ApplyMotionTransform apply_motion_transform) const {
  return (GetLayoutObject() && GetLayoutObject()->StyleRef().HasTransform()) ||
         (apply_motion_transform == kIncludeMotionTransform &&
          HasSVGRareData());
}

static inline bool TransformUsesBoxSize(const ComputedStyle& style) {
  if ((style.TransformOriginX().IsPercent() ||
       style.TransformOriginY().IsPercent()) &&
      style.RequireTransformOrigin(ComputedStyle::kIncludeTransformOrigin,
                                   ComputedStyle::kExcludeMotionPath))
    return true;
  if (style.Transform().DependsOnBoxSize())
    return true;
  if (style.Translate() && style.Translate()->DependsOnBoxSize())
    return true;
  if (style.HasOffset())
    return true;
  return false;
}

static FloatRect ComputeTransformReferenceBox(const SVGElement& element) {
  const LayoutObject& layout_object = *element.GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();
  if (style.TransformBox() == ETransformBox::kFillBox)
    return layout_object.ObjectBoundingBox();
  DCHECK_EQ(style.TransformBox(), ETransformBox::kViewBox);
  SVGLengthContext length_context(&element);
  FloatSize viewport_size;
  length_context.DetermineViewport(viewport_size);
  return FloatRect(FloatPoint(), viewport_size);
}

AffineTransform SVGElement::CalculateTransform(
    ApplyMotionTransform apply_motion_transform) const {
  const ComputedStyle* style =
      GetLayoutObject() ? GetLayoutObject()->Style() : nullptr;

  // If CSS property was set, use that, otherwise fallback to attribute (if
  // set).
  AffineTransform matrix;
  if (style && style->HasTransform()) {
    FloatRect reference_box = ComputeTransformReferenceBox(*this);
    if (TransformUsesBoxSize(*style))
      UseCounter::Count(GetDocument(), WebFeature::kTransformUsesBoxSizeOnSVG);

    // CSS transforms operate with pre-scaled lengths. To make this work with
    // SVG (which applies the zoom factor globally, at the root level) we
    //
    //   * pre-scale the reference box (to bring it into the same space as the
    //     other CSS values)
    //   * invert the zoom factor (to effectively compute the CSS transform
    //     under a 1.0 zoom)
    //
    // Note: objectBoundingBox is an emptyRect for elements like pattern or
    // clipPath. See
    // https://svgwg.org/svg2-draft/coords.html#ObjectBoundingBoxUnits
    float zoom = style->EffectiveZoom();
    TransformationMatrix transform;
    if (zoom != 1)
      reference_box.Scale(zoom);
    style->ApplyTransform(
        transform, reference_box, ComputedStyle::kIncludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
    if (zoom != 1)
      transform.Zoom(1 / zoom);
    // Flatten any 3D transform.
    matrix = transform.ToAffineTransform();
  }

  // Apply any "motion transform" contribution if requested (and existing.)
  if (apply_motion_transform == kIncludeMotionTransform && HasSVGRareData())
    matrix.PreMultiply(*SvgRareData()->AnimateMotionTransform());

  return matrix;
}

Node::InsertionNotificationRequest SVGElement::InsertedInto(
    ContainerNode& root_parent) {
  Element::InsertedInto(root_parent);
  UpdateRelativeLengthsInformation();

  const AtomicString& nonce_value = FastGetAttribute(nonceAttr);
  if (!nonce_value.IsEmpty()) {
    setNonce(nonce_value);
    if (InActiveDocument() &&
        GetDocument().GetContentSecurityPolicy()->HasHeaderDeliveredPolicy()) {
      setAttribute(nonceAttr, g_empty_atom);
    }
  }
  return kInsertionDone;
}

void SVGElement::RemovedFrom(ContainerNode& root_parent) {
  bool was_in_document = root_parent.isConnected();

  if (was_in_document && HasRelativeLengths()) {
    // The root of the subtree being removed should take itself out from its
    // parent's relative length set. For the other nodes in the subtree we don't
    // need to do anything: they will get their own removedFrom() notification
    // and just clear their sets.
    if (root_parent.IsSVGElement() && !parentNode()) {
      DCHECK(ToSVGElement(root_parent)
                 .elements_with_relative_lengths_.Contains(this));
      ToSVGElement(root_parent).UpdateRelativeLengthsInformation(false, this);
    }

    elements_with_relative_lengths_.clear();
  }

  SECURITY_DCHECK(!root_parent.IsSVGElement() ||
                  !ToSVGElement(root_parent)
                       .elements_with_relative_lengths_.Contains(this));

  Element::RemovedFrom(root_parent);

  if (was_in_document) {
    RebuildAllIncomingReferences();
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
    const QualifiedName& attr_name) {
  if (!attr_name.NamespaceURI().IsNull())
    return CSSPropertyInvalid;

  static HashMap<StringImpl*, CSSPropertyID>* property_name_to_id_map = nullptr;
  if (!property_name_to_id_map) {
    property_name_to_id_map = new HashMap<StringImpl*, CSSPropertyID>;
    // This is a list of all base CSS and SVG CSS properties which are exposed
    // as SVG XML attributes
    const QualifiedName* const attr_names[] = {
        &kAlignmentBaselineAttr,
        &kBaselineShiftAttr,
        &kBufferedRenderingAttr,
        &kClipAttr,
        &kClipPathAttr,
        &kClipRuleAttr,
        &svg_names::kColorAttr,
        &kColorInterpolationAttr,
        &kColorInterpolationFiltersAttr,
        &kColorRenderingAttr,
        &kCursorAttr,
        &svg_names::kDirectionAttr,
        &kDisplayAttr,
        &kDominantBaselineAttr,
        &kFillAttr,
        &kFillOpacityAttr,
        &kFillRuleAttr,
        &kFilterAttr,
        &kFloodColorAttr,
        &kFloodOpacityAttr,
        &kFontFamilyAttr,
        &kFontSizeAttr,
        &kFontStretchAttr,
        &kFontStyleAttr,
        &kFontVariantAttr,
        &kFontWeightAttr,
        &kImageRenderingAttr,
        &kLetterSpacingAttr,
        &kLightingColorAttr,
        &kMarkerEndAttr,
        &kMarkerMidAttr,
        &kMarkerStartAttr,
        &kMaskAttr,
        &kMaskTypeAttr,
        &kOpacityAttr,
        &kOverflowAttr,
        &kPaintOrderAttr,
        &kPointerEventsAttr,
        &kShapeRenderingAttr,
        &kStopColorAttr,
        &kStopOpacityAttr,
        &kStrokeAttr,
        &kStrokeDasharrayAttr,
        &kStrokeDashoffsetAttr,
        &kStrokeLinecapAttr,
        &kStrokeLinejoinAttr,
        &kStrokeMiterlimitAttr,
        &kStrokeOpacityAttr,
        &kStrokeWidthAttr,
        &kTextAnchorAttr,
        &kTextDecorationAttr,
        &kTextRenderingAttr,
        &kTransformOriginAttr,
        &kUnicodeBidiAttr,
        &kVectorEffectAttr,
        &kVisibilityAttr,
        &kWordSpacingAttr,
        &kWritingModeAttr,
    };
    for (size_t i = 0; i < arraysize(attr_names); i++) {
      CSSPropertyID property_id = cssPropertyID(attr_names[i]->LocalName());
      DCHECK_GT(property_id, 0);
      property_name_to_id_map->Set(attr_names[i]->LocalName().Impl(),
                                   property_id);
    }
  }

  return property_name_to_id_map->at(attr_name.LocalName().Impl());
}

void SVGElement::UpdateRelativeLengthsInformation(
    bool client_has_relative_lengths,
    SVGElement* client_element) {
  DCHECK(client_element);

  // Through an unfortunate chain of events, we can end up calling this while a
  // subtree is being removed, and before the subtree has been properly
  // "disconnected". Hence check the entire ancestor chain to avoid propagating
  // relative length clients up into ancestors that have already been
  // disconnected.
  // If we're not yet in a document, this function will be called again from
  // insertedInto(). Do nothing now.
  for (Node& current_node : NodeTraversal::InclusiveAncestorsOf(*this)) {
    if (!current_node.isConnected())
      return;
  }

  // An element wants to notify us that its own relative lengths state changed.
  // Register it in the relative length map, and register us in the parent
  // relative length map.  Register the parent in the grandparents map, etc.
  // Repeat procedure until the root of the SVG tree.
  for (Node& current_node : NodeTraversal::InclusiveAncestorsOf(*this)) {
    if (!current_node.IsSVGElement())
      break;
    SVGElement& current_element = ToSVGElement(current_node);
#if DCHECK_IS_ON()
    DCHECK(!current_element.in_relative_length_clients_invalidation_);
#endif

    bool had_relative_lengths = current_element.HasRelativeLengths();
    if (client_has_relative_lengths)
      current_element.elements_with_relative_lengths_.insert(client_element);
    else
      current_element.elements_with_relative_lengths_.erase(client_element);

    // If the relative length state hasn't changed, we can stop propagating the
    // notification.
    if (had_relative_lengths == current_element.HasRelativeLengths())
      return;

    client_element = &current_element;
    client_has_relative_lengths = client_element->HasRelativeLengths();
  }

  // Register root SVG elements for top level viewport change notifications.
  if (auto* svg = ToSVGSVGElementOrNull(*client_element)) {
    SVGDocumentExtensions& svg_extensions = GetDocument().AccessSVGExtensions();
    if (client_element->HasRelativeLengths())
      svg_extensions.AddSVGRootWithRelativeLengthDescendents(svg);
    else
      svg_extensions.RemoveSVGRootWithRelativeLengthDescendents(svg);
  }
}

void SVGElement::InvalidateRelativeLengthClients(
    SubtreeLayoutScope* layout_scope) {
  if (!isConnected())
    return;

#if DCHECK_IS_ON()
  DCHECK(!in_relative_length_clients_invalidation_);
  base::AutoReset<bool> in_relative_length_clients_invalidation_change(
      &in_relative_length_clients_invalidation_, true);
#endif

  if (LayoutObject* layout_object = this->GetLayoutObject()) {
    if (HasRelativeLengths() && layout_object->IsSVGResourceContainer()) {
      ToLayoutSVGResourceContainer(layout_object)
          ->InvalidateCacheAndMarkForLayout(
              LayoutInvalidationReason::kSizeChanged, layout_scope);
    } else if (SelfHasRelativeLengths()) {
      layout_object->SetNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::kUnknown, kMarkContainerChain,
          layout_scope);
    }
  }

  for (SVGElement* element : elements_with_relative_lengths_) {
    if (element != this)
      element->InvalidateRelativeLengthClients(layout_scope);
  }
}

SVGSVGElement* SVGElement::ownerSVGElement() const {
  ContainerNode* n = ParentOrShadowHostNode();
  while (n) {
    if (IsSVGSVGElement(*n))
      return ToSVGSVGElement(n);

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
    if (IsSVGSVGElement(*n) || IsSVGImageElement(*n) || IsSVGSymbolElement(*n))
      return ToSVGElement(n);

    n = n->ParentOrShadowHostNode();
  }

  return nullptr;
}

void SVGElement::MapInstanceToElement(SVGElement* instance) {
  DCHECK(instance);
  DCHECK(instance->InUseShadowTree());

  HeapHashSet<WeakMember<SVGElement>>& instances =
      EnsureSVGRareData()->ElementInstances();
  DCHECK(!instances.Contains(instance));

  instances.insert(instance);
}

void SVGElement::RemoveInstanceMapping(SVGElement* instance) {
  DCHECK(instance);
  DCHECK(instance->InUseShadowTree());

  if (!HasSVGRareData())
    return;

  HeapHashSet<WeakMember<SVGElement>>& instances =
      SvgRareData()->ElementInstances();

  instances.erase(instance);
}

static HeapHashSet<WeakMember<SVGElement>>& EmptyInstances() {
  DEFINE_STATIC_LOCAL(Persistent<HeapHashSet<WeakMember<SVGElement>>>,
                      empty_instances,
                      (new HeapHashSet<WeakMember<SVGElement>>));
  return *empty_instances;
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

SVGUseElement* SVGElement::CorrespondingUseElement() const {
  if (ShadowRoot* root = ContainingShadowRoot()) {
    if (IsSVGUseElement(root->host()))
      return &ToSVGUseElement(root->host());
  }
  return nullptr;
}

void SVGElement::SetCorrespondingElement(SVGElement* corresponding_element) {
  EnsureSVGRareData()->SetCorrespondingElement(corresponding_element);
}

bool SVGElement::InUseShadowTree() const {
  return CorrespondingUseElement();
}

void SVGElement::ParseAttribute(const AttributeModificationParams& params) {
  // Note about the 'class' attribute:
  // The "special storage" (SVGAnimatedString) for the 'class' attribute (and
  // the 'className' property) is updated by the follow block (|class_name_|
  // registered in |attribute_to_property_map_|.). SvgAttributeChanged then
  // triggers the resulting style updates (instead of
  // Element::ParseAttribute). We don't tell Element about the change to avoid
  // parsing the class list twice.
  if (SVGAnimatedPropertyBase* property = PropertyFromAttribute(params.name)) {
    SVGParsingError parse_error = property->AttributeChanged(params.new_value);
    ReportAttributeParsingError(parse_error, params.name, params.new_value);
    return;
  }

  const AtomicString& event_name =
      HTMLElement::EventNameForAttributeName(params.name);
  if (!event_name.IsNull()) {
    SetAttributeEventListener(
        event_name,
        CreateAttributeEventListener(this, params.name, params.new_value));
    return;
  }

  Element::ParseAttribute(params);
}

// If the attribute is not present in the map, the map will return the "empty
// value" - which is kAnimatedUnknown.
struct AnimatedPropertyTypeHashTraits : HashTraits<AnimatedPropertyType> {
  static const bool kEmptyValueIsZero = true;
  static AnimatedPropertyType EmptyValue() { return kAnimatedUnknown; }
};

using AttributeToPropertyTypeMap = HashMap<QualifiedName,
                                           AnimatedPropertyType,
                                           DefaultHash<QualifiedName>::Hash,
                                           HashTraits<QualifiedName>,
                                           AnimatedPropertyTypeHashTraits>;
AnimatedPropertyType SVGElement::AnimatedPropertyTypeForCSSAttribute(
    const QualifiedName& attribute_name) {
  DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, css_property_map, ());

  if (css_property_map.IsEmpty()) {
    // Fill the map for the first use.
    struct AttrToTypeEntry {
      const QualifiedName& attr;
      const AnimatedPropertyType prop_type;
    };
    const AttrToTypeEntry attr_to_types[] = {
        {kAlignmentBaselineAttr, kAnimatedString},
        {kBaselineShiftAttr, kAnimatedString},
        {kBufferedRenderingAttr, kAnimatedString},
        {kClipPathAttr, kAnimatedString},
        {kClipRuleAttr, kAnimatedString},
        {svg_names::kColorAttr, kAnimatedColor},
        {kColorInterpolationAttr, kAnimatedString},
        {kColorInterpolationFiltersAttr, kAnimatedString},
        {kColorRenderingAttr, kAnimatedString},
        {kCursorAttr, kAnimatedString},
        {kDisplayAttr, kAnimatedString},
        {kDominantBaselineAttr, kAnimatedString},
        {kFillAttr, kAnimatedColor},
        {kFillOpacityAttr, kAnimatedNumber},
        {kFillRuleAttr, kAnimatedString},
        {kFilterAttr, kAnimatedString},
        {kFloodColorAttr, kAnimatedColor},
        {kFloodOpacityAttr, kAnimatedNumber},
        {kFontFamilyAttr, kAnimatedString},
        {kFontSizeAttr, kAnimatedLength},
        {kFontStretchAttr, kAnimatedString},
        {kFontStyleAttr, kAnimatedString},
        {kFontVariantAttr, kAnimatedString},
        {kFontWeightAttr, kAnimatedString},
        {kImageRenderingAttr, kAnimatedString},
        {kLetterSpacingAttr, kAnimatedLength},
        {kLightingColorAttr, kAnimatedColor},
        {kMarkerEndAttr, kAnimatedString},
        {kMarkerMidAttr, kAnimatedString},
        {kMarkerStartAttr, kAnimatedString},
        {kMaskAttr, kAnimatedString},
        {kMaskTypeAttr, kAnimatedString},
        {kOpacityAttr, kAnimatedNumber},
        {kOverflowAttr, kAnimatedString},
        {kPaintOrderAttr, kAnimatedString},
        {kPointerEventsAttr, kAnimatedString},
        {kShapeRenderingAttr, kAnimatedString},
        {kStopColorAttr, kAnimatedColor},
        {kStopOpacityAttr, kAnimatedNumber},
        {kStrokeAttr, kAnimatedColor},
        {kStrokeDasharrayAttr, kAnimatedLengthList},
        {kStrokeDashoffsetAttr, kAnimatedLength},
        {kStrokeLinecapAttr, kAnimatedString},
        {kStrokeLinejoinAttr, kAnimatedString},
        {kStrokeMiterlimitAttr, kAnimatedNumber},
        {kStrokeOpacityAttr, kAnimatedNumber},
        {kStrokeWidthAttr, kAnimatedLength},
        {kTextAnchorAttr, kAnimatedString},
        {kTextDecorationAttr, kAnimatedString},
        {kTextRenderingAttr, kAnimatedString},
        {kVectorEffectAttr, kAnimatedString},
        {kVisibilityAttr, kAnimatedString},
        {kWordSpacingAttr, kAnimatedLength},
    };
    for (size_t i = 0; i < arraysize(attr_to_types); i++)
      css_property_map.Set(attr_to_types[i].attr, attr_to_types[i].prop_type);
  }
  return css_property_map.at(attribute_name);
}

void SVGElement::AddToPropertyMap(SVGAnimatedPropertyBase* property) {
  attribute_to_property_map_.Set(property->AttributeName(), property);
}

SVGAnimatedPropertyBase* SVGElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  AttributeToPropertyMap::const_iterator it =
      attribute_to_property_map_.Find<SVGAttributeHashTranslator>(
          attribute_name);
  if (it == attribute_to_property_map_.end())
    return nullptr;

  return it->value.Get();
}

bool SVGElement::IsAnimatableCSSProperty(const QualifiedName& attr_name) {
  return AnimatedPropertyTypeForCSSAttribute(attr_name) != kAnimatedUnknown;
}

bool SVGElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (const SVGAnimatedPropertyBase* property = PropertyFromAttribute(name))
    return property->HasPresentationAttributeMapping();
  return CssPropertyIdForSVGAttributeName(name) > 0;
}

bool SVGElement::IsPresentationAttributeWithSVGDOM(
    const QualifiedName& name) const {
  const SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  return property && property->HasPresentationAttributeMapping();
}

void SVGElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  CSSPropertyID property_id = CssPropertyIdForSVGAttributeName(name);
  if (property_id > 0)
    AddPropertyToPresentationAttributeStyle(style, property_id, value);
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

  DCHECK(!element->InstanceUpdatesBlocked());

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
  AddEventListenerOptionsResolved options = registered_listener.Options();
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
  EventListenerOptions options = registered_listener.Options();
  const EventListener* listener = registered_listener.Callback();
  for (SVGElement* shadow_tree_element : instances) {
    DCHECK(shadow_tree_element);

    shadow_tree_element->Node::RemoveEventListenerInternal(event_type, listener,
                                                           options);
  }
}

static bool HasLoadListener(Element* element) {
  if (element->HasEventListeners(EventTypeNames::load))
    return true;

  for (element = element->ParentOrShadowHostElement(); element;
       element = element->ParentOrShadowHostElement()) {
    EventListenerVector* entry =
        element->GetEventListeners(EventTypeNames::load);
    if (!entry)
      continue;
    for (wtf_size_t i = 0; i < entry->size(); ++i) {
      if (entry->at(i).Capture())
        return true;
    }
  }

  return false;
}

bool SVGElement::SendSVGLoadEventIfPossible() {
  if (!HaveLoadedRequiredResources())
    return false;
  if ((IsStructurallyExternal() || IsSVGSVGElement(*this)) &&
      HasLoadListener(this))
    DispatchEvent(*Event::Create(EventTypeNames::load));
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

  if (!parent || !parent->IsSVGElement())
    return;

  ToSVGElement(parent)->SendSVGLoadEventToSelfAndAncestorChainIfPossible();
}

void SVGElement::AttributeChanged(const AttributeModificationParams& params) {
  Element::AttributeChanged(params);

  if (params.name == HTMLNames::idAttr) {
    RebuildAllIncomingReferences();
    InvalidateInstances();
    return;
  }

  // Changes to the style attribute are processed lazily (see
  // Element::getAttribute() and related methods), so we don't want changes to
  // the style attribute to result in extra work here.
  if (params.name == HTMLNames::styleAttr)
    return;

  SvgAttributeBaseValChanged(params.name);
}

void SVGElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  CSSPropertyID prop_id =
      SVGElement::CssPropertyIdForSVGAttributeName(attr_name);
  if (prop_id > 0) {
    InvalidateInstances();
    return;
  }

  if (attr_name == HTMLNames::classAttr) {
    ClassAttributeChanged(AtomicString(class_name_->CurrentValue()->Value()));
    InvalidateInstances();
    return;
  }
}

void SVGElement::SvgAttributeBaseValChanged(const QualifiedName& attribute) {
  SvgAttributeChanged(attribute);

  if (!HasSVGRareData() || SvgRareData()->WebAnimatedAttributes().IsEmpty())
    return;

  // TODO(alancutter): Only mark attributes as dirty if their animation depends
  // on the underlying value.
  SvgRareData()->SetWebAnimatedAttributesDirty(true);
  GetElementData()->animated_svg_attributes_are_dirty_ = true;
}

void SVGElement::EnsureAttributeAnimValUpdated() {
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled())
    return;

  if ((HasSVGRareData() && SvgRareData()->WebAnimatedAttributesDirty()) ||
      (GetElementAnimations() &&
       DocumentAnimations::NeedsAnimationTimingUpdate(GetDocument()))) {
    DocumentAnimations::UpdateAnimationTimingIfNeeded(GetDocument());
    ApplyActiveWebAnimations();
  }
}

void SVGElement::SynchronizeAnimatedSVGAttribute(
    const QualifiedName& name) const {
  if (!GetElementData() ||
      !GetElementData()->animated_svg_attributes_are_dirty_)
    return;

  // We const_cast here because we have deferred baseVal mutation animation
  // updates to this point in time.
  const_cast<SVGElement*>(this)->EnsureAttributeAnimValUpdated();

  if (name == AnyQName()) {
    AttributeToPropertyMap::const_iterator::ValuesIterator it =
        attribute_to_property_map_.Values().begin();
    AttributeToPropertyMap::const_iterator::ValuesIterator end =
        attribute_to_property_map_.Values().end();
    for (; it != end; ++it) {
      if ((*it)->NeedsSynchronizeAttribute())
        (*it)->SynchronizeAttribute();
    }

    GetElementData()->animated_svg_attributes_are_dirty_ = false;
  } else {
    SVGAnimatedPropertyBase* property = attribute_to_property_map_.at(name);
    if (property && property->NeedsSynchronizeAttribute())
      property->SynchronizeAttribute();
  }
}

scoped_refptr<ComputedStyle> SVGElement::CustomStyleForLayoutObject() {
  if (!CorrespondingElement())
    return GetDocument().EnsureStyleResolver().StyleForElement(this);

  const ComputedStyle* style = nullptr;
  if (Element* parent = ParentOrShadowHostElement())
    style = parent->GetComputedStyle();

  return GetDocument().EnsureStyleResolver().StyleForElement(
      CorrespondingElement(), style, style);
}

bool SVGElement::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  return IsValid() && HasSVGParent() && Element::LayoutObjectIsNeeded(style);
}

bool SVGElement::HasSVGParent() const {
  // Should we use the flat tree parent instead? If so, we should probably fix a
  // few other checks.
  return ParentOrShadowHostElement() &&
         ParentOrShadowHostElement()->IsSVGElement();
}

MutableCSSPropertyValueSet* SVGElement::AnimatedSMILStyleProperties() const {
  if (HasSVGRareData())
    return SvgRareData()->AnimatedSMILStyleProperties();
  return nullptr;
}

MutableCSSPropertyValueSet* SVGElement::EnsureAnimatedSMILStyleProperties() {
  return EnsureSVGRareData()->EnsureAnimatedSMILStyleProperties();
}

void SVGElement::SetUseOverrideComputedStyle(bool value) {
  if (HasSVGRareData())
    SvgRareData()->SetUseOverrideComputedStyle(value);
}

const ComputedStyle* SVGElement::EnsureComputedStyle(
    PseudoId pseudo_element_specifier) {
  if (!HasSVGRareData() || !SvgRareData()->UseOverrideComputedStyle())
    return Element::EnsureComputedStyle(pseudo_element_specifier);

  const ComputedStyle* parent_style = nullptr;
  if (Element* parent = ParentOrShadowHostElement()) {
    if (LayoutObject* layout_object = parent->GetLayoutObject())
      parent_style = layout_object->Style();
  }

  return SvgRareData()->OverrideComputedStyle(this, parent_style);
}

bool SVGElement::HasFocusEventListeners() const {
  return HasEventListeners(EventTypeNames::focusin) ||
         HasEventListeners(EventTypeNames::focusout) ||
         HasEventListeners(EventTypeNames::focus) ||
         HasEventListeners(EventTypeNames::blur);
}

void SVGElement::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject& layout_object) {
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
      layout_object, true);
}

void SVGElement::InvalidateInstances() {
  if (InstanceUpdatesBlocked())
    return;

  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.IsEmpty())
    return;

  // Mark all use elements referencing 'element' for rebuilding
  for (SVGElement* instance : set) {
    instance->SetCorrespondingElement(nullptr);

    if (SVGUseElement* element = instance->CorrespondingUseElement()) {
      if (element->isConnected())
        element->InvalidateShadowTree();
    }
  }

  SvgRareData()->ElementInstances().clear();
}

void SVGElement::SetNeedsStyleRecalcForInstances(
    StyleChangeType change_type,
    const StyleChangeReasonForTracing& reason) {
  const HeapHashSet<WeakMember<SVGElement>>& set = InstancesForElement();
  if (set.IsEmpty())
    return;

  for (SVGElement* instance : set)
    instance->SetNeedsStyleRecalc(change_type, reason);
}

SVGElement::InstanceUpdateBlocker::InstanceUpdateBlocker(
    SVGElement* target_element)
    : target_element_(target_element) {
  if (target_element_)
    target_element_->SetInstanceUpdatesBlocked(true);
}

SVGElement::InstanceUpdateBlocker::~InstanceUpdateBlocker() {
  if (target_element_)
    target_element_->SetInstanceUpdatesBlocked(false);
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

  if (name == classAttr)
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
  DEFINE_STATIC_LOCAL(Persistent<SVGElementSet>, invalidating_dependencies,
                      (new SVGElementSet));
  return *invalidating_dependencies;
}

void SVGElement::RebuildAllIncomingReferences() {
  if (!HasSVGRareData())
    return;

  const SVGElementSet& incoming_references =
      SvgRareData()->IncomingReferences();

  // Iterate on a snapshot as |incomingReferences| may be altered inside loop.
  HeapVector<Member<SVGElement>> incoming_references_snapshot;
  CopyToVector(incoming_references, incoming_references_snapshot);

  // Force rebuilding the |sourceElement| so it knows about this change.
  for (SVGElement* source_element : incoming_references_snapshot) {
    // Before rebuilding |sourceElement| ensure it was not removed from under
    // us.
    if (incoming_references.Contains(source_element))
      source_element->SvgAttributeChanged(svg_names::kHrefAttr);
  }
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

SVGResourceClient* SVGElement::GetSVGResourceClient() {
  if (!HasSVGRareData())
    return nullptr;
  return SvgRareData()->GetSVGResourceClient();
}

SVGResourceClient& SVGElement::EnsureSVGResourceClient() {
  return EnsureSVGRareData()->EnsureSVGResourceClient(this);
}

void SVGElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(elements_with_relative_lengths_);
  visitor->Trace(attribute_to_property_map_);
  visitor->Trace(svg_rare_data_);
  visitor->Trace(class_name_);
  Element::Trace(visitor);
}

}  // namespace blink
