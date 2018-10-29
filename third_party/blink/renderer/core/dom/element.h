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

#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/element_data.h"
#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/dom/whitespace_attacher.h"
#include "third_party/blink/renderer/core/html/focus_options.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class AccessibleNode;
class Attr;
class Attribute;
class CSSPropertyValueSet;
class CSSStyleDeclaration;
class CustomElementDefinition;
class DOMRect;
class DOMRectList;
class DOMStringMap;
class DOMTokenList;
class Document;
class ElementAnimations;
class ElementIntersectionObserverData;
class ElementRareData;
class ExceptionState;
class FloatQuad;
class FocusOptions;
class Image;
class InputDeviceCapabilities;
class Locale;
class MutableCSSPropertyValueSet;
class NamedNodeMap;
class PseudoElement;
class PseudoStyleRequest;
class ResizeObservation;
class ScrollIntoViewOptions;
class ScrollIntoViewOptionsOrBoolean;
class ScrollToOptions;
class ShadowRoot;
class ShadowRootInit;
class SpaceSplitString;
class StringOrTrustedHTML;
class StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL;
class StringOrTrustedScript;
class StringOrTrustedScriptURL;
class StylePropertyMap;
class StylePropertyMapReadOnly;
class USVStringOrTrustedURL;
class V0CustomElementDefinition;
class V8DisplayLockCallback;

enum SpellcheckAttributeState {
  kSpellcheckAttributeTrue,
  kSpellcheckAttributeFalse,
  kSpellcheckAttributeDefault
};

enum class ElementFlags {
  kTabIndexWasSetExplicitly = 1 << 0,
  kStyleAffectedByEmpty = 1 << 1,
  kIsInCanvasSubtree = 1 << 2,
  kContainsFullScreenElement = 1 << 3,
  kIsInTopLayer = 1 << 4,
  kContainsPersistentVideo = 1 << 5,

  kNumberOfElementFlags = 6,  // Size of bitfield used to store the flags.
};

enum class ShadowRootType;

enum class SelectionBehaviorOnFocus {
  kReset,
  kRestore,
  kNone,
};

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem-filter
enum class NamedItemType {
  kNone,
  kName,
  kNameOrId,
  kNameOrIdWithName,
};

enum class InvisibleState {
  kMissing,
  kStatic,
  kInvisible,
};

struct FocusParams {
  STACK_ALLOCATED();

 public:
  FocusParams() = default;
  FocusParams(SelectionBehaviorOnFocus selection,
              WebFocusType focus_type,
              InputDeviceCapabilities* capabilities,
              FocusOptions focus_options = FocusOptions())
      : selection_behavior(selection),
        type(focus_type),
        source_capabilities(capabilities),
        options(focus_options) {}

  SelectionBehaviorOnFocus selection_behavior =
      SelectionBehaviorOnFocus::kRestore;
  WebFocusType type = kWebFocusTypeNone;
  Member<InputDeviceCapabilities> source_capabilities = nullptr;
  FocusOptions options = FocusOptions();
};

typedef HeapVector<TraceWrapperMember<Attr>> AttrNodeList;

class CORE_EXPORT Element : public ContainerNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Element* Create(const QualifiedName&, Document*);
  ~Element() override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search);

  bool hasAttribute(const QualifiedName&) const;
  const AtomicString& getAttribute(const QualifiedName&) const;

  // Passing g_null_atom as the second parameter removes the attribute when
  // calling either of these set methods.
  void setAttribute(const QualifiedName&, const AtomicString& value);
  void SetSynchronizedLazyAttribute(const QualifiedName&,
                                    const AtomicString& value);

  void removeAttribute(const QualifiedName&);

  // Typed getters and setters for language bindings.
  int GetIntegralAttribute(const QualifiedName& attribute_name) const;
  void SetIntegralAttribute(const QualifiedName& attribute_name, int value);
  void SetUnsignedIntegralAttribute(const QualifiedName& attribute_name,
                                    unsigned value,
                                    unsigned default_value = 0);
  double GetFloatingPointAttribute(
      const QualifiedName& attribute_name,
      double fallback_value = std::numeric_limits<double>::quiet_NaN()) const;
  void SetFloatingPointAttribute(const QualifiedName& attribute_name,
                                 double value);

  // Call this to get the value of an attribute that is known not to be the
  // style attribute or one of the SVG animatable attributes.
  bool FastHasAttribute(const QualifiedName&) const;
  const AtomicString& FastGetAttribute(const QualifiedName&) const;
#if DCHECK_IS_ON()
  bool FastAttributeLookupAllowed(const QualifiedName&) const;
#endif

#ifdef DUMP_NODE_STATISTICS
  bool HasNamedNodeMap() const;
