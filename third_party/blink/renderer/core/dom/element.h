/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/common/input/pointer_id.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_filter.h"
#include "third_party/blink/renderer/core/css/style_recalc_change.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/element_data.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focusgroup_flags.h"
#include "third_party/blink/renderer/core/dom/named_animation_trigger_map.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/transform_view.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "third_party/blink/renderer/platform/restriction_target_id.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/theme_types.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_table.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace gfx {
class QuadF;
class RectF;
class Vector2dF;
}  // namespace gfx

namespace blink {

class AnchorElementObserver;
class AnchorPositionScrollData;
class Animation;
class AnimationTrigger;
class AriaNotificationOptions;
class Attr;
class Attribute;
class CheckVisibilityOptions;
class ColumnPseudoElement;
class ComputedStyleBuilder;
class ContainerQueryData;
class ContainerQueryEvaluator;
class ContentData;
class CSSPropertyName;
class CSSPropertyValueSet;
class CSSPseudoElement;
class CSSStyleDeclaration;
class CustomElementDefinition;
class CustomElementRegistry;
class DisplayLockContext;
class DisplayStyle;
class Document;
class DOMRect;
class DOMRectList;
class DOMStringMap;
class DOMTokenList;
class EditContext;
class Element;
class ElementAnimations;
class ElementInternals;
class ElementIntersectionObserverData;
class ElementRareDataVector;
class ExceptionState;
class FocusOptions;
class GetAnimationsOptions;
class HTMLElement;
class HTMLTemplateElement;
class Image;
class IndexedPseudoElement;
class InputDeviceCapabilities;
class InterestInvokerTargetData;
class InvokerData;
class KURL;
class Locale;
class MutableCSSPropertyValueSet;
class NamedNodeMap;
class OverscrollAreaTracker;
class PointerLockOptions;
class PopoverData;
class PseudoElement;
class ResizeObservation;
class ResizeObserver;
class ResizeObserverSize;
class ScopedCSSName;
class ScriptState;
class ScriptValue;
class ScrollIntoViewOptions;
class ScrollMarkerGroupData;
class ScrollMarkerPseudoElement;
class ScrollToOptions;
class SetHTMLOptions;
class SetHTMLUnsafeOptions;
class ShadowRoot;
class ShadowRootInit;
class SpaceSplitString;
class StyleAdjuster;
class StyleEngine;
class StyleHighlightData;
class StylePropertyMap;
class StylePropertyMapReadOnly;
class StyleRecalcContext;
class StyleScopeData;
class TextVisitor;
class V8UnionBooleanOrScrollIntoViewOptions;
class V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble;
class V8UnionStringLegacyNullToEmptyStringOrTrustedHTML;
class V8UnionStringOrTrustedHTML;

template <typename IDLType>
class FrozenArray;

enum class CSSPropertyID;
enum class CSSValueID;
enum class DisplayLockActivationReason;
enum class DocumentUpdateReason;

struct FocusParams;

using ScrollOffset = gfx::Vector2dF;

struct AttributeToNameTransform {
  String operator()(const Attribute& attr) const {
    return attr.GetName().ToString();
  }
};

using AttributeNamesView =
    bindings::TransformedView<AttributeCollection, AttributeToNameTransform>;

using ColumnPseudoElementsVector = GCedHeapVector<Member<ColumnPseudoElement>>;
using OverscrollAreaParentPseudoElementsVector =
    HeapVector<Member<IndexedPseudoElement>>;

enum SpellcheckAttributeState {
  kSpellcheckAttributeTrue,
  kSpellcheckAttributeFalse,
  kSpellcheckAttributeDefault
};

enum class ElementFlags {
  kTabIndexWasSetExplicitly = 1 << 0,
  kStyleAffectedByEmpty = 1 << 1,
  kIsCanvasOrInCanvasSubtree = 1 << 2,
  kContainsFullScreenElement = 1 << 3,
  kIsInTopLayer = 1 << 4,
  kContainsPersistentVideo = 1 << 5,
  kIsEligibleForElementCapture = 1 << 6,
  kHasCheckedElementCaptureEligibility = 1 << 7,

  kNumberOfElementFlags = 8,  // Size of bitfield used to store the flags.
};

enum class ShadowRootMode;

enum class SlotAssignmentMode { kManual, kNamed };
enum class FocusDelegation { kNone, kDelegateFocus };

enum class SelectionBehaviorOnFocus {
  kReset,
  kRestore,
  kNone,
};

enum class FocusableState {
  kNotFocusable,
  kFocusable,
  kKeyboardFocusableScroller,
};

// https://html.spec.whatwg.org/C/#dom-document-nameditem-filter
enum class NamedItemType {
  kNone,
  kName,
  kNameOrId,
  kNameOrIdWithName,
};

enum class CommandEventType {
  // Action is neither custom, nor built-in (effectively invalid)
  kNone,

  // Custom actions include a `-`.
  kCustom,

  // Popover
  kTogglePopover,
  kHidePopover,
  kShowPopover,
  // Dialog
  kShowModal,
  kClose,
  kRequestClose,
  // Details
  kToggle,
  kOpen,
  // kClose
  // Input / Select
  kShowPicker,
  // Number Input
  kStepUp,
  kStepDown,
  // Fullscreen
  kToggleFullscreen,
  kRequestFullscreen,
  kExitFullscreen,
  // Audio/Video
  kPlayPause,
  kPause,
  kPlay,
  kToggleMuted,
  // Menu
  kToggleMenu,
  kHideMenu,
  kShowMenu,
  // Scroll
  kPageUp,
  kPageDown,
  kPageLeft,
  kPageRight,
  kPageBlockStart,
  kPageBlockEnd,
  kPageInlineStart,
  kPageInlineEnd,
  // Overscroll,
  kToggleOverscroll,
};

// Defaults for the `interestfor` API's `normal` value.
static constexpr double kDefaultInterestDelayStartSeconds = 0.5;
static constexpr double kDefaultInterestDelayEndSeconds = 0.25;

typedef HeapVector<Member<Attr>> AttrNodeList;

struct GetAnimationsOptionsResolved {
  bool use_subtree;
};

// https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-get-trusted-type-data-for-attribute
typedef HashMap<AtomicString, std::pair<SpecificTrustedType, AtomicString>>
    AttrNameToTrustedType;

class CORE_EXPORT Element : public ContainerNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Element(const QualifiedName& tag_name,
          Document*,
          ConstructionType = kCreateElement);

  // ParseDeclarativeShadowRoots specifies whether declarative shadow roots
  // should be parsed by the HTML parser.
  enum class ParseDeclarativeShadowRoots {
    kDontParse = 0,
    kParse = 1,
  };
  // ForceHtml specifies whether the HTML parser should be used when parsing
  // markup even if we are in an XML document.
  enum class ForceHtml {
    kDontForce = 0,
    kForce = 1,
  };

  // Animatable implementation.
  // https://drafts.csswg.org/web-animations-1/#the-animatable-interface-mixin

  // Returns the target element of the animation that these methods are being
  // called on.
  Element* GetAnimationTarget();

  Animation* animate(
      ScriptState* script_state,
      const ScriptValue& keyframes,
      const V8UnionKeyframeAnimationOptionsOrUnrestrictedDouble* options,
      ExceptionState& exception_state);

  Animation* animate(ScriptState*, const ScriptValue&, ExceptionState&);

  HeapVector<Member<Animation>> getAnimations(
      GetAnimationsOptions* options = nullptr);

