/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_OBJECT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_OBJECT_H_

#include <memory>
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_mode.h"

class SkMatrix44;

namespace gfx {
class Point;
}

namespace ui {
struct AXNodeData;
}

namespace blink {

class AXObject;
class WebAXObject;
class WebNode;
class WebDocument;
class WebString;
class WebURL;
struct WebFloatRect;
struct WebRect;
struct WebSize;

class BLINK_EXPORT WebAXSparseAttributeClient {
 public:
  WebAXSparseAttributeClient() = default;
  virtual ~WebAXSparseAttributeClient() = default;

  virtual void AddBoolAttribute(WebAXBoolAttribute, bool) = 0;
  virtual void AddIntAttribute(WebAXIntAttribute, int32_t) = 0;
  virtual void AddUIntAttribute(WebAXUIntAttribute, uint32_t) = 0;
  virtual void AddStringAttribute(WebAXStringAttribute, const WebString&) = 0;
  virtual void AddObjectAttribute(WebAXObjectAttribute, const WebAXObject&) = 0;
  virtual void AddObjectVectorAttribute(WebAXObjectVectorAttribute,
                                        const WebVector<WebAXObject>&) = 0;
};

// A container for passing around a reference to AXObject.
class WebAXObject {
 public:
  ~WebAXObject() { Reset(); }

  WebAXObject() = default;
  WebAXObject(const WebAXObject& o) { Assign(o); }
  WebAXObject& operator=(const WebAXObject& o) {
    Assign(o);
    return *this;
  }

  BLINK_EXPORT bool operator==(const WebAXObject& other) const;
  BLINK_EXPORT bool operator!=(const WebAXObject& other) const;
  BLINK_EXPORT bool operator<(const WebAXObject& other) const;
  BLINK_EXPORT bool operator<=(const WebAXObject& other) const;
  BLINK_EXPORT bool operator>(const WebAXObject& other) const;
  BLINK_EXPORT bool operator>=(const WebAXObject& other) const;
  BLINK_EXPORT static WebAXObject FromWebNode(const WebNode&);
  BLINK_EXPORT static WebAXObject FromWebDocument(const WebDocument&);
  BLINK_EXPORT static WebAXObject FromWebDocumentByID(const WebDocument&, int);
  BLINK_EXPORT static WebAXObject FromWebDocumentFocused(const WebDocument&);

  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebAXObject&);
  BLINK_EXPORT bool Equals(const WebAXObject&) const;

  bool IsNull() const { return private_.IsNull(); }
  // isDetached also checks for null, so it's safe to just call isDetached.
  BLINK_EXPORT bool IsDetached() const;

  BLINK_EXPORT int AxID() const;

  // Get a new AXID that's not used by any accessibility node in this process,
  // for when the client needs to insert additional nodes into the accessibility
  // tree.
  BLINK_EXPORT int GenerateAXID() const;

  // Update layout if necessary on the underlying tree, and return true if this
  // object is still valid (not detached). Note that calling this method can
  // cause other WebAXObjects to become invalid, too, so always call isDetached
  // if any other blink/renderer/core code has run.
  BLINK_EXPORT bool UpdateLayoutAndCheckValidity();

  BLINK_EXPORT unsigned ChildCount() const;

  BLINK_EXPORT WebAXObject ChildAt(unsigned) const;
  BLINK_EXPORT WebAXObject ParentObject() const;

  // Retrieve accessibility attributes that apply to only a small
  // fraction of WebAXObjects by passing an implementation of
  // WebAXSparseAttributeClient, which will be called with only the attributes
  // that apply to this object.
  BLINK_EXPORT void GetSparseAXAttributes(WebAXSparseAttributeClient&) const;

  // Serialize the properties of this node into |node_data|.
  //
  // TODO(crbug.com/1068668): AX onion soup - finish migrating
  // BlinkAXTreeSource::SerializeNode into AXObject::Serialize and removing
  // the unneeded WebAXObject interfaces below.
  BLINK_EXPORT void Serialize(ui::AXNodeData* node_data,
                              ui::AXMode accessibility_mode) const;

  BLINK_EXPORT bool IsAnchor() const;
  BLINK_EXPORT ax::mojom::CheckedState CheckedState() const;
  BLINK_EXPORT bool IsCheckable() const;
  BLINK_EXPORT bool IsClickable() const;
  BLINK_EXPORT bool IsControl() const;
  BLINK_EXPORT bool IsFocused() const;
  BLINK_EXPORT bool IsLineBreakingObject() const;
  BLINK_EXPORT bool IsLinked() const;
  BLINK_EXPORT bool IsModal() const;
  // Returns true if this object is an input element of a text field type, such
  // as type="text" or type="tel", or a textarea.
  BLINK_EXPORT bool IsNativeTextControl() const;
  BLINK_EXPORT bool IsOffScreen() const;
  BLINK_EXPORT bool IsSelectedOptionActive() const;
  BLINK_EXPORT bool IsVisited() const;

