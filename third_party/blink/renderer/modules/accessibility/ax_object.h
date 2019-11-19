/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.
 * Redistributiothird_party/blink/renderer/modules/exported/web_ax_object.ccns
 * in binary form must reproduce the above copyright notice, this list of
 * conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution. 3.  Neither the name of Apple
 * Computer, Inc. ("Apple") nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_

#include <ostream>

#include "base/macros.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/inspector/protocol/Accessibility.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

class SkMatrix44;

namespace blink {

class AccessibleNodeList;
class AXObject;
class AXObjectCacheImpl;
class AXRange;
class IntPoint;
class LayoutObject;
class LocalFrameView;
class Node;
class ScrollableArea;

enum class AOMBooleanProperty;
enum class AOMStringProperty;
enum class AOMUIntProperty;
enum class AOMIntProperty;
enum class AOMFloatProperty;
enum class AOMRelationProperty;
enum class AOMRelationListProperty;

class AXSparseAttributeClient {
 public:
  virtual void AddBoolAttribute(AXBoolAttribute, bool) = 0;
  virtual void AddIntAttribute(AXIntAttribute, int32_t) = 0;
  virtual void AddUIntAttribute(AXUIntAttribute, uint32_t) = 0;
  virtual void AddStringAttribute(AXStringAttribute, const String&) = 0;
  virtual void AddObjectAttribute(AXObjectAttribute, AXObject&) = 0;
  virtual void AddObjectVectorAttribute(AXObjectVectorAttribute,
                                        HeapVector<Member<AXObject>>&) = 0;
};

class IgnoredReason {
  DISALLOW_NEW();

 public:
  AXIgnoredReason reason;
  Member<const AXObject> related_object;

  explicit IgnoredReason(AXIgnoredReason reason)
      : reason(reason), related_object(nullptr) {}

  IgnoredReason(AXIgnoredReason r, const AXObject* obj)
      : reason(r), related_object(obj) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(related_object); }
};

class NameSourceRelatedObject final
    : public GarbageCollected<NameSourceRelatedObject> {
 public:
  WeakMember<AXObject> object;
  String text;

  NameSourceRelatedObject(AXObject* object, String text)
      : object(object), text(text) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(object); }

  DISALLOW_COPY_AND_ASSIGN(NameSourceRelatedObject);
};

typedef HeapVector<Member<NameSourceRelatedObject>> AXRelatedObjectVector;
class NameSource {
  DISALLOW_NEW();

 public:
  String text;
  bool superseded = false;
  bool invalid = false;
  ax::mojom::NameFrom type = ax::mojom::NameFrom::kUninitialized;
  const QualifiedName& attribute;
  AtomicString attribute_value;
  AXTextFromNativeHTML native_source = kAXTextFromNativeHTMLUninitialized;
  AXRelatedObjectVector related_objects;

  NameSource(bool superseded, const QualifiedName& attr)
      : superseded(superseded), attribute(attr) {}

  explicit NameSource(bool superseded)
      : superseded(superseded), attribute(QualifiedName::Null()) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(related_objects); }
};

class DescriptionSource {
  DISALLOW_NEW();

 public:
  String text;
  bool superseded = false;
  bool invalid = false;
  ax::mojom::DescriptionFrom type = ax::mojom::DescriptionFrom::kUninitialized;
  const QualifiedName& attribute;
  AtomicString attribute_value;
  AXTextFromNativeHTML native_source = kAXTextFromNativeHTMLUninitialized;
  AXRelatedObjectVector related_objects;

  DescriptionSource(bool superseded, const QualifiedName& attr)
      : superseded(superseded), attribute(attr) {}

  explicit DescriptionSource(bool superseded)
      : superseded(superseded), attribute(QualifiedName::Null()) {}

  void Trace(blink::Visitor* visitor) { visitor->Trace(related_objects); }
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::IgnoredReason)
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::NameSource)
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::DescriptionSource)

namespace blink {

class MODULES_EXPORT AXObject : public GarbageCollected<AXObject> {
 public:
  typedef HeapVector<Member<AXObject>> AXObjectVector;

  // Iterator for doing an in-order traversal of the accessibility tree.
  // Includes ignored objects in the traversal.
  class MODULES_EXPORT InOrderTraversalIterator final
      : public GarbageCollected<InOrderTraversalIterator> {
   public:
    ~InOrderTraversalIterator() = default;

    InOrderTraversalIterator(const InOrderTraversalIterator& other)
        : current_(other.current_), previous_(other.previous_) {}

    InOrderTraversalIterator& operator=(const InOrderTraversalIterator& other) {
      current_ = other.current_;
      previous_ = other.previous_;
      return *this;
    }

    InOrderTraversalIterator& operator++() {
      previous_ = current_;
      current_ = (current_ && !current_->IsDetached())
                     ? current_->NextInTreeObject()
                     : nullptr;
      return *this;
    }

    InOrderTraversalIterator operator++(int) {
      InOrderTraversalIterator ret = *this;
      ++*this;
      return ret;
    }

    InOrderTraversalIterator& operator--() {
      current_ = previous_;
      previous_ = (current_ && !current_->IsDetached())
                      ? current_->PreviousInTreeObject()
                      : nullptr;
      return *this;
    }

    InOrderTraversalIterator operator--(int) {
      InOrderTraversalIterator ret = *this;
      --*this;
      return ret;
    }

    AXObject& operator*() const {
      DCHECK(current_);
      return *current_;
    }

    AXObject* operator->() const {
      DCHECK(current_);
      return static_cast<AXObject*>(current_);
    }

    void Trace(blink::Visitor* visitor) {
      visitor->Trace(current_);
      visitor->Trace(previous_);
    }

    MODULES_EXPORT friend void swap(InOrderTraversalIterator& left,
                                    InOrderTraversalIterator& right) {
      std::swap(left.current_, right.current_);
      std::swap(left.previous_, right.previous_);
    }

    MODULES_EXPORT friend bool operator==(
        const InOrderTraversalIterator& left,
        const InOrderTraversalIterator& right) {
      return left.current_ == right.current_;
    }

    MODULES_EXPORT friend bool operator!=(
        const InOrderTraversalIterator& left,
        const InOrderTraversalIterator& right) {
      return !(left == right);
    }

   private:
    InOrderTraversalIterator() = default;

    explicit InOrderTraversalIterator(AXObject& current)
        : current_(&current), previous_(nullptr) {}

    friend class AXObject;
    friend class AXObjectCacheImpl;

    Member<AXObject> current_;
    Member<AXObject> previous_;
  };

