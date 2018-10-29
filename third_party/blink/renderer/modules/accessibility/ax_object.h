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
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/inspector/protocol/Accessibility.h"
#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
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

class NameSourceRelatedObject
    : public GarbageCollectedFinalized<NameSourceRelatedObject> {
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

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::IgnoredReason);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::NameSource);
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::DescriptionSource);

namespace blink {

class MODULES_EXPORT AXObject : public GarbageCollectedFinalized<AXObject> {
 public:
  typedef HeapVector<Member<AXObject>> AXObjectVector;

  struct AXSelection {
    DISALLOW_NEW();
    // The deepest descendant in which the range starts.
    // (nullptr means the current object.)
    Persistent<AXObject> anchor_object;
    // The number of characters and child objects in the anchor object
    // before the range starts.
    int anchor_offset;
    // When the same character offset could correspond to two possible
    // cursor positions, upstream means it's on the previous line rather
    // than the next line.
    TextAffinity anchor_affinity;

    // The deepest descendant in which the range ends.
    // (nullptr means the current object.)
    Persistent<AXObject> focus_object;
    // The number of characters and child objects in the focus object
    // before the range ends.
    int focus_offset;
    // When the same character offset could correspond to two possible
    // cursor positions, upstream means it's on the previous line rather
    // than the next line.
    TextAffinity focus_affinity;

    AXSelection()
        : anchor_object(nullptr),
          anchor_offset(-1),
          anchor_affinity(TextAffinity::kUpstream),
          focus_object(nullptr),
          focus_offset(-1),
          focus_affinity(TextAffinity::kDownstream) {}

    AXSelection(int start_offset, int end_offset)
        : anchor_object(nullptr),
          anchor_offset(start_offset),
          anchor_affinity(TextAffinity::kUpstream),
          focus_object(nullptr),
          focus_offset(end_offset),
          focus_affinity(TextAffinity::kDownstream) {}

    AXSelection(AXObject* anchor_object,
                int anchor_offset,
                TextAffinity anchor_affinity,
                AXObject* focus_object,
                int focus_offset,
                TextAffinity focus_affinity)
        : anchor_object(anchor_object),
          anchor_offset(anchor_offset),
          anchor_affinity(anchor_affinity),
          focus_object(focus_object),
          focus_offset(focus_offset),
          focus_affinity(focus_affinity) {}

    bool IsValid() const {
      return ((anchor_object && focus_object) ||
              (!anchor_object && !focus_object)) &&
             anchor_offset >= 0 && focus_offset >= 0;
    }

    // Determines if the range only refers to text offsets under the current
    // object.
    bool IsSimple() const {
      return anchor_object == focus_object || !anchor_object || !focus_object;
    }
  };