  BLINK_EXPORT bool HasAriaAttribute() const;
  BLINK_EXPORT WebString AccessKey() const;
  BLINK_EXPORT unsigned BackgroundColor() const;
  BLINK_EXPORT bool CanPress() const;
  BLINK_EXPORT bool CanSetValueAttribute() const;
  BLINK_EXPORT unsigned GetColor() const;
  // Deprecated.
  BLINK_EXPORT void ColorValue(int& r, int& g, int& b) const;
  BLINK_EXPORT unsigned ColorValue() const;
  BLINK_EXPORT WebAXObject AriaActiveDescendant() const;
  BLINK_EXPORT WebString AutoComplete() const;
  BLINK_EXPORT ax::mojom::AriaCurrentState AriaCurrentState() const;
  BLINK_EXPORT bool IsEditableRoot() const;
  BLINK_EXPORT bool IsEditable() const;
  BLINK_EXPORT bool AriaOwns(WebVector<WebAXObject>& owns_elements) const;
  BLINK_EXPORT WebString FontFamily() const;
  BLINK_EXPORT float FontSize() const;
  BLINK_EXPORT float FontWeight() const;
  BLINK_EXPORT bool CanvasHasFallbackContent() const;
  BLINK_EXPORT WebAXObject ErrorMessage() const;
  // If this is an image, returns the image (scaled to maxSize) as a data url.
  BLINK_EXPORT WebString ImageDataUrl(const WebSize& max_size) const;
  BLINK_EXPORT ax::mojom::InvalidState InvalidState() const;
  // Only used when invalidState() returns WebAXInvalidStateOther.
  BLINK_EXPORT WebString AriaInvalidValue() const;
  BLINK_EXPORT int HeadingLevel() const;
  BLINK_EXPORT int HierarchicalLevel() const;
  BLINK_EXPORT WebAXObject HitTest(const gfx::Point&) const;
  // Get the WebAXObject's bounds in frame-relative coordinates as a WebRect.
  BLINK_EXPORT WebRect GetBoundsInFrameCoordinates() const;
  BLINK_EXPORT WebString KeyboardShortcut() const;
  BLINK_EXPORT WebString Language() const;
  BLINK_EXPORT WebAXObject InPageLinkTarget() const;
  BLINK_EXPORT WebVector<WebAXObject> RadioButtonsInGroup() const;
  BLINK_EXPORT ax::mojom::Role Role() const;
  BLINK_EXPORT WebString StringValue() const;
  BLINK_EXPORT ax::mojom::ListStyle GetListStyle() const;
  BLINK_EXPORT ax::mojom::WritingDirection GetTextDirection() const;
  BLINK_EXPORT ax::mojom::TextPosition GetTextPosition() const;
  BLINK_EXPORT void GetTextStyleAndTextDecorationStyle(
      int32_t* text_style,
      ax::mojom::TextDecorationStyle* text_overline_style,
      ax::mojom::TextDecorationStyle* text_strikethrough_style,
      ax::mojom::TextDecorationStyle* text_underline_style) const;
  BLINK_EXPORT WebURL Url() const;
  BLINK_EXPORT WebAXObject ChooserPopup() const;

  // Retrieves the accessible name of the object, an enum indicating where the
  // name was derived from, and a list of related objects that were used to
  // derive the name, if any.
  BLINK_EXPORT WebString GetName(ax::mojom::NameFrom&,
                                 WebVector<WebAXObject>& name_objects) const;
  // Simplified version of |name| when nameFrom and nameObjects aren't needed.
  BLINK_EXPORT WebString GetName() const;
  // Takes the result of nameFrom from calling |name|, above, and retrieves the
  // accessible description of the object, which is secondary to |name|, an enum
  // indicating where the description was derived from, and a list of objects
  // that were used to derive the description, if any.
  BLINK_EXPORT WebString
  Description(ax::mojom::NameFrom,
              ax::mojom::DescriptionFrom&,
              WebVector<WebAXObject>& description_objects) const;
  // Takes the result of nameFrom and descriptionFrom from calling |name| and
  // |description|, above, and retrieves the placeholder of the object, if
  // present and if it wasn't already exposed by one of the two functions above.
  BLINK_EXPORT WebString Placeholder(ax::mojom::NameFrom) const;

