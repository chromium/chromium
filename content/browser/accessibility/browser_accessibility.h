// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_buildflags.h"
#include "content/browser/accessibility/browser_accessibility_position.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

// Set PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL if this platform has
// a platform-specific subclass of BrowserAccessibility and
// BrowserAccessibilityManager.
#undef PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL

#if defined(OS_WIN)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_MAC)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_ANDROID) && !defined(USE_AURA)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if BUILDFLAG(USE_ATK)
#define PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL 1
#endif

#if defined(OS_MAC) && __OBJC__
@class BrowserAccessibilityCocoa;
#endif

namespace content {
class BrowserAccessibilityManager;

////////////////////////////////////////////////////////////////////////////////
//
// BrowserAccessibility
//
// A BrowserAccessibility object represents one node in the accessibility
// tree on the browser side. It exactly corresponds to one WebAXObject from
// Blink. It's owned by a BrowserAccessibilityManager.
//
// There are subclasses of BrowserAccessibility for each platform where
// we implement native accessibility APIs. This base class is used occasionally
// for tests.
//
////////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT BrowserAccessibility : public ui::AXPlatformNodeDelegate {
 public:
  // Creates a platform specific BrowserAccessibility. Ownership passes to the
  // caller.
  static BrowserAccessibility* Create();

  // Returns |delegate| as a BrowserAccessibility object, if |delegate| is
  // non-null and an object in the BrowserAccessibility class hierarchy.
  static BrowserAccessibility* FromAXPlatformNodeDelegate(
      ui::AXPlatformNodeDelegate* delegate);

  BrowserAccessibility();
  ~BrowserAccessibility() override;

  // Called only once, immediately after construction. The constructor doesn't
  // take any arguments because in the Windows subclass we use a special
  // function to construct a COM object.
  virtual void Init(BrowserAccessibilityManager* manager, ui::AXNode* node);

  // Called after the object is first initialized and again every time
  // its data changes.
  virtual void OnDataChanged() {}

  // Called when the location changed.
  virtual void OnLocationChanged() {}

  // This is called when the platform-specific attributes for a node need
  // to be recomputed, which may involve firing native events, due to a
  // change other than an update from OnAccessibilityEvents.
  virtual void UpdatePlatformAttributes() {}

  // Return true if this object is equal to or a descendant of |ancestor|.
  bool IsDescendantOf(const BrowserAccessibility* ancestor) const;

  bool IsDocument() const;

  bool IsIgnored() const;

  bool IsLineBreakObject() const;

  // See AXNode::IsLeaf().
  bool PlatformIsLeaf() const;

  // Returns true if this object can fire events.
  virtual bool CanFireEvents() const;

  // Return the AXPlatformNode corresponding to this node, if applicable
  // on this platform.
  virtual ui::AXPlatformNode* GetAXPlatformNode() const;

  // Returns the number of children of this object, or 0 if PlatformIsLeaf()
  // returns true.
  virtual uint32_t PlatformChildCount() const;

  // Return a pointer to the child at the given index, or NULL for an
  // invalid index. Returns nullptr if PlatformIsLeaf() returns true.
  virtual BrowserAccessibility* PlatformGetChild(uint32_t child_index) const;

  BrowserAccessibility* PlatformGetParent() const;
  virtual BrowserAccessibility* PlatformGetFirstChild() const;
  virtual BrowserAccessibility* PlatformGetLastChild() const;
  virtual BrowserAccessibility* PlatformGetNextSibling() const;
  virtual BrowserAccessibility* PlatformGetPreviousSibling() const;

  class CONTENT_EXPORT PlatformChildIterator : public ChildIterator {
   public:
    PlatformChildIterator(const BrowserAccessibility* parent,
                          BrowserAccessibility* child);
    PlatformChildIterator(const PlatformChildIterator& it);
    ~PlatformChildIterator() override;
    bool operator==(const ChildIterator& rhs) const override;
    bool operator!=(const ChildIterator& rhs) const override;
    void operator++() override;
    void operator++(int) override;
    void operator--() override;
    void operator--(int) override;
    gfx::NativeViewAccessible GetNativeViewAccessible() const override;
    BrowserAccessibility* get() const;
    int GetIndexInParent() const override;
    BrowserAccessibility& operator*() const override;
    BrowserAccessibility* operator->() const override;