  // Iterator for the ancestors of an |AXObject|.
  // Walks through all the unignored parents of the object up to the root.
  // Does not include the object itself in the list of ancestors.
  class MODULES_EXPORT AncestorsIterator final
      : public GarbageCollected<AncestorsIterator> {
   public:
    ~AncestorsIterator() = default;

    AncestorsIterator(const AncestorsIterator& other)
        : current_(other.current_) {}

    AncestorsIterator& operator=(const AncestorsIterator& other) {
      current_ = other.current_;
      return *this;
    }

    AncestorsIterator& operator++() {
      current_ = (current_ && !current_->IsDetached())
                     ? current_->ParentObjectUnignored()
                     : nullptr;
      return *this;
    }

    AncestorsIterator operator++(int) {
      AncestorsIterator ret = *this;
      ++*this;
      return ret;
    }

    AXObject& operator*() const {
      DCHECK(current_);
      return *current_;
    }

    AXObject* operator->() const {
      DCHECK(current_);
      return static_cast<AXObject*>(current_);
    }

    void Trace(blink::Visitor* visitor) { visitor->Trace(current_); }

    MODULES_EXPORT friend void swap(AncestorsIterator& left,
                                    AncestorsIterator& right) {
      std::swap(left.current_, right.current_);
    }

    MODULES_EXPORT friend bool operator==(const AncestorsIterator& left,
                                          const AncestorsIterator& right) {
      return left.current_ == right.current_;
    }

    MODULES_EXPORT friend bool operator!=(const AncestorsIterator& left,
                                          const AncestorsIterator& right) {
      return !(left == right);
    }

   private:
    AncestorsIterator() = default;

    explicit AncestorsIterator(AXObject& current) : current_(&current) {}

    friend class AXObject;
    friend class AXObjectCacheImpl;

    Member<AXObject> current_;
  };

 protected:
  AXObject(AXObjectCacheImpl&);

 public:
  virtual ~AXObject();
  virtual void Trace(blink::Visitor*);

  static unsigned NumberOfLiveAXObjects() { return number_of_live_ax_objects_; }

  // After constructing an AXObject, it must be given a
  // unique ID, then added to AXObjectCacheImpl, and finally init() must
  // be called last.
  void SetAXObjectID(AXID ax_object_id) { id_ = ax_object_id; }
  virtual void Init();

  // When the corresponding WebCore object that this AXObject
  // wraps is deleted, it must be detached.
  virtual void Detach();
  virtual bool IsDetached() const;

  // If the parent of this object is known, this can be faster than using
  // computeParent().
  virtual void SetParent(AXObject* parent) { parent_ = parent; }

  // The AXObjectCacheImpl that owns this object, and its unique ID within this
  // cache.
  AXObjectCacheImpl& AXObjectCache() const {
    DCHECK(ax_object_cache_);
    return *ax_object_cache_;
  }

  AXID AXObjectID() const { return id_; }