  // Takes the result of nameFrom and retrieves the HTML Title of the object,
  // if present and if it wasn't already exposed by |GetName| above.
  // HTML Title is typically used as a tooltip.
  BLINK_EXPORT WebString Title(ax::mojom::NameFrom) const;

  //
  // Document-level interfaces.
  //
  // These are intended to be called on the root WebAXObject.
  //

  BLINK_EXPORT bool IsLoaded() const;
  BLINK_EXPORT double EstimatedLoadingProgress() const;

  BLINK_EXPORT WebAXObject RootScroller() const;

  // The following selection functions get or set the global document
  // selection and can be called on any object in the tree.

  BLINK_EXPORT void Selection(bool& is_selection_backward,
                              WebAXObject& anchor_object,
                              int& anchor_offset,
                              ax::mojom::TextAffinity& anchor_affinity,
                              WebAXObject& focus_object,
                              int& focus_offset,
                              ax::mojom::TextAffinity& focus_affinity) const;

  // The following selection functions return text offsets calculated starting
  // from the current object. They only report on a selection that is placed on
  // the current object or on any of its descendants.

  BLINK_EXPORT unsigned SelectionEnd() const;
  BLINK_EXPORT unsigned SelectionStart() const;

  // 1-based position in set & Size of set.
  BLINK_EXPORT int PosInSet() const;
  BLINK_EXPORT int SetSize() const;

  // Live regions.
  BLINK_EXPORT bool IsInLiveRegion() const;
  BLINK_EXPORT bool LiveRegionAtomic() const;
  BLINK_EXPORT WebString LiveRegionRelevant() const;
  BLINK_EXPORT WebString LiveRegionStatus() const;
  BLINK_EXPORT WebAXObject LiveRegionRoot() const;
  BLINK_EXPORT bool ContainerLiveRegionAtomic() const;
  BLINK_EXPORT bool ContainerLiveRegionBusy() const;
  BLINK_EXPORT WebString ContainerLiveRegionRelevant() const;
  BLINK_EXPORT WebString ContainerLiveRegionStatus() const;

  BLINK_EXPORT bool SupportsRangeValue() const;
  BLINK_EXPORT bool ValueForRange(float* out_value) const;
  BLINK_EXPORT bool MaxValueForRange(float* out_value) const;
  BLINK_EXPORT bool MinValueForRange(float* out_value) const;
  BLINK_EXPORT bool StepValueForRange(float* out_value) const;

  BLINK_EXPORT WebNode GetNode() const;
  BLINK_EXPORT WebDocument GetDocument() const;
  BLINK_EXPORT WebString ComputedStyleDisplay() const;
  BLINK_EXPORT bool AccessibilityIsIgnored() const;
  BLINK_EXPORT bool AccessibilityIsIncludedInTree() const;
  BLINK_EXPORT void Markers(WebVector<ax::mojom::MarkerType>& types,
                            WebVector<int>& starts,
                            WebVector<int>& ends) const;

  // Actions. Return true if handled.
  BLINK_EXPORT ax::mojom::DefaultActionVerb Action() const;
  BLINK_EXPORT bool ClearAccessibilityFocus() const;
  BLINK_EXPORT bool Click() const;
  BLINK_EXPORT bool Decrement() const;
  BLINK_EXPORT bool Increment() const;
  BLINK_EXPORT bool Focus() const;
  BLINK_EXPORT bool SetAccessibilityFocus() const;
  BLINK_EXPORT bool SetSelected(bool) const;
  BLINK_EXPORT bool SetSelection(const WebAXObject& anchor_object,
                                 int anchor_offset,
                                 const WebAXObject& focus_object,
                                 int focus_offset) const;
  BLINK_EXPORT bool SetSequentialFocusNavigationStartingPoint() const;
  BLINK_EXPORT bool SetValue(WebString) const;
  BLINK_EXPORT bool ShowContextMenu() const;
  // Make this object visible by scrolling as many nested scrollable views as
  // needed.
  BLINK_EXPORT bool ScrollToMakeVisible() const;
  // Same, but if the whole object can't be made visible, try for this subrect,
  // in local coordinates. We also allow passing horizontal and vertical scroll
  // alignments. These specify where in the content area to scroll the object.
  BLINK_EXPORT bool ScrollToMakeVisibleWithSubFocus(
      const WebRect&,
      ax::mojom::ScrollAlignment horizontal_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollAlignment vertical_scroll_alignment =
          ax::mojom::ScrollAlignment::kScrollAlignmentCenter,
      ax::mojom::ScrollBehavior scroll_behavior =
          ax::mojom::ScrollBehavior::kDoNotScrollIfVisible) const;
  // Scroll this object to a given point in global coordinates of the top-level
  // window.
  BLINK_EXPORT bool ScrollToGlobalPoint(const gfx::Point&) const;