  HeapVector<Member<Animation>> GetAnimationsInternal(
      GetAnimationsOptionsResolved options);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy, kBeforecopy)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut, kBeforecut)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste, kBeforepaste)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search, kSearch)

  bool hasAttribute(const QualifiedName&) const;
  const AtomicString& getAttribute(const QualifiedName&) const;

  // Set an attribute without Trusted Type validation. Passing g_null_atom
  // is the same as removing the attribute. This should only be used directly
  // if we know the `QualifiedName` is not a special attribute.
  // TODO(crbug.com/374263390): Rename this method and audit callers.
  void setAttribute(const QualifiedName& name, const AtomicString& value) {
    SetAttributeWithoutValidation(name, value);
  }

  // Set an attribute without Trusted Type validation. Passing g_null_atom
  // is the same as removing the attribute. This should only be used directly
  // if we know the `QualifiedName` is not a special attribute or the value
  // has already been validated.
  void SetAttributeWithoutValidation(const QualifiedName&,
                                     const AtomicString& value);

  void SetAttributeWithoutValidation(const QualifiedName& name,
                                     const String& value) {
    SetAttributeWithoutValidation(name, AtomicString(value));
  }

  // Set an attribute with Trusted Type validation. Passing g_null_atom
  // is the same as removing the attribute.
  void SetAttributeWithValidation(Attr*,
                                  const AtomicString& value,
                                  ExceptionState&);

  // TODO(crbug.com/374263390): This method should likely CHECK if
  // QualifiedName is a trusted type.
  void SetSynchronizedLazyAttribute(const QualifiedName&,
                                    const AtomicString& value);

  void removeAttribute(const QualifiedName&);

  // Typed getters and setters for language bindings.
  int GetIntegralAttribute(const QualifiedName& attribute_name) const;
  int GetIntegralAttribute(const QualifiedName& attribute_name,
                           int default_value) const;
  unsigned int GetUnsignedIntegralAttribute(
      const QualifiedName& attribute_name) const;
  void SetIntegralAttribute(const QualifiedName& attribute_name, int value);
  void SetUnsignedIntegralAttribute(const QualifiedName& attribute_name,
                                    unsigned value,
                                    unsigned default_value = 0);
  double GetFloatingPointAttribute(
      const QualifiedName& attribute_name,
      double fallback_value = std::numeric_limits<double>::quiet_NaN()) const;
  void SetFloatingPointAttribute(const QualifiedName& attribute_name,
                                 double value);

  // If this element hosts a shadow root with a referenceTarget, returns the
  // target element inside the shadow root. In the case where there are multiple
  // nested layers of shadow roots, returns the innermost target element.
  Element* GetShadowReferenceTarget(const QualifiedName& name) const;

  // If this element hosts a shadow root with a referenceTarget, returns the
  // target element inside the shadow root (recursively). If at any layer of
  // shadow root, referenceTarget is specified but the ID is invalid, returns
  // nullptr. In other cases, return this element.
  Element* GetShadowReferenceTargetOrSelf(const QualifiedName& name);

  // Returns true if |this| element has attr-associated elements that were set
  // via the IDL, rather than computed from the content attribute.
  // See
  // https://whatpr.org/html/3917/common-dom-interfaces.html#reflecting-content-attributes-in-idl-attributes:element
  // for more information.
  // This is only exposed as an implementation detail to AXRelationCache, which
  // computes aria-owns differently for element reflection.
  bool HasAnyExplicitlySetAttrAssociatedElements() const;
  bool HasExplicitlySetAttrAssociatedElements(const QualifiedName& name) const;
  GCedHeapLinkedHashSet<WeakMember<Element>>* GetExplicitlySetElementsForAttr(
      const QualifiedName& name) const;

  Element* GetElementAttribute(const QualifiedName& name) const;
  Element* GetElementAttributeResolvingReferenceTarget(
      const QualifiedName& name) const;
  void SetElementAttribute(const QualifiedName&, Element*);
  GCedHeapVector<Member<Element>>* GetAttrAssociatedElements(
      const QualifiedName& name) const;

  // If treescope_element is connected, then we will search treescope_element's
  // TreeScope for an element with the id. If treescope_element is disconnected,
  // then we will use its TreeRoot() to search for an element with the id
  // instead.
  Element* getElementByIdIncludingDisconnected(const Element& treescope_element,
                                               const AtomicString& id) const;

  FrozenArray<Element>* ariaControlsElements();
  void setAriaControlsElements(GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaDescribedByElements();
  void setAriaDescribedByElements(
      GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaDetailsElements();
  void setAriaDetailsElements(GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaErrorMessageElements();
  void setAriaErrorMessageElements(
      GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaFlowToElements();
  void setAriaFlowToElements(GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaLabelledByElements();
  void setAriaLabelledByElements(
      GCedHeapVector<Member<Element>>* given_elements);
  FrozenArray<Element>* ariaOwnsElements();
  void setAriaOwnsElements(GCedHeapVector<Member<Element>>* given_elements);

  // Call this to get the value of an attribute that is known not to be the
  // style attribute or one of the SVG animatable attributes.
  bool FastHasAttribute(const QualifiedName&) const;
  const AtomicString& FastGetAttribute(const QualifiedName&) const;
#if DCHECK_IS_ON()
  bool FastAttributeLookupAllowed(const QualifiedName&) const;
#endif

#if DUMP_NODE_STATISTICS
  bool HasNamedNodeMap() const;
#endif
  bool hasAttributes() const;

  bool hasAttribute(const AtomicString& name) const;
  bool hasAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& local_name) const;

  // Ignores namespace.
  bool HasAttributeIgnoringNamespace(const AtomicString& local_name) const;

  const AtomicString& getAttribute(const AtomicString& local_name) const {
    return GetAttributeHinted(local_name, WeakLowercaseIfNecessary(local_name));
  }

  const AtomicString& getAttributeNS(const AtomicString& namespace_uri,
                                     const AtomicString& local_name) const;

  void setAttribute(AtomicString name,
                    String value,
                    ExceptionState& exception_state = ASSERT_NO_EXCEPTION) {
    AtomicStringTable::WeakResult weak_lowercase_name =
        WeakLowercaseIfNecessary(name);
    SetAttributeHinted(std::move(name), weak_lowercase_name, std::move(value),
                       exception_state);
  }

  // Trusted Types variant for explicit setAttribute() use.
  void setAttribute(AtomicString name,
                    const V8TrustedType* trusted_string,
                    ExceptionState& exception_state) {
    AtomicStringTable::WeakResult weak_lowercase_name =
        WeakLowercaseIfNecessary(name);
    SetAttributeHinted(std::move(name), weak_lowercase_name, trusted_string,
                       exception_state);
  }

  // Returns attributes that should be checked against Trusted Types
  virtual const AttrNameToTrustedType& GetCheckedAttributeTypes() const;
  const std::tuple<SpecificTrustedType, const AtomicString, const AtomicString>
  GetTrustedTypeDataForAttribute(const QualifiedName& q_name,
                                 const char* legacy_sink_name) const;

  static std::optional<QualifiedName> ParseAttributeName(
      const AtomicString& namespace_uri,
      const AtomicString& qualified_name,
      ExceptionState&);
  void setAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& qualified_name,
                      String value,
                      ExceptionState& exception_state);
  void setAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& qualified_name,
                      const V8TrustedType* trusted_string,
                      ExceptionState& exception_state);

  bool toggleAttribute(const AtomicString&, ExceptionState&);
  bool toggleAttribute(const AtomicString&, bool force, ExceptionState&);

  const AtomicString& GetIdAttribute() const;
  void SetIdAttribute(const AtomicString&);

  const AtomicString& GetNameAttribute() const;
  const AtomicString& GetClassAttribute() const;

  // This is an operation defined in the DOM standard like:
  //   If element is in the HTML namespace and its node document is an HTML
  //   document, then set qualifiedName to qualifiedName in ASCII lowercase.
  //   https://dom.spec.whatwg.org/#concept-element-attributes-get-by-name
  AtomicString LowercaseIfNecessary(AtomicString) const;
  AtomicStringTable::WeakResult WeakLowercaseIfNecessary(
      const AtomicString&) const;

  // NoncedElement implementation: this is only used by HTMLElement and
  // SVGElement, but putting the implementation here allows us to use
  // ElementRareData to hold the data.
  const AtomicString& nonce() const;
  void setNonce(const AtomicString&);

  // Call this to get the value of the id attribute for style resolution
  // purposes.  The value will already be lowercased if the document is in
  // compatibility mode, so this function is not suitable for non-style uses.
  const AtomicString& IdForStyleResolution() const;

  // This getter takes care of synchronizing all attributes before returning the
  // AttributeCollection. If the Element has no attributes, an empty
  // AttributeCollection will be returned. This is not a trivial getter and its
  // return value should be cached for performance.
  AttributeCollection Attributes() const;
  // This variant will not update the potentially invalid attributes. To be used
  // when not interested in style attribute or one of the SVG attributes.
  AttributeCollection AttributesWithoutUpdate() const;
  // Similar to AttributesWithoutUpdate(), but with only the style attribute
  // exempt (ie., SVG attributes are always synchronized, for simplicity).
  // The style attribute is special because it is so frequently updated from
  // JavaScript and also easily identifiable (it is a single attribute).
  AttributeCollection AttributesWithoutStyleUpdate() const;

  void scrollIntoViewWithOptions(const ScrollIntoViewOptions*);
  void ScrollIntoViewNoVisualUpdate(mojom::blink::ScrollIntoViewParamsPtr,
                                    const Element* container = nullptr,
                                    bool include_self = false);
  void scrollIntoViewIfNeeded(bool center_if_needed = true);

  int OffsetLeft();
  int OffsetTop();
  int OffsetWidth();
  int OffsetHeight();

  Element* OffsetParent();
  int clientLeft();
  int clientTop();
  int ClientLeftNoLayout() const;
  int ClientTopNoLayout() const;
  int clientWidth();
  int clientHeight();
  double currentCSSZoom();
  double scrollLeft();
  double scrollTop();
  void setScrollLeft(double);
  void setScrollTop(double);
  virtual int scrollWidth();
  virtual int scrollHeight();

  ScriptPromise<IDLUndefined> scrollIntoView(
      ScriptState* script_state,
      const V8UnionBooleanOrScrollIntoViewOptions* arg);
  ScriptPromise<IDLUndefined> scrollIntoView(ScriptState* script_state,
                                             bool align_to_top = true);
  ScriptPromise<IDLUndefined> scrollBy(ScriptState* script_state,
                                       double x,
                                       double y);
  ScriptPromise<IDLUndefined> scrollBy(ScriptState* script_state,
                                       const ScrollToOptions*);
  ScriptPromise<IDLUndefined> scrollTo(ScriptState* script_state,
                                       double x,
                                       double y);
  ScriptPromise<IDLUndefined> scrollTo(ScriptState* script_state,
                                       const ScrollToOptions*);

  void scrollIntoViewForTesting(
      const V8UnionBooleanOrScrollIntoViewOptions* arg);
  void scrollIntoViewForTesting();
  void scrollByForTesting(double x, double y);
  void scrollToForTesting(double x, double y);

  bool SetScrollOffset(const ScrollToOptions*);

  // Returns the bounds of this Element, unclipped, in the coordinate space of
  // the local root's widget. That is, in the outermost main frame, this will
  // scale and transform the bounds by the visual viewport transform (i.e.
  // pinch-zoom). In a local root that isn't main (i.e. a remote frame), the
  // returned bounds are unscaled by the visual viewport and are relative to
  // the local root frame.
  gfx::Rect BoundsInWidget() const;

  // Same as above but for outline rects.
  Vector<gfx::Rect> OutlineRectsInWidget(
      DocumentUpdateReason reason = DocumentUpdateReason::kUnknown) const;

  // Returns the bounds of this element relative to the local root frame's
  // origin. While the rect is relative to the local root, it is intersected
  // with all ancestor frame clips, including the visual viewport transform and
  // clip in the main frame. While this applies ancestor frame clipping, it
  // does not (yet) apply (overflow) element clipping (crbug.com/41417572).
  gfx::Rect VisibleBoundsInLocalRoot() const;

  // TODO(crbug.com/41417572): This method should replace the above method.
  gfx::Rect VisibleBoundsRespectingClipsInLocalRoot() const;

  DOMRectList* getClientRects();
  // Returns a list of clients Rects in zoomed pixel units.
  Vector<gfx::RectF> GetClientRectsNoAdjustment();

  // Returns a rectangle in zoomed pixel units.
  gfx::RectF GetBoundingClientRectNoLifecycleUpdateNoAdjustment() const;
  // Returns a rectangle in CSS pixel units.  i.e. ignoring zoom.
  gfx::RectF GetBoundingClientRectNoLifecycleUpdate() const;
  DOMRect* GetBoundingClientRect();
  DOMRect* GetBoundingClientRectForBinding();

  // Call the NoLifecycleUpdate variants if you are sure that the lifcycle is
  // already updated to at least pre-paint clean.
  const AtomicString& computedRole();
  const AtomicString& ComputedRoleNoLifecycleUpdate();
  String computedName();
  String ComputedNameNoLifecycleUpdate();

  void ariaNotify(const String& announcement,
                  const AriaNotificationOptions* options);

  void DidMoveToNewDocument(Document&) override;

  void removeAttribute(const AtomicString& name) {
    RemoveAttributeHinted(name, WeakLowercaseIfNecessary(name));
  }
  void removeAttributeNS(const AtomicString& namespace_uri,
                         const AtomicString& local_name);

  Attr* DetachAttribute(wtf_size_t index);

  Attr* getAttributeNode(const AtomicString& name);
  Attr* getAttributeNodeNS(const AtomicString& namespace_uri,
                           const AtomicString& local_name);
  Attr* setAttributeNode(Attr*, ExceptionState&);
  Attr* setAttributeNodeNS(Attr*, ExceptionState&);
  Attr* removeAttributeNode(Attr*, ExceptionState&);

  Attr* AttrIfExists(const QualifiedName&);
  Attr* EnsureAttr(const QualifiedName&);

  AttrNodeList* GetAttrNodeList();

  CSSStyleDeclaration* style();
  StylePropertyMap* attributeStyleMap();
  StylePropertyMapReadOnly* ComputedStyleMap();

  const QualifiedName& TagQName() const { return tag_name_; }
  String tagName() const { return nodeName(); }

  bool HasTagName(const QualifiedName& tag_name) const {
    return tag_name_.Matches(tag_name);
  }
  bool HasTagName(const HTMLQualifiedName& tag_name) const {
    return ContainerNode::HasTagName(tag_name);
  }
  bool HasTagName(const MathMLQualifiedName& tag_name) const {
    return ContainerNode::HasTagName(tag_name);
  }
  bool HasTagName(const SVGQualifiedName& tag_name) const {
    return ContainerNode::HasTagName(tag_name);
  }

  // Should be called only by Document::createElementNS to fix up tag_name_
  // immediately after construction.
  void SetTagNameForCreateElementNS(const QualifiedName&);

  // A fast function for checking the local name against another atomic string.
  bool HasLocalName(const AtomicString& other) const {
    return tag_name_.LocalName() == other;
  }

  const AtomicString& localName() const { return tag_name_.LocalName(); }
  AtomicString LocalNameForSelectorMatching() const;
  const AtomicString& prefix() const { return tag_name_.Prefix(); }
  const AtomicString& namespaceURI() const { return tag_name_.NamespaceURI(); }

  const AtomicString& LocateNamespacePrefix(
      const AtomicString& namespace_uri) const;

  String nodeName() const override;

  Element& CloneWithChildren(NodeCloningData& data,
                             Document*,
                             ContainerNode*,
                             CustomElementRegistry*,
                             ExceptionState& = ASSERT_NO_EXCEPTION) const;
  Element& CloneWithoutChildren(NodeCloningData& data,
                                CustomElementRegistry*,
                                Document* = nullptr) const;
  Element& CloneWithoutChildren() const;

  void SetBooleanAttribute(const QualifiedName&, bool);

  virtual const CSSPropertyValueSet* AdditionalPresentationAttributeStyle() {
    return nullptr;
  }
  virtual void InvalidateStyleAttribute(
      bool only_changed_independent_properties);

  const CSSPropertyValueSet* InlineStyle() const {
    return HasElementData() ? GetElementData()->inline_style_.Get() : nullptr;
  }

  void SetInlineStyleProperty(CSSPropertyID,
                              CSSValueID identifier,
                              bool important = false);
  void SetInlineStyleProperty(CSSPropertyID,
                              double value,
                              CSSPrimitiveValue::UnitType,
                              bool important = false);
  void SetInlineStyleProperty(CSSPropertyID,
                              const CSSValue&,
                              bool important = false);
  bool SetInlineStyleProperty(CSSPropertyID,
                              const String& value,
                              bool important = false);
  void SetInlineStyleProperty(const CSSPropertyName&,
                              const CSSValue&,
                              bool important = false);

  bool RemoveInlineStyleProperty(CSSPropertyID);
  bool RemoveInlineStyleProperty(const AtomicString&);
  void RemoveAllInlineStyleProperties();

  void SynchronizeStyleAttributeInternal() const;

  const CSSPropertyValueSet* PresentationAttributeStyle();
  virtual bool IsPresentationAttribute(const QualifiedName& attr) const {
    return false;
  }
  virtual void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      HeapVector<CSSPropertyValue, 8>&) {}
  // Subclasses can override these functions if there is extra style that needs
  // to be mapped like attributes.
  virtual bool HasExtraStyleForPresentationAttribute() const { return false; }
  virtual void CollectExtraStyleForPresentationAttribute(
      HeapVector<CSSPropertyValue, 8>&) {}

  // For exposing to DOM only.
  NamedNodeMap* attributesForBindings() const;
  AttributeNamesView getAttributeNamesForBindings() const;
  // Note that the method above returns a live view of underlying
  // attribute collection, which may be unsafe to use for iteration
  // if element attributes are modified during iteration, hence the
  // safe (but slower) alternative below.
  Vector<AtomicString> getAttributeNames() const;
  Vector<QualifiedName> getAttributeQualifiedNames() const;

  enum class AttributeModificationReason {
    kDirectly,
    kByParser,
    kByCloning,
    kByMoveToNewDocument,
    kBySynchronizationOfLazyAttribute
  };
  struct AttributeModificationParams {
    STACK_ALLOCATED();

   public:
    AttributeModificationParams(const QualifiedName& qname,
                                const AtomicString& old_value,
                                const AtomicString& new_value,
                                AttributeModificationReason reason)
        : name(qname),
          old_value(old_value),
          new_value(new_value),
          reason(reason) {}

    const QualifiedName& name;
    const AtomicString& old_value;
    const AtomicString& new_value;
    const AttributeModificationReason reason;
  };

  // |attributeChanged| is called whenever an attribute is added, changed or
  // removed. It handles very common attributes such as id, class, name, style,
  // and slot.
  //
  // While the owner document is parsed, this function is called after all
  // attributes in a start tag were added to the element.
  virtual void AttributeChanged(const AttributeModificationParams&);

  // |ParseAttribute()| is called by |AttributeChanged()|. If an element
  // implementation needs to check an attribute update, override this function.
  // This function is called before Element handles the change. This means
  // changes like `kSlotAttr` will not have been processed. Subclasses should
  // take care to avoid any processing that needs Element to have handled the
  // change. For example, flat-tree-travesal could be problematic. In such
  // cases subclasses should override AttributeChanged() and do the processing
  // after calling Element::AttributeChanged().
  //
  // While the owner document is parsed, this function is called after all
  // attributes in a start tag were added to the element.
  virtual void ParseAttribute(const AttributeModificationParams&);

  virtual bool HasLegalLinkAttribute(const QualifiedName&) const;

  // Only called by the parser immediately after element construction.
  void ParserSetAttributes(const Vector<Attribute, kAttributePrealloc>&);

  // Remove attributes that might introduce scripting from the vector leaving
  // the element unchanged.
  void StripScriptingAttributes(Vector<Attribute, kAttributePrealloc>&) const;

  bool SharesSameElementData(const Element& other) const {
    return GetElementData() == other.GetElementData();
  }

  // Clones attributes only.
  void CloneAttributesFrom(const Element&);

  bool HasEquivalentAttributes(const Element& other) const;

  // Returns false if the element definitely does not have an attribute
  // matching the given name. Is allowed to return false positives.
  bool CouldHaveAttribute(const QualifiedName& attribute_name) const {
    return CouldMatchFilter(FilterForAttribute(attribute_name));
  }
  bool CouldHaveClass(const AtomicString& class_name) const {
    return CouldMatchFilter(FilterForString(class_name));
  }

  // A variant of CouldHave{Attribute,Class}() that allows you to compute
  // the filter ahead-of-time; useful if you want to test many elements
  // against the same attribute/class name, or to test against multiple
  // attributes/classes at the same time.
  using TinyBloomFilter = uint32_t;
  static TinyBloomFilter FilterForAttribute(
      const QualifiedName& attribute_name) {
    return FilterForString(attribute_name.LocalNameUpper());
  }
  static TinyBloomFilter FilterForString(const AtomicString& str) {
    unsigned hash = str.Hash();
    TinyBloomFilter filter = 0;
    // Build a 32-bit Bloom filter, with k=2. We extract the two
    // (5-bit) hashes that we need from non-overlapping parts of the
    // (24-bit) String hash, which should be independent.
    filter |= 1u << (hash & 31);
    filter |= 1u << ((hash >> 5) & 31);
    return filter;
  }
  bool SubtreeMayMatchClassOrAttrFilter(TinyBloomFilter filter) const {
    bool match = CouldMatchFilter(filter);
#if DCHECK_IS_ON()
    if (!match) {
      // The caller is going to skip this entire subtree,
      // so verify that we're not missing anything.
      VerifyBloomFilterTreeConsistencyIncludingChildren();
    }
#endif
    return match;
  }
  // Exactly the same as SubtreeMayMatchClassOrAttrFilter(),
  // except that it can be called before the entire tree is
  // attached correctly, so we don't DCHECK that the Bloom filters
  // are consistent.
  bool CouldMatchFilter(TinyBloomFilter filter) const {
    return (attribute_or_class_bloom_ & filter) == filter;
  }
  // Useful if you are to match the same element against a lot of different
  // selectors in quick succession.
  TinyBloomFilter AttributeOrClassBloomFilter() const {
    return attribute_or_class_bloom_;
  }

#if DCHECK_IS_ON()
  void VerifyBloomFilterTreeConsistency() const {
    if (!parentElement()) {
      return;
    }

    if ((parentElement()->attribute_or_class_bloom_ &
         attribute_or_class_bloom_) != attribute_or_class_bloom_) {
      char bitsstr[256];
      snprintf(bitsstr, sizeof(bitsstr),
               "bits=0x%08x subtree=0x%08x parentbits=0x%08x missing=0x%08x",
               attribute_or_class_bloom_, attribute_or_class_bloom_,
               parentElement()->attribute_or_class_bloom_,
               attribute_or_class_bloom_ &
                   ~parentElement()->attribute_or_class_bloom_);
      LOG(FATAL) << this << " Bloom bits were not properly propagated up to "
                 << parentElement() << " " << bitsstr;
    }
  }

  // Used when skipping over an entire subtree, to check that we're not
  // missing anything inside it.
  void VerifyBloomFilterTreeConsistencyIncludingChildren() const;
#endif

  // Step 5 of https://dom.spec.whatwg.org/#concept-node-clone
  virtual void CloneNonAttributePropertiesFrom(const Element&,
                                               NodeCloningData&) {}

  const ComputedStyle* GetComputedStyle() const {
    return computed_style_.Get();
  }
  const ComputedStyle& ComputedStyleRef() const {
    DCHECK(computed_style_);
    return *computed_style_;
  }

  void SetComputedStyle(const ComputedStyle* computed_style) {
    computed_style_ = computed_style;
  }

  using Node::DetachLayoutTree;
  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(bool performing_reattach) override;

  virtual LayoutObject* CreateLayoutObject(const ComputedStyle&);
  virtual bool LayoutObjectIsNeeded(const DisplayStyle&) const;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const;

  const ComputedStyle* ParentComputedStyle() const;

  void RecalcStyle(const StyleRecalcChange, const StyleRecalcContext&);
  void RecalcStyleForTraversalRootAncestor();

  // RecalcHighlightStyles for the originating element's new_style and return
  // a new new_style if highlight styles were added. Otherwise return a pointer
  // to the passed in new_style.
  const ComputedStyle* RecalcHighlightStyles(
      const StyleRecalcContext& style_recalc_context,
      const ComputedStyle* old_style,
      const ComputedStyle& new_style,
      const ComputedStyle* parent_style);

  void RebuildLayoutTreeForTraversalRootAncestor() {
    RebuildFirstLetterLayoutTree();
    WhitespaceAttacher whitespace_attacher;
    RebuildPseudoElementLayoutTree(kPseudoIdMarker, whitespace_attacher);
    HandleSubtreeModifications();
  }
  void RebuildLayoutTreeForSizeContainerAncestor() {
    RebuildFirstLetterLayoutTree();
  }
  bool NeedsRebuildChildLayoutTrees(
      const WhitespaceAttacher& whitespace_attacher) const {
    return ChildNeedsReattachLayoutTree() || NeedsWhitespaceChildrenUpdate() ||
           (whitespace_attacher.TraverseIntoDisplayContents() &&
            HasDisplayContentsStyle());
  }
  bool NeedsRebuildLayoutTree(
      const WhitespaceAttacher& whitespace_attacher) const {
    return NeedsReattachLayoutTree() ||
           NeedsRebuildChildLayoutTrees(whitespace_attacher) ||
           NeedsLayoutSubtreeUpdate();
  }
  void RebuildLayoutTree(WhitespaceAttacher&);

  // Reattach layout tree for all children but not the element itself. This is
  // only used for UpdateStyleAndLayoutTreeForContainer when:
  // 1. Re-attaching fieldset when the fieldset layout tree changes and the size
  //    query container is a fieldset.
  // 2. Re-attaching for legacy box tree when table-* boxes have columns.
  //
  // Case 2 is only necessary until table fragmentation is shipped for LayoutNG.
  //
  void ReattachLayoutTreeChildren(base::PassKey<StyleEngine>);

  void HandleSubtreeModifications();
  void PseudoStateChanged(CSSSelector::PseudoType);
  void PseudoStateChangedForTesting(CSSSelector::PseudoType);
  void SetAnimationStyleChange(bool);
  void SetNeedsAnimationStyleRecalc();

  void SetNeedsCompositingUpdate();

  // Associates the element with a RegionCaptureCropId, which is the object
  // internally backing a CropTarget.
  // This method may be called at most once. The ID must be non-null.
  void SetRegionCaptureCropId(std::unique_ptr<RegionCaptureCropId> id);

  // If SetRegionCaptureCropId(id) was previously called on `this`,
  // returns the non-empty `id` which it previously provided.
  // Otherwise, returns a nullptr.
  const RegionCaptureCropId* GetRegionCaptureCropId() const;

  // Associates the element with a RestrictionTargetId, which is the object
  // internally backing a RestrictionTarget.
  // This method may be called at most once. The ID must be non-null.
  void SetRestrictionTargetId(std::unique_ptr<RestrictionTargetId> id);

  // If SetRestrictionTargetId(id) was previously called on `this`,
  // returns the non-empty `id` which it previously provided.
  // Otherwise, returns a nullptr.
  const RestrictionTargetId* GetRestrictionTargetId() const;

  // Set whether the element is eligible for element level capture. This is
  // based on how the element is painted. Should only be called if the element
  // has a RestrictionTargetId.
  void SetIsEligibleForElementCapture(bool value);

  ShadowRoot* attachShadow(const ShadowRootInit*, ExceptionState&);

  // Returns true if the attachment was successful.
  bool AttachDeclarativeShadowRoot(HTMLTemplateElement&,
                                   String,
                                   FocusDelegation,
                                   SlotAssignmentMode,
                                   bool serializable,
                                   bool clonable,
                                   const AtomicString& adopted_stylesheets,
                                   const AtomicString& reference_target,
                                   const bool waiting_for_scoped_registry);

  ShadowRoot& CreateUserAgentShadowRoot(
      SlotAssignmentMode = SlotAssignmentMode::kNamed);
  ShadowRoot& AttachShadowRootInternal(ShadowRootMode,
                                       FocusDelegation,
                                       SlotAssignmentMode,
                                       CustomElementRegistry*,
                                       bool serializable,
                                       bool clonable,
                                       const AtomicString& reference_target);
  // This version is for testing only, and allows easy attachment of a shadow
  // root, specifying only the type and none of the other arguments.
  ShadowRoot& AttachShadowRootForTesting(ShadowRootMode type);

  // Returns the shadow root attached to this element if it is a shadow host.
  ALWAYS_INLINE ShadowRoot* GetShadowRoot() const {
    return HasShadowRoot() ? GetShadowRootInternal() : nullptr;
  }
  ShadowRoot* OpenShadowRoot() const;
  ShadowRoot* ClosedShadowRoot() const;
  ShadowRoot* AuthorShadowRoot() const;
  ShadowRoot* UserAgentShadowRoot() const;

  ShadowRoot& EnsureUserAgentShadowRoot(
      SlotAssignmentMode = SlotAssignmentMode::kNamed);

  // Implements manual slot assignment for user agent shadow roots.
  virtual void ManuallyAssignSlots() { DCHECK(false); }

  bool IsInDescendantTreeOf(const Element* shadow_host) const;

  // Returns the Element’s ComputedStyle. If the ComputedStyle is not already
  // stored on the Element, computes the ComputedStyle and stores it on the
  // Element’s ElementRareData.  Used for getComputedStyle when Element is
  // display none.
  const ComputedStyle* EnsureComputedStyle(
      PseudoId = kPseudoIdNone,
      const AtomicString& pseudo_argument = g_null_atom);

  bool HasDisplayContentsStyle() const;

  bool ShouldStoreComputedStyle(const ComputedStyle&) const;

  // Methods for indicating the style is affected by dynamic updates (e.g.,
  // children changing, our position changing in our sibling list, etc.)
  bool StyleAffectedByEmpty() const {
    return HasElementFlag(ElementFlags::kStyleAffectedByEmpty);
  }
  void SetStyleAffectedByEmpty() {
    SetElementFlag(ElementFlags::kStyleAffectedByEmpty);
  }

  void SetIsCanvasOrInCanvasSubtree(bool);
  bool IsCanvasOrInCanvasSubtree() const {
    return HasElementFlag(ElementFlags::kIsCanvasOrInCanvasSubtree);
  }
  // Called when `IsCanvasOrInCanvasSubtree()` has changed.
  virtual void DidChangeIsCanvasOrInCanvasSubtree() {}
  // Like `IsCanvasOrInCanvasSubtree()`, but excludes the outermost <canvas>.
  bool IsInCanvasSubtree() const;

  bool IsDefined() const {
    // An element whose custom element state is "uncustomized" or "custom"
    // is said to be defined.
    // https://dom.spec.whatwg.org/#concept-element-defined
    return GetCustomElementState() == CustomElementState::kUncustomized ||
           GetCustomElementState() == CustomElementState::kCustom;
  }

  AtomicString ComputeInheritedLanguage() const;
  Locale& GetLocale() const;

  virtual void AccessKeyAction(SimulatedClickCreationScope) {}

  virtual bool IsURLAttribute(const Attribute&) const { return false; }
  virtual bool IsHTMLContentAttribute(const Attribute&) const { return false; }
  bool IsJavaScriptURLAttribute(const Attribute&) const;
  virtual bool IsSVGAnimationAttributeSettingJavaScriptURL(
      const Attribute&) const {
    return false;
  }
  bool IsScriptingAttribute(const Attribute&) const;

  virtual bool IsLiveLink() const { return false; }
  KURL HrefURL() const;

  String GetURLAttribute(const QualifiedName&) const;
  KURL GetURLAttributeAsKURL(const QualifiedName&) const;

  KURL GetNonEmptyURLAttribute(const QualifiedName&) const;

  virtual const AtomicString ImageSourceURL() const;
  virtual Image* ImageContents() { return nullptr; }

  // Returns true if this is a shadow host, and its ShadowRoot has
  // delegatesFocus flag.
  bool IsShadowHostWithDelegatesFocus() const;
  // in_descendant_traversal is used in GetFocusableArea and GetFocusDelegate to
  // indicate that GetFocusDelegate is currently iterating over all descendants
  // in a DOM subtree. Since GetFocusDelegate calls GetFocusableArea and
  // GetFocusableArea calls GetFocusDelegate, this allows us to skip redundant
  // recursive calls to the same descendants.
  Element* GetFocusableArea(bool in_descendant_traversal = false) const;
  Element* GetFocusDelegate(bool in_descendant_traversal = false) const;
  // Element focus function called through IDL (i.e. element.focus() in JS)
  // Delegates to Focus() with focus type set to kScript
  void focusForBindings(const FocusOptions*);
  // Element focus function called from outside IDL (user focus,
  // accessibility, etc...)
  virtual void Focus(const FocusParams&);
  // Delegates to virtual Focus() with focus type set to kNone
  void Focus();
  void Focus(const FocusOptions*);

  virtual void SetFocused(bool received, mojom::blink::FocusType);
  virtual void SetHasFocusWithinUpToAncestor(bool has_focus_within,
                                             Element* ancestor,
                                             bool need_snap_container_search);
  void FocusStateChanged();
  void FocusVisibleStateChanged();
  void FocusWithinStateChanged();
  void ActiveViewTransitionStateChanged();
  void ActiveViewTransitionTypeStateChanged();
  void OverscrollTargetStateChanged();
  void SetDragged(bool) override;

  void UpdateSelectionOnFocus(SelectionBehaviorOnFocus);
  // This function is called after SetFocused(true) before dispatching 'focus'
  // event, or is called just after a layout after changing <input> type.
  virtual void UpdateSelectionOnFocus(SelectionBehaviorOnFocus,
                                      const FocusOptions*);
  virtual void blur();

  enum class UpdateBehavior {
    // The normal update behavior - update style and layout if needed.
    kStyleAndLayout,
    // Don't update style and layout. This should only be called by
    // accessibility-related code, when needed.
    kNoneForAccessibility,
    // Don't update style and layout. This should only be called by
    // functions that are updating focused state, such as
    // ShouldHaveFocusAppearance() and ClearFocusedElementIfNeeded().
    kNoneForFocusManagement,
    // Don't update style and layout, and assert that layout is clean already.
    kAssertNoLayoutUpdates,
  };

  // Focusability logic:
  //   IsFocusable: true if the element can be focused via element.focus().
  //   IsMouseFocusable: true if clicking on the element will focus it.
  //   IsKeyboardFocusableSlow: true if the element appears in the sequential
  //     focus navigation loop. I.e. if the tab key can focus it.
  //
  // Helpers:
  //   SupportsFocus: true if it is *possible* for the element to be focused. An
  //     element supports focus if it has a tabindex attribute, or it is
  //     editable, etc. Note that the element might *support* focus while not
  //     *being focusable*, e.g. when the element is disconnected.
  //   IsFocusableState: can be not focusable, focusable, or focusable because
  //     of keyboard focusable scrollers.
  //
  // IsFocusable can only be true if SupportsFocus is true. And both
  // IsMouseFocusable and IsKeyboardFocusableSlow require IsFocusable to be
  // true. But it is possible for an element to be keyboard-focusable without
  // being mouse-focusable, or vice versa.
  //
  // All of these methods can be called when layout is not clean, but a
  // lifecycle update might be triggered in that case. If layout is already
  // clean, these methods will not trigger an additional lifecycle update.
  // If UpdateBehavior::kNoneForAccessibility is passed (only to be used by
  // accessibility code), then no layout updates will be performed even in the
  // case that layout is dirty.
  bool IsFocusable(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;
  bool IsMouseFocusable(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;
  // If the element might be a keyboard-focusable scroller, then it will call
  // IsKeyboardFocusableScroller which can be slow. Avoid calling this function
  // outside of focus sequential navigation.
  virtual bool IsKeyboardFocusableSlow(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;

  bool IsFocusedElementInDocument() const;
  Element* AdjustedFocusedElementInTreeScope() const;
  bool IsAutofocusable() const;

  // Returns true if `last_focus_type_` was not the result of an unknown or
  // script source. For more see:
  // https://explainers-by-googlers.github.io/user-dictionary-leaks/
  bool WasLastFocusFromUserGesture() const {
    return RareData() && WasLastFocusFromUserGestureInternal();
  }

  // Returns false if the event was canceled, and true otherwise.
  virtual bool DispatchFocusEvent(
      Element* old_focused_element,
      mojom::blink::FocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchBlurEvent(
      Element* new_focused_element,
      mojom::blink::FocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchFocusInEvent(
      const AtomicString& event_type,
      Element* old_focused_element,
      mojom::blink::FocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  void DispatchFocusOutEvent(
      const AtomicString& event_type,
      Element* new_focused_element,
      InputDeviceCapabilities* source_capabilities = nullptr);

  static bool IsScrollCommand(CommandEventType command) {
    return command == CommandEventType::kPageUp ||
           command == CommandEventType::kPageDown ||
           command == CommandEventType::kPageLeft ||
           command == CommandEventType::kPageRight ||
           command == CommandEventType::kPageBlockStart ||
           command == CommandEventType::kPageBlockEnd ||
           command == CommandEventType::kPageInlineStart ||
           command == CommandEventType::kPageInlineEnd;
  }

  static bool IsOverscrollCommand(CommandEventType command) {
    return command == CommandEventType::kToggleOverscroll;
  }

  // This allows customization of how Invoker Commands are handled, per element.
  // See: crbug.com/1490919, https://open-ui.org/components/invokers.explainer/
  virtual bool IsValidBuiltinCommand(HTMLElement& invoker,
                                     CommandEventType command) {
    return false;
  }
  virtual bool HandleCommandInternal(HTMLElement& invoker,
                                     CommandEventType command) {
    CHECK(command != CommandEventType::kCustom &&
          command != CommandEventType::kNone);

    // Handle scroll commands
    if (IsScrollCommand(command)) {
      return HandleScrollCommand(command);
    }

    return false;
  }

  // Helper method to handle scroll commands
  bool HandleScrollCommand(CommandEventType command);

  // These are slightly different than e.g. checking popover->popoverOpen(),
  // because they also catch the case where the element *was* open as a popover
  // or dialog, but is in the process of transitioning out of the top layer.
  bool IsPopoverInTopLayer();
  bool IsDialogInTopLayer();

  // If this element is a triggering element for an *open* popover, in one of
  // several ways, this returns the targeted popover. These forms of triggering
  // are supported:
  //   <button popovertarget=foo>
  //   <button command=*-popover commandfor=foo>
  //   <button interestfor=foo>
  //   (JS) popover.showPopover({source: foo})
  // Note: this function returns the *target* popover. Or nullptr if there isn't
  // a target, it isn't a popover, or the popover isn't open as the result of
  // this triggering element. (E.g. if the popover is just open on its own and
  // wasn't triggered by this invoker, this will return nullptr.)
  HTMLElement* GetOpenPopoverTarget() const;
  // Represents the current state of an interest invoker.
  enum class InterestState {
    // No interest.
    kNoInterest,
    // Invoker has full interest.
    kFullInterest,
  };

  enum class InterestLostCancelable {
    kNotCancelable,
    kCancelable,
  };
  enum class InterestLostPopoverBehavior {
    kDontClosePopovers,
    kClosePopovers,
  };

  // Implementation of the `interestfor` feature. These are called on the
  // element with the `interestfor` attribute, and not on the target itself.
  // These are called when interest is actually gained or lost on the element,
  // e.g. after any hover-delays. They return true if the event was *not*
  // cancelled, and the action was performed.
  bool InterestGained(Element* target);
  bool InterestLost(
      Element* target,
      InterestLostCancelable = InterestLostCancelable::kCancelable,
      InterestLostPopoverBehavior =
          InterestLostPopoverBehavior::kClosePopovers);

  // Returns the target of the `interestfor` attribute, if any, and only if
  // the element supports this attribute. For example, `interestfor` is not
  // allowed on a `<div>`.
  Element* InterestForElement() const;
  // Checks that the provided interest invoker relationship is valid. For this
  // call, `this` is the interest invoker (with the `interestfor` attribute),
  // and the provided `target` is the proposed target element.
  virtual bool IsValidInterestInvoker(Element& target) const { return false; }
  // Returns the active interest invoker for which this element is the target,
  // or nullptr otherwise.
  Element* SourceInterestInvoker() const;
  // Returns the current state of "interest" in an element that is an interest
  // invoker.
  InterestState GetInterestState();
  // Used in some situations (e.g. mobile device context menu activation) to
  // immediately show interest in an element, ignoring any show delays that may
  // be set on the element. If the element is not an interest invoker, nothing
  // happens. If the target of the interest invoker is a popover, the popover
  // will be shown.
  void ShowInterestNow();
  // Used in some situations (e.g. target popover closed via other means) to
  // immediately lose interest in an element, ignoring any hide delays that may
  // be set on the element. Element must already be an an interest invoker that
  // has interest, or a DCHECK will fail. If the target of the interest invoker
  // is a popover, the popover will be hidden.
  void LoseInterestNow(InterestLostCancelable, InterestLostPopoverBehavior);

  // Lose interest immediately in all elements that currently have interest.
  static void LoseInterestInAllElements(Document&);

  // Returns true if any of its (non-inclusive) flat tree descendants is
  // keyboard focusable. Note that this is quite slow, since it traverses the
  // entire subtree, and calls `IsKeyboardFocusableSlow()` on each element.
  // See the comment next to IsFocusable() above for a description of
  // update_behavior.
  bool ContainsKeyboardFocusableElementsSlow(
      UpdateBehavior update_behavior) const;

  // The implementations of |innerText()| and |GetInnerTextWithoutUpdate()| are
  // found in "element_inner_text.cc".
  // Avoids layout update.
  String GetInnerTextWithoutUpdate(TextVisitor* visitor = nullptr);
  // `visitor` is called as each node is considered. Note that this is not
  // called for nodes that are not considered in generating the text. For
  // example, all descendants of hidden nodes are not considered.
  String innerText(TextVisitor* visitor = nullptr);
  String outerText();

  Element* insertAdjacentElement(const String& where,
                                 Element* new_child,
                                 ExceptionState&);
  void insertAdjacentText(const String& where,
                          const String& text,
                          ExceptionState&);
  void InsertAdjacentHTMLWithoutTrustedTypes(const String& where,
                                             const String& html,
                                             ExceptionState&);
  void insertAdjacentHTML(const String& where,
                          const V8UnionStringOrTrustedHTML* html,
                          ExceptionState&);

  String GetInnerHTMLString() const;
  String GetOuterHTMLString() const;
  void SetInnerHTMLWithoutTrustedTypes(const String&,
                                       ExceptionState& = ASSERT_NO_EXCEPTION);
  void SetOuterHTMLWithoutTrustedTypes(const String&,
                                       ExceptionState& = ASSERT_NO_EXCEPTION);

  V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* innerHTML() const;
  V8UnionStringLegacyNullToEmptyStringOrTrustedHTML* outerHTML() const;
  void setInnerHTML(const V8UnionStringLegacyNullToEmptyStringOrTrustedHTML*,
                    ExceptionState&);
  void setOuterHTML(const V8UnionStringLegacyNullToEmptyStringOrTrustedHTML*,
                    ExceptionState&);

  // The setHTMLUnsafe method is like `setInnerHTML()` except that a) it parses
  // declarative shadow DOM by default, and b) will eventually have a second
  // argument to set Sanitizer parameters.
  // See https://github.com/whatwg/html/pull/9538.
  void SetHTMLUnsafeWithoutTrustedTypes(const String& html,
                                        ExceptionState& = ASSERT_NO_EXCEPTION);
  void setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html, ExceptionState&);
  void setHTMLUnsafe(const V8UnionStringOrTrustedHTML* html,
                     SetHTMLUnsafeOptions*,
                     ExceptionState&);
  void setHTML(const String& html, SetHTMLOptions*, ExceptionState&);

  void setPointerCapture(PointerId poinetr_id, ExceptionState&);
  void releasePointerCapture(PointerId pointer_id, ExceptionState&);

  // Returns true iff the element would capture the next pointer event. This
  // is true between a setPointerCapture call and a releasePointerCapture (or
  // implicit release) call:
  // https://w3c.github.io/pointerevents/#dom-element-haspointercapture
  bool hasPointerCapture(PointerId pointer_id) const;

  String TextFromChildren();

  virtual String title() const { return String(); }
  virtual String DefaultToolTip() const { return String(); }

  virtual const AtomicString& ShadowPseudoId() const;
  // The specified string must start with "-webkit-" or "-internal-". The
  // former can be used as a selector in any places, and the latter can be
  // used only in UA stylesheet.
  void SetShadowPseudoId(const AtomicString&);

  // Called by the parser when this element's close tag is reached, signaling
  // that all child tags have been parsed and added.  This is needed for
  // <applet> and <object> elements, which can't lay themselves out until they
  // know all of their nested <param>s. [Radar 3603191, 4040848].  Also used for
  // script elements and some SVG elements for similar purposes, but making
  // parsing a special case in this respect should be avoided if possible.
  virtual void FinishParsingChildren();

  void BeginParsingChildren() { SetIsFinishedParsingChildren(false); }

  // Returns the pseudo-element for the given PseudoId type.
  // |pseudo_argument| is used to uniquely identify a pseudo-element
  // from a set of pseudo-elements which share the same |pseudo_id|. The current
  // usage of this ID is limited to pseudo-elements generated for a
  // ViewTransition. See
  // third_party/blink/renderer/core/view_transition/README.md
  //
  // Also see GetStyledPseudoElement() below.
  PseudoElement* GetPseudoElement(
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom) const;
  LayoutObject* PseudoElementLayoutObject(PseudoId) const;
  CSSPseudoElement* pseudo(const AtomicString& type);

  // Used to cache CSSPseudoElement objects.
  CSSPseudoElement* EnsureCSSPseudoElement(PseudoId);
  void CacheCSSPseudoElement(PseudoId, CSSPseudoElement&);
  CSSPseudoElement* GetCSSPseudoElement(PseudoId) const;

  // Returns true if this element contains any ::scroll-button or
  // ::scroll-marker-group pseudos.
  bool HasScrollButtonOrMarkerGroupPseudos() const;

  bool PseudoElementStylesAffectCounters() const;

  bool PseudoElementStylesDependOnFontMetrics() const;
  bool PseudoElementStylesDependOnAttr() const;

  // Retrieve the ComputedStyle (if any) corresponding to the provided
  // PseudoId from cache, calculating the ComputedStyle on-demand if it's
  // missing from the cache. The |pseudo_argument| is also used to match the
  // ComputedStyle in cases where the PseudoId corresponds to a pseudo-element
  // that takes arguments (e.g. ::highlight()).
  const ComputedStyle* CachedStyleForPseudoElement(
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom);

  // Calculate the ComputedStyle corresponding to the provided StyleRequest,
  // bypassing the pseudo style cache.
  //
  // This is appropriate to use if the cached version is invalid in a given
  // situation.
  const ComputedStyle* UncachedStyleForPseudoElement(const StyleRequest&);

  // This is the same as UncachedStyleForPseudoElement, except that the caller
  // must provide an appropriate StyleRecalcContext such that e.g. @container
  // queries are evaluated correctly.
  //
  // See StyleRecalcContext for more information.
  const ComputedStyle* StyleForPseudoElement(const StyleRecalcContext&,
                                             const StyleRequest&);

  // These are used by ResolveStyle with Highlight Inheritance when caching
  // is not used.
  const ComputedStyle* StyleForHighlightPseudoElement(
      const StyleRecalcContext& style_recalc_context,
      const ComputedStyle* highlight_parent,
      const ComputedStyle& originating_style,
      const PseudoId pseudo_id,
      const AtomicString& pseudo_argument = g_null_atom);
  const ComputedStyle* StyleForSearchTextPseudoElement(
      const StyleRecalcContext& style_recalc_context,
      const ComputedStyle* highlight_parent,
      const ComputedStyle& originating_style,
      StyleRequest::SearchTextRequest search_text_request);

  virtual bool CanGeneratePseudoElement(PseudoId) const;

  virtual bool MatchesDefaultPseudoClass() const { return false; }
  virtual bool MatchesEnabledPseudoClass() const { return false; }
  virtual bool MatchesReadOnlyPseudoClass() const { return false; }
  virtual bool MatchesReadWritePseudoClass() const { return false; }
  virtual bool MatchesValidityPseudoClasses() const { return false; }

  virtual bool MayTriggerVirtualKeyboard() const;

  // https://dom.spec.whatwg.org/#dom-element-matches
  bool matches(const AtomicString& selectors, ExceptionState&);
  bool matches(const AtomicString& selectors);

  // https://dom.spec.whatwg.org/#dom-element-closest
  Element* closest(const AtomicString& selectors, ExceptionState&);
  Element* closest(const AtomicString& selectors);

  virtual bool ShouldAppearIndeterminate() const { return false; }

  DOMTokenList& classList();

  DOMStringMap& dataset();

  virtual bool IsDateTimeEditElement() const { return false; }
  virtual bool IsDateTimeFieldElement() const { return false; }
  virtual bool IsPickerIndicatorElement() const { return false; }

  virtual bool IsFormControlElement() const { return false; }
  virtual bool IsFormControlElementWithState() const { return false; }
  virtual bool IsSpinButtonElement() const { return false; }
  // This returns true for <textarea> and some types of <input>.
  virtual bool IsTextControl() const { return false; }
  virtual bool IsOptionalFormControl() const { return false; }
  virtual bool IsRequiredFormControl() const { return false; }
  virtual bool willValidate() const { return false; }
  virtual bool IsValidElement() { return false; }
  virtual bool IsInRange() const { return false; }
  virtual bool IsOutOfRange() const { return false; }
  virtual bool IsClearButtonElement() const { return false; }
  virtual bool IsScriptElement() const { return false; }
  virtual bool IsVTTCueBackgroundBox() const { return false; }
  virtual bool IsVTTCueBox() const { return false; }
  virtual bool IsSliderThumbElement() const { return false; }
  virtual bool IsOutputElement() const { return false; }

  // Elements that may have an insertion mode other than "in body" should
  // override this and return true.
  // https://html.spec.whatwg.org/C/#reset-the-insertion-mode-appropriately
  virtual bool HasNonInBodyInsertionMode() const { return false; }

  bool CanContainRangeEndPoint() const override { return true; }

  // Used for disabled form elements; if true, prevents mouse events from being
  // dispatched to event listeners, and prevents DOMActivate events from being
  // sent at all.
  virtual bool IsDisabledFormControl() const { return false; }

  void SetCustomElementDefinition(CustomElementDefinition*);
  CustomElementDefinition* GetCustomElementDefinition() const;

  // Scoped Custom Elements
  CustomElementRegistry* customElementRegistry() const;
  // When it comes to storing an element's custom element registry, we have an
  // optimization where if the registry to be set is the same as element's tree
  // scope's registry, we don't store it in the element itself and rely on tree
  // scope to find the registry to save memory. In the scenario of cross scope
  // adoption, we can set explicitly_set to true to force the registry storage
  // so we can retain knowledge of the prior registry even when the scope is
  // changed.
  void SetCustomElementRegistry(CustomElementRegistry*,
                                bool explicitly_set = false);

  // https://dom.spec.whatwg.org/#concept-element-is-value
  void SetIsValue(const AtomicString&);
  const AtomicString& IsValue() const;
  void SetDidAttachInternals();
  bool DidAttachInternals() const;
  ElementInternals& EnsureElementInternals();
  const ElementInternals* GetElementInternals() const;

  bool ContainsFullScreenElement() const {
    return HasElementFlag(ElementFlags::kContainsFullScreenElement);
  }
  void SetContainsFullScreenElement(bool);
  void SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);

  bool ContainsPersistentVideo() const {
    return HasElementFlag(ElementFlags::kContainsPersistentVideo);
  }
  void SetContainsPersistentVideo(bool);

  bool IsInTopLayer() const {
    return HasElementFlag(ElementFlags::kIsInTopLayer);
  }
  void SetIsInTopLayer(bool);

  ScriptPromise<IDLUndefined> requestPointerLock(
      ScriptState* script_state,
      const PointerLockOptions* options,
      ExceptionState& exception_state);

  bool IsSpellCheckingEnabled() const;

  // FIXME: public for LayoutTreeBuilder, we shouldn't expose this though.
  const ComputedStyle* StyleForLayoutObject(const StyleRecalcContext&);

  // Called by StyleAdjuster during style resolution. Provides an opportunity to
  // make final Element-specific adjustments to the ComputedStyle.
  void AdjustStyle(base::PassKey<StyleAdjuster>, ComputedStyleBuilder&);

  bool HasID() const;
  bool HasClass() const;
  const SpaceSplitString& ClassNames() const;
  bool HasClassName(const AtomicString& class_name) const;

  // Returns true if the element has 1 or more part names.
  bool HasPart() const;
  // Returns the list of part names if it has ever been created.
  DOMTokenList* GetPart() const;
  // IDL method.
  // Returns the list of part names, creating it if it doesn't exist.
  DOMTokenList& part();

  bool HasPartNamesMap() const;
  const NamesMap* PartNamesMap() const;

  ScrollOffset SavedLayerScrollOffset() const;
  void SetSavedLayerScrollOffset(const ScrollOffset&);

  ElementAnimations* GetElementAnimations() const;
  ElementAnimations& EnsureElementAnimations();
  bool HasAnimations() const;

  void SynchronizeAttribute(const AtomicString& local_name) const {
    SynchronizeAttributeHinted(local_name,
                               WeakLowercaseIfNecessary(local_name));
  }

  MutableCSSPropertyValueSet& EnsureMutableInlineStyle();
  void ClearMutableInlineStyleIfEmpty();

  CSSPropertyValueSet* CreatePresentationAttributeStyle();

  void setTabIndex(int);
  int tabIndex() const;
  // If element is a reading flow container or display: contents whose layout
  // parent is one, return the nodes corresponding to its direct children
  // sorted in reading flow order.
  const HeapVector<Member<Node>> ReadingFlowChildren() const;

  void setHeadingOffset(int);
  int headingOffset() const;
  void setHeadingReset(bool);
  bool headingReset() const;
  int GetComputedHeadingOffset(int max_offset);

  void setEditContext(EditContext* editContext, ExceptionState&);
  EditContext* editContext() const;

  // Helpers for V8DOMActivityLogger::logEvent.  They call logEvent only if
  // the element is isConnected() and the context is an isolated world.
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1);
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1,
                                                 const QualifiedName& attr2);
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1,
                                                 const QualifiedName& attr2,
                                                 const QualifiedName& attr3);
  void LogUpdateAttributeIfIsolatedWorldAndInDocument(
      const char element[],
      const AttributeModificationParams&);

  void Trace(Visitor*) const override;

  SpellcheckAttributeState GetSpellcheckAttributeState() const;

  ElementIntersectionObserverData* IntersectionObserverData() const;
  ElementIntersectionObserverData& EnsureIntersectionObserverData();

  HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>*
  ResizeObserverData() const;
  HeapHashMap<Member<ResizeObserver>, Member<ResizeObservation>>&
  EnsureResizeObserverData();

  DisplayLockContext* GetDisplayLockContext() const {
    if (!HasDisplayLockContext()) [[likely]] {
      return nullptr;
    }
    return GetDisplayLockContextFromRareData();
  }
  DisplayLockContext& EnsureDisplayLockContext();

  bool ChildStyleRecalcBlockedByDisplayLock() const;

  // Activates all activatable (for a given reason) locked ancestors for this
  // element. Return true if we activated at least one previously locked
  // element.
  bool ActivateDisplayLockIfNeeded(DisplayLockActivationReason reason);

  ContainerQueryData* GetContainerQueryData() const;
  ContainerQueryEvaluator* GetContainerQueryEvaluator() const;
  ContainerQueryEvaluator& EnsureContainerQueryEvaluator();
  bool SkippedContainerStyleRecalc() const;

  StyleScopeData& EnsureStyleScopeData();
  StyleScopeData* GetStyleScopeData() const;

  OutOfFlowData& EnsureOutOfFlowData();
  OutOfFlowData* GetOutOfFlowData() const;
  bool SetPendingRememberedScrollOffsets(
      const OutOfFlowData::RememberedScrollOffsets*);

  // See PostStyleUpdateScope::PseudoData::AddPendingBackdrop
  void ApplyPendingBackdropPseudoElementUpdate();

  virtual void SetActive(bool active);
  virtual void SetHovered(bool hovered);

  // Manages the element's ad-related status.
  //
  // NOTE: `HTMLFrameOwnerElement` manages its ad status separately by
  // deriving it from its frame. It overrides these virtual methods, and
  // `SetIsAdRelated()` should not be called on it directly.

  // Marks this element as being ad-related.
  void SetIsAdRelated();

  // Returns true if the element is considered ad-related.
  virtual bool IsAdRelated() const;

  // Returns true if a paint-time ad highlight should be drawn.
  // This is the authoritative check for painters, encapsulating:
  // 1. The element's ad status (i.e., `IsAdRelated()`).
  // 2. The "Highlight ads" DevTools setting.
  // 3. Logic to exclude nested ads (e.g., in an ad iframe) to avoid redundant,
  // overlapping highlights.
  virtual bool ShouldHighlightAd() const;

  void NotifyInlineStyleMutation();

  // For undo stack cleanup
  bool HasUndoStack() const;
  void SetHasUndoStack(bool);

  // For font-related style invalidation.
  void SetScrollbarPseudoElementStylesDependOnFontMetrics(bool);

  void SetPseudoElementStylesChangeCounters(bool value);

  // Get (or create if it doesn't already exist) a per-column (fragmentainer)
  // ::column pseudo-element for the given column index.
  ///
  // Also, if ::column::scroll-marker is specified, create one ::scroll-marker
  // per ::column pseudo-element, if needed, and if it doesn't already exist.
  ColumnPseudoElement* GetOrCreateColumnPseudoElementIfNeeded(
      wtf_size_t index,
      const PhysicalRect& column_rect);
  const ColumnPseudoElementsVector* GetColumnPseudoElements() const;
  const OverscrollAreaParentPseudoElementsVector*
  GetOverscrollAreaParentPseudoElements() const;

  // Clear all ::column pseudo-elements, except for the leading `to_keep` ones.
  void ClearColumnPseudoElements(wtf_size_t to_keep = 0);

  // True if a scroller has not been explicitly scrolled by a user or by a
  // programmatic scroll. Indicates that we should use the CSS scroll-start
  // property.
  bool HasBeenExplicitlyScrolled() const;
  void SetHasBeenExplicitlyScrolled();

  void SetAffectedByStartingStyles();
  bool AffectedByStartingStyles() const;

  bool AffectedBySubjectHas() const;
  void SetAffectedBySubjectHas();
  bool AffectedByNonSubjectHas() const;
  void SetAffectedByNonSubjectHas();
  bool AncestorsOrAncestorSiblingsAffectedByHas() const;
  void SetAncestorsOrAncestorSiblingsAffectedByHas();
  unsigned GetSiblingsAffectedByHasFlags() const;
  bool HasSiblingsAffectedByHasFlags(unsigned flags) const;
  void SetSiblingsAffectedByHasFlags(unsigned flags);
  bool AffectedByPseudoInHas() const;
  void SetAffectedByPseudoInHas();
  bool AncestorsOrSiblingsAffectedByHoverInHas() const;
  void SetAncestorsOrSiblingsAffectedByHoverInHas();
  bool AncestorsOrSiblingsAffectedByActiveInHas() const;
  void SetAncestorsOrSiblingsAffectedByActiveInHas();
  bool AncestorsOrSiblingsAffectedByFocusInHas() const;
  void SetAncestorsOrSiblingsAffectedByFocusInHas();
  bool AncestorsOrSiblingsAffectedByFocusVisibleInHas() const;
  void SetAncestorsOrSiblingsAffectedByFocusVisibleInHas();
  bool AffectedByLogicalCombinationsInHas() const;
  void SetAffectedByLogicalCombinationsInHas();
  bool AffectedByMultipleHas() const;
  void SetAffectedByMultipleHas();

  // This is meant to be used by document's resize observer to notify that the
  // size has changed.
  void LastRememberedSizeChanged(ResizeObserverSize* size);

  void SetLastRememberedInlineSize(std::optional<LayoutUnit>);
  void SetLastRememberedBlockSize(std::optional<LayoutUnit>);
  std::optional<LayoutUnit> LastRememberedInlineSize() const;
  std::optional<LayoutUnit> LastRememberedBlockSize() const;

  // Returns the element that represents the given |pseudo_id| and
  // |pseudo_argument| originating from this DOM element.  The
  // returned element may be a PseudoElement, or (for element-backed
  // pseudo-elements) an Element.
  //
  // The returned pseudo-element may be directly associated with this
  // element or (as with view transition pseudo-elements) nested inside
  // a hierarchy of pseudo-elements.
  //
  // Callers that need to deal with all CSS pseudo-elements should use
  // this rather than GetPseudoElement().
  Element* GetStyledPseudoElement(PseudoId pseudo_id,
                                  const AtomicString& pseudo_argument) const;

  // Performs an update of the overscroll pseudo-elements.
  void UpdateOverscrollPseudoElements(const StyleRecalcChange,
                                      const StyleRecalcContext&);

  // Performs an update of the view-transition pseudo-elements.
  void UpdateTransitionPseudoElements(const StyleRecalcChange,
                                      const StyleRecalcContext&);

  // Returns true if the element has the 'inert' attribute, forcing itself and
  // all its subtree to be inert.
  // TODO(crbug.com/370065759): This API is only used in HasEditableLevel().
  virtual bool IsInertRoot() const;

  FocusgroupData GetFocusgroupData() const;
  Element* GetFocusgroupLastFocused() const;
  // May only be called on a focusgroup that supports restoring the last focused
  // element.
  void SetFocusgroupLastFocused(Element& element);
  void ClearFocusgroupLastFocused();

  bool checkVisibility(CheckVisibilityOptions* options) const;

  bool IsDocumentElement() const;

  bool IsReplacedElementRespectingCSSOverflow() const;

  void RemovePopoverData();
  PopoverData& EnsurePopoverData();
  PopoverData* GetPopoverData() const;

  // Alt content data is used by pseudo-elements to store a mutable copy
  // of content data when it contains counter() or counters() in alt text.
  ContentData* GetAltContentData() const;
  void SetAltContentData(ContentData*);

  InvokerData& EnsureInvokerData();
  InvokerData* GetInvokerData() const;
  void ChangeInterestState(Element* target, InterestState new_state);

  void RemoveInterestInvokerTargetData();
  InterestInvokerTargetData& EnsureInterestInvokerTargetData();
  InterestInvokerTargetData* GetInterestInvokerTargetData() const;
  void HandlePointerEventsForInterestFor(const AtomicString& event_type);

  void DefaultEventHandler(Event&) override;

  // Set on elements with scroll-target-group property to
  // collect HTMLAnchorElement scroll markers.
  ScrollMarkerGroupData& EnsureScrollTargetGroupData();
  void RemoveScrollTargetGroupData();
  ScrollMarkerGroupData* GetScrollTargetGroupData() const;

  // Used for HTMLAnchorElement scroll markers to point to
  // its scroll marker group container (element with scroll-target-group).
  void SetScrollTargetGroupContainerData(ScrollMarkerGroupData*);
  ScrollMarkerGroupData* GetScrollTargetGroupContainerData() const;

  // Retrieves the element pointed to by this element's 'anchor' content
  // attribute, if that element exists.
  // TODO(crbug.com/40059176) If the HTMLAnchorAttribute feature is disabled,
  // this will return nullptr;
  Element* anchorElement() const;
  Element* anchorElementForBinding() const;
  void setAnchorElementForBinding(Element*);

  AnchorPositionScrollData& EnsureAnchorPositionScrollData();
  void RemoveAnchorPositionScrollData();
  AnchorPositionScrollData* GetAnchorPositionScrollData() const;

  // Returns true if any element may be implicitly anchored to this element.
  // This flag is sticky once set. An element may be an implicit anchor for
  // multiple elements, and all elements may be the implicit anchor for any
  // pseudo element if the pseudo element is using anchor positioning. Since
  // using anchor positioning depends on style, it would be tricky to keep
  // track of when an element is no longer an implicit anchor, hence once we
  // start considering an element as a potential implicit anchor, it will stay
  // so.
  bool MayBeImplicitAnchor() const;
  void SetMayBeImplicitAnchor();

  bool HasAnchorElementObserverForTesting() const {
    return GetAnchorElementObserver();
  }

  // https://drafts.csswg.org/css-anchor-1/#implicit-anchor-element
  Element* ImplicitAnchorElement() const;

  void UpdateDirectionalityAndDescendant(TextDirection direction);
  void UpdateDescendantHasDirAutoAttribute(bool has_dir_auto);
  enum class UpdateAncestorTraversal {
    IncludeSelf,  // self and ancestors
    ExcludeSelf,  // ancestors, but not self
  };
  void UpdateAncestorWithDirAuto(UpdateAncestorTraversal traversal);
  void AdjustDirectionalityIfNeededAfterChildrenChanged(
      const ChildrenChange& change);

  void UpdateDescendantHasContainerTiming(bool has_container_timing);
  void AdjustContainerTimingIfNeededAfterChildrenChanged(
      const ChildrenChange& change);
  bool ShouldAdjustContainerTimingForInsert(const ChildrenChange& change) const;
  bool DoesChildContainerTimingNeedChange(const Node& node) const;

  bool RecalcSelfOrAncestorHasContainerTiming() const;

  // The "nonce" attribute is hidden when:
  // 1) The Content-Security-Policy is delivered from the HTTP headers.
  // 2) The Element is part of the active document.
  // See https://github.com/whatwg/html/pull/2373
  //
  // This applies to the element of the HTML and SVG namespaces.
  //
  // This function clears the "nonce" attribute whenever conditions (1) and (2)
  // are met.
  void HideNonce();

  // These update every scroll container that is an ancestor of
  // of this element, letting them know which snap area of theirs, if any,
  // either is a targeted[1] element or contains a targeted[1] element.
  // [1]https://drafts.csswg.org/selectors/#the-target-pseudo
  void SetTargetedSnapAreaIdsForSnapContainers();
  void ClearTargetedSnapAreaIdsForSnapContainers();

  // Subclasses can override this method to specify a CascadeFilter to
  // filter out any unwanted CSS properties.
  virtual CascadeFilter GetCascadeFilter() const { return CascadeFilter(); }

  GCedHeapVector<Member<Element>>* ElementsFromAttributeOrInternals(
      const QualifiedName& attribute) const;

  bool IsClickableFormControlNode() const;

  bool HasTabIndexWasSetExplicitly() const;

  void SetNamedTriggers(NamedAnimationTriggerMap&& named_triggers);
  NamedAnimationTriggerMap* NamedTriggers() const;
  AnimationTrigger* NamedTrigger(const ScopedCSSName* name) const;

  enum AttributesToExcludeHashesFor {
    // Exclude [id], [style] and [class], which are the attributes
    // ignored by SelectorFilter by default.
    kExcludeStandardAttributesOnly,

    // Exclude any attribute that may be lazily synchronized and thus
    // not show up in Element's Bloom filter (in particular, its subtree).
    // Note that this may be overly conservative (the set required for SVG
    // is rather large), but it should at least be safe.
    kExcludeAllLazilySynchronizedAttributes,

    // Same, but case-sensitive (used for non-HTML documents); this means
    // that e.g. STYLE="" will _not_ be ignored.
    kExcludeLowercaseLazilySynchronizedAttributes,
  };
  static bool IsExcludedAttribute(
      const QualifiedName& qname,
      AttributesToExcludeHashesFor attributes_to_exclude);

  // IsAppearanceBase returns true if the appearance value from GetComputedStyle
  // returns true when given to SupportsBaseAppearance.
  bool IsAppearanceBase() const;
  enum class BaseAppearanceValue { kBaseSelect, kBase };
  // Returns true if this element supports base appearance given a value for the
  // appearance property, such as `base` or `base-select`.
  bool SupportsBaseAppearance(AppearanceValue) const;

  OverscrollAreaTracker& EnsureOverscrollAreaTracker();
  OverscrollAreaTracker* OverscrollAreaTracker() const;

  Element* OverscrollContainer() const;
  void SetOverscrollContainer(Element*);
  void ClearOverscrollContainer();

 protected:
  bool HasElementData() const { return static_cast<bool>(element_data_); }
  const ElementData* GetElementData() const { return element_data_.Get(); }
  UniqueElementData& EnsureUniqueElementData();

  bool IsViewportScrollElement();

  void AddPropertyToPresentationAttributeStyle(HeapVector<CSSPropertyValue, 8>&,
                                               CSSPropertyID,
                                               CSSValueID identifier);
  void AddPropertyToPresentationAttributeStyle(HeapVector<CSSPropertyValue, 8>&,
                                               CSSPropertyID,
                                               double value,
                                               CSSPrimitiveValue::UnitType);
  void AddPropertyToPresentationAttributeStyle(HeapVector<CSSPropertyValue, 8>&,
                                               CSSPropertyID,
                                               const String& value);
  void AddPropertyToPresentationAttributeStyle(HeapVector<CSSPropertyValue, 8>&,
                                               CSSPropertyID,
                                               const CSSValue&);
  void MapLanguageAttributeToLocale(const AtomicString&,
                                    HeapVector<CSSPropertyValue, 8>&);

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void ChildrenChanged(const ChildrenChange&) override;

  // This is an implementation of
  // https://whatpr.org/html/10657/infrastructure.html#html-element-moving-steps
  void MovedFrom(ContainerNode& old_parent) override;

  virtual void WillRecalcStyle(const StyleRecalcChange);
  virtual void DidRecalcStyle(const StyleRecalcChange);
  virtual const ComputedStyle* CustomStyleForLayoutObject(
      const StyleRecalcContext&);
  virtual void AdjustStyle(ComputedStyleBuilder&);

  virtual NamedItemType GetNamedItemType() const {
    return NamedItemType::kNone;
  }

  // See description of SupportsFocus and IsFocusableState above, near
  // IsFocusable(). These two methods should stay protected. Use IsFocusable()
  // and friends.
  virtual FocusableState SupportsFocus(UpdateBehavior update_behavior) const;
  virtual FocusableState IsFocusableState(UpdateBehavior update_behavior) const;

  bool SupportsSpatialNavigationFocus() const;

  void ClearTabIndexExplicitlyIfNeeded();
  void SetTabIndexExplicitly();
  // Returns false if the style prevents focus. Returning true doesn't imply
  // focusability, there may be other conditions like SupportsFocus().
  // Subclasses may override this method to affect focusability. This method
  // might update layout/style, as it may use existence of layoutObject and the
  // LayoutObject::style() to reason about focusability.
  // However, it must not retrieve layout information like position and size.
  // This method cannot be moved to LayoutObject because some focusable nodes
  // don't have layoutObjects. e.g., HTMLOptionElement.
  // If UpdateBehavior::kNoneForAccessibility argument is passed, which should
  // only be used by a11y code, layout updates will never be performed.
  virtual bool IsFocusableStyle(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;
  // Is the node descendant of this in something clickable/activatable, such
  // that we shouldn't handle events targeting it?
  bool IsClickableControl(Node*);

  // ClassAttributeChanged() and UpdateClassList() exist to share code between
  // ParseAttribute (called via setAttribute()) and SvgAttributeChanged (called
  // when element.className.baseVal is set or when the 'class' attribute is
  // animated by SMIL).
  void ClassAttributeChanged(const AtomicString& new_class_string);
  void UpdateClassList(const AtomicString& old_class_string,
                       const AtomicString& new_class_string);

  // Update parents' subtree Bloom filters recursively. Must be called after
  // anything that could add bits, or when this element is attached to a new
  // parent, so that the tree is consistent.
  void UpdateSubtreeBloomFilterAfterInsert();

  // Update this element's subtree Bloom filter after removing a child.
  // Unlike UpdateSubtreeBloomFilterAfterInsert, this is called on the
  // _parent_ of the element that's being removed or changed. It is also
  // voluntary; it is generally hard to remove bits from a Bloom filter,
  // so we only do updates for some special cases (such as the entire
  // subtree under an element going away).
  void UpdateSubtreeBloomFilterAfterChildRemoval();

  // Recompute the desired value of the subtree Bloom filter, given only
  // this element's attributes and classes (not including children).
  TinyBloomFilter RecomputeLocalBloomFilter() const;

  static bool AttributeValueIsJavaScriptURL(const Attribute&);

  const ComputedStyle* OriginalStyleForLayoutObject(const StyleRecalcContext&);

  // Step 4 of http://domparsing.spec.whatwg.org/#insertadjacenthtml()
  Node* InsertAdjacent(const String& where, Node* new_child, ExceptionState&);

  virtual void ParserDidSetAttributes() {}

  // Mark for style invalidation/recalc for :lang() selectors to pick up the
  // changes.
  virtual void LangAttributeChanged();

  TextDirection ParentDirectionality() const;
  bool RecalcSelfOrAncestorHasDirAuto();
  std::optional<TextDirection> ResolveAutoDirectionality() const;

  void AttachPseudoElement(PseudoId, AttachContext&);
  void DetachPseudoElement(PseudoId, bool performing_reattach);

  void ProcessElementRenderBlocking(const AtomicString& id_or_name);

  virtual bool SupportsBaseAppearanceInternal(BaseAppearanceValue) const {
    return false;
  }

 private:
  friend class AXObject;
  friend class KeyboardEventManager;
  struct AffectedByPseudoStateChange;

  ShadowRoot* GetShadowRootInternal() const;

  template <typename Functor>
  bool PseudoElementStylesDependOnFunc(Functor& func) const;

  // Returns true if the element satisfies conditions for focusability for
  // spatial navigation, even if the spatial navigation is not currently
  // enabled.
  bool HasSpatialNavigationFocusHeuristics() const;

  // Returns true if this element has generate a pseudo-element whose box is a
  // sibling box of its originating element's box. In this case we cannot skip
  // style recalc for size containers because that would break necessary layout
  // containment by modifying the box tree outside the container during layout.
  bool HasSiblingBoxPseudoElements() const;

  bool ScrollLayoutBoxBy(const ScrollToOptions*);
  bool ScrollLayoutBoxTo(const ScrollToOptions*);
  bool ScrollFrameBy(const ScrollToOptions*);
  bool ScrollFrameTo(const ScrollToOptions*);

  bool HasElementFlag(ElementFlags mask) const;
  void SetElementFlag(ElementFlags, bool value = true);
  void ClearElementFlag(ElementFlags);

  void ClearPseudoElement(PseudoId,
                          const AtomicString& pseudo_argument = g_null_atom);

  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  bool CanAttachShadowRoot() const;
  const char* ErrorMessageForAttachShadow(String mode_string,
                                          bool for_declarative,
                                          ShadowRootMode& mode_out) const;

  void StyleAttributeChanged(const AtomicString& new_style_string,
                             AttributeModificationReason);

  void UpdatePresentationAttributeStyle();

  void InlineStyleChanged();
  void SetInlineStyleFromString(const AtomicString&);

  void NotifyAXOfAttachedSubtree();

  AnchorElementObserver& EnsureAnchorElementObserver();
  AnchorElementObserver* GetAnchorElementObserver() const;

  // If the only inherited changes in the parent element are independent,
  // these changes can be directly propagated to this element (the child).
  // If these conditions are met, propagates the changes to the current style
  // and returns the new style. Otherwise, returns null.
  const ComputedStyle* PropagateInheritedProperties();

  const ComputedStyle* EnsureOwnComputedStyle(
      const StyleRecalcContext&,
      PseudoId,
      const AtomicString& pseudo_argument = g_null_atom);

  enum class HighlightRecalc {
    // No highlight recalc is needed.
    kNone,
    // The HighlightData from the old style can be reused.
    kReuse,
    // The HighlightData contains relative units and may need recalc.
    kOriginatingDependent,
    // Highlights must be calculated in full.
    kFull,
  };

  // Determine whether pseudo highlight style must be recalculated,
  // either because full recalc is required or the parent has relative
  // units and the parent's relative units source differs from the
  // originating element (font size, container or writing mode).
  bool ShouldRecalcHighlightPseudoStyle(
      HighlightRecalc highlight_recalc,
      const ComputedStyle* highlight_parent,
      const ComputedStyle& originating_style,
      const Element* originating_container) const;

  // Recalc those custom highlights that require it.
  void RecalcCustomHighlightPseudoStyle(const StyleRecalcContext&,
                                        HighlightRecalc,
                                        ComputedStyleBuilder&,
                                        const StyleHighlightData*,
                                        const ComputedStyle&);

  // Recalculate the ComputedStyle for this element and return a
  // StyleRecalcChange for propagation/traversal into child nodes.
  StyleRecalcChange RecalcOwnStyle(const StyleRecalcChange,
                                   const StyleRecalcContext&);

  // Returns true if we should skip style recalc for the subtree because this
  // element is a container for size container queries and we are guaranteed to
  // reach this element during the subsequent layout to continue doing
  // interleaved style and layout.
  bool SkipStyleRecalcForContainer(const ComputedStyle& style,
                                   const StyleRecalcChange& child_change,
                                   const StyleRecalcContext&);

  void MarkNonSlottedHostChildrenForStyleRecalc();

  void RebuildPseudoElementLayoutTree(PseudoId, WhitespaceAttacher&);
  void RebuildColumnLayoutTrees(WhitespaceAttacher&);
  void RebuildFirstLetterLayoutTree();
  void RebuildTransitionLayoutTree(WhitespaceAttacher&);
  void RebuildShadowRootLayoutTree(WhitespaceAttacher&);
  inline void CheckForEmptyStyleChange(const Node* node_before_change,
                                       const Node* node_after_change);

  void UpdateColumnPseudoElements(const StyleRecalcChange,
                                  const StyleRecalcContext&);
  PseudoElement* UpdatePseudoElement(
      PseudoId,
      const StyleRecalcChange,
      const StyleRecalcContext&,
      const AtomicString& pseudo_argument = g_null_atom);
  enum class StyleUpdatePhase {
    kRecalc,
    kRebuildLayoutTree,
    kAttachLayoutTree,
  };

  bool ShouldUpdateBackdropPseudoElement(const StyleRecalcChange);

  void UpdateBackdropPseudoElement(const StyleRecalcChange,
                                   const StyleRecalcContext&);

  void UpdateFirstLetterPseudoElement(StyleUpdatePhase,
                                      const StyleRecalcContext&);

  // Creates a StyleRecalcContext and invokes the method above. Only use this
  // when there is no StyleRecalcContext available.
  void UpdateFirstLetterPseudoElement(StyleUpdatePhase);

  ALWAYS_INLINE PseudoElement* CreatePseudoElementIfNeeded(
      PseudoId,
      const StyleRecalcContext&,
      const AtomicString& pseudo_argument = g_null_atom);

  ALWAYS_INLINE bool SetAssociatedPseudoElement(PseudoElement* pseudo_element,
                                                const StyleRecalcContext&);

  // For document element scroll control pseudo-elements become not layout
  // siblings, but layout children.
  void AttachDocumentElementPrecedingPseudoElements(AttachContext& context) {
    if (!IsDocumentElement()) {
      return;
    }
    AttachPrecedingScrollControlsPseudoElements(context);
  }

  void AttachLayoutPrecedingPseudoElements(AttachContext& context) {
    if (IsDocumentElement()) {
      return;
    }
    AttachPrecedingScrollControlsPseudoElements(context);
  }

  void AttachPrecedingScrollControlsPseudoElements(AttachContext& context) {
    AttachPseudoElement(kPseudoIdScrollMarkerGroupBefore, context);
  }

  void AttachPrecedingPseudoElements(AttachContext& context) {
    AttachDocumentElementPrecedingPseudoElements(context);
    AttachOverscrollPseudoElements(context);
    AttachPseudoElement(kPseudoIdScrollMarker, context);
    AttachPseudoElement(kPseudoIdMarker, context);
    AttachPseudoElement(kPseudoIdCheckMark, context);
    AttachPseudoElement(kPseudoIdBefore, context);
  }

  // For document element scroll control pseudo-elements become not layout
  // siblings, but layout children.
  void AttachDocumentElementSucceedingPseudoElements(AttachContext& context) {
    if (!IsDocumentElement()) {
      return;
    }
    AttachSucceedingScrollControlsPseudoElements(context);
  }

  void AttachLayoutSucceedingPseudoElements(AttachContext& context) {
    if (IsDocumentElement()) {
      return;
    }
    AttachSucceedingScrollControlsPseudoElements(context);
  }

  void AttachSucceedingPseudoElements(AttachContext& context) {
    AttachPseudoElement(kPseudoIdInterestHint, context);
    AttachPseudoElement(kPseudoIdPickerIcon, context);
    AttachPseudoElement(kPseudoIdAfter, context);
    AttachDocumentElementSucceedingPseudoElements(context);
    AttachPseudoElement(kPseudoIdBackdrop, context);
    UpdateFirstLetterPseudoElement(StyleUpdatePhase::kAttachLayoutTree);
    AttachPseudoElement(kPseudoIdFirstLetter, context);
  }

  void AttachSucceedingScrollControlsPseudoElements(AttachContext& context) {
    // The order for buttons is described in
    // https://drafts.csswg.org/css-overflow-5/#scroll-buttons.
    AttachPseudoElement(kPseudoIdScrollButtonBlockStart, context);
    AttachPseudoElement(kPseudoIdScrollButtonInlineStart, context);
    AttachPseudoElement(kPseudoIdScrollButtonInlineEnd, context);
    AttachPseudoElement(kPseudoIdScrollButtonBlockEnd, context);
    AttachPseudoElement(kPseudoIdScrollMarkerGroupAfter, context);
  }

  // These pseudo-elements are added as siblings of the contents of this
  // element's layout children.
  void AttachOverscrollPseudoElements(AttachContext& context);

  void AttachColumnPseudoElements(AttachContext& context);
  void AttachTransitionPseudoElements(AttachContext& context);

  void DetachPrecedingPseudoElements(bool performing_reattach) {
    DetachPseudoElement(kPseudoIdScrollMarker, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollMarkerGroupBefore, performing_reattach);
    DetachPseudoElement(kPseudoIdMarker, performing_reattach);
    DetachPseudoElement(kPseudoIdCheckMark, performing_reattach);
    DetachPseudoElement(kPseudoIdBefore, performing_reattach);
  }

  void DetachSucceedingPseudoElements(bool performing_reattach) {
    DetachPseudoElement(kPseudoIdInterestHint, performing_reattach);
    DetachPseudoElement(kPseudoIdPickerIcon, performing_reattach);
    DetachPseudoElement(kPseudoIdAfter, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollButtonBlockStart, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollButtonInlineStart, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollButtonInlineEnd, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollButtonBlockEnd, performing_reattach);
    DetachPseudoElement(kPseudoIdScrollMarkerGroupAfter, performing_reattach);
    DetachPseudoElement(kPseudoIdBackdrop, performing_reattach);
    DetachPseudoElement(kPseudoIdFirstLetter, performing_reattach);
  }

  void DetachColumnPseudoElements(bool performing_reattach);
  void DetachTransitionPseudoElements(bool performing_reattach);

  void RecomputeDirectionFromParent();

  // Returns true if the directionality needs to be updated for insert.
  bool ShouldAdjustDirectionalityForInsert(const ChildrenChange& change) const;

  // Returns true if node is a text node and the direction is not set, or
  // matches this. Generally only useful from
  // ShouldAdjustDirectionalityForInsert().
  bool DoesChildTextNodesDirectionMatchThis(const Node& node) const;

  ShadowRoot& CreateAndAttachShadowRoot(
      ShadowRootMode,
      SlotAssignmentMode = SlotAssignmentMode::kNamed);

  virtual void DidAddUserAgentShadowRoot(ShadowRoot&) {}
  virtual bool AlwaysCreateUserAgentShadowRoot() const { return false; }

  void DidAddAttribute(const QualifiedName&, const AtomicString&);
  void WillModifyAttribute(const QualifiedName&,
                           const AtomicString& old_value,
                           const AtomicString& new_value);
  void DidModifyAttribute(const QualifiedName&,
                          const AtomicString& old_value,
                          const AtomicString& new_value,
                          AttributeModificationReason reason);
  void DidRemoveAttribute(const QualifiedName&, const AtomicString& old_value);

  void SynchronizeAllAttributes() const;
  void SynchronizeAttribute(const QualifiedName&) const;
  void SynchronizeAllAttributesExceptStyle() const;

  void UpdateId(const AtomicString& old_id, const AtomicString& new_id);
  void UpdateId(TreeScope&,
                const AtomicString& old_id,
                const AtomicString& new_id);
  void UpdateName(const AtomicString& old_name, const AtomicString& new_name);

  void UpdateFocusgroup(const AtomicString& input);
  void UpdateFocusgroupInShadowRootIfNeeded();

  void ClientQuads(Vector<gfx::QuadF>& quads) const;

  bool ChildTypeAllowed(NodeType) const final;

  // Returns the attribute's index or `kNotFound` if not found.
  wtf_size_t FindAttributeIndex(const QualifiedName&) const;

  void SetAttributeInternal(wtf_size_t index,
                            const QualifiedName&,
                            const AtomicString& value,
                            AttributeModificationReason);
  void AppendAttributeInternal(const QualifiedName&,
                               const AtomicString& value,
                               AttributeModificationReason);
  void RemoveAttributeInternal(wtf_size_t index, AttributeModificationReason);
  String TrustedTypesCheckForAttribute(const QualifiedName&,
                                       String value,
                                       const char* legacy_sink_name,
                                       ExceptionState&) const;
  String TrustedTypesCheckForAttribute(const QualifiedName&,
                                       const V8TrustedType* value,
                                       const char* legacy_sink_name,
                                       ExceptionState&) const;
  String TrustedTypesCheckForAttribute(const QualifiedName&,
                                       const AtomicString&,
                                       const char* legacy_sink_name,
                                       ExceptionState&) const;

  // These Hinted versions of the functions are subtle hot path
  // optimizations designed to reduce the number of unnecessary AtomicString
  // creations, AtomicStringTable lookups, and LowerCaseIfNecessary calls.
  //
  // The `hint` is the result of a WeakLowercaseIfNecessary() call unless it is
  // known that the the incoming string already has the right case. Then
  // the `hint` can be constructed from calling AtomicString::Impl().
  const AtomicString& GetAttributeHinted(
      const AtomicString& name,
      AtomicStringTable::WeakResult hint) const;
  void RemoveAttributeHinted(const AtomicString& name,
                             AtomicStringTable::WeakResult hint);
  void SynchronizeAttributeHinted(const AtomicString& name,
                                  AtomicStringTable::WeakResult hint) const;
  void SetAttributeHinted(AtomicString name,
                          AtomicStringTable::WeakResult hint,
                          String value,
                          ExceptionState& = ASSERT_NO_EXCEPTION);
  void SetAttributeHinted(AtomicString name,
                          AtomicStringTable::WeakResult hint,
                          const V8TrustedType* trusted_string,
                          ExceptionState& exception_state);
  std::pair<wtf_size_t, const QualifiedName> LookupAttributeQNameHinted(
      AtomicString name,
      AtomicStringTable::WeakResult hint) const;
  wtf_size_t ValidateAttributeIndex(wtf_size_t index,
                                    const QualifiedName& qname) const;

  void CancelSelectionAfterLayout();
  virtual int DefaultTabIndex() const;

  bool WasLastFocusFromUserGestureInternal() const;

  inline void UpdateCallbackSelectors(const ComputedStyle* old_style,
                                      const ComputedStyle* new_style);
  inline void NotifyIfMatchedDocumentRulesSelectorsChanged(
      const ComputedStyle* old_style,
      const ComputedStyle* new_style);

  // Clone is private so that non-virtual CloneElementWithChildren and
  // CloneElementWithoutChildren are used instead.
  Node* Clone(Document& factory,
              NodeCloningData& data,
              ContainerNode* append_to,
              CustomElementRegistry* fallback_registry,
              ExceptionState& append_exception_state) const override;

  virtual Element& CloneWithoutAttributesAndChildren(
      Document& factory,
      CustomElementRegistry* registry) const;

  void UpdateNamedItemRegistration(NamedItemType,
                                   const AtomicString& old_name,
                                   const AtomicString& new_name);
  void UpdateIdNamedItemRegistration(NamedItemType,
                                     const AtomicString& old_name,
                                     const AtomicString& new_name);

  void CreateUniqueElementData();

  bool ShouldInvalidateDistributionWhenAttributeChanged(ShadowRoot&,
                                                        const QualifiedName&,
                                                        const AtomicString&);

  void SetInnerHTMLInternal(
      const String&,
      ParseDeclarativeShadowRoots parse_declarative_shadows,
      ForceHtml force_html_over_xml,
      // When called from SetHTML or SetHTMLUnsafe, SetInnerHTMLInternal must
      // process their options dictionary, which you can pass into |options|.
      // When called from a method without options, like the classic innerHTML
      // setter, you can pass std::monostate{} to designate no options.
      std::variant<std::monostate, SetHTMLOptions*, SetHTMLUnsafeOptions*>
          options,
      ExceptionState&);

  ElementRareDataVector* GetElementRareData() const;
  ElementRareDataVector& EnsureElementRareData();

  void RemoveAttrNodeList();
  void DetachAllAttrNodesFromElement();
  void DetachAttrNodeFromElementWithValue(Attr*, const AtomicString& value);
  void DetachAttrNodeAtIndex(Attr*, wtf_size_t index);

  void SynchronizeContentAttributeAndElementReference(
      const QualifiedName& name);

  DisplayLockContext* GetDisplayLockContextFromRareData() const;

  void PseudoStateChanged(CSSSelector::PseudoType pseudo,
                          AffectedByPseudoStateChange&&);

  void ProcessContainIntrinsicSizeChanges();

  bool ShouldUpdateLastRememberedBlockSize() const;
  bool ShouldUpdateLastRememberedInlineSize() const;

  bool IsStyleAttributeChangeAllowed(const AtomicString& style_string);

  // These schedule interest gained/lost events, for `interestfor` invokers.
  void ScheduleInterestGainedTask();
  void ScheduleInterestLostTask();
  enum class InterestSource {
    kHover,
    kDeHover,
    kFocus,
    kBlur,
  };
  void HandleInterestForHoverOrFocus(InterestSource source,
                                     bool recursive_call = false);

  // Highlight pseudos inherit all properties from the corresponding highlight
  // in the parent, but virtually all existing content uses universal rules
  // like *::selection. To improve runtime and keep copy-on-write inheritance,
  // avoid recalc if neither parent nor child matched any non-universal rules.
  HighlightRecalc CalculateHighlightRecalc(
      const ComputedStyle* old_style,
      const ComputedStyle& new_style,
      const ComputedStyle* parent_style) const;

  // This checks that the element is a scroller by calling IsScrollableNode,
  // which might update layout.
  // If UpdateBehavior::kNoneForAccessibility argument is passed, which should
  // only be used by a11y code, layout updates will never be performed.
  bool CanBeKeyboardFocusableScroller(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;
  // This checks whether the element is a scrollable container that should be
  // made keyboard focusable. Note that this is slow, because it must do a tree
  // walk to look for descendant focusable nodes.
  // If UpdateBehavior::kNoneForAccessibility argument is passed, which should
  // only be used by a11y code, layout updates will never be performed.
  bool IsKeyboardFocusableScroller(
      UpdateBehavior update_behavior = UpdateBehavior::kStyleAndLayout) const;

  FrozenArray<Element>* GetElementArrayAttribute(const QualifiedName& name);
  void SetElementArrayAttribute(
      const QualifiedName& name,
      const GCedHeapVector<Member<Element>>* given_elements);

  // Find the scroll-marker that should be active when told to scroll |this|
  // into view.
  ScrollMarkerPseudoElement* FindScrollMarkerForTargetedScroll();
  // Let the appropriate scroll-marker-group know to pin its active
  // scroll-marker due to a targeted scroll.
  void NotifyScrollMarkerGroupOfTargetedScroll();

  QualifiedName tag_name_;
  // This `ComputedStyle` field is a hot accessed member. Keep uncompressed for
  // performance reasons.
  subtle::UncompressedMember<const ComputedStyle> computed_style_;
  Member<ElementData> element_data_;

  // A tiny Bloom filter for which attribute names and class names exist
  // in this subtree; saves going to ElementData if the attribute/class
  // doesn't exist, and used to accelerate querySelector() (can quickly
  // skip entire subtrees). May have false positives, of course.
  // We do not currently update this when attributes/classes are removed,
  // only when they are added. Attribute _values_ are not part of this
  // filter, except for the values of class="".
  uint32_t attribute_or_class_bloom_ = 0;

  // Do not add new members to Element without a good reason; prefer to
  // add to ElementRareData unless it is performance-critical. Element
  // is 88 bytes on typical 64-bit platforms, and growing it can cause
  // both memory and performance regressions if you are not careful.
};

template <>
struct DowncastTraits<Element> {
  static bool AllowFrom(const Node& node) { return node.IsElementNode(); }
};

inline bool IsDisabledFormControl(const Node* node) {
  auto* element = DynamicTo<Element>(node);
  return element && element->IsDisabledFormControl();
}

inline Element* Node::parentElement() const {
  return DynamicTo<Element>(parentNode());
}

inline bool Element::FastHasAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name))
      << TagQName().ToString().Utf8() << "/@" << name.ToString().Utf8();
#endif
  return CouldHaveAttribute(name) && HasElementData() &&
         GetElementData()->Attributes().Find(name);
}