  // Iterator for doing an in-order traversal of the accessibility tree.
  // Includes ignored objects in the traversal.
  class MODULES_EXPORT InOrderTraversalIterator final
      : public GarbageCollectedFinalized<InOrderTraversalIterator> {
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
      : public GarbageCollectedFinalized<AncestorsIterator> {
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

  virtual void GetSparseAXAttributes(AXSparseAttributeClient&) const;

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
  virtual bool IsARIARow() const { return false; }
  virtual bool IsAnchor() const { return false; }
  bool IsButton() const;
  bool IsCanvas() const { return RoleValue() == ax::mojom::Role::kCanvas; }
  bool IsCheckbox() const { return RoleValue() == ax::mojom::Role::kCheckBox; }
  bool IsCheckboxOrRadio() const { return IsCheckbox() || IsRadioButton(); }
  bool IsColorWell() const {
    return RoleValue() == ax::mojom::Role::kColorWell;
  }
  virtual bool IsControl() const { return false; }
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
  virtual bool IsPasswordFieldAndShouldHideValue() const;
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
  virtual bool IsVirtualObject() const { return false; }
  bool IsWebArea() const {
    return RoleValue() == ax::mojom::Role::kRootWebArea;
  }

  // Check object state.
  virtual bool IsAutofillAvailable() { return false; }
  virtual bool IsClickable() const;
  virtual AccessibilityExpanded IsExpanded() const {
    return kExpandedUndefined;
  }
  virtual bool IsFocused() const { return false; }
  virtual bool IsHovered() const { return false; }
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
  virtual bool IsVisible() const { return true; }
  virtual bool IsVisited() const { return false; }

  // Check whether certain properties can be modified.
  virtual bool CanSetFocusAttribute() const;
  bool CanSetValueAttribute() const;

  // Whether objects are ignored, i.e. not included in the tree.
  bool AccessibilityIsIgnored() const;
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
  const AXObject* DatetimeAncestor(int max_levels_to_check = 3) const;
  const AXObject* DisabledAncestor() const;
  bool LastKnownIsIgnoredValue() const;
  void SetLastKnownIsIgnoredValue(bool);
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
  virtual String GetName(NameSources*) const;

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
  virtual AtomicString FontFamily() const { return g_null_atom; }
  // Font size is in pixels.
  virtual float FontSize() const { return 0.0f; }
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
  virtual String GetText() const { return String(); }
  virtual ax::mojom::TextDirection GetTextDirection() const {
    return ax::mojom::TextDirection::kLtr;
  }
  virtual ax::mojom::TextPosition GetTextPosition() const {
    return ax::mojom::TextPosition::kNone;
  }
  virtual int TextLength() const { return 0; }
  virtual TextStyle GetTextStyle() const { return kTextStyleNone; }
  virtual AXObjectVector RadioButtonsInGroup() const {
    return AXObjectVector();
  }
  virtual KURL Url() const { return KURL(); }

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
  virtual void GetWordBoundaries(Vector<AXRange>&) const;

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
  virtual String AriaAutoComplete() const { return String(); }
  virtual void AriaOwnsElements(AXObjectVector& owns) const {}
  virtual void AriaDescribedbyElements(AXObjectVector&) const {}
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
  virtual bool SupportsARIADropping() const { return false; }
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
  bool IsLiveRegion() const;
  AXObject* LiveRegionRoot() const;
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
  AXObject* FirstChild() const;
  AXObject* LastChild() const;
  AXObject* DeepestFirstChild() const;
  AXObject* DeepestLastChild() const;
  bool IsAncestorOf(const AXObject&) const;
  bool IsDescendantOf(const AXObject&) const;
  AXObject* NextSibling() const;
  AXObject* PreviousSibling() const;
  // Next object in tree using depth-first pre-order traversal.
  AXObject* NextInTreeObject(bool can_wrap_to_first_element = false) const;
  // Previous object in tree using depth-first pre-order traversal.
  AXObject* PreviousInTreeObject(bool can_wrap_to_last_element = false) const;
  AXObject* ParentObject() const;
  AXObject* ParentObjectIfExists() const;
  virtual AXObject* ComputeParent() const = 0;
  virtual AXObject* ComputeParentIfExists() const { return nullptr; }
  AXObject* CachedParentObject() const { return parent_; }
  AXObject* ParentObjectUnignored() const;
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
  virtual void DetachFromParent() { parent_ = nullptr; }
  virtual AXObject* ScrollBar(AccessibilityOrientation) { return nullptr; }
  virtual void AddAccessibleNodeChildren();
  virtual void SelectedOptions(AXObjectVector&) const {}

  // Properties of the object's owning document or page.
  virtual double EstimatedLoadingProgress() const { return 0; }

  // DOM and layout tree access.
  virtual Node* GetNode() const { return nullptr; }
  virtual Element* GetElement() const;  // Same as GetNode, if it's an Element.
  virtual LayoutObject* GetLayoutObject() const { return nullptr; }
  virtual Document* GetDocument() const;
  virtual LocalFrameView* DocumentFrameView() const;
  virtual Element* AnchorElement() const { return nullptr; }
  virtual Element* ActionElement() const { return nullptr; }
  virtual AtomicString Language() const;
  bool HasAttribute(const QualifiedName&) const;
  const AtomicString& GetAttribute(const QualifiedName&) const;

  // Methods that retrieve or manipulate the current selection.

  // Get the current selection from anywhere in the accessibility tree.
  virtual AXSelection Selection() const { return AXSelection(); }
  // Gets only the start and end offsets of the selection computed using the
  // current object as the starting point. Returns a null selection if there is
  // no selection in the subtree rooted at this object.
  virtual AXSelection SelectionUnderObject() const { return AXSelection(); }

  // Scrollable containers.
  bool IsScrollableContainer() const;
  IntPoint GetScrollOffset() const;
  IntPoint MinimumScrollOffset() const;
  IntPoint MaximumScrollOffset() const;
  void SetScrollOffset(const IntPoint&) const;

  // Tables and grids.
  virtual bool IsTableLikeRole() const;
  virtual bool IsTableRowLikeRole() const;
  virtual bool IsTableCellLikeRole() const;
  virtual bool IsDataTable() const { return false; }
  virtual bool IsTableCol() const { return false; }

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
  virtual unsigned AriaColumnIndex() const;
  virtual unsigned AriaRowIndex() const;
  virtual int AriaColumnCount() const;
  virtual int AriaRowCount() const;
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
  bool RequestScrollToMakeVisibleWithSubFocusAction(const IntRect&);
  bool RequestSetSelectedAction(bool);
  bool RequestSetSelectionAction(const AXSelection&);
  bool RequestSetSequentialFocusNavigationStartingPointAction();
  bool RequestSetValueAction(const String&);
  bool RequestShowContextMenuAction();

  // These are actions, just like the actions above, and they allow us
  // to keep track of nodes that gain or lose accessibility focus, but
  // this isn't exposed to the open web so they're explicitly marked as
  // internal so it's clear that these should not dispatch DOM events.
  virtual bool InternalClearAccessibilityFocusAction();
  virtual bool InternalSetAccessibilityFocusAction();

  // Native implementations of actions that aren't handled by AOM
  // event listeners. These all return true if handled.
  virtual bool OnNativeDecrementAction();
  virtual bool OnNativeClickAction();
  virtual bool OnNativeFocusAction();
  virtual bool OnNativeIncrementAction();
  virtual bool OnNativeScrollToGlobalPointAction(const IntPoint&) const;
  virtual bool OnNativeScrollToMakeVisibleAction() const;
  virtual bool OnNativeScrollToMakeVisibleWithSubFocusAction(
      const IntRect&) const;
  virtual bool OnNativeSetSelectedAction(bool);
  virtual bool OnNativeSetSelectionAction(const AXSelection&);
  virtual bool OnNativeSetSequentialFocusNavigationStartingPointAction();
  virtual bool OnNativeSetValueAction(const String&);
  virtual bool OnNativeShowContextMenuAction();

  // Notifications that this object may have changed.
  virtual void ChildrenChanged() {}
  virtual void HandleActiveDescendantChanged() {}
  virtual void HandleAutofillStateChanged(bool) {}
  virtual void HandleAriaExpandedChanged() {}
  virtual void SelectionChanged();
  virtual void TextChanged() {}

  // Text metrics. Most of these should be deprecated, needs major cleanup.
  virtual VisiblePosition VisiblePositionForIndex(int) const;
  int LineForPosition(const VisiblePosition&) const;
  virtual int Index(const VisiblePosition&) const { return -1; }
  virtual void LineBreaks(Vector<int>&) const {}

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

 protected:
  AXID id_;
  AXObjectVector children_;
  mutable bool have_children_;
  ax::mojom::Role role_;
  ax::mojom::Role aria_role_;
  mutable AXObjectInclusion last_known_is_ignored_value_;
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

  virtual bool CanSetSelectedAttribute() const;
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
  bool IsARIAControlledByTextboxWithActiveDescendant() const;
  bool AncestorExposesActiveDescendant() const;
  bool IsCheckable() const;
  static bool IsNativeCheckboxInMixedState(const Node*);
  static bool IncludesARIAWidgetRole(const String&);
  static bool HasInteractiveARIAAttribute(const Element&);
  ax::mojom::Role RemapAriaRoleDueToParent(ax::mojom::Role) const;
  unsigned ComputeAriaColumnIndex() const;
  unsigned ComputeAriaRowIndex() const;

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