  // Wrappers that retrieve either an Accessibility Object Model property,
  // or the equivalent ARIA attribute, in that order.
  virtual const AtomicString& GetAOMPropertyOrARIAAttribute(
      AOMStringProperty) const;
  Element* GetAOMPropertyOrARIAAttribute(AOMRelationProperty) const;
  bool HasAOMProperty(AOMRelationListProperty,
                      HeapVector<Member<Element>>& result) const;
  bool HasAOMPropertyOrARIAAttribute(AOMRelationListProperty,
                                     HeapVector<Member<Element>>& result) const;
  virtual bool HasAOMPropertyOrARIAAttribute(AOMBooleanProperty,
                                             bool& result) const;
  bool AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty) const;
  bool AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty) const;
  bool HasAOMPropertyOrARIAAttribute(AOMUIntProperty, uint32_t& result) const;
  bool HasAOMPropertyOrARIAAttribute(AOMIntProperty, int32_t& result) const;
  bool HasAOMPropertyOrARIAAttribute(AOMFloatProperty, float& result) const;
  bool HasAOMPropertyOrARIAAttribute(AOMStringProperty,
                                     AtomicString& result) const;
  virtual AccessibleNode* GetAccessibleNode() const;

  void TokenVectorFromAttribute(Vector<String>&, const QualifiedName&) const;

  void GetSparseAXAttributes(AXSparseAttributeClient&) const;

  // Determine subclass type.
  virtual bool IsAXNodeObject() const { return false; }
  virtual bool IsAXLayoutObject() const { return false; }
  virtual bool IsAXInlineTextBox() const { return false; }
  virtual bool IsAXListBox() const { return false; }
  virtual bool IsAXListBoxOption() const { return false; }
  virtual bool IsAXRadioInput() const { return false; }
  virtual bool IsAXSVGRoot() const { return false; }

  // Check object role or purpose.
  virtual ax::mojom::Role RoleValue() const { return role_; }
  bool IsARIATextControl() const;
  virtual bool IsAnchor() const { return false; }
  bool IsButton() const;
  bool IsCanvas() const { return RoleValue() == ax::mojom::Role::kCanvas; }
  bool IsCheckbox() const { return RoleValue() == ax::mojom::Role::kCheckBox; }
  bool IsCheckboxOrRadio() const { return IsCheckbox() || IsRadioButton(); }
  bool IsColorWell() const {
    return RoleValue() == ax::mojom::Role::kColorWell;
  }
  virtual bool IsControl() const { return false; }
  virtual bool IsDefault() const { return false; }
  virtual bool IsEmbeddedObject() const { return false; }
  virtual bool IsFieldset() const { return false; }
  virtual bool IsHeading() const { return false; }
  virtual bool IsImage() const { return false; }
  virtual bool IsImageMapLink() const { return false; }
  virtual bool IsInputImage() const { return false; }
  bool IsLandmarkRelated() const;
  virtual bool IsLink() const { return false; }
  virtual bool IsInPageLinkTarget() const { return false; }
  virtual bool IsList() const { return false; }
  virtual bool IsMenu() const { return false; }
  virtual bool IsMenuButton() const { return false; }
  virtual bool IsMenuList() const { return false; }
  virtual bool IsMenuListOption() const { return false; }
  virtual bool IsMenuListPopup() const { return false; }
  bool IsMenuRelated() const;
  virtual bool IsMeter() const { return false; }
  virtual bool IsMockObject() const { return false; }
  virtual bool IsNativeSpinButton() const { return false; }
  virtual bool IsNativeTextControl() const {
    return false;
  }  // input or textarea
  virtual bool IsNonNativeTextControl() const {
    return false;
  }  // contenteditable or role=textbox
  virtual bool IsPasswordField() const { return false; }
  bool IsPasswordFieldAndShouldHideValue() const;
  bool IsPresentational() const {
    return RoleValue() == ax::mojom::Role::kNone ||
           RoleValue() == ax::mojom::Role::kPresentational;
  }
  virtual bool IsProgressIndicator() const { return false; }
  bool IsRadioButton() const {
    return RoleValue() == ax::mojom::Role::kRadioButton;
  }
  bool IsRange() const {
    return RoleValue() == ax::mojom::Role::kProgressIndicator ||
           RoleValue() == ax::mojom::Role::kScrollBar ||
           RoleValue() == ax::mojom::Role::kSlider ||
           RoleValue() == ax::mojom::Role::kSpinButton || IsMoveableSplitter();
  }
  bool IsScrollbar() const {
    return RoleValue() == ax::mojom::Role::kScrollBar;
  }
  virtual bool IsSlider() const { return false; }
  virtual bool IsNativeSlider() const { return false; }
  virtual bool IsMoveableSplitter() const { return false; }
  virtual bool IsSpinButton() const {
    return RoleValue() == ax::mojom::Role::kSpinButton;
  }
  bool IsTabItem() const { return RoleValue() == ax::mojom::Role::kTab; }
  virtual bool IsTextControl() const { return false; }
  bool IsTextObject() const;
  bool IsTree() const { return RoleValue() == ax::mojom::Role::kTree; }
  virtual bool IsValidationMessage() const { return false; }
  virtual bool IsVirtualObject() const { return false; }
  bool IsWebArea() const {
    return RoleValue() == ax::mojom::Role::kRootWebArea;
  }

  // Check object state.
  virtual bool IsAutofillAvailable() const { return false; }
  virtual bool IsClickable() const;
  virtual AccessibilityExpanded IsExpanded() const {
    return kExpandedUndefined;
  }
  virtual bool IsFocused() const { return false; }
  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  virtual AccessibilityGrabbedState IsGrabbed() const {
    return kGrabbedStateUndefined;
  }
  virtual bool IsHovered() const { return false; }
  virtual bool IsLineBreakingObject() const { return false; }
  virtual bool IsLinked() const { return false; }
  virtual bool IsLoaded() const { return false; }
  virtual bool IsModal() const { return false; }
  virtual bool IsMultiSelectable() const { return false; }
  virtual bool IsOffScreen() const { return false; }
  virtual bool IsRequired() const { return false; }
  virtual AccessibilitySelectedState IsSelected() const {
    return kSelectedStateUndefined;
  }
  // Is the object selected because selection is following focus?
  virtual bool IsSelectedFromFocus() const { return false; }
  virtual bool IsSelectedOptionActive() const { return false; }
  virtual bool IsVisible() const;
  virtual bool IsVisited() const { return false; }

  // Check whether certain properties can be modified.
  bool CanSetFocusAttribute() const;
  bool CanSetValueAttribute() const;

  // Whether objects are ignored, i.e. hidden from the AT.
  bool AccessibilityIsIgnored() const;
  // Whether objects are ignored but included in the tree.
  bool AccessibilityIsIgnoredButIncludedInTree() const;

  // Whether objects are included in the tree. Nodes that are included in the
  // tree are serialized, even if they are ignored. This allows browser-side
  // accessibility code to have a more accurate representation of the tree. e.g.
  // inspect hidden nodes referenced by labeled-by, know where line breaking
  // elements are, etc.
  bool AccessibilityIsIncludedInTree() const;
  typedef HeapVector<IgnoredReason> IgnoredReasons;
  virtual bool ComputeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const {
    return true;
  }
  bool AccessibilityIsIgnoredByDefault(IgnoredReasons* = nullptr) const;
  AXObjectInclusion AccessibilityPlatformIncludesObject() const;
  virtual AXObjectInclusion DefaultObjectInclusion(
      IgnoredReasons* = nullptr) const;
  bool IsInertOrAriaHidden() const;
  const AXObject* AriaHiddenRoot() const;
  bool ComputeIsInertOrAriaHidden(IgnoredReasons* = nullptr) const;
  bool IsDescendantOfLeafNode() const;
  AXObject* LeafNodeAncestor() const;
  bool IsDescendantOfDisabledNode() const;
  bool ComputeAccessibilityIsIgnoredButIncludedInTree() const;
  const AXObject* DatetimeAncestor(int max_levels_to_check = 3) const;
  const AXObject* DisabledAncestor() const;
  bool LastKnownIsIgnoredValue() const;
  void SetLastKnownIsIgnoredValue(bool);
  bool LastKnownIsIgnoredButIncludedInTreeValue() const;
  void SetLastKnownIsIgnoredButIncludedInTreeValue(bool);
  bool HasInheritedPresentationalRole() const;
  bool IsPresentationalChild() const;
  bool CanBeActiveDescendant() const;
  // Some objects, such as table cells, could be the children of more than one
  // object but have only one primary parent.
  bool HasIndirectChildren() const;

  //
  // Accessible name calculation
  //

  // Retrieves the accessible name of the object, an enum indicating where the
  // name was derived from, and a list of objects that were used to derive the
  // name, if any.
  virtual String GetName(ax::mojom::NameFrom&,
                         AXObjectVector* name_objects) const;

  typedef HeapVector<NameSource> NameSources;
  // Retrieves the accessible name of the object and a list of all potential
  // sources for the name, indicating which were used.
  String GetName(NameSources*) const;

  typedef HeapVector<DescriptionSource> DescriptionSources;
  // Takes the result of nameFrom from calling |name|, above, and retrieves the
  // accessible description of the object, which is secondary to |name|, an enum
  // indicating where the description was derived from, and a list of objects
  // that were used to derive the description, if any.
  virtual String Description(ax::mojom::NameFrom,
                             ax::mojom::DescriptionFrom&,
                             AXObjectVector* description_objects) const {
    return String();
  }

  // Same as above, but returns a list of all potential sources for the
  // description, indicating which were used.
  virtual String Description(ax::mojom::NameFrom,
                             ax::mojom::DescriptionFrom&,
                             DescriptionSources*,
                             AXRelatedObjectVector*) const {
    return String();
  }

  // Takes the result of nameFrom and descriptionFrom from calling |name| and
  // |description|, above, and retrieves the placeholder of the object, if
  // present and if it wasn't already exposed by one of the two functions above.
  virtual String Placeholder(ax::mojom::NameFrom) const { return String(); }

  // Takes the result of nameFrom and retrieves the HTML Title of the object,
  // if present and if it wasn't already exposed by |GetName| above.
  // HTML Title is typically used as a tooltip.
  virtual String Title(ax::mojom::NameFrom) const { return String(); }

  // Internal functions used by name and description, above.
  typedef HeapHashSet<Member<const AXObject>> AXObjectSet;
  virtual String TextAlternative(bool recursive,
                                 bool in_aria_labelled_by_traversal,
                                 AXObjectSet& visited,
                                 ax::mojom::NameFrom& name_from,
                                 AXRelatedObjectVector* related_objects,
                                 NameSources* name_sources) const {
    return String();
  }
  virtual String TextFromDescendants(AXObjectSet& visited,
                                     bool recursive) const {
    return String();
  }

  // Returns result of Accessible Name Calculation algorithm.
  // This is a simpler high-level interface to |name| used by Inspector.
  String ComputedName() const;

  // Internal function used to determine whether the result of calling |name| on
  // this object would return text that came from the an HTML label element or
  // not. This is intended to be faster than calling |name| or
  // |textAlternative|, and without side effects (it won't call
  // axObjectCache->getOrCreate).
  virtual bool NameFromLabelElement() const { return false; }

  //
  // Properties of static elements.
  //

  virtual const AtomicString& AccessKey() const { return g_null_atom; }
  RGBA32 BackgroundColor() const;
  virtual RGBA32 ComputeBackgroundColor() const { return Color::kTransparent; }
  virtual RGBA32 GetColor() const { return Color::kBlack; }
  // Used by objects of role ColorWellRole.
  virtual RGBA32 ColorValue() const { return Color::kTransparent; }
  virtual bool CanvasHasFallbackContent() const { return false; }
  virtual String FontFamily() const { return String(); }
  // Font size is in pixels.
  virtual float FontSize() const { return 0.0f; }
  virtual float FontWeight() const { return 0.0f; }
  // Value should be 1-based. 0 means not supported.
  virtual int HeadingLevel() const { return 0; }
  // Value should be 1-based. 0 means not supported.
  virtual unsigned HierarchicalLevel() const { return 0; }
  // Return the content of an image or canvas as an image data url in
  // PNG format. If |maxSize| is not empty and if the image is larger than
  // those dimensions, the image will be resized proportionally first to fit.
  virtual String ImageDataUrl(const IntSize& max_size) const {
    return g_null_atom;
  }
  virtual AXObject* InPageLinkTarget() const { return nullptr; }
  virtual AccessibilityOrientation Orientation() const;
  virtual ax::mojom::ListStyle GetListStyle() const {
    return ax::mojom::ListStyle::kNone;
  }
  virtual String GetText() const { return String(); }
  virtual ax::mojom::TextDirection GetTextDirection() const {
    return ax::mojom::TextDirection::kLtr;
  }
  virtual ax::mojom::TextPosition GetTextPosition() const {
    return ax::mojom::TextPosition::kNone;
  }
  virtual int TextLength() const { return 0; }

  virtual void GetTextStyleAndTextDecorationStyle(
      int32_t* text_style,
      ax::mojom::TextDecorationStyle* text_overline_style,
      ax::mojom::TextDecorationStyle* text_strikethrough_style,
      ax::mojom::TextDecorationStyle* text_underline_style) const {
    *text_style = 0;
    *text_overline_style = ax::mojom::TextDecorationStyle::kNone;
    *text_strikethrough_style = ax::mojom::TextDecorationStyle::kNone;
    *text_underline_style = ax::mojom::TextDecorationStyle::kNone;
  }

  virtual AXObjectVector RadioButtonsInGroup() const {
    return AXObjectVector();
  }
  virtual KURL Url() const { return KURL(); }
  virtual AXObject* ChooserPopup() const { return nullptr; }

  // Load inline text boxes for just this node, even if
  // settings->inlineTextBoxAccessibilityEnabled() is false.
  virtual void LoadInlineTextBoxes() {}

  // Walk the AXObjects on the same line. This is supported on any
  // object type but primarily intended to be used for inline text boxes.
  virtual AXObject* NextOnLine() const { return nullptr; }
  virtual AXObject* PreviousOnLine() const { return nullptr; }

  // For all node objects. The start and end character offset of each
  // marker, such as spelling or grammar error.
  virtual void Markers(Vector<DocumentMarker::MarkerType>&,
                       Vector<AXRange>&) const;
  // For an inline text box.
  // The integer horizontal pixel offset of each character in the string;
  // negative values for RTL.
  virtual void TextCharacterOffsets(Vector<int>&) const;
  // The start and end character offset of each word in the object's text.
  virtual void GetWordBoundaries(Vector<int>& word_starts,
                                 Vector<int>& word_ends) const;
  // Returns the text offset (text offset as in AXPosition, not as in
  // pixel offset) in the container of an inline text box.
  virtual unsigned TextOffsetInContainer(unsigned offset) const {
    return offset;
  }

  // Properties of interactive elements.
  ax::mojom::DefaultActionVerb Action() const;
  ax::mojom::CheckedState CheckedState() const;
  virtual ax::mojom::AriaCurrentState GetAriaCurrentState() const {
    return ax::mojom::AriaCurrentState::kNone;
  }
  virtual ax::mojom::InvalidState GetInvalidState() const {
    return ax::mojom::InvalidState::kNone;
  }
  // Only used when invalidState() returns InvalidStateOther.
  virtual String AriaInvalidValue() const { return String(); }
  virtual String ValueDescription() const { return String(); }
  virtual bool ValueForRange(float* out_value) const { return false; }
  virtual bool MaxValueForRange(float* out_value) const { return false; }
  virtual bool MinValueForRange(float* out_value) const { return false; }
  virtual bool StepValueForRange(float* out_value) const { return false; }
  virtual String StringValue() const { return String(); }
  virtual AXRestriction Restriction() const;

  // ARIA attributes.
  virtual ax::mojom::Role DetermineAccessibilityRole();
  ax::mojom::Role DetermineAriaRoleAttribute() const;
  virtual ax::mojom::Role AriaRoleAttribute() const;
  virtual AXObject* ActiveDescendant() { return nullptr; }
  virtual String AutoComplete() const { return String(); }
  virtual void AriaOwnsElements(AXObjectVector& owns) const {}
  virtual void AriaDescribedbyElements(AXObjectVector&) const {}
  virtual AXObject* ErrorMessage() const { return nullptr; }
  virtual ax::mojom::HasPopup HasPopup() const {
    return ax::mojom::HasPopup::kFalse;
  }
  virtual bool IsEditable() const { return false; }
  bool IsEditableRoot() const;
  virtual bool ComputeIsEditableRoot() const { return false; }
  virtual bool IsMultiline() const { return false; }
  virtual bool IsRichlyEditable() const { return false; }
  bool AriaCheckedIsPresent() const;
  bool AriaPressedIsPresent() const;
  bool HasGlobalARIAAttribute() const;
  bool SupportsARIAExpanded() const;
  virtual bool SupportsARIADragging() const { return false; }
  virtual void Dropeffects(Vector<ax::mojom::Dropeffect>& dropeffects) const {}
  virtual bool SupportsARIAFlowTo() const { return false; }
  virtual bool SupportsARIAOwns() const { return false; }
  bool SupportsRangeValue() const;
  bool SupportsARIAReadOnly() const;

  // Returns 0-based index.
  int IndexInParent() const;

  // Value should be 1-based. 0 means not supported.
  virtual int PosInSet() const { return 0; }
  virtual int SetSize() const { return 0; }
  bool SupportsARIASetSizeAndPosInSet() const;

  // ARIA live-region features.
  bool IsLiveRegionRoot() const;  // Any live region, including polite="off".
  bool IsActiveLiveRegionRoot() const;  // Live region that is not polite="off".
  AXObject* LiveRegionRoot() const;  // Container that controls live politeness.
  virtual const AtomicString& LiveRegionStatus() const { return g_null_atom; }
  virtual const AtomicString& LiveRegionRelevant() const { return g_null_atom; }
  bool LiveRegionAtomic() const;

  const AtomicString& ContainerLiveRegionStatus() const;
  const AtomicString& ContainerLiveRegionRelevant() const;
  bool ContainerLiveRegionAtomic() const;
  bool ContainerLiveRegionBusy() const;

  // Every object's bounding box is returned relative to a
  // container object (which is guaranteed to be an ancestor) and
  // optionally a transformation matrix that needs to be applied too.
  // To compute the absolute bounding box of an element, start with its
  // boundsInContainer and apply the transform. Then as long as its container is
  // not null, walk up to its container and offset by the container's offset
  // from origin, the container's scroll position if any, and apply the
  // container's transform.  Do this until you reach the root of the tree.
  // If the object clips its children, for example by having overflow:hidden,
  // set |clips_children| to true.
  virtual void GetRelativeBounds(AXObject** out_container,
                                 FloatRect& out_bounds_in_container,
                                 SkMatrix44& out_container_transform,
                                 bool* clips_children = nullptr) const;

  FloatRect LocalBoundingBoxRectForAccessibility();

  // Get the bounds in frame-relative coordinates as a LayoutRect.
  LayoutRect GetBoundsInFrameCoordinates() const;

  // Explicitly set an object's bounding rect and offset container.
  void SetElementRect(LayoutRect r, AXObject* container) {
    explicit_element_rect_ = r;
    explicit_container_id_ = container->AXObjectID();
  }

  // Hit testing.
  // Called on the root AX object to return the deepest available element.
  virtual AXObject* AccessibilityHitTest(const IntPoint&) const {
    return nullptr;
  }
  // Called on the AX object after the layout tree determines which is the right
  // AXLayoutObject.
  virtual AXObject* ElementAccessibilityHitTest(const IntPoint&) const;

  // High-level accessibility tree access. Other modules should only use these
  // functions.
  AncestorsIterator AncestorsBegin();
  AncestorsIterator AncestorsEnd();
  InOrderTraversalIterator GetInOrderTraversalIterator();
  int ChildCount() const;
  const AXObjectVector& Children() const;
  const AXObjectVector& Children();
  // Returns the first child for this object.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* FirstChild() const;
  // Returns the last child for this object.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* LastChild() const;
  // Returns the deepest first child for this object.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* DeepestFirstChild() const;
  // Returns the deepest last child for this object.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* DeepestLastChild() const;
  bool IsAncestorOf(const AXObject&) const;
  bool IsDescendantOf(const AXObject&) const;
  // Next sibling for this object, where the sibling may be
  // an accessibility ignored object.
  // Works for all nodes that are included in the accessibility tree,
  // and may return nodes that are accessibility ignored.
  AXObject* NextSiblingIncludingIgnored() const;
  // Previous sibling for this object, where the sibling may be
  // an accessibility ignored object.
  // Works for all nodes that are included in the accessibility tree,
  // and may return nodes that are accessibility ignored.
  AXObject* PreviousSiblingIncludingIgnored() const;
  // Returns the next object in tree using depth-first pre-order traversal,
  // optionally staying within a specified AXObject.
  // Works for all nodes that are included in the accessibility tree,
  // and may return nodes that are accessibility ignored.
  AXObject* NextInPreOrderIncludingIgnored(
      const AXObject* within = nullptr) const;
  // Returns the previous object in tree using depth-first pre-order traversal,
  // optionally staying within a specified AXObject.
  // Works for all nodes that are included in the accessibility tree,
  // and may return nodes that are accessibility ignored.
  AXObject* PreviousInPreOrderIncludingIgnored(
      const AXObject* within = nullptr) const;
  // Returns the previous object in tree using depth-first post-order traversal,
  // optionally staying within a specified AXObject.
  // Works for all nodes that are included in the accessibility tree,
  // and may return nodes that are accessibility ignored.
  AXObject* PreviousInPostOrderIncludingIgnored(
      const AXObject* within = nullptr) const;
  // Next sibling for this object that's not accessibility ignored.
  // Flattens accessibility ignored nodes, so the sibling will have the
  // same unignored parent, but may have a different parent in tree.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* NextSibling() const;
  // Previous sibling for this object that's not accessibility ignored.
  // Flattens accessibility ignored nodes, so the sibling will have the
  // same unignored parent, but may have a different parent in tree.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* PreviousSibling() const;
  // Next object in tree using depth-first pre-order traversal that's
  // not accessibility ignored.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* NextInTreeObject() const;
  // Previous object in tree using depth-first pre-order traversal that's
  // not accessibility ignored.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* PreviousInTreeObject() const;
  // Get or create the parent of this object.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* ParentObject() const;
  // Get the parent of this object if it has already been created.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* ParentObjectIfExists() const;
  virtual AXObject* ComputeParent() const = 0;
  virtual AXObject* ComputeParentIfExists() const { return nullptr; }
  AXObject* CachedParentObject() const { return parent_; }
  // Get or create the first ancestor that's not accessibility ignored.
  // Works for all nodes.
  AXObject* ParentObjectUnignored() const;
  // Get or create the first ancestor that's included in the accessibility tree.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* ParentObjectIncludedInTree() const;
  AXObject* ContainerWidget() const;
  bool IsContainerWidget() const;

  // Low-level accessibility tree exploration, only for use within the
  // accessibility module.
  virtual AXObject* RawFirstChild() const { return nullptr; }
  virtual AXObject* RawNextSibling() const { return nullptr; }
  virtual void AddChildren() {}
  virtual bool CanHaveChildren() const { return true; }
  bool HasChildren() const { return have_children_; }
  virtual void UpdateChildrenIfNecessary();
  virtual bool NeedsToUpdateChildren() const { return false; }
  virtual void SetNeedsToUpdateChildren() {}
  virtual void ClearChildren();
  void DetachFromParent() { parent_ = nullptr; }
  void AddAccessibleNodeChildren();
  virtual void SelectedOptions(AXObjectVector&) const {}

  // Properties of the object's owning document or page.
  virtual double EstimatedLoadingProgress() const { return 0; }

  // DOM and layout tree access.
  virtual Node* GetNode() const { return nullptr; }
  Element* GetElement() const;  // Same as GetNode, if it's an Element.
  virtual LayoutObject* GetLayoutObject() const { return nullptr; }
  virtual Document* GetDocument() const;
  virtual LocalFrameView* DocumentFrameView() const;
  virtual Element* AnchorElement() const { return nullptr; }
  virtual Element* ActionElement() const { return nullptr; }
  virtual AtomicString Language() const;
  bool HasAttribute(const QualifiedName&) const;
  const AtomicString& GetAttribute(const QualifiedName&) const;

  // Scrollable containers.
  bool IsScrollableContainer() const;
  IntPoint GetScrollOffset() const;
  IntPoint MinimumScrollOffset() const;
  IntPoint MaximumScrollOffset() const;
  void SetScrollOffset(const IntPoint&) const;

  // Tables and grids.
  bool IsTableLikeRole() const;
  bool IsTableRowLikeRole() const;
  bool IsTableCellLikeRole() const;
  virtual bool IsDataTable() const { return false; }

  // For a table.
  virtual unsigned ColumnCount() const;
  virtual unsigned RowCount() const;
  virtual void ColumnHeaders(AXObjectVector&) const;
  virtual void RowHeaders(AXObjectVector&) const;
  virtual AXObject* CellForColumnAndRow(unsigned column, unsigned row) const;

  // For a cell.
  virtual unsigned ColumnIndex() const;
  virtual unsigned RowIndex() const;
  virtual unsigned ColumnSpan() const;
  virtual unsigned RowSpan() const;
  unsigned AriaColumnIndex() const;
  unsigned AriaRowIndex() const;
  int AriaColumnCount() const;
  int AriaRowCount() const;
  virtual ax::mojom::SortDirection GetSortDirection() const {
    return ax::mojom::SortDirection::kNone;
  }

  // For a row or column.
  virtual AXObject* HeaderObject() const { return nullptr; }

  // If this object itself scrolls, return its ScrollableArea.
  virtual ScrollableArea* GetScrollableAreaIfScrollable() const {
    return nullptr;
  }

  // Modify or take an action on an object.
  //
  // These are the public interfaces, called from outside of Blink.
  // Each one first tries to fire an Accessibility Object Model event,
  // if applicable, and if that isn't handled, falls back on the
  // native implementation via a virtual member function, below.
  //
  // For example, |RequestIncrementAction| fires the AOM event and if
  // that isn't handled it calls |DoNativeIncrement|.
  //
  // These all return true if handled.
  bool RequestDecrementAction();
  bool RequestClickAction();
  bool RequestFocusAction();
  bool RequestIncrementAction();
  bool RequestScrollToGlobalPointAction(const IntPoint&);
  bool RequestScrollToMakeVisibleAction();
  bool RequestScrollToMakeVisibleWithSubFocusAction(
      const IntRect&,
      blink::ScrollAlignment horizontal_scroll_alignment,
      blink::ScrollAlignment vertical_scroll_alignment);
  bool RequestSetSelectedAction(bool);
  bool RequestSetSequentialFocusNavigationStartingPointAction();
  bool RequestSetValueAction(const String&);
  bool RequestShowContextMenuAction();

  // These are actions, just like the actions above, and they allow us
  // to keep track of nodes that gain or lose accessibility focus, but
  // this isn't exposed to the open web so they're explicitly marked as
  // internal so it's clear that these should not dispatch DOM events.
  bool InternalClearAccessibilityFocusAction();
  bool InternalSetAccessibilityFocusAction();

  // Native implementations of actions that aren't handled by AOM
  // event listeners. These all return true if handled.
  virtual bool OnNativeDecrementAction();
  virtual bool OnNativeClickAction();
  virtual bool OnNativeFocusAction();
  virtual bool OnNativeIncrementAction();
  bool OnNativeScrollToGlobalPointAction(const IntPoint&) const;
  bool OnNativeScrollToMakeVisibleAction() const;
  bool OnNativeScrollToMakeVisibleWithSubFocusAction(
      const IntRect&,
      blink::ScrollAlignment horizontal_scroll_alignment,
      blink::ScrollAlignment vertical_scroll_alignment) const;
  virtual bool OnNativeSetSelectedAction(bool);
  virtual bool OnNativeSetSequentialFocusNavigationStartingPointAction();
  virtual bool OnNativeSetValueAction(const String&);
  bool OnNativeShowContextMenuAction();

  // Notifications that this object may have changed.
  virtual void ChildrenChanged() {}
  virtual void HandleActiveDescendantChanged() {}
  virtual void HandleAutofillStateChanged(WebAXAutofillState) {}
  virtual void HandleAriaExpandedChanged() {}
  virtual void SelectionChanged();
  virtual void TextChanged() {}

  // Static helper functions.
  static bool IsARIAControl(ax::mojom::Role);
  static bool IsARIAInput(ax::mojom::Role);
  // Is this a widget that requires container widget.
  bool IsSubWidget() const;
  static ax::mojom::Role AriaRoleToWebCoreRole(const String&);
  static const AtomicString& RoleName(ax::mojom::Role);
  static const AtomicString& InternalRoleName(ax::mojom::Role);
  static void AccessibleNodeListToElementVector(const AccessibleNodeList&,
                                                HeapVector<Member<Element>>&);

  // Given two AX objects, returns the lowest common ancestor and the child
  // indices in that ancestor corresponding to the branch under which each
  // object is to be found. If the lowest common ancestor is the same as either
  // of the objects, the corresponding index is set to -1 to indicate this.
  static const AXObject* LowestCommonAncestor(const AXObject& first,
                                              const AXObject& second,
                                              int* index_in_ancestor1,
                                              int* index_in_ancestor2);

  // Returns a string representation of this object.
  String ToString() const;

 protected:
  AXID id_;
  AXObjectVector children_;
  mutable bool have_children_;
  ax::mojom::Role role_;
  ax::mojom::Role aria_role_;
  mutable AXObjectInclusion last_known_is_ignored_value_;
  mutable AXObjectInclusion last_known_is_ignored_but_included_in_tree_value_;
  LayoutRect explicit_element_rect_;
  AXID explicit_container_id_;

  // Used only inside textAlternative():
  static String CollapseWhitespace(const String&);
  static String RecursiveTextAlternative(const AXObject&,
                                         bool in_aria_labelled_by_traversal,
                                         AXObjectSet& visited);
  static String RecursiveTextAlternative(const AXObject&,
                                         bool in_aria_labelled_by_traversal,
                                         AXObjectSet& visited,
                                         ax::mojom::NameFrom& name_from);
  bool IsHiddenForTextAlternativeCalculation() const;
  String AriaTextAlternative(bool recursive,
                             bool in_aria_labelled_by_traversal,
                             AXObjectSet& visited,
                             ax::mojom::NameFrom&,
                             AXRelatedObjectVector*,
                             NameSources*,
                             bool* found_text_alternative) const;
  String TextFromElements(bool in_aria_labelled_by_traversal,
                          AXObjectSet& visited,
                          HeapVector<Member<Element>>& elements,
                          AXRelatedObjectVector* related_objects) const;
  void ElementsFromAttribute(HeapVector<Member<Element>>& elements,
                             const QualifiedName&,
                             Vector<String>& ids) const;
  void AriaLabelledbyElementVector(HeapVector<Member<Element>>& elements,
                                   Vector<String>& ids) const;
  String TextFromAriaLabelledby(AXObjectSet& visited,
                                AXRelatedObjectVector* related_objects,
                                Vector<String>& ids) const;
  String TextFromAriaDescribedby(AXRelatedObjectVector* related_objects,
                                 Vector<String>& ids) const;
  virtual const AXObject* InheritsPresentationalRoleFrom() const {
    return nullptr;
  }

  bool CanReceiveAccessibilityFocus() const;
  bool NameFromContents(bool recursive) const;
  bool NameFromSelectedOption(bool recursive) const;

  ax::mojom::Role ButtonRoleType() const;

  virtual LayoutObject* LayoutObjectForRelativeBounds() const {
    return nullptr;
  }

  bool CanSetSelectedAttribute() const;
  const AXObject* InertRoot() const;

  // Returns true if the event was handled.
  bool DispatchEventToAOMEventListeners(Event&);

  // Finds table, table row, and table cell parents and children
  // skipping over generic containers.
  AXObjectVector TableRowChildren() const;
  AXObjectVector TableCellChildren() const;
  const AXObject* TableRowParent() const;
  const AXObject* TableParent() const;

  mutable Member<AXObject> parent_;

  // The following cached attribute values (the ones starting with m_cached*)
  // are only valid if m_lastModificationCount matches
  // AXObjectCacheImpl::modificationCount().
  mutable int last_modification_count_;
  mutable RGBA32 cached_background_color_;
  mutable bool cached_is_ignored_ : 1;
  mutable bool cached_is_ignored_but_included_in_tree_ : 1;

  mutable bool cached_is_inert_or_aria_hidden_ : 1;
  mutable bool cached_is_descendant_of_leaf_node_ : 1;
  mutable bool cached_is_descendant_of_disabled_node_ : 1;
  mutable bool cached_has_inherited_presentational_role_ : 1;
  mutable bool cached_is_editable_root_;
  mutable Member<AXObject> cached_live_region_root_;
  mutable int cached_aria_column_index_;
  mutable int cached_aria_row_index_;
  mutable FloatRect cached_local_bounding_box_rect_for_accessibility_;

  Member<AXObjectCacheImpl> ax_object_cache_;

  // Updates the cached attribute values. This may be recursive, so to prevent
  // deadlocks,
  // functions called here may only search up the tree (ancestors), not down.
  void UpdateCachedAttributeValuesIfNeeded() const;

 private:
  void UpdateDistributionForFlatTreeTraversal() const;
  bool IsARIAControlledByTextboxWithActiveDescendant() const;
  bool AncestorExposesActiveDescendant() const;
  bool IsCheckable() const;
  static bool IsNativeCheckboxInMixedState(const Node*);
  static bool IncludesARIAWidgetRole(const String&);
  static bool HasInteractiveARIAAttribute(const Element&);
  ax::mojom::Role RemapAriaRoleDueToParent(ax::mojom::Role) const;
  unsigned ComputeAriaColumnIndex() const;
  unsigned ComputeAriaRowIndex() const;
  bool HasInternalsAttribute(Element&, const QualifiedName&) const;
  const AtomicString& GetInternalsAttribute(Element&,
                                            const QualifiedName&) const;

  static unsigned number_of_live_ax_objects_;

  DISALLOW_COPY_AND_ASSIGN(AXObject);
};

MODULES_EXPORT bool operator==(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator!=(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator<(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator<=(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator>(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator>=(const AXObject& first, const AXObject& second);
MODULES_EXPORT std::ostream& operator<<(std::ostream&, const AXObject&);

#define DEFINE_AX_OBJECT_TYPE_CASTS(thisType, predicate)           \
  DEFINE_TYPE_CASTS(thisType, AXObject, object, object->predicate, \
                    object.predicate)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_