inline const AtomicString& Element::FastGetAttribute(
    const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name))
      << TagQName().ToString().Utf8() << "/@" << name.ToString().Utf8();
#endif
  if (CouldHaveAttribute(name) && HasElementData()) {
    if (const Attribute* attribute = GetElementData()->Attributes().Find(name))
      return attribute->Value();
  }
  return g_null_atom;
}

inline AttributeCollection Element::Attributes() const {
  if (!HasElementData())
    return AttributeCollection();
  SynchronizeAllAttributes();
  return GetElementData()->Attributes();
}

inline AttributeCollection Element::AttributesWithoutUpdate() const {
  if (!HasElementData())
    return AttributeCollection();
  return GetElementData()->Attributes();
}

inline AttributeCollection Element::AttributesWithoutStyleUpdate() const {
  if (!HasElementData())
    return AttributeCollection();
  SynchronizeAllAttributesExceptStyle();
  return GetElementData()->Attributes();
}

inline bool Element::hasAttributes() const {
  return !Attributes().IsEmpty();
}

inline const AtomicString& Element::IdForStyleResolution() const {
  DCHECK(HasID());
  return GetElementData()->IdForStyleResolution();
}

inline const AtomicString& Element::GetIdAttribute() const {
  return HasID() ? FastGetAttribute(html_names::kIdAttr) : g_null_atom;
}