  // For a table
  BLINK_EXPORT int AriaColumnCount() const;
  BLINK_EXPORT unsigned AriaColumnIndex() const;
  BLINK_EXPORT int AriaRowCount() const;
  BLINK_EXPORT unsigned AriaRowIndex() const;
  BLINK_EXPORT unsigned ColumnCount() const;
  BLINK_EXPORT unsigned RowCount() const;
  BLINK_EXPORT WebAXObject CellForColumnAndRow(unsigned column,
                                               unsigned row) const;
  BLINK_EXPORT void RowHeaders(WebVector<WebAXObject>&) const;
  BLINK_EXPORT void ColumnHeaders(WebVector<WebAXObject>&) const;

  // For a table row
  BLINK_EXPORT unsigned RowIndex() const;
  BLINK_EXPORT WebAXObject RowHeader() const;

  // For a table column
  BLINK_EXPORT unsigned ColumnIndex() const;
  BLINK_EXPORT WebAXObject ColumnHeader() const;

  // For a table cell
  BLINK_EXPORT unsigned CellColumnIndex() const;
  BLINK_EXPORT unsigned CellColumnSpan() const;
  BLINK_EXPORT unsigned CellRowIndex() const;
  BLINK_EXPORT unsigned CellRowSpan() const;
  BLINK_EXPORT ax::mojom::SortDirection SortDirection() const;

  // Load inline text boxes for just this subtree, even if
  // settings->inlineTextBoxAccessibilityEnabled() is false.
  BLINK_EXPORT void LoadInlineTextBoxes() const;

  // Walk the WebAXObjects on the same line. This is supported on any
  // object type but primarily intended to be used for inline text boxes.
  BLINK_EXPORT WebAXObject NextOnLine() const;
  BLINK_EXPORT WebAXObject PreviousOnLine() const;

  // For an inline text box.
  BLINK_EXPORT void CharacterOffsets(WebVector<int>&) const;
  BLINK_EXPORT void GetWordBoundaries(WebVector<int>& starts,
                                      WebVector<int>& ends) const;

  // Scrollable containers.
  // Programmatically scrollable.
  BLINK_EXPORT bool IsScrollableContainer() const;
  // Also scrollable by user.
  BLINK_EXPORT bool IsUserScrollable() const;
  BLINK_EXPORT gfx::Point GetScrollOffset() const;
  BLINK_EXPORT gfx::Point MinimumScrollOffset() const;
  BLINK_EXPORT gfx::Point MaximumScrollOffset() const;
  BLINK_EXPORT void SetScrollOffset(const gfx::Point&) const;

  // aria-dropeffect is deprecated in WAI-ARIA 1.1
  BLINK_EXPORT void Dropeffects(
      WebVector<ax::mojom::Dropeffect>& dropeffects) const;

  // Every object's bounding box is returned relative to a
  // container object (which is guaranteed to be an ancestor) and
  // optionally a transformation matrix that needs to be applied too.
  // To compute the absolute bounding box of an element, start with its
  // boundsInContainer and apply the transform. Then as long as its container is
  // not null, walk up to its container and offset by the container's offset
  // from origin, the container's scroll position if any, and apply the
  // container's transform.  Do this until you reach the root of the tree.
  // If the container clips its children, for example with overflow:hidden
  // or similar, set |clips_children| to true.
  BLINK_EXPORT void GetRelativeBounds(WebAXObject& offset_container,
                                      WebFloatRect& bounds_in_container,
                                      SkMatrix44& container_transform,
                                      bool* clips_children = nullptr) const;

  // Retrieves a vector of all WebAXObjects in this document whose
  // bounding boxes may have changed since the last query. Can be called
  // on any object.
  BLINK_EXPORT void GetAllObjectsWithChangedBounds(
      WebVector<WebAXObject>& out_changed_bounds_objects) const;

  // Exchanges a WebAXObject with another.
  BLINK_EXPORT void Swap(WebAXObject& other);

  // Returns a brief description of the object, suitable for debugging. E.g. its
  // role and name.
  BLINK_EXPORT WebString ToString(bool verbose = false) const;

  BLINK_EXPORT void HandleAutofillStateChanged(
      const WebAXAutofillState state) const;

#if INSIDE_BLINK
  BLINK_EXPORT WebAXObject(AXObject*);
  WebAXObject& operator=(AXObject*);
  operator AXObject*() const;
#endif

 private:
  WebPrivatePtr<AXObject> private_;
};

}  // namespace blink

#endif