   private:
    const BrowserAccessibility* parent_;
    ui::AXNode::ChildIteratorBase<
        BrowserAccessibility,
        &BrowserAccessibility::PlatformGetNextSibling,
        &BrowserAccessibility::PlatformGetPreviousSibling,
        &BrowserAccessibility::PlatformGetFirstChild,
        &BrowserAccessibility::PlatformGetLastChild>
        platform_iterator;
  };

  PlatformChildIterator PlatformChildrenBegin() const;
  PlatformChildIterator PlatformChildrenEnd() const;
  // Return a pointer to the first ancestor that is a selection container
  BrowserAccessibility* PlatformGetSelectionContainer() const;

  // If this object is exposed to the platform, returns this object. Otherwise,
  // returns the platform leaf under which this object is found.
  BrowserAccessibility* PlatformGetClosestPlatformObject() const;

  bool IsPreviousSiblingOnSameLine() const;
  bool IsNextSiblingOnSameLine() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* PlatformDeepestLastChild() const;

  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestFirstChild() const;
  // Returns nullptr if there are no children.
  BrowserAccessibility* InternalDeepestLastChild() const;

  // Derivative utils for AXPlatformNodeDelegate::GetBoundsRect
  gfx::Rect GetClippedScreenBoundsRect(
      ui::AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetUnclippedScreenBoundsRect(
      ui::AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetClippedRootFrameBoundsRect(
      ui::AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetUnclippedRootFrameBoundsRect(
      ui::AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetClippedFrameBoundsRect(
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // Derivative utils for AXPlatformNodeDelegate::GetHypertextRangeBoundsRect
  gfx::Rect GetUnclippedRootFrameHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // Derivative utils for AXPlatformNodeDelegate::GetInnerTextRangeBoundsRect
  gfx::Rect GetUnclippedScreenInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;
  gfx::Rect GetUnclippedRootFrameInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // DEPRECATED: Prefer using the interfaces provided by AXPlatformNodeDelegate
  // when writing new code.
  gfx::Rect GetScreenHypertextRangeBoundsRect(
      int start,
      int len,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // Returns the bounds of the given range in coordinates relative to the
  // top-left corner of the overall web area. Only valid when the role is
  // WebAXRoleStaticText.
  // DEPRECATED (for public use): Prefer using the interfaces provided by
  // AXPlatformNodeDelegate when writing new non-private code.
  gfx::Rect GetRootFrameHypertextRangeBoundsRect(
      int start,
      int len,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // Returns the value of a control, such as the value of a text field, a slider
  // or a scrollbar.
  //
  // For text fields, computes the value of the field from its internal
  // representation in the accessibility tree if necessary.
  //
  // This is to handle the cases such as ARIA textbox, where the value should
  // be calculated from the object's inner text, as well as all text fields
  // originating from Blink where the HTML value attribute cannot always be
  // trusted.
  //
  // TODO(nektar): Move this method to AXNode when AXNodePosition and
  // BrowserAccessibilityPosition are merged into one class.
  virtual base::string16 GetValue() const;

  // This is an approximate hit test that only uses the information in
  // the browser process to compute the correct result. It will not return
  // correct results in many cases of z-index, overflow, and absolute
  // positioning, so BrowserAccessibilityManager::CachingAsyncHitTest
  // should be used instead, which falls back on calling ApproximateHitTest
  // automatically.
  //
  // Note that unlike BrowserAccessibilityManager::CachingAsyncHitTest, this
  // method takes a parameter in Blink's definition of screen coordinates.
  // This is so that the scale factor is consistent with what we receive from
  // Blink and store in the AX tree.
  // Blink screen coordinates are 1:1 with physical pixels if use-zoom-for-dsf
  // is disabled; they're physical pixels divided by device scale factor if
  // use-zoom-for-dsf is disabled. For more information see:
  // http://www.chromium.org/developers/design-documents/blink-coordinate-spaces
  BrowserAccessibility* ApproximateHitTest(
      const gfx::Point& blink_screen_point);

  //
  // Accessors
  //

  BrowserAccessibilityManager* manager() const { return manager_; }
  ui::AXNode* node() const { return node_; }

  // These access the internal unignored accessibility tree, which doesn't
  // necessarily reflect the accessibility tree that should be exposed on each
  // platform. Use PlatformChildCount and PlatformGetChild to implement platform
  // accessibility APIs.
  uint32_t InternalChildCount() const;
  BrowserAccessibility* InternalGetChild(uint32_t child_index) const;
  BrowserAccessibility* InternalGetParent() const;
  BrowserAccessibility* InternalGetFirstChild() const;
  BrowserAccessibility* InternalGetLastChild() const;
  BrowserAccessibility* InternalGetNextSibling() const;
  BrowserAccessibility* InternalGetPreviousSibling() const;
  using InternalChildIterator = ui::AXNode::ChildIteratorBase<
      BrowserAccessibility,
      &BrowserAccessibility::InternalGetNextSibling,
      &BrowserAccessibility::InternalGetPreviousSibling,
      &BrowserAccessibility::InternalGetFirstChild,
      &BrowserAccessibility::InternalGetLastChild>;
  InternalChildIterator InternalChildrenBegin() const;
  InternalChildIterator InternalChildrenEnd() const;

  ui::AXNode::AXID GetId() const;
  gfx::RectF GetLocation() const;
  ax::mojom::Role GetRole() const;
  int32_t GetState() const;

  typedef base::StringPairs HtmlAttributes;
  const HtmlAttributes& GetHtmlAttributes() const;

  // Accessing accessibility attributes:
  //
  // There are dozens of possible attributes for an accessibility node,
  // but only a few tend to apply to any one object, so we store them
  // in sparse arrays of <attribute id, attribute value> pairs, organized
  // by type (bool, int, float, string, int list).
  //
  // There are three accessors for each type of attribute: one that returns
  // true if the attribute is present and false if not, one that takes a
  // pointer argument and returns true if the attribute is present (if you
  // need to distinguish between the default value and a missing attribute),
  // and another that returns the default value for that type if the
  // attribute is not present. In addition, strings can be returned as
  // either std::string or base::string16, for convenience.

  bool HasBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr) const;
  bool GetBoolAttribute(ax::mojom::BoolAttribute attr, bool* value) const;

  bool HasFloatAttribute(ax::mojom::FloatAttribute attr) const;
  float GetFloatAttribute(ax::mojom::FloatAttribute attr) const;
  bool GetFloatAttribute(ax::mojom::FloatAttribute attr, float* value) const;

  bool HasInheritedStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetInheritedStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  base::string16 GetInheritedString16Attribute(
      ax::mojom::StringAttribute attribute) const;

  bool HasIntAttribute(ax::mojom::IntAttribute attribute) const;
  int GetIntAttribute(ax::mojom::IntAttribute attribute) const;
  bool GetIntAttribute(ax::mojom::IntAttribute attribute, int* value) const;

  bool HasStringAttribute(ax::mojom::StringAttribute attribute) const;
  const std::string& GetStringAttribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetStringAttribute(ax::mojom::StringAttribute attribute,
                          std::string* value) const;

  base::string16 GetString16Attribute(
      ax::mojom::StringAttribute attribute) const;
  bool GetString16Attribute(ax::mojom::StringAttribute attribute,
                            base::string16* value) const;

  bool HasIntListAttribute(ax::mojom::IntListAttribute attribute) const;
  const std::vector<int32_t>& GetIntListAttribute(
      ax::mojom::IntListAttribute attribute) const;
  bool GetIntListAttribute(ax::mojom::IntListAttribute attribute,
                           std::vector<int32_t>* value) const;

  // Retrieve the value of a html attribute from the attribute map and
  // returns true if found.
  bool GetHtmlAttribute(const char* attr, std::string* value) const;
  bool GetHtmlAttribute(const char* attr, base::string16* value) const;

  // Returns true if the bit corresponding to the given enum is 1.
  bool HasState(ax::mojom::State state_enum) const;
  bool HasAction(ax::mojom::Action action_enum) const;

  // True if this is a web area, and its grandparent is a presentational iframe.
  bool IsWebAreaForPresentationalIframe() const;

  virtual bool IsClickable() const;

  // See AXNodeData::IsTextField().
  bool IsTextField() const;

  // See AXNodeData::IsPasswordField().
  bool IsPasswordField() const;

  // See AXNodeData::IsPlainTextField().
  bool IsPlainTextField() const;

  // See AXNodeData::IsRichTextField().
  bool IsRichTextField() const;

  // Returns true if the accessible name was explicitly set to "" by the author
  bool HasExplicitlyEmptyName() const;

  // Get text to announce for a live region change, for ATs that do not
  // implement this functionality.
  std::string GetLiveRegionText() const;

  // Creates a text position rooted at this object. Does not conver to a
  // leaf text position - see CreatePositionForSelectionAt, below.
  BrowserAccessibilityPosition::AXPositionInstance CreatePositionAt(
      int offset,
      ax::mojom::TextAffinity affinity =
          ax::mojom::TextAffinity::kDownstream) const;

  // |offset| could either be a text character or a child index in case of
  // non-text objects. Converts to a leaf text position if you pass a
  // character offset on a container node.
  BrowserAccessibilityPosition::AXPositionInstance CreatePositionForSelectionAt(
      int offset) const;

  // Gets the text offsets where new lines start.
  std::vector<int> GetLineStartOffsets() const;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  // AXPosition Support

  // Returns the text that is present inside this node, where the
  // representation of text found in descendant nodes depends on the platform.
  // For example some platforms may include descendant text while while other
  // platforms may use a special character to represent descendant text.
  // Prefer either GetHypertext or GetInnerText so it's clear which API is
  // called.
  //
  // TODO(nektar): Move this method to AXNode when AXNodePosition and
  // BrowserAccessibilityPosition are merged into one class.
  virtual base::string16 GetText() const;

  base::string16 GetNameAsString16() const;

  // AXPlatformNodeDelegate.
  base::string16 GetAuthorUniqueId() const override;
  const ui::AXNodeData& GetData() const override;
  const ui::AXTreeData& GetTreeData() const override;
  const ui::AXTree::Selection GetUnignoredSelection() const override;
  ui::AXNodePosition::AXPositionInstance CreateTextPositionAt(
      int offset) const override;
  gfx::NativeViewAccessible GetNSWindow() override;
  gfx::NativeViewAccessible GetParent() override;
  int GetChildCount() const override;
  gfx::NativeViewAccessible ChildAtIndex(int index) override;
  bool HasModalDialog() const override;
  gfx::NativeViewAccessible GetFirstChild() override;
  gfx::NativeViewAccessible GetLastChild() override;
  gfx::NativeViewAccessible GetNextSibling() override;
  gfx::NativeViewAccessible GetPreviousSibling() override;

  bool IsChildOfLeaf() const override;
  bool IsChildOfPlainTextField() const override;
  bool IsLeaf() const override;
  bool IsToplevelBrowserWindow() override;
  gfx::NativeViewAccessible GetClosestPlatformObject() const override;

  std::unique_ptr<ChildIterator> ChildrenBegin() override;
  std::unique_ptr<ChildIterator> ChildrenEnd() override;

  std::string GetName() const override;
  base::string16 GetHypertext() const override;
  bool SetHypertextSelection(int start_offset, int end_offset) override;
  base::string16 GetInnerText() const override;
  gfx::Rect GetBoundsRect(
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetHypertextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::Rect GetInnerTextRangeBoundsRect(
      const int start_offset,
      const int end_offset,
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const override;
  gfx::NativeViewAccessible HitTestSync(int physical_pixel_x,
                                        int physical_pixel_y) const override;
  gfx::NativeViewAccessible GetFocus() override;
  ui::AXPlatformNode* GetFromNodeID(int32_t id) override;
  ui::AXPlatformNode* GetFromTreeIDAndNodeID(const ui::AXTreeID& ax_tree_id,
                                             int32_t id) override;
  int GetIndexInParent() override;
  gfx::AcceleratedWidget GetTargetForNativeAccessibilityEvent() override;

  base::Optional<int> FindTextBoundary(
      ax::mojom::TextBoundary boundary,
      int offset,
      ax::mojom::MoveDirection direction,
      ax::mojom::TextAffinity affinity) const override;

  const std::vector<gfx::NativeViewAccessible> GetUIADescendants()
      const override;

  std::string GetLanguage() const override;

  bool IsTable() const override;
  base::Optional<int> GetTableColCount() const override;
  base::Optional<int> GetTableRowCount() const override;
  base::Optional<int> GetTableAriaColCount() const override;
  base::Optional<int> GetTableAriaRowCount() const override;
  base::Optional<int> GetTableCellCount() const override;
  base::Optional<bool> GetTableHasColumnOrRowHeaderNode() const override;
  std::vector<ui::AXNode::AXID> GetColHeaderNodeIds() const override;
  std::vector<ui::AXNode::AXID> GetColHeaderNodeIds(
      int col_index) const override;
  std::vector<ui::AXNode::AXID> GetRowHeaderNodeIds() const override;
  std::vector<ui::AXNode::AXID> GetRowHeaderNodeIds(
      int row_index) const override;
  ui::AXPlatformNode* GetTableCaption() const override;

  bool IsTableRow() const override;
  base::Optional<int> GetTableRowRowIndex() const override;

  bool IsTableCellOrHeader() const override;
  base::Optional<int> GetTableCellIndex() const override;
  base::Optional<int> GetTableCellColIndex() const override;
  base::Optional<int> GetTableCellRowIndex() const override;
  base::Optional<int> GetTableCellColSpan() const override;
  base::Optional<int> GetTableCellRowSpan() const override;
  base::Optional<int> GetTableCellAriaColIndex() const override;
  base::Optional<int> GetTableCellAriaRowIndex() const override;
  base::Optional<int32_t> GetCellId(int row_index,
                                    int col_index) const override;
  base::Optional<int32_t> CellIndexToId(int cell_index) const override;

  bool IsCellOrHeaderOfARIATable() const override;
  bool IsCellOrHeaderOfARIAGrid() const override;

  bool AccessibilityPerformAction(const ui::AXActionData& data) override;
  base::string16 GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override;
  base::string16 GetLocalizedRoleDescriptionForUnlabeledImage() const override;
  base::string16 GetLocalizedStringForLandmarkType() const override;
  base::string16 GetLocalizedStringForRoleDescription() const override;
  base::string16 GetStyleNameAttributeAsLocalizedString() const override;
  ui::TextAttributeMap ComputeTextAttributeMap(
      const ui::TextAttributeList& default_attributes) const override;
  std::string GetInheritedFontFamilyName() const override;
  bool ShouldIgnoreHoveredStateForTesting() override;
  bool IsOffscreen() const override;
  bool IsMinimized() const override;
  bool IsText() const override;
  bool IsWebContent() const override;
  bool HasVisibleCaretOrSelection() const override;
  ui::AXPlatformNode* GetTargetNodeForRelation(
      ax::mojom::IntAttribute attr) override;
  std::vector<ui::AXPlatformNode*> GetTargetNodesForRelation(
      ax::mojom::IntListAttribute attr) override;
  std::set<ui::AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntAttribute attr) override;
  std::set<ui::AXPlatformNode*> GetReverseRelations(
      ax::mojom::IntListAttribute attr) override;
  bool IsOrderedSetItem() const override;
  bool IsOrderedSet() const override;
  base::Optional<int> GetPosInSet() const override;
  base::Optional<int> GetSetSize() const override;

  bool IsInListMarker() const;
  bool IsCollapsedMenuListPopUpButton() const;
  BrowserAccessibility* GetCollapsedMenuListPopUpButtonAncestor() const;

  // Returns true if:
  // 1. This node is a list, AND
  // 2. This node has a list ancestor or a list descendant.
  bool IsHierarchicalList() const;

  // Returns a string representation of this object for debugging purposes.
  std::string ToString() const;

 protected:
  // The UIA tree formatter needs access to GetUniqueId() to identify the
  // starting point for tree dumps.
  friend class AccessibilityTreeFormatterUia;

  using BrowserAccessibilityPositionInstance =
      BrowserAccessibilityPosition::AXPositionInstance;
  using AXPlatformRange =
      ui::AXRange<BrowserAccessibilityPositionInstance::element_type>;

  virtual ui::TextAttributeList ComputeTextAttributes() const;

  // The manager of this tree of accessibility objects.
  BrowserAccessibilityManager* manager_ = nullptr;

  // The underlying node.
  ui::AXNode* node_ = nullptr;

  // Protected so that it can't be called directly on a BrowserAccessibility
  // where it could be confused with an id that comes from the node data,
  // which is only unique to the Blink process.
  // Does need to be called by subclasses such as BrowserAccessibilityAndroid.
  const ui::AXUniqueId& GetUniqueId() const override;

  // Returns a text attribute map indicating the offsets in the text of a leaf
  // object, such as a text field or static text, where spelling and grammar
  // errors are present.
  ui::TextAttributeMap GetSpellingAndGrammarAttributes() const;

  std::string SubtreeToStringHelper(size_t level) override;

 private:
  // Return the bounds after converting from this node's coordinate system
  // (which is relative to its nearest scrollable ancestor) to the coordinate
  // system specified. If the clipping behavior is set to clipped, clipping is
  // applied to all bounding boxes so that the resulting rect is within the
  // window. If the clipping behavior is unclipped, the resulting rect may be
  // outside of the window or offscreen. If an offscreen result address is
  // provided, it will be populated depending on whether the returned bounding
  // box is onscreen or offscreen.
  gfx::Rect RelativeToAbsoluteBounds(
      gfx::RectF bounds,
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const;

  // Return a rect for a 1-width character past the end of text. This is what
  // ATs expect when getting the character extents past the last character in
  // a line, and equals what the caret bounds would be when past the end of
  // the text.
  gfx::Rect GetRootFrameHypertextBoundsPastEndOfText(
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result = nullptr) const;

  // Return the bounds of inline text in this node's coordinate system (which
  // is relative to its container node specified in AXRelativeBounds).
  gfx::RectF GetInlineTextRect(const int start_offset,
                               const int end_offset,
                               const int max_length) const;

  // Recursive helper function for GetInnerTextRangeBounds.
  gfx::Rect GetInnerTextRangeBoundsRectInSubtree(
      const int start_offset,
      const int end_offset,
      const ui::AXCoordinateSystem coordinate_system,
      const ui::AXClippingBehavior clipping_behavior,
      ui::AXOffscreenResult* offscreen_result) const;

  // Given a set of node ids, return the nodes in this delegate's tree to
  // which they correspond.
  std::set<ui::AXPlatformNode*> GetNodesForNodeIdSet(
      const std::set<int32_t>& ids);

  // If the node has a child tree, get the root node.
  BrowserAccessibility* PlatformGetRootOfChildTree() const;

  // Given a set of map of spelling text attributes and a start offset, merge
  // them into the given map of existing text attributes. Merges the given
  // spelling attributes, i.e. document marker information, into the given
  // text attributes starting at the given character offset. This is required
  // because document markers that are present on text leaves need to be
  // propagated to their parent object for compatibility with Firefox.
  static void MergeSpellingAndGrammarIntoTextAttributes(
      const ui::TextAttributeMap& spelling_attributes,
      int start_offset,
      ui::TextAttributeMap* text_attributes);

  // Return true is the list of text attributes already includes an invalid
  // attribute originating from ARIA.
  static bool HasInvalidAttribute(const ui::TextAttributeList& attributes);

  // A unique ID, since node IDs are frame-local.
  ui::AXUniqueId unique_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibility);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_H_
