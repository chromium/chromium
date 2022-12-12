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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_

#include <ostream>
#include <utility>

#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/inspector/protocol/accessibility.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/quad_f.h"

namespace gfx {
class Transform;
}

namespace ui {
struct AXActionData;
struct AXNodeData;
struct AXRelativeBounds;
}

namespace blink {

class AccessibleNodeList;
class AXObject;
class AXObjectCacheImpl;
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

class IgnoredReason {
  DISALLOW_NEW();

 public:
  AXIgnoredReason reason;
  Member<const AXObject> related_object;

  explicit IgnoredReason(AXIgnoredReason reason)
      : reason(reason), related_object(nullptr) {}

  IgnoredReason(AXIgnoredReason r, const AXObject* obj)
      : reason(r), related_object(obj) {}

  void Trace(Visitor* visitor) const { visitor->Trace(related_object); }
};

class NameSourceRelatedObject final
    : public GarbageCollected<NameSourceRelatedObject> {
 public:
  WeakMember<AXObject> object;
  String text;

  NameSourceRelatedObject(AXObject* object, String text)
      : object(object), text(text) {}

  NameSourceRelatedObject(const NameSourceRelatedObject&) = delete;
  NameSourceRelatedObject& operator=(const NameSourceRelatedObject&) = delete;

  void Trace(Visitor* visitor) const { visitor->Trace(object); }
};

typedef HeapVector<Member<NameSourceRelatedObject>> AXRelatedObjectVector;
class NameSource {
  DISALLOW_NEW();

 public:
  String text;
  bool superseded = false;
  bool invalid = false;
  ax::mojom::blink::NameFrom type = ax::mojom::blink::NameFrom::kNone;
  const QualifiedName& attribute;
  AtomicString attribute_value;
  AXTextSource native_source = kAXTextFromNativeSourceUninitialized;
  AXRelatedObjectVector related_objects;

  NameSource(bool superseded, const QualifiedName& attr)
      : superseded(superseded), attribute(attr) {}

  explicit NameSource(bool superseded)
      : superseded(superseded), attribute(QualifiedName::Null()) {}

  void Trace(Visitor* visitor) const { visitor->Trace(related_objects); }
};

class DescriptionSource {
  DISALLOW_NEW();

 public:
  String text;
  bool superseded = false;
  bool invalid = false;
  ax::mojom::blink::DescriptionFrom type =
      ax::mojom::blink::DescriptionFrom::kNone;
  const QualifiedName& attribute;
  AtomicString attribute_value;
  AXTextSource native_source = kAXTextFromNativeSourceUninitialized;
  AXRelatedObjectVector related_objects;

  DescriptionSource(bool superseded, const QualifiedName& attr)
      : superseded(superseded), attribute(attr) {}

  explicit DescriptionSource(bool superseded)
      : superseded(superseded), attribute(QualifiedName::Null()) {}

  void Trace(Visitor* visitor) const { visitor->Trace(related_objects); }
};

// Returns a ax::mojom::blink::MarkerType cast to an int, suitable
// for serializing into AXNodeData.
int32_t ToAXMarkerType(DocumentMarker::MarkerType marker_type);

// Returns a ax::mojom::blink::HighlightType cast to an int, suitable
// for serializing into AXNodeData.
int32_t ToAXHighlightType(const AtomicString& highlight_type);

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::IgnoredReason)
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::NameSource)
WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::DescriptionSource)

namespace blink {

class MODULES_EXPORT AXObject : public GarbageCollected<AXObject> {
 public:
  typedef HeapVector<Member<AXObject>> AXObjectVector;