#endif
  bool hasAttributes() const;

  bool hasAttribute(const AtomicString& name) const;
  bool hasAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& local_name) const;

  const AtomicString& getAttribute(const AtomicString& name) const;
  const AtomicString& getAttributeNS(const AtomicString& namespace_uri,
                                     const AtomicString& local_name) const;

  void setAttribute(const AtomicString& name,
                    const AtomicString& value,
                    ExceptionState&);
  void setAttribute(const AtomicString& name, const AtomicString& value);

  // Trusted Types variant for explicit setAttribute() use.
  void setAttribute(
      const AtomicString&,
      const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&,
      ExceptionState&);

  // Returns attributes that should be checked against Trusted Types
  virtual const HashSet<AtomicString>& GetCheckedAttributeNames() const;

  // Trusted Type HTML variant
  void setAttribute(const QualifiedName&,
                    const StringOrTrustedHTML&,
                    ExceptionState&);

  // Trusted Type Script variant
  void setAttribute(const QualifiedName&,
                    const StringOrTrustedScript&,
                    ExceptionState&);

  // Trusted Type ScriptURL variant
  void setAttribute(const QualifiedName&,
                    const StringOrTrustedScriptURL&,
                    ExceptionState&);

  // Trusted Type URL variant
  void setAttribute(const QualifiedName&,
                    const USVStringOrTrustedURL&,
                    ExceptionState&);

  static bool ParseAttributeName(QualifiedName&,
                                 const AtomicString& namespace_uri,
                                 const AtomicString& qualified_name,
                                 ExceptionState&);
  void setAttributeNS(
      const AtomicString& namespace_uri,
      const AtomicString& qualified_name,
      const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURLOrTrustedURL&,
      ExceptionState&);

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
  AtomicString LowercaseIfNecessary(const AtomicString&) const;

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
  // when not interested in style attribute or one of the SVG animation
  // attributes.
  AttributeCollection AttributesWithoutUpdate() const;

  void scrollIntoView(ScrollIntoViewOptionsOrBoolean);
  void scrollIntoView(bool align_to_top = true);
  void scrollIntoViewWithOptions(const ScrollIntoViewOptions&);
  void ScrollIntoViewNoVisualUpdate(const ScrollIntoViewOptions&);
  void scrollIntoViewIfNeeded(bool center_if_needed = true);

  int OffsetLeft();
  int OffsetTop();
  int OffsetWidth();
  int OffsetHeight();

  Element* OffsetParent();
  int clientLeft();
  int clientTop();
  int clientWidth();
  int clientHeight();
  double scrollLeft();
  double scrollTop();
  void setScrollLeft(double);
  void setScrollTop(double);
  int scrollWidth();
  int scrollHeight();

  void scrollBy(double x, double y);
  virtual void scrollBy(const ScrollToOptions&);
  void scrollTo(double x, double y);
  virtual void scrollTo(const ScrollToOptions&);

  IntRect BoundsInViewport() const;
  // Returns an intersection rectangle of the bounds rectangle and the visual
  // viewport's rectangle in the visual viewport's coordinate space.
  // Applies ancestors' frames' clipping, but does not (yet) apply (overflow)
  // element clipping (crbug.com/889840).
  IntRect VisibleBoundsInVisualViewport() const;

  DOMRectList* getClientRects();
  DOMRect* getBoundingClientRect();

  bool HasNonEmptyLayoutSize() const;

  const AtomicString& computedRole();
  String computedName();

  AccessibleNode* ExistingAccessibleNode() const;
  AccessibleNode* accessibleNode();

  InvisibleState Invisible() const;
  void DispatchActivateInvisibleEventIfNeeded();
  bool IsInsideInvisibleStaticSubtree();

  void DefaultEventHandler(Event&) override;

  void DidMoveToNewDocument(Document&) override;

  void removeAttribute(const AtomicString& name);
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

  Element* CloneWithChildren(Document* = nullptr) const;
  Element* CloneWithoutChildren(Document* = nullptr) const;

  void SetBooleanAttribute(const QualifiedName&, bool);

  virtual const CSSPropertyValueSet* AdditionalPresentationAttributeStyle() {
    return nullptr;
  }
  void InvalidateStyleAttribute();

  const CSSPropertyValueSet* InlineStyle() const {
    return GetElementData() ? GetElementData()->inline_style_.Get() : nullptr;
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

  bool RemoveInlineStyleProperty(CSSPropertyID);
  bool RemoveInlineStyleProperty(const AtomicString&);
  void RemoveAllInlineStyleProperties();

  void SynchronizeStyleAttributeInternal() const;

  const CSSPropertyValueSet* PresentationAttributeStyle();
  virtual bool IsPresentationAttribute(const QualifiedName&) const {
    return false;
  }
  virtual void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) {}

  // For exposing to DOM only.
  NamedNodeMap* attributesForBindings() const;
  Vector<AtomicString> getAttributeNames() const;

  enum class AttributeModificationReason { kDirectly, kByParser, kByCloning };
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

  // |parseAttribute| is called by |attributeChanged|. If an element
  // implementation needs to check an attribute update, override this function.
  //
  // While the owner document is parsed, this function is called after all
  // attributes in a start tag were added to the element.
  virtual void ParseAttribute(const AttributeModificationParams&);

  virtual bool HasLegalLinkAttribute(const QualifiedName&) const;
  virtual const QualifiedName& SubResourceAttributeName() const;

  // Only called by the parser immediately after element construction.
  void ParserSetAttributes(const Vector<Attribute>&);

  // Remove attributes that might introduce scripting from the vector leaving
  // the element unchanged.
  void StripScriptingAttributes(Vector<Attribute>&) const;

  bool SharesSameElementData(const Element& other) const {
    return GetElementData() == other.GetElementData();
  }

  // Clones attributes only.
  void CloneAttributesFrom(const Element&);

  bool HasEquivalentAttributes(const Element& other) const;

  // Step 5 of https://dom.spec.whatwg.org/#concept-node-clone
  virtual void CloneNonAttributePropertiesFrom(const Element&,
                                               CloneChildrenFlag) {}

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(const AttachContext& = AttachContext()) override;

  virtual LayoutObject* CreateLayoutObject(const ComputedStyle&);
  virtual bool LayoutObjectIsNeeded(const ComputedStyle&) const;
  void RecalcStyle(StyleRecalcChange);
  void RecalcStyleForTraversalRootAncestor();
  void RebuildLayoutTreeForTraversalRootAncestor() {
    RebuildFirstLetterLayoutTree();
  }
  bool NeedsRebuildLayoutTree(
      const WhitespaceAttacher& whitespace_attacher) const {
    // TODO(futhark@chromium.org): !CanParticipateInFlatTree() can be removed
    // when Shadow DOM V0 support is removed.
    return NeedsReattachLayoutTree() || ChildNeedsReattachLayoutTree() ||
           !CanParticipateInFlatTree() ||
           (whitespace_attacher.TraverseIntoDisplayContents() &&
            HasDisplayContentsStyle());
  }
  void RebuildLayoutTree(WhitespaceAttacher&);
  void PseudoStateChanged(CSSSelector::PseudoType);
  void SetAnimationStyleChange(bool);
  void ClearAnimationStyleChange();
  void SetNeedsAnimationStyleRecalc();

  void SetNeedsCompositingUpdate();

  // If type of ShadowRoot (either closed or open) is explicitly specified,
  // creation of multiple shadow roots is prohibited in any combination and
  // throws an exception.  Multiple shadow roots are allowed only when
  // createShadowRoot() is used without any parameters from JavaScript.
  ShadowRoot* createShadowRoot(ExceptionState&);
  ShadowRoot* attachShadow(const ShadowRootInit&,
                           ExceptionState&);

  ShadowRoot& CreateV0ShadowRootForTesting() {
    return CreateShadowRootInternal();
  }
  ShadowRoot& CreateUserAgentShadowRoot();
  ShadowRoot& AttachShadowRootInternal(ShadowRootType,
                                       bool delegates_focus = false,
                                       bool manual_slotting = false);

  // Returns the shadow root attached to this element if it is a shadow host.
  ShadowRoot* GetShadowRoot() const;
  ShadowRoot* OpenShadowRoot() const;
  ShadowRoot* ClosedShadowRoot() const;
  ShadowRoot* AuthorShadowRoot() const;
  ShadowRoot* UserAgentShadowRoot() const;

  ShadowRoot* ShadowRootIfV1() const;

  ShadowRoot& EnsureUserAgentShadowRoot();

  bool IsInDescendantTreeOf(const Element* shadow_host) const;

  // Returns the Element’s ComputedStyle. If the ComputedStyle is not already
  // stored on the Element, computes the ComputedStyle and stores it on the
  // Element’s ElementRareData.  Used for getComputedStyle when Element is
  // display none.
  const ComputedStyle* EnsureComputedStyle(PseudoId = kPseudoIdNone);

  const ComputedStyle* NonLayoutObjectComputedStyle() const;

  bool HasDisplayContentsStyle() const;

  ComputedStyle* MutableNonLayoutObjectComputedStyle() const {
    return const_cast<ComputedStyle*>(NonLayoutObjectComputedStyle());
  }

  bool ShouldStoreNonLayoutObjectComputedStyle(const ComputedStyle&) const;
  void StoreNonLayoutObjectComputedStyle(scoped_refptr<ComputedStyle>);

  // Methods for indicating the style is affected by dynamic updates (e.g.,
  // children changing, our position changing in our sibling list, etc.)
  bool StyleAffectedByEmpty() const {
    return HasElementFlag(ElementFlags::kStyleAffectedByEmpty);
  }
  void SetStyleAffectedByEmpty() {
    SetElementFlag(ElementFlags::kStyleAffectedByEmpty);
  }

  void SetIsInCanvasSubtree(bool value) {
    SetElementFlag(ElementFlags::kIsInCanvasSubtree, value);
  }
  bool IsInCanvasSubtree() const {
    return HasElementFlag(ElementFlags::kIsInCanvasSubtree);
  }

  bool IsDefined() const {
    return !(static_cast<int>(GetCustomElementState()) &
             static_cast<int>(CustomElementState::kNotDefinedFlag));
  }
  bool IsUpgradedV0CustomElement() {
    return GetV0CustomElementState() == kV0Upgraded;
  }
  bool IsUnresolvedV0CustomElement() {
    return GetV0CustomElementState() == kV0WaitingForUpgrade;
  }

  AtomicString ComputeInheritedLanguage() const;
  Locale& GetLocale() const;

  virtual void AccessKeyAction(bool /*sendToAnyEvent*/) {}

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

  KURL GetURLAttribute(const QualifiedName&) const;
  void GetURLAttribute(const QualifiedName&, StringOrTrustedScriptURL&) const;
  void GetURLAttribute(const QualifiedName&, USVStringOrTrustedURL&) const;
  void FastGetAttribute(const QualifiedName&, USVStringOrTrustedURL&) const;
  void FastGetAttribute(const QualifiedName&, StringOrTrustedHTML&) const;

  KURL GetNonEmptyURLAttribute(const QualifiedName&) const;

  virtual const AtomicString ImageSourceURL() const;
  virtual Image* ImageContents() { return nullptr; }

  virtual void focus(const FocusParams& = FocusParams());
  void focus(FocusOptions);

  void UpdateFocusAppearance(SelectionBehaviorOnFocus);
  virtual void UpdateFocusAppearanceWithOptions(SelectionBehaviorOnFocus,
                                                const FocusOptions&);
  virtual void blur();

  // Whether this element can receive focus at all. Most elements are not
  // focusable but some elements, such as form controls and links, are. Unlike
  // layoutObjectIsFocusable(), this method may be called when layout is not up
  // to date, so it must not use the layoutObject to determine focusability.
  virtual bool SupportsFocus() const;
  // IsFocusable(), IsKeyboardFocusable(), and IsMouseFocusable() check
  // whether the element can actually be focused. Callers should ensure
  // ComputedStyle is up to date;
  // e.g. by calling Document::UpdateStyleAndLayoutTree().
  bool IsFocusable() const;
  virtual bool IsKeyboardFocusable() const;
  virtual bool IsMouseFocusable() const;
  bool IsFocusedElementInDocument() const;
  Element* AdjustedFocusedElementInTreeScope() const;

  virtual void DispatchFocusEvent(
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchBlurEvent(
      Element* new_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchFocusInEvent(
      const AtomicString& event_type,
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  void DispatchFocusOutEvent(
      const AtomicString& event_type,
      Element* new_focused_element,
      InputDeviceCapabilities* source_capabilities = nullptr);

  // The implementation of |innerText()| is found in "element_inner_text.cc".
  String innerText();
  String outerText();
  String InnerHTMLAsString() const;
  String OuterHTMLAsString() const;
  void SetInnerHTMLFromString(const String& html, ExceptionState&);
  void SetInnerHTMLFromString(const String& html);
  void SetOuterHTMLFromString(const String& html, ExceptionState&);

  Element* insertAdjacentElement(const String& where,
                                 Element* new_child,
                                 ExceptionState&);
  void insertAdjacentText(const String& where,
                          const String& text,
                          ExceptionState&);
  void insertAdjacentHTML(const String& where,
                          const String& html,
                          ExceptionState&);

  // TrustedTypes variants of the above.
  // TODO(mkwst): Write a spec for these bits. https://crbug.com/739170
  void innerHTML(StringOrTrustedHTML&) const;
  void outerHTML(StringOrTrustedHTML&) const;
  void setInnerHTML(const StringOrTrustedHTML&, ExceptionState&);
  void setInnerHTML(const StringOrTrustedHTML&);
  void setOuterHTML(const StringOrTrustedHTML&, ExceptionState&);
  void insertAdjacentHTML(const String& where,
                          const StringOrTrustedHTML&,
                          ExceptionState&);

  void setPointerCapture(int pointer_id, ExceptionState&);
  void releasePointerCapture(int pointer_id, ExceptionState&);

  // Returns true iff the element would capture the next pointer event. This
  // is true between a setPointerCapture call and a releasePointerCapture (or
  // implicit release) call:
  // https://w3c.github.io/pointerevents/#dom-element-haspointercapture
  bool hasPointerCapture(int pointer_id) const;

  // Returns true iff the element has received a gotpointercapture event for
  // the |pointerId| but hasn't yet received a lostpointercapture event for
  // the same id. The time window during which this is true is "delayed" from
  // (but overlapping with) the time window for hasPointerCapture():
  // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
  bool HasProcessedPointerCapture(int pointer_id) const;

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

  PseudoElement* GetPseudoElement(PseudoId) const;
  LayoutObject* PseudoElementLayoutObject(PseudoId) const;

  const ComputedStyle* CachedStyleForPseudoElement(
      const PseudoStyleRequest&,
      const ComputedStyle* parent_style = nullptr);
  scoped_refptr<ComputedStyle> StyleForPseudoElement(
      const PseudoStyleRequest&,
      const ComputedStyle* parent_style = nullptr);
  bool CanGeneratePseudoElement(PseudoId) const;

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

  // Elements that may have an insertion mode other than "in body" should
  // override this and return true.
  // https://html.spec.whatwg.org/multipage/parsing.html#reset-the-insertion-mode-appropriately
  virtual bool HasNonInBodyInsertionMode() const { return false; }

  bool CanContainRangeEndPoint() const override { return true; }

  // Used for disabled form elements; if true, prevents mouse events from being
  // dispatched to event listeners, and prevents DOMActivate events from being
  // sent at all.
  virtual bool IsDisabledFormControl() const { return false; }

  virtual bool ShouldForceLegacyLayout() const { return false; }

  virtual void BuildPendingResource() {}

  void V0SetCustomElementDefinition(V0CustomElementDefinition*);
  V0CustomElementDefinition* GetV0CustomElementDefinition() const;

  void SetCustomElementDefinition(CustomElementDefinition*);
  CustomElementDefinition* GetCustomElementDefinition() const;
  // https://dom.spec.whatwg.org/#concept-element-is-value
  void SetIsValue(const AtomicString&);
  const AtomicString& IsValue() const;

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

  void requestPointerLock();

  bool IsSpellCheckingEnabled() const;

  // FIXME: public for LayoutTreeBuilder, we shouldn't expose this though.
  scoped_refptr<ComputedStyle> StyleForLayoutObject();

  bool HasID() const;
  bool HasClass() const;
  const SpaceSplitString& ClassNames() const;
  bool HasClassName(const AtomicString& class_name) const;

  bool HasPartName() const;
  const SpaceSplitString* PartNames() const;

  bool HasPartNamesMap() const;
  const NamesMap* PartNamesMap() const;

  ScrollOffset SavedLayerScrollOffset() const;
  void SetSavedLayerScrollOffset(const ScrollOffset&);

  ElementAnimations* GetElementAnimations() const;
  ElementAnimations& EnsureElementAnimations();
  bool HasAnimations() const;

  void SynchronizeAttribute(const AtomicString& local_name) const;

  MutableCSSPropertyValueSet& EnsureMutableInlineStyle();
  void ClearMutableInlineStyleIfEmpty();

  void setTabIndex(int);
  int tabIndex() const override;

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

  void Trace(blink::Visitor*) override;

  SpellcheckAttributeState GetSpellcheckAttributeState() const;

  ElementIntersectionObserverData* IntersectionObserverData() const;
  ElementIntersectionObserverData& EnsureIntersectionObserverData();
  void ComputeIntersectionObservations(unsigned flags);

  HeapHashMap<TraceWrapperMember<ResizeObserver>, Member<ResizeObservation>>*
  ResizeObserverData() const;
  HeapHashMap<TraceWrapperMember<ResizeObserver>, Member<ResizeObservation>>&
  EnsureResizeObserverData();
  void SetNeedsResizeObserverUpdate();

  ScriptPromise acquireDisplayLock(ScriptState*, V8DisplayLockCallback*);

 protected:
  Element(const QualifiedName& tag_name, Document*, ConstructionType);

  const ElementData* GetElementData() const { return element_data_.Get(); }
  UniqueElementData& EnsureUniqueElementData();

  void AddPropertyToPresentationAttributeStyle(MutableCSSPropertyValueSet*,
                                               CSSPropertyID,
                                               CSSValueID identifier);
  void AddPropertyToPresentationAttributeStyle(MutableCSSPropertyValueSet*,
                                               CSSPropertyID,
                                               double value,
                                               CSSPrimitiveValue::UnitType);
  void AddPropertyToPresentationAttributeStyle(MutableCSSPropertyValueSet*,
                                               CSSPropertyID,
                                               const String& value);
  void AddPropertyToPresentationAttributeStyle(MutableCSSPropertyValueSet*,
                                               CSSPropertyID,
                                               const CSSValue&);

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void ChildrenChanged(const ChildrenChange&) override;

  virtual void WillRecalcStyle(StyleRecalcChange);
  virtual void DidRecalcStyle(StyleRecalcChange);
  virtual scoped_refptr<ComputedStyle> CustomStyleForLayoutObject();

  virtual NamedItemType GetNamedItemType() const {
    return NamedItemType::kNone;
  }

  bool SupportsSpatialNavigationFocus() const;

  void ClearTabIndexExplicitlyIfNeeded();
  void SetTabIndexExplicitly();
  // Subclasses may override this method to affect focusability. This method
  // must be called on an up-to-date ComputedStyle, so it may use existence of
  // layoutObject and the LayoutObject::style() to reason about focusability.
  // However, it must not retrieve layout information like position and size.
  // This method cannot be moved to LayoutObject because some focusable nodes
  // don't have layoutObjects. e.g., HTMLOptionElement.
  virtual bool IsFocusableStyle() const;

  virtual bool ChildrenCanHaveStyle() const { return true; }

  // classAttributeChanged() exists to share code between
  // parseAttribute (called via setAttribute()) and
  // svgAttributeChanged (called when element.className.baseValue is set)
  void ClassAttributeChanged(const AtomicString& new_class_string);

  static bool AttributeValueIsJavaScriptURL(const Attribute&);

  scoped_refptr<ComputedStyle> OriginalStyleForLayoutObject();

  Node* InsertAdjacent(const String& where, Node* new_child, ExceptionState&);

  virtual void ParserDidSetAttributes() {}

 private:
  void ScrollLayoutBoxBy(const ScrollToOptions&);
  void ScrollLayoutBoxTo(const ScrollToOptions&);
  void ScrollFrameBy(const ScrollToOptions&);
  void ScrollFrameTo(const ScrollToOptions&);

  bool HasElementFlag(ElementFlags mask) const {
    return HasRareData() && HasElementFlagInternal(mask);
  }
  void SetElementFlag(ElementFlags, bool value = true);
  void ClearElementFlag(ElementFlags);
  bool HasElementFlagInternal(ElementFlags) const;

  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  bool CanAttachShadowRoot() const;
  ShadowRoot& CreateShadowRootInternal();

  void StyleAttributeChanged(const AtomicString& new_style_string,
                             AttributeModificationReason);

  void UpdatePresentationAttributeStyle();

  void InlineStyleChanged();
  void SetInlineStyleFromString(const AtomicString&);

  void InvisibleAttributeChanged(const AtomicString& old_value,
                                 const AtomicString& new_value);

  // If the only inherited changes in the parent element are independent,
  // these changes can be directly propagated to this element (the child).
  // If these conditions are met, propagates the changes to the current style
  // and returns the new style. Otherwise, returns null.
  scoped_refptr<ComputedStyle> PropagateInheritedProperties(StyleRecalcChange);

  StyleRecalcChange RecalcOwnStyle(StyleRecalcChange);

  // Returns true if we should traverse shadow including children and pseudo
  // elements for RecalcStyle.
  bool ShouldCallRecalcStyleForChildren(StyleRecalcChange);

  void RebuildPseudoElementLayoutTree(PseudoId, WhitespaceAttacher&);
  void RebuildFirstLetterLayoutTree();
  void RebuildShadowRootLayoutTree(WhitespaceAttacher&);
  inline void CheckForEmptyStyleChange(const Node* node_before_change,
                                       const Node* node_after_change);

  void UpdatePseudoElement(PseudoId, StyleRecalcChange);

  enum class StyleUpdatePhase {
    kRecalc,
    kRebuildLayoutTree,
    kAttachLayoutTree,
  };

  void UpdateFirstLetterPseudoElement(StyleUpdatePhase);

  inline PseudoElement* CreatePseudoElementIfNeeded(PseudoId);
  void AttachPseudoElement(PseudoId, AttachContext&);
  void DetachPseudoElement(PseudoId, const AttachContext&);

  ShadowRoot& CreateAndAttachShadowRoot(ShadowRootType);

  // FIXME: Everyone should allow author shadows.
  virtual bool AreAuthorShadowsAllowed() const { return true; }
  virtual void DidAddUserAgentShadowRoot(ShadowRoot&) {}
  virtual bool AlwaysCreateUserAgentShadowRoot() const { return false; }

  enum SynchronizationOfLazyAttribute {
    kNotInSynchronizationOfLazyAttribute = 0,
    kInSynchronizationOfLazyAttribute
  };

  void DidAddAttribute(const QualifiedName&, const AtomicString&);
  void WillModifyAttribute(const QualifiedName&,
                           const AtomicString& old_value,
                           const AtomicString& new_value);
  void DidModifyAttribute(const QualifiedName&,
                          const AtomicString& old_value,
                          const AtomicString& new_value);
  void DidRemoveAttribute(const QualifiedName&, const AtomicString& old_value);

  void SynchronizeAllAttributes() const;
  void SynchronizeAttribute(const QualifiedName&) const;

  void UpdateId(const AtomicString& old_id, const AtomicString& new_id);
  void UpdateId(TreeScope&,
                const AtomicString& old_id,
                const AtomicString& new_id);
  void UpdateName(const AtomicString& old_name, const AtomicString& new_name);

  void ClientQuads(Vector<FloatQuad>& quads);

  NodeType getNodeType() const final;
  bool ChildTypeAllowed(NodeType) const final;

  void SetAttributeInternal(wtf_size_t index,
                            const QualifiedName&,
                            const AtomicString& value,
                            SynchronizationOfLazyAttribute);
  void AppendAttributeInternal(const QualifiedName&,
                               const AtomicString& value,
                               SynchronizationOfLazyAttribute);
  void RemoveAttributeInternal(wtf_size_t index,
                               SynchronizationOfLazyAttribute);

  void CancelFocusAppearanceUpdate();

  const ComputedStyle* VirtualEnsureComputedStyle(
      PseudoId pseudo_element_specifier = kPseudoIdNone) override {
    return EnsureComputedStyle(pseudo_element_specifier);
  }

  inline void UpdateCallbackSelectors(const ComputedStyle* old_style,
                                      const ComputedStyle* new_style);
  inline void RemoveCallbackSelectors();
  inline void AddCallbackSelectors();

  // Clone is private so that non-virtual CloneElementWithChildren and
  // CloneElementWithoutChildren are used instead.
  Node* Clone(Document&, CloneChildrenFlag) const override;
  virtual Element* CloneWithoutAttributesAndChildren(Document& factory) const;

  QualifiedName tag_name_;

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

  ElementRareData* GetElementRareData() const;
  ElementRareData& EnsureElementRareData();

  void RemoveAttrNodeList();
  void DetachAllAttrNodesFromElement();
  void DetachAttrNodeFromElementWithValue(Attr*, const AtomicString& value);
  void DetachAttrNodeAtIndex(Attr*, wtf_size_t index);

  Member<ElementData> element_data_;
};

DEFINE_NODE_TYPE_CASTS(Element, IsElementNode());
template <typename T>
bool IsElementOfType(const Node&);
template <>
inline bool IsElementOfType<const Element>(const Node& node) {
  return node.IsElementNode();
}
template <typename T>
inline bool IsElementOfType(const Element& element) {
  return IsElementOfType<T>(static_cast<const Node&>(element));
}
template <>
inline bool IsElementOfType<const Element>(const Element&) {
  return true;
}

// Type casting.
template <typename T>
inline T& ToElement(Node& node) {
  SECURITY_DCHECK(IsElementOfType<const T>(node));
  return static_cast<T&>(node);
}
template <typename T>
inline T* ToElement(Node* node) {
  SECURITY_DCHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<T*>(node);
}
template <typename T>
inline const T& ToElement(const Node& node) {
  SECURITY_DCHECK(IsElementOfType<const T>(node));
  return static_cast<const T&>(node);
}
template <typename T>
inline const T* ToElement(const Node* node) {
  SECURITY_DCHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<const T*>(node);
}

template <typename T>
inline T* ToElementOrNull(Node& node) {
  return IsElementOfType<const T>(node) ? static_cast<T*>(&node) : nullptr;
}
template <typename T>
inline T* ToElementOrNull(Node* node) {
  return (node && IsElementOfType<const T>(*node)) ? static_cast<T*>(node)
                                                   : nullptr;
}
template <typename T>
inline const T* ToElementOrNull(const Node& node) {
  return IsElementOfType<const T>(node) ? static_cast<const T*>(&node)
                                        : nullptr;
}
template <typename T>
inline const T* ToElementOrNull(const Node* node) {
  return (node && IsElementOfType<const T>(*node)) ? static_cast<const T*>(node)
                                                   : nullptr;
}

template <typename T>
inline T& ToElementOrDie(Node& node) {
  CHECK(IsElementOfType<const T>(node));
  return static_cast<T&>(node);
}
template <typename T>
inline T* ToElementOrDie(Node* node) {
  CHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<T*>(node);
}
template <typename T>
inline const T& ToElementOrDie(const Node& node) {
  CHECK(IsElementOfType<const T>(node));
  return static_cast<const T&>(node);
}
template <typename T>
inline const T* ToElementOrDie(const Node* node) {
  CHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<const T*>(node);
}

inline bool IsDisabledFormControl(const Node* node) {
  return node->IsElementNode() && ToElement(node)->IsDisabledFormControl();
}

inline Element* Node::parentElement() const {
  ContainerNode* parent = parentNode();
  return parent && parent->IsElementNode() ? ToElement(parent) : nullptr;
}

inline bool Element::FastHasAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name));
#endif
  return GetElementData() &&
         GetElementData()->Attributes().FindIndex(name) != kNotFound;
}