inline const AtomicString& Element::GetNameAttribute() const {
  return HasName() ? FastGetAttribute(html_names::kNameAttr) : g_null_atom;
}

inline const AtomicString& Element::GetClassAttribute() const {
  if (!HasClass())
    return g_null_atom;
  if (IsSVGElement())
    return getAttribute(html_names::kClassAttr);
  return FastGetAttribute(html_names::kClassAttr);
}

inline void Element::SetIdAttribute(const AtomicString& value) {
  setAttribute(html_names::kIdAttr, value);
}

inline const SpaceSplitString& Element::ClassNames() const {
  DCHECK(HasClass());
  DCHECK(HasElementData());
  return GetElementData()->ClassNames();
}

inline bool Element::HasClassName(const AtomicString& class_name) const {
  return HasElementData() &&
         GetElementData()->ClassNames().Contains(class_name);
}

inline bool Element::HasID() const {
  return HasElementData() && GetElementData()->HasID();
}

inline bool Element::HasClass() const {
  return HasElementData() && GetElementData()->HasClass();
}

inline UniqueElementData& Element::EnsureUniqueElementData() {
  if (!HasElementData() || !GetElementData()->IsUnique())
    CreateUniqueElementData();
  return To<UniqueElementData>(*element_data_);
}

inline const CSSPropertyValueSet* Element::PresentationAttributeStyle() {
  if (!HasElementData())
    return nullptr;
  if (GetElementData()->presentation_attribute_style_is_dirty())
    UpdatePresentationAttributeStyle();
  // Need to call elementData() again since updatePresentationAttributeStyle()
  // might swap it with a UniqueElementData.
  return GetElementData()->PresentationAttributeStyle();
}

inline void Element::SetTagNameForCreateElementNS(
    const QualifiedName& tag_name) {
  // We expect this method to be called only to reset the prefix.
  DCHECK_EQ(tag_name.LocalName(), tag_name_.LocalName());
  DCHECK_EQ(tag_name.NamespaceURI(), tag_name_.NamespaceURI());
  tag_name_ = tag_name;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_H_