  // Iterator for the ancestors of an |AXObject|.
  // Walks through all the unignored parents of the object up to the root.
  // Does not include the object itself in the list of ancestors.
  class MODULES_EXPORT AncestorsIterator final
      : public GarbageCollected<AncestorsIterator> {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = AXObject;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

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

    void Trace(Visitor* visitor) const { visitor->Trace(current_); }

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
  explicit AXObject(AXObjectCacheImpl&);

#if DCHECK_IS_ON()
  bool is_initializing_ = false;
  bool is_computing_role_ = false;
  mutable bool is_updating_cached_values_ = false;
#endif

#if defined(AX_FAIL_FAST_BUILD)
  bool is_adding_children_ = false;
  mutable bool is_computing_text_from_descendants_ = false;
#endif

 public:
  AXObject(const AXObject&) = delete;
  AXObject& operator=(const AXObject&) = delete;

  virtual ~AXObject();
  virtual void Trace(Visitor*) const;

  static unsigned NumberOfLiveAXObjects() { return number_of_live_ax_objects_; }

  // After constructing an AXObject, it must be given a
  // unique ID, then added to AXObjectCacheImpl, and finally Init() must
  // be called last.
  void SetAXObjectID(AXID ax_object_id) { id_ = ax_object_id; }
  // Initialize the object and set the |parent|, which can only be null for the
  // root of the tree.
  virtual void Init(AXObject* parent);

  // When the corresponding WebCore object that this AXObject
  // wraps is deleted, it must be detached.
  virtual void Detach();
  bool IsDetached() const;

  // Updates the cached attribute values. This may be recursive, so to prevent
  // deadlocks, functions called here may only search up the tree (ancestors),
  // not down.
  // Fires children change on the parent if the node's ignored or included in
  // tree status changes. Use |notify_parent_of_ignored_changes = false| to
  // prevent this.
  void UpdateCachedAttributeValuesIfNeeded(
      bool notify_parent_of_ignored_changes = true) const;

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

  static void TokenVectorFromAttribute(Element* element,
                                       Vector<String>&,
                                       const QualifiedName&);

  // Serialize the properties of this node into |node_data|.
  //
  // TODO(crbug.com/1068668): AX onion soup - finish migrating
  // BlinkAXTreeSource::SerializeNode into AXObject::Serialize.
  void Serialize(ui::AXNodeData* node_data, ui::AXMode accessibility_mode);

  // Determine subclass type.
  virtual bool IsImageMapLink() const;
  virtual bool IsAXNodeObject() const;
  virtual bool IsAXLayoutObject() const;
  virtual bool IsAXInlineTextBox() const;
  virtual bool IsList() const;
  virtual bool IsAXListBox() const;
  virtual bool IsAXListBoxOption() const;
  virtual bool IsMenuList() const;
  virtual bool IsMenuListOption() const;
  virtual bool IsMenuListPopup() const;
  virtual bool IsMockObject() const;
  virtual bool IsProgressIndicator() const;
  virtual bool IsAXRadioInput() const;
  virtual bool IsSlider() const;
  virtual bool IsValidationMessage() const;
  virtual bool IsVirtualObject() const;

  // Check object role or purpose.
  ax::mojom::blink::Role RoleValue() const;

  // This method is useful in cases where the final role exposed to ATs needs
  // to change based on contextual information. For instance, an svgRoot should
  // be exposed as an image if it lacks accessible children. Whether or not it
  // has accessible children is not known at the time the role is assigned and
  // may depend on whether or not a given platform includes children that other
  // platforms ignore.
  ax::mojom::blink::Role ComputeFinalRoleForSerialization() const;

  // Returns true if this object is an ARIA text field, i.e. it is neither an
  // <input> nor a <textarea>, but it has an ARIA role of textbox, searchbox or
  // (on certain platforms) combobox.
  bool IsARIATextField() const;

  bool IsButton() const;
  bool IsCanvas() const;
  bool IsColorWell() const;
  virtual bool IsControl() const;
  virtual bool IsDefault() const;
  virtual bool IsFieldset() const;
  bool IsHeading() const;
  bool IsImage() const;
  virtual bool IsInputImage() const;
  bool IsLink() const;
  bool IsMenu() const;
  bool IsMenuRelated() const;
  bool IsMeter() const;
  virtual bool IsNativeImage() const;
  virtual bool IsNativeSpinButton() const;

  // Returns true if this object is an input element of a text field type, such
  // as type="text" or type="tel", or a textarea.
  bool IsAtomicTextField() const;

  // Returns true if this object is not an <input> or a <textarea>, and is
  // either a contenteditable, or has the CSS user-modify style set to something
  // editable.
  bool IsNonAtomicTextField() const;

  // Returns true if this object is a text field that is used for entering
  // passwords, i.e. <input type=password>.
  bool IsPasswordField() const;

  // Returns true if this object is a text field that is used for entering
  // passwords, but its input should be masked from the user.
  bool IsPasswordFieldAndShouldHideValue() const;

  bool IsPresentational() const;
  bool IsRangeValueSupported() const;
  bool IsScrollbar() const;
  virtual bool IsNativeSlider() const;
  virtual bool IsSpinButton() const;
  bool IsTabItem() const;

  // This object is a text field. This is any widget in which the user should be
  // able to enter and edit text.
  //
  // Examples include <input type="text">, <input type="password">, <textarea>,
  // <div contenteditable="true">, <div role="textbox">, <div role="searchbox">
  // and <div role="combobox">. Note that when an ARIA role that indicates that
  // the widget is editable is used, such as "role=textbox", the element doesn't
  // need to be contenteditable for this method to return true, as in theory
  // JavaScript could be used to implement editing functionality. In practice,
  // this situation should be rare.
  bool IsTextField() const;

  bool IsTextObject() const;
  bool IsTree() const { return RoleValue() == ax::mojom::blink::Role::kTree; }
  bool IsWebArea() const {
    return RoleValue() == ax::mojom::blink::Role::kRootWebArea;
  }

  // Check object state.
  virtual bool IsAutofillAvailable() const;
  virtual bool IsClickable() const;
  // Is |this| disabled for any of the follow reasons:
  // * aria-disabled
  // * disabled form control
  // * a focusable descendant of a disabled container
  bool IsDisabled() const;
  virtual AccessibilityExpanded IsExpanded() const;
  virtual bool IsFocused() const;
  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  virtual AccessibilityGrabbedState IsGrabbed() const;
  virtual bool IsHovered() const;

  // Returns true if this object starts a new paragraph in the accessibility
  // tree's text representation.
  virtual bool IsLineBreakingObject() const;

  virtual bool IsLinked() const;
  virtual bool IsLoaded() const;
  virtual bool IsModal() const;
  virtual bool IsMultiSelectable() const;
  virtual bool IsOffScreen() const;
  virtual bool IsRequired() const;
  virtual AccessibilitySelectedState IsSelected() const;
  virtual bool IsSelectedFromFocusSupported() const;
  // Is the object selected because selection is following focus?
  virtual bool IsSelectedFromFocus() const;
  virtual bool IsSelectedOptionActive() const;
  virtual bool IsNotUserSelectable() const;
  virtual bool IsVisible() const;
  virtual bool IsVisited() const;

  // Check whether value can be modified.
  bool CanSetValueAttribute() const;

  // Is the element focusable?
  bool CanSetFocusAttribute() const;
  // Is the element in the tab order?
  bool IsKeyboardFocusable() const;

  // Whether objects are ignored, i.e. hidden from the AT.
  bool AccessibilityIsIgnored() const;
  // Whether objects are ignored but included in the tree.
  bool AccessibilityIsIgnoredButIncludedInTree() const;
  // Is visibility:hidden or display:none being used to hide this element.
  bool IsHiddenViaStyle() const;

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
  virtual AXObjectInclusion DefaultObjectInclusion(
      IgnoredReasons* = nullptr) const;
  bool IsInert() const;
  bool IsAriaHidden() const;
  bool CachedIsAriaHidden() { return cached_is_aria_hidden_; }
  const AXObject* AriaHiddenRoot() const;
  bool ComputeIsInert(IgnoredReasons* = nullptr) const;
  bool ComputeIsAriaHidden(IgnoredReasons* = nullptr) const;
  bool IsBlockedByAriaModalDialog(IgnoredReasons* = nullptr) const;
  bool IsDescendantOfDisabledNode() const;
  bool ComputeAccessibilityIsIgnoredButIncludedInTree() const;
  const AXObject* GetAtomicTextFieldAncestor(int max_levels_to_check = 3) const;
  const AXObject* DatetimeAncestor(int max_levels_to_check = 3) const;
  bool ComputeIsDescendantOfDisabledNode() const;
  bool LastKnownIsIgnoredValue() const;
  bool LastKnownIsIgnoredButIncludedInTreeValue() const;
  bool LastKnownIsIncludedInTreeValue() const;
  // Some objects, such as table header containers, could be the children of
  // more than one object but have only one primary parent.
  bool HasIndirectChildren() const;

  //
  // Accessible name calculation
  //

  // Retrieves the accessible name of the object, an enum indicating where the
  // name was derived from, and a list of objects that were used to derive the
  // name, if any.
  virtual String GetName(ax::mojom::blink::NameFrom&,
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
  virtual String Description(ax::mojom::blink::NameFrom,
                             ax::mojom::blink::DescriptionFrom&,
                             AXObjectVector* description_objects) const {
    return String();
  }

  // Same as above, but returns a list of all potential sources for the
  // description, indicating which were used.
  virtual String Description(ax::mojom::blink::NameFrom,
                             ax::mojom::blink::DescriptionFrom&,
                             DescriptionSources*,
                             AXRelatedObjectVector*) const {
    return String();
  }

  // Takes the result of nameFrom and descriptionFrom from calling |name| and
  // |description|, above, and retrieves the placeholder of the object, if
  // present and if it wasn't already exposed by one of the two functions above.
  virtual String Placeholder(ax::mojom::blink::NameFrom) const {
    return String();
  }

  // Takes the result of nameFrom and retrieves the HTML Title of the object,
  // if present and if it wasn't already exposed by |GetName| above.
  // HTML Title is typically used as a tooltip.
  virtual String Title(ax::mojom::blink::NameFrom) const { return String(); }

  // Internal functions used by name and description, above.
  typedef HeapHashSet<Member<const AXObject>> AXObjectSet;
  virtual String TextAlternative(bool recursive,
                                 const AXObject* aria_label_or_description_root,
                                 AXObjectSet& visited,
                                 ax::mojom::blink::NameFrom& name_from,
                                 AXRelatedObjectVector* related_objects,
                                 NameSources* name_sources) const {
    return String();
  }
  virtual String TextFromDescendants(
      AXObjectSet& visited,
      const AXObject* aria_label_or_description_root,
      bool recursive) const {
    return String();
  }

  // Returns result of Accessible Name Calculation algorithm.
  // This is a simpler high-level interface to |name| used by Inspector.
  String ComputedName() const;

  // Internal function used to determine whether the element supports deriving
  // its accessible name from its descendants. The result of calling |GetName|
  // may be derived by other means even when this returns true.
  // This is intended to be faster than calling |GetName| or
  // |TextAlternative|, and without side effects (it won't call
  // AXObjectCache->GetOrCreate).
  bool SupportsNameFromContents(bool recursive) const;

  //
  // Properties of static elements.
  //

  virtual const AtomicString& AccessKey() const { return g_null_atom; }
  virtual RGBA32 BackgroundColor() const { return Color::kTransparent.Rgb(); }
  virtual RGBA32 GetColor() const { return Color::kBlack.Rgb(); }
  // Used by objects of role ColorWellRole.
  virtual RGBA32 ColorValue() const { return Color::kTransparent.Rgb(); }
  virtual bool CanvasHasFallbackContent() const { return false; }
  // Returns the font family that was cascaded onto ComputedStyle. This may
  // contain non-user-friendly internal names.
  virtual const AtomicString& ComputedFontFamily() const { return g_null_atom; }
  // Returns the font family used on this platform. This is a user friendly name
  // that is appropriate for serialization.
  virtual String FontFamilyForSerialization() const { return String(); }
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
  virtual String ImageDataUrl(const gfx::Size& max_size) const {
    return g_null_atom;
  }
  // If this element points to another element in the same page, e.g.
  // <a href="#foo">, this will return the AXObject for the target.
  // The object returned should be unignored. If necessary, it will return
  // a descendant of the actual target.
  virtual AXObject* InPageLinkTarget() const { return nullptr; }
  virtual AccessibilityOrientation Orientation() const;
  virtual ax::mojom::blink::ListStyle GetListStyle() const {
    return ax::mojom::blink::ListStyle::kNone;
  }
  virtual ax::mojom::blink::TextAlign GetTextAlign() const {
    return ax::mojom::blink::TextAlign::kNone;
  }
  virtual ax::mojom::blink::WritingDirection GetTextDirection() const {
    return ax::mojom::blink::WritingDirection::kLtr;
  }
  virtual float GetTextIndent() const { return 0.0f; }
  virtual ax::mojom::blink::TextPosition GetTextPosition() const {
    return ax::mojom::blink::TextPosition::kNone;
  }

  virtual void GetTextStyleAndTextDecorationStyle(
      int32_t* text_style,
      ax::mojom::blink::TextDecorationStyle* text_overline_style,
      ax::mojom::blink::TextDecorationStyle* text_strikethrough_style,
      ax::mojom::blink::TextDecorationStyle* text_underline_style) const {
    *text_style = 0;
    *text_overline_style = ax::mojom::blink::TextDecorationStyle::kNone;
    *text_strikethrough_style = ax::mojom::blink::TextDecorationStyle::kNone;
    *text_underline_style = ax::mojom::blink::TextDecorationStyle::kNone;
  }

  virtual AXObject* GetChildFigcaption() const;

  virtual AXObjectVector RadioButtonsInGroup() const {
    return AXObjectVector();
  }
  virtual KURL Url() const { return KURL(); }
  virtual AXObject* ChooserPopup() const { return nullptr; }

  // Load inline text boxes for just this node, even if
  // settings->inlineTextBoxAccessibilityEnabled() is false.
  virtual void LoadInlineTextBoxes();
  virtual void ForceAddInlineTextBoxChildren();

  // Walk the AXObjects on the same line.
  virtual AXObject* NextOnLine() const;
  virtual AXObject* PreviousOnLine() const;

  // Searches the object's ancestors for an aria-invalid attribute of type
  // spelling or grammar, and returns a document marker representing the value
  // of this attribute. As an optimization, goes up until the deepest line
  // breaking object which, in most cases, is the paragraph containing this
  // object.
  absl::optional<const DocumentMarker::MarkerType>
  GetAriaSpellingOrGrammarMarker() const;

  // For all inline text objects: Returns the horizontal pixel offset of each
  // character in the object's text, rounded to the nearest integer. Negative
  // values are returned for RTL text.
  virtual void TextCharacterOffsets(Vector<int>&) const;

  // For all inline text boxes: Returns the start and end character offset of
  // each word in the object's text.
  virtual void GetWordBoundaries(Vector<int>& word_starts,
                                 Vector<int>& word_ends) const;

  // For all inline text boxes and atomic text fields: Returns the length of the
  // inline's text or the field's value respectively.
  virtual int TextLength() const;

  // Supported on layout inline, layout text, layout replaced, and layout block
  // flow, provided that they are at inline-level, i.e. "display=inline" or
  // "display=inline-block". Also supported on atomic text fields. For all other
  // object types, returns |offset|.
  //
  // For layout inline, text, replaced, and block flow: Translates the given
  // character offset to the equivalent offset in the object's formatting
  // context. The formatting context is the deepest block flow ancestor,
  // (excluding the current object), e.g. the containing paragraph. If this
  // object is somehow not a descendant of a block flow in the layout tree,
  // returns |offset|.
  //
  // For example, if this object is a span, and |offset| is 0, this method would
  // return the number of characters, excluding any collapsed white space found
  // in the DOM, from the start of the layout inline's deepest block flow
  // ancestor, e.g. the beginning of the paragraph in which the span is found.
  //
  // For atomic text fields: Simply returns |offset|, because atomic text fields
  // have no collapsed white space and so no translation from a DOM to an
  // accessible text offset is necessary. An atomic text field does not expose
  // its internal implementation to assistive software, appearing as a single
  // leaf node in the accessibility tree. It includes <input> and <textarea>.
  virtual int TextOffsetInFormattingContext(int offset) const;

  // For all inline text boxes and atomic text fields. For all other object
  // types, returns |offset|.
  //
  // For inline text boxes: Translates the given character offset to the
  // equivalent offset in the object's static text or line break parent. If this
  // object is somehow not a descendant of a block flow in the layout tree,
  // returns the given offset.
  //
  // For example, if the given offset is 0, this would return the number of
  // characters, excluding any collapsed white space found in the DOM, from the
  // start of the inline text box's static text parent.
  //
  // For atomic text fields: Simply returns |offset|, because atomic text fields
  // have no collapsed white space and so no translation from a DOM to an
  // accessible text offset is necessary.
  virtual int TextOffsetInContainer(int offset) const;

  // Properties of interactive elements.
  ax::mojom::blink::DefaultActionVerb Action() const;
  ax::mojom::blink::CheckedState CheckedState() const;
  virtual ax::mojom::blink::AriaCurrentState GetAriaCurrentState() const {
    return ax::mojom::blink::AriaCurrentState::kNone;
  }
  virtual ax::mojom::blink::InvalidState GetInvalidState() const {
    return ax::mojom::blink::InvalidState::kNone;
  }
  // Only used when invalidState() returns InvalidStateOther.
  virtual String AriaInvalidValue() const { return String(); }
  virtual bool ValueForRange(float* out_value) const { return false; }
  virtual bool MaxValueForRange(float* out_value) const { return false; }
  virtual bool MinValueForRange(float* out_value) const { return false; }
  virtual bool StepValueForRange(float* out_value) const { return false; }

  // Returns the value of a control such as a plain text field, a content
  // editable, a submit button, a slider, a progress bar, a scroll bar, a meter,
  // a spinner, a <select> element, a date picker or an ARIA combo box. In order
  // to improve performance during our cross-process communication with the
  // browser, we avoid computing the value of a content editable from its inner
  // text. (See `AXObject::SlowGetValueForControlIncludingContentEditable()`.)
  // For range controls, such as sliders and scroll bars, the value of
  // aria-valuetext takes priority over the value of aria-valuenow.
  virtual String GetValueForControl() const;

  // Similar to `AXObject::GetValueForControl()` above, but also computes the
  // value of a content editable from its inner text. Sending this value to the
  // browser process might be slow if the content editable has a lot of content.
  // So, we should prefer computing the value of a content editable on the
  // browser side.
  virtual String SlowGetValueForControlIncludingContentEditable() const;

  virtual AXRestriction Restriction() const;

  // ARIA attributes.
  virtual ax::mojom::blink::Role DetermineAccessibilityRole();
  // Determine the ARIA role purely based on the role attribute, when no
  // additional rules or limitations on role usage are applied.
  ax::mojom::blink::Role RawAriaRole() const;
  // Determine the ARIA role after post-processing on the raw ARIA role.
  ax::mojom::blink::Role DetermineAriaRoleAttribute() const;
  virtual ax::mojom::blink::Role AriaRoleAttribute() const;
  bool HasAriaAttribute(bool does_undo_role_presentation = false) const;
  virtual AXObject* ActiveDescendant() { return nullptr; }
  virtual String AutoComplete() const { return String(); }
  virtual void AriaOwnsElements(AXObjectVector& owns) const {}
  virtual void AriaDescribedbyElements(AXObjectVector&) const {}
  virtual AXObject* ErrorMessage() const { return nullptr; }

  // Determines whether this object has an associated popup menu, list, or grid,
  // such as in the case of an ARIA combobox or when the browser offers an
  // autocomplete suggestion.
  virtual ax::mojom::blink::HasPopup HasPopup() const;

  // Heuristic to get the listbox for an <input role="combobox">.
  AXObject* GetControlsListboxForTextfieldCombobox();

  // Returns true if this object is within or at the root of an editable region,
  // such as a contenteditable. Also, returns true if this object is an atomic
  // text field, i.e. an input or a textarea. Note that individual subtrees
  // within an editable region could be made non-editable via e.g.
  // contenteditable="false".
  bool IsEditable() const;

  // Returns true if this object is at the root of an editable region, such as a
  // contenteditable. Does not return true if this object is an atomic text
  // field, i.e. an input or a textarea.
  //
  // https://w3c.github.io/editing/execCommand.html#editing-host
  virtual bool IsEditableRoot() const;

  // Returns true if this object has contenteditable="true" or
  // contenteditable="plaintext-only".
  virtual bool HasContentEditableAttributeSet() const;

  // Returns true if the user can enter multiple lines of text inside this
  // editable region. By default, textareas and content editables can accept
  // multiple lines of text.
  bool IsMultiline() const;

  // Same as `IsEditable()` but returns whether the region accepts rich text
  // as well.
  bool IsRichlyEditable() const;

  bool AriaCheckedIsPresent() const;
  bool AriaPressedIsPresent() const;
  bool SupportsARIAExpanded() const;
  virtual bool SupportsARIADragging() const { return false; }
  virtual void Dropeffects(
      Vector<ax::mojom::blink::Dropeffect>& dropeffects) const {}
  bool SupportsARIAReadOnly() const;

  // Returns 0-based index.
  int IndexInParent() const;

  // Value should be 1-based. 0 means not supported.
  virtual int PosInSet() const { return 0; }
  virtual int SetSize() const { return 0; }
  bool SupportsARIASetSizeAndPosInSet() const;

  // Returns true if the attribute is prohibited (e.g. by ARIA), and we plan
  // to enforce that prohibition. An example of something prohibited that we
  // do not enforce is aria-label/aria-labelledby on certain text containers.
  bool IsProhibited(ax::mojom::blink::StringAttribute attribute) const;
  bool IsProhibited(ax::mojom::blink::IntAttribute attribute) const;

  // ARIA live-region features.
  bool IsLiveRegionRoot() const;  // Any live region, including polite="off".
  bool IsActiveLiveRegionRoot() const;  // Live region that is not polite="off".
  AXObject* LiveRegionRoot() const;  // Container that controls live politeness.
  virtual const AtomicString& LiveRegionStatus() const;
  virtual const AtomicString& LiveRegionRelevant() const;
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
                                 gfx::RectF& out_bounds_in_container,
                                 gfx::Transform& out_container_transform,
                                 bool* clips_children = nullptr) const;

  gfx::RectF LocalBoundingBoxRectForAccessibility();

  // Get the bounds in frame-relative coordinates as a LayoutRect.
  LayoutRect GetBoundsInFrameCoordinates() const;

  // Explicitly set an object's bounding rect and offset container.
  void SetElementRect(LayoutRect r, AXObject* container) {
    explicit_element_rect_ = r;
    explicit_container_id_ = container->AXObjectID();
  }

  // Hit testing.
  // Called on the root AX object to return the deepest available element.
  virtual AXObject* AccessibilityHitTest(const gfx::Point&) const {
    return nullptr;
  }
  // Called on the AX object after the layout tree determines which is the right
  // AXLayoutObject.
  AXObject* ElementAccessibilityHitTest(const gfx::Point&) const;

  //
  // High-level accessibility tree access. Other modules should only use these
  // methods.
  //
  // The following methods may support one or more kinds of objects. There are
  // three kinds: Objects that are excluded from the accessibility tree by
  // default, such as white space found in HTML, objects that are included in
  // the tree but that are ignored, such as an empty div, and unignored objects.

  // Iterates through the node's unignored ancestors up to the root, starting
  // from the node's unignored parent, i.e. does not include the node itself in
  // the list of ancestors.
  //
  // Initially, it can be called on all nodes, including those that are
  // accessibility ignored, but only traverses through the list of ancestors
  // that are unignored and included in the accessibility tree.
  AncestorsIterator UnignoredAncestorsBegin() const;
  AncestorsIterator UnignoredAncestorsEnd() const;

  // Returns the number of children, including children that are included in the
  // accessibility tree but are accessibility ignored.
  //
  // Can be called on all nodes, even on nodes that are excluded from the
  // accessibility tree.
  int ChildCountIncludingIgnored() const;

  // Returns the child with the given index in the list of all children,
  // including those that are accessibility ignored.
  //
  // Can be called on all nodes, even on nodes that are excluded from the
  // accessibility tree.
  AXObject* ChildAtIncludingIgnored(int index) const;

  // Returns the node's children, including any children that are included in
  // the accessibility tree but are accessibility ignored.
  //
  // Can be called on all nodes, including nodes that are excluded from the
  // accessibility tree.
  const AXObjectVector& ChildrenIncludingIgnored() const;
  const AXObjectVector& ChildrenIncludingIgnored();

  // Returns the node's unignored descendants that are one level deeper than
  // this node, after removing all accessibility ignored nodes from the tree.
  //
  // Flattens accessibility ignored nodes, so each unignored child will have the
  // same unignored parent, but may have a different parent in tree.
  //
  // Can be called on all nodes that are included in the accessibility tree,
  // including those that are accessibility ignored.
  // TODO(accessibility) This actually returns ignored children when they are
  // included in the tree. A better name would be ChildrenIncludedInTree().
  const AXObjectVector UnignoredChildren() const;
  const AXObjectVector UnignoredChildren();

  // Returns the first child for this object.
  // Works for all nodes that are included in the accessibility tree, and may
  // return nodes that are accessibility ignored.
  AXObject* FirstChildIncludingIgnored() const;

  // Returns the last child for this object.
  // Works for all nodes that are included in the accessibility tree, and may
  // return nodes that are accessibility ignored.
  AXObject* LastChildIncludingIgnored() const;

  // Returns the deepest first child for this object.
  // Works for all nodes that are included in the accessibility tree, and may
  // return nodes that are accessibility ignored.
  AXObject* DeepestFirstChildIncludingIgnored() const;

  // Returns the deepest last child for this object.
  // Works for all nodes that are included in the accessibility tree, and may
  // return nodes that are accessibility ignored.
  AXObject* DeepestLastChildIncludingIgnored() const;

  // Returns true if this node is strictly an ancestor of the given node, i.e.
  // doesn't include the current node in the list of its ancestors. Works for
  // all nodes that are included in the accessibility tree, including nodes that
  // are accessibility ignored.
  bool IsAncestorOf(const AXObject&) const;

  // Returns true if this node is strictly a descendant of the given node, i.e.
  // doesn't include the current node in the list of its descendants. Works for
  // all nodes that are included in the accessibility tree, including nodes that
  // are accessibility ignored.
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

  // Returns the number of children that are not accessibility ignored.
  //
  // Unignored children are the objects that are one level deeper than the
  // current object after all accessibility ignored descendants are removed.
  //
  // Can be called on all nodes that are included in the accessibility tree,
  // including those that are accessibility ignored.
  int UnignoredChildCount() const;

  // Returns the unignored child with the given index.
  //
  // Unignored children are the objects that are one level deeper than the
  // current object after all accessibility ignored descendants are removed.
  //
  // Can be called on all nodes that are included in the accessibility tree,
  // including those that are accessibility ignored.
  AXObject* UnignoredChildAt(int index) const;

  // Next sibling for this object that's not accessibility ignored.
  //
  // Flattens accessibility ignored nodes, so the sibling will have the
  // same unignored parent, but may have a different parent in tree.
  //
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* UnignoredNextSibling() const;

  // Previous sibling for this object that's not accessibility ignored.
  //
  // Flattens accessibility ignored nodes, so the sibling will have the
  // same unignored parent, but may have a different parent in tree.
  //
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* UnignoredPreviousSibling() const;

  // Next object in tree using depth-first pre-order traversal that's
  // not accessibility ignored.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* UnignoredNextInPreOrder() const;

  // Previous object in tree using depth-first pre-order traversal that's
  // not accessibility ignored.
  // Doesn't work with nodes that are accessibility ignored.
  AXObject* UnignoredPreviousInPreOrder() const;

  // Get or create the parent of this object.
  //
  // Works for all nodes, and may return nodes that are accessibility ignored,
  // including nodes that might not be in the tree.
  AXObject* ParentObject() const;

  // Get the parent of this object if it has already been created.
  // Works for all nodes, and may return nodes that are accessibility ignored,
  // including nodes that might not be in the tree.
  AXObject* CachedParentObject() const { return parent_; }

  // Get the current unignored children without refreshing them, even if
  // children_dirty_ aka NeedsToUpdateChildren() is true.
  const AXObjectVector& CachedChildrenIncludingIgnored() const {
    return children_;
  }

  // Sets the parent AXObject directly. If the parent of this object is known,
  // this can be faster than using ComputeParent().
  void SetParent(AXObject* new_parent) const;

  // If parent was not initialized during AddChildren() it can be computed by
  // walking the DOM (or layout for nodeless aka anonymous layout object).
  // ComputeParent() adds DCHECKs to ensure that it is not being called when
  // an attached parent_ is already cached, and that it is possible to compute
  // the parent. It calls ComputeParentImpl() for the actual work.
  AXObject* ComputeParent() const;

  // Same as ComputeParent, but does not assert if there is no parent to compute
  // (i.e. because the parent does not belong to the tree anymore).
  AXObject* ComputeParentOrNull() const;

  // Can this node be used to compute the natural parent of an object?
  // These are objects that can have some children, but the children are
  // only of a certain type or from another part of the tree, and therefore
  // the parent-child relationships are not natural and must be handled
  // specially. For example, a <select> may be an innapropriate natural parent
  // for all of its child nodes as determined by LayoutTreeBuilderTraversal,
  // such as an <optgroup> or <div> in the shadow DOM, because an AXMenuList, if
  // used, only allows <option>/AXMenuListOption children.
  static bool CanComputeAsNaturalParent(Node*);

  // For a given image, return a <map> that's actually used for it.
  static HTMLMapElement* GetMapForImage(Node* image);

  // Compute the AXObject parent for the given node or layout_object.
  // The layout object is only necessary if the node is null, which is the case
  // only for pseudo elements. ** Does not take aria-owns into account. **
  static AXObject* ComputeNonARIAParent(AXObjectCacheImpl& cache,
                                        Node* node,
                                        LayoutObject* layout_object = nullptr);

  // Compute parent for an AccessibleNode, which is not backed up a DOM node
  // or layout object.
  static AXObject* ComputeAccessibleNodeParent(AXObjectCacheImpl& cache,
                                               AccessibleNode& accessible_node);

  // Returns true if |parent_| is null and not at the root.
  bool IsMissingParent() const;

  // Compute a missing parent, and ask it to update children.
  // Must only be called if IsMissingParent() is true.
  void RepairMissingParent() const;

  // Is this the root of this object hierarchy.
  bool IsRoot() const;

#if DCHECK_IS_ON()
  // When the parent on children during AddChildren(), take the opportunity to
  // check out ComputeParent() implementation. It should match.
  void EnsureCorrectParentComputation();
#endif

  // Get or create the first ancestor that's not accessibility ignored.
  // Works for all nodes.
  AXObject* ParentObjectUnignored() const;

  // Get or create the first ancestor that's included in the accessibility tree.
  // Works for all nodes, and may return nodes that are accessibility ignored.
  AXObject* ParentObjectIncludedInTree() const;

  AXObject* ContainerWidget() const;
  bool IsContainerWidget() const;

  AXObject* ContainerListMarkerIncludingIgnored() const;

  // There are two types of traversal for obtaining children:
  // 1. LayoutTreeBuilderTraversal. Despite the name, this traverses a flattened
  // DOM tree that includes pseudo element children such as ::before, and where
  // shadow DOM slotting has been run.
  // 2. LayoutObject traversal. This is necessary if there is no parent node,
  // or in a pseudo element subtree.
  bool ShouldUseLayoutObjectTraversalForChildren() const;
  virtual bool CanHaveChildren() const { return true; }
  void UpdateChildrenIfNecessary();
  bool NeedsToUpdateChildren() const;
  virtual void SetNeedsToUpdateChildren() const;
  virtual void ClearChildren() const;
  void DetachFromParent() { parent_ = nullptr; }
  virtual void SelectedOptions(AXObjectVector&) const {}

  // Properties of the object's owning document or page.
  virtual double EstimatedLoadingProgress() const { return 0; }
  virtual AXObject* RootScroller() const;

  //
  // DOM and layout tree access.
  //

  // Returns the associated DOM node or, if an associated layout object is
  // present, the node of the associated layout object.
  //
  // If this object is associated with generated content, or a list marker,
  // returns a pseudoelement. It does not return the node that generated the
  // content or the list marker.
  virtual Node* GetNode() const;

  // Returns the associated layout object if any.
  virtual LayoutObject* GetLayoutObject() const;

  // Returns the same as `AXObject::GetNode()` if the node is an Element,
  // otherwise returns nullptr.
  Element* GetElement() const;

  virtual Document* GetDocument() const = 0;
  LocalFrameView* DocumentFrameView() const;
  virtual Element* AnchorElement() const { return nullptr; }
  virtual Element* ActionElement() const { return nullptr; }
  virtual AtomicString Language() const;
  virtual bool HasAttribute(const QualifiedName&) const { return false; }
  virtual const AtomicString& GetAttribute(const QualifiedName&) const {
    return g_null_atom;
  }

  // Scrollable containers.
  bool IsScrollableContainer() const;
  bool IsUserScrollable() const;  // Only true if actual scrollbars are present.
  gfx::Point GetScrollOffset() const;
  gfx::Point MinimumScrollOffset() const;
  gfx::Point MaximumScrollOffset() const;
  void Scroll(ax::mojom::blink::Action scroll_action) const;
  void SetScrollOffset(const gfx::Point&) const;

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
  virtual ax::mojom::blink::SortDirection GetSortDirection() const {
    return ax::mojom::blink::SortDirection::kNone;
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
  //
  // Note: we're migrating to have PerformAction() be the only public
  // interface, the others will all be private.
  bool PerformAction(const ui::AXActionData&);
  bool RequestDecrementAction();
  bool RequestClickAction();
  bool RequestFocusAction();
  bool RequestIncrementAction();
  bool RequestScrollToGlobalPointAction(const gfx::Point&);
  bool RequestScrollToMakeVisibleAction();
  bool RequestScrollToMakeVisibleWithSubFocusAction(
      const gfx::Rect&,
      blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
      blink::mojom::blink::ScrollAlignment vertical_scroll_alignment);
  bool RequestSetSelectedAction(bool);
  bool RequestSetSequentialFocusNavigationStartingPointAction();
  bool RequestSetValueAction(const String&);
  bool RequestShowContextMenuAction();

  // These are actions, just like the actions above, and they allow us
  // to keep track of nodes that gain or lose accessibility focus, but
  // this isn't exposed to the open web so they're explicitly marked as
  // internal so it's clear that these should not dispatch DOM events.
  virtual bool InternalSetAccessibilityFocusAction();
  virtual bool InternalClearAccessibilityFocusAction();

  // Native implementations of actions that aren't handled by AOM
  // event listeners. These all return true if handled.
  virtual bool OnNativeDecrementAction();
  virtual bool OnNativeClickAction();
  virtual bool OnNativeFocusAction();
  virtual bool OnNativeIncrementAction();
  bool OnNativeScrollToGlobalPointAction(const gfx::Point&) const;
  bool OnNativeScrollToMakeVisibleAction() const;
  bool OnNativeScrollToMakeVisibleWithSubFocusAction(
      const gfx::Rect&,
      blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
      blink::mojom::blink::ScrollAlignment vertical_scroll_alignment) const;
  virtual bool OnNativeSetSelectedAction(bool);
  virtual bool OnNativeSetSequentialFocusNavigationStartingPointAction();
  virtual bool OnNativeSetValueAction(const String&);
  bool OnNativeShowContextMenuAction();

  // Notifications that this object may have changed.
  virtual void ChildrenChangedWithCleanLayout() {}
  virtual void HandleActiveDescendantChanged() {}
  virtual void HandleAutofillStateChanged(WebAXAutofillState) {}
  virtual void HandleAriaExpandedChanged() {}

  // Static helper functions.
  // TODO(accessibility) Move these to a static helper util class.
  static bool IsARIAControl(ax::mojom::blink::Role);
  static bool IsARIAInput(ax::mojom::blink::Role);
  static bool IsFrame(const Node*);
  static bool HasARIAOwns(Element* element);
  // Should this own a child tree (e.g. an iframe).
  virtual bool IsChildTreeOwner() const { return false; }
  // Is this a widget that requires container widget.
  bool IsSubWidget() const;
  static ax::mojom::blink::Role AriaRoleStringToRoleEnum(const String&);

  // Return the equivalent ARIA name for an enumerated role, or g_null_atom.
  static const AtomicString& ARIARoleName(ax::mojom::blink::Role);

  // Return the equivalent internal role name as a string.
  static const String InternalRoleName(ax::mojom::blink::Role);

  // Return a role name, preferring the ARIA over the internal name.
  // Optional boolean out param |*is_internal| will be false if the role matches
  // an ARIA role, and true if an internal role name is used (no ARIA mapping).
  static const String RoleName(ax::mojom::blink::Role,
                               bool* is_internal = nullptr);

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

  // Blink-internal DOM Node ID. Currently used for PDF exporting.
  int GetDOMNodeId() const;

  bool IsHiddenForTextAlternativeCalculation(
      const AXObject* aria_label_or_description_root) const;

  // What should the role be assuming an ARIA role is not present?
  virtual ax::mojom::blink::Role NativeRoleIgnoringAria() const = 0;

  // Get the role to be used in StringAttribute::kRole, which is used in the
  // xml-roles object attribute.
  const AtomicString& GetRoleAttributeStringForObjectAttribute();

  // Extra checks that occur right before a node is evaluated for serialization.
  void PreSerializationConsistencyCheck();

  // Returns a string representation of this object.
  // |cached_values_only| avoids recomputing cached values, and thus can be
  // used during UpdateCachedValuesIfNecessary() without causing recursion.
  String ToString(bool verbose = false, bool cached_values_only = false) const;

  void PopulateAXRelativeBounds(ui::AXRelativeBounds& bounds,
                                bool* clips_children) const;

  void MarkAllImageAXObjectsDirty();

 protected:
  AXID id_;
  // Any parent, regardless of whether it's ignored or not included in the tree.
  mutable Member<AXObject> parent_;
  // Only children that are included in tree, maybe rename to children_in_tree_.
  mutable AXObjectVector children_;
  mutable bool children_dirty_;

  // The final role, taking into account the ARIA role and native role.
  ax::mojom::blink::Role role_;

  LayoutRect explicit_element_rect_;
  AXID explicit_container_id_;

  virtual void AddChildren() = 0;

  // Collapses multiple whitespace characters into one. Used by GetName().
  String SimplifyName(const String&) const;
  static String RecursiveTextAlternative(
      const AXObject&,
      const AXObject* aria_label_or_description_root,
      AXObjectSet& visited);
  static String RecursiveTextAlternative(
      const AXObject&,
      const AXObject* aria_label_or_description_root,
      AXObjectSet& visited,
      ax::mojom::blink::NameFrom& name_from);
  String AriaTextAlternative(bool recursive,
                             const AXObject* aria_label_or_description_root,
                             AXObjectSet& visited,
                             ax::mojom::blink::NameFrom&,
                             AXRelatedObjectVector*,
                             NameSources*,
                             bool* found_text_alternative) const;
  String TextFromElements(bool in_aria_labelledby_traversal,
                          AXObjectSet& visited,
                          HeapVector<Member<Element>>& elements,
                          AXRelatedObjectVector* related_objects) const;
  static bool ElementsFromAttribute(Element* from,
                                    HeapVector<Member<Element>>& elements,
                                    const QualifiedName&,
                                    Vector<String>& ids);
  static bool AriaLabelledbyElementVector(Element* from,
                                          HeapVector<Member<Element>>& elements,
                                          Vector<String>& ids);
  // Return true if the ame is from @aria-label / @aria-labelledby.
  static bool IsNameFromAriaAttribute(Element* element);
  // Return true if the name is from @aria-label / @aria-labelledby / @title.
  bool IsNameFromAuthorAttribute() const;

  ax::mojom::blink::Role ButtonRoleType() const;

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

  // Helpers for serialization.
  void SerializeBoundingBoxAttributes(ui::AXNodeData& dst) const;
  void SerializeActionAttributes(ui::AXNodeData* node_data);
  void SerializeChildTreeID(ui::AXNodeData* node_data);
  void SerializeChooserPopupAttributes(ui::AXNodeData* node_data);
  void SerializeColorAttributes(ui::AXNodeData* node_data);
  void SerializeElementAttributes(ui::AXNodeData* node_data);
  void SerializeHTMLTagAndClass(ui::AXNodeData* node_data);
  void SerializeHTMLAttributes(ui::AXNodeData* node_data);
  void SerializeInlineTextBoxAttributes(ui::AXNodeData* node_data) const;
  void SerializeLangAttribute(ui::AXNodeData* node_data);
  void SerializeListAttributes(ui::AXNodeData* node_data);
  void SerializeListMarkerAttributes(ui::AXNodeData* dst) const;
  void SerializeLiveRegionAttributes(ui::AXNodeData* node_data) const;
  void SerializeNameAndDescriptionAttributes(ui::AXMode accessibility_mode,
                                             ui::AXNodeData* node_data) const;
  void SerializeOtherScreenReaderAttributes(ui::AXNodeData* node_data) const;
  void SerializeScreenReaderAttributes(ui::AXNodeData* node_data);
  void SerializeScrollAttributes(ui::AXNodeData* node_data);
  void SerializeSparseAttributes(ui::AXNodeData* node_data);
  void SerializeStyleAttributes(ui::AXNodeData* node_data);
  void SerializeTableAttributes(ui::AXNodeData* node_data);
  void SerializeUnignoredAttributes(ui::AXNodeData* node_data,
                                    ui::AXMode accessibility_mode);

  // Serialization implemented in specific subclasses.
  virtual void SerializeMarkerAttributes(ui::AXNodeData* node_data) const;

 private:
  bool ComputeCanSetFocusAttribute() const;
  String KeyboardShortcut() const;

  mutable int last_modification_count_;

  // The following cached attribute values (the ones starting with m_cached*)
  // are only valid if last_modification_count_ matches
  // AXObjectCacheImpl::ModificationCount().
  mutable bool cached_is_ignored_ : 1;
  mutable bool cached_is_ignored_but_included_in_tree_ : 1;
  mutable bool cached_is_inert_ : 1;
  mutable bool cached_is_aria_hidden_ : 1;
  mutable bool cached_is_hidden_via_style : 1;
  mutable bool cached_is_descendant_of_disabled_node_ : 1;
  mutable bool cached_can_set_focus_attribute_ : 1;

  // Focusability can change in response to a new style (e.g. content-visibility
  // added/removed), new dom (e.g. tabindex set/unset), or new AXCache
  // modification count (e.g. new ax tree).
  // TODO(accessibility) Determine whether it's worth it to store these extra
  // variables rather than just using the usual caching mechanism in
  // UpdateCachedAttributeValuesIfNeeded(). This reduces the number of calls to
  // CanSetFocusAttribute() by 25% extra. It also causes updates when AXCache
  // ModificationCount doesn't change but DOM version/style version do change.
  // This can happen during focus action which forces a new style recalc without
  // modifying the AX tree.
  mutable uint64_t focus_attribute_style_version_ = 0;
  mutable uint64_t focus_attribute_dom_tree_version_ = 0;
  mutable int focus_attribute_cache_modification_count_ = -1;

  mutable Member<AXObject> cached_live_region_root_;
  mutable int cached_aria_column_index_;
  mutable int cached_aria_row_index_;
  mutable gfx::RectF cached_local_bounding_box_rect_for_accessibility_;

  Member<AXObjectCacheImpl> ax_object_cache_;

  bool IsCheckable() const;
  static bool IsNativeCheckboxInMixedState(const Node*);
  static bool IncludesARIAWidgetRole(const String&);
  static bool HasInteractiveARIAAttribute(const Element&);
  ax::mojom::blink::Role RemapAriaRoleDueToParent(ax::mojom::blink::Role) const;
  unsigned ComputeAriaColumnIndex() const;
  unsigned ComputeAriaRowIndex() const;
  const ComputedStyle* GetComputedStyle() const;
  bool ComputeIsHiddenViaStyle(const ComputedStyle*) const;
  bool ComputeIsInertViaStyle(const ComputedStyle*,
                              IgnoredReasons* = nullptr) const;

  // This returns true if the element associated with this AXObject is has
  // focusable style, meaning that it is visible. Note that we prefer to rely on
  // `Element::IsFocusableStyle()` for this, but sometimes it isn't available
  // because the style or layout tree needs an update. In these situations, we
  // use the cached AX state to compute the same value.
  bool IsFocusableStyleUsingBestAvailableState() const;

  // Returns an updated layout object to be used in a native scroll action. Note
  // that this updates style for `GetNode()` as well as layout for any layout
  // objects generated. Returns nullptr if a native scroll action to the node is
  // not possible.
  LayoutObject* GetLayoutObjectForNativeScrollAction() const;

  static unsigned number_of_live_ax_objects_;
};

MODULES_EXPORT bool operator==(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator!=(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator<(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator<=(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator>(const AXObject& first, const AXObject& second);
MODULES_EXPORT bool operator>=(const AXObject& first, const AXObject& second);
MODULES_EXPORT std::ostream& operator<<(std::ostream&, const AXObject&);
MODULES_EXPORT std::ostream& operator<<(std::ostream&, const AXObject*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_H_