inline const AtomicString& Element::FastGetAttribute(
    const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name));
#endif
  if (GetElementData()) {
    if (const Attribute* attribute = GetElementData()->Attributes().Find(name))
      return attribute->Value();
  }
  return g_null_atom;
}

inline AttributeCollection Element::Attributes() const {
  if (!GetElementData())
    return AttributeCollection();
  SynchronizeAllAttributes();
  return GetElementData()->Attributes();
}

inline AttributeCollection Element::AttributesWithoutUpdate() const {
  if (!GetElementData())
    return AttributeCollection();
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
  return HasID() ? FastGetAttribute(HTMLNames::idAttr) : g_null_atom;
}

inline const AtomicString& Element::GetNameAttribute() const {
  return HasName() ? FastGetAttribute(HTMLNames::nameAttr) : g_null_atom;
}

inline const AtomicString& Element::GetClassAttribute() const {
  if (!HasClass())
    return g_null_atom;
  if (IsSVGElement())
    return getAttribute(HTMLNames::classAttr);
  return FastGetAttribute(HTMLNames::classAttr);
}

inline void Element::SetIdAttribute(const AtomicString& value) {
  setAttribute(HTMLNames::idAttr, value);
}

inline const SpaceSplitString& Element::ClassNames() const {
  DCHECK(HasClass());
  DCHECK(GetElementData());
  return GetElementData()->ClassNames();
}

inline bool Element::HasClassName(const AtomicString& class_name) const {
  return HasClass() && ClassNames().Contains(class_name);
}

inline bool Element::HasID() const {
  return GetElementData() && GetElementData()->HasID();
}

inline bool Element::HasClass() const {
  return GetElementData() && GetElementData()->HasClass();
}

inline UniqueElementData& Element::EnsureUniqueElementData() {
  if (!GetElementData() || !GetElementData()->IsUnique())
    CreateUniqueElementData();
  return ToUniqueElementData(*element_data_);
}

inline void Element::InvalidateStyleAttribute() {
  DCHECK(GetElementData());
  GetElementData()->style_attribute_is_dirty_ = true;
}

inline const CSSPropertyValueSet* Element::PresentationAttributeStyle() {
  if (!GetElementData())
    return nullptr;
  if (GetElementData()->presentation_attribute_style_is_dirty_)
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

inline bool IsShadowHost(const Node* node) {
  return node && node->GetShadowRoot();
}

inline bool IsShadowHost(const Node& node) {
  return node.GetShadowRoot();
}

inline bool IsShadowHost(const Element* element) {
  return element && element->GetShadowRoot();
}

inline bool IsShadowHost(const Element& element) {
  return element.GetShadowRoot();
}

inline bool IsAtShadowBoundary(const Element* element) {
  if (!element)
    return false;
  ContainerNode* parent_node = element->parentNode();
  return parent_node && parent_node->IsShadowRoot();
}

// These macros do the same as their NODE equivalents but additionally provide a
// template specialization for isElementOfType<>() so that the Traversal<> API
// works for these Element types.
#define DEFINE_ELEMENT_TYPE_CASTS(thisType, predicate)            \
  template <>                                                     \
  inline bool IsElementOfType<const thisType>(const Node& node) { \
    return node.predicate;                                        \
  }                                                               \
  DEFINE_NODE_TYPE_CASTS(thisType, predicate)

#define DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)         \
  template <>                                                     \
  inline bool IsElementOfType<const thisType>(const Node& node) { \
    return Is##thisType(node);                                    \
  }                                                               \
  DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(thisType)

#define DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(T) \
  static T* Create(const QualifiedName&, Document&)
#define DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(T)                     \
  T* T::Create(const QualifiedName& tagName, Document& document) { \
    return new T(tagName, document);                               \
  }

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_H_
