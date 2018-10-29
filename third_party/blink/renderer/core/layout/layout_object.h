/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_H_

#include "base/auto_reset.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/api/hit_test_action.h"
#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object_child_list.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_type.h"
#include "third_party/blink/renderer/core/layout/subtree_layout_scope.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_observer.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_state.h"
#include "third_party/blink/renderer/core/paint/fragment_data.h"
#include "third_party/blink/renderer/core/paint/paint_phase.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/hit_test_rect.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/subtree_paint_property_update_reason.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class AffineTransform;
class Cursor;
class HitTestLocation;
class HitTestRequest;
class InlineBox;
class LayoutBoxModelObject;
class LayoutBlock;
class LayoutBlockFlow;
class LayoutFlowThread;
class LayoutGeometryMap;
class LayoutMultiColumnSpannerPlaceholder;
class LayoutView;
class LocalFrameView;
class NGPaintFragment;
class NGPhysicalBoxFragment;
class PaintLayer;
class PseudoStyleRequest;

struct PaintInfo;
struct PaintInvalidatorContext;
struct WebScrollIntoViewParams;

enum VisualRectFlags {
  kDefaultVisualRectFlags = 0,
  kEdgeInclusive = 1 << 0,
  // Use the GeometryMapper fast-path, if possible.
  kUseGeometryMapper = 1 << 1,
};

enum CursorDirective { kSetCursorBasedOnStyle, kSetCursor, kDoNotSetCursor };

enum HitTestFilter { kHitTestAll, kHitTestSelf, kHitTestDescendants };

enum MarkingBehavior {
  kMarkOnlyThis,
  kMarkContainerChain,
};

enum ScheduleRelayoutBehavior { kScheduleRelayout, kDontScheduleRelayout };

struct AnnotatedRegionValue {
  DISALLOW_NEW();
  bool operator==(const AnnotatedRegionValue& o) const {
    return draggable == o.draggable && bounds == o.bounds;
  }

  LayoutRect bounds;
  bool draggable;
};

#ifndef NDEBUG
const int kShowTreeCharacterOffset = 39;
#endif

// LayoutObject is the base class for all layout tree objects.
//
// LayoutObjects form a tree structure that is a close mapping of the DOM tree.
// The root of the LayoutObject tree is the LayoutView, which is the
// LayoutObject associated with the Document.
//
// Some LayoutObjects don't have an associated Node and are called "anonymous"
// (see the constructor below). Anonymous LayoutObjects exist for several
// purposes but are usually required by CSS. A good example is anonymous table
// parts (see LayoutTable for the expected structure). Anonymous LayoutObjects
// are generated when a new child is added to the tree in addChild(). See the
// function for some important information on this.
//
// Also some Node don't have an associated LayoutObjects e.g. if display: none
// or display: contents is set. For more detail, see LayoutObject::createObject
// that creates the right LayoutObject based on the style.
//
// Because the SVG and CSS classes both inherit from this object, functions can
// belong to either realm and sometimes to both.
//
// The purpose of the layout tree is to do layout (aka reflow) and store its
// results for painting and hit-testing. Layout is the process of sizing and
// positioning Nodes on the page. In Blink, layouts always start from a relayout
// boundary (see objectIsRelayoutBoundary in LayoutObject.cpp). As such, we
// need to mark the ancestors all the way to the enclosing relayout boundary in
// order to do a correct layout.
//
// Due to the high cost of layout, a lot of effort is done to avoid doing full
// layouts of nodes. This is why there are several types of layout available to
// bypass the complex operations. See the comments on the layout booleans in
// LayoutObjectBitfields below about the different layouts.
//
// To save memory, especially for the common child class LayoutText,
// LayoutObject doesn't provide storage for children. Descendant classes that do
// allow children have to have a LayoutObjectChildList member that stores the
// actual children and override virtualChildren().
//
// LayoutObject is an ImageResourceObserver, which means that it gets notified
// when associated images are changed. This is used for 2 main use cases:
// - reply to 'background-image' as we need to invalidate the background in this
//   case.
//   (See https://drafts.csswg.org/css-backgrounds-3/#the-background-image)
// - image (LayoutImage, LayoutSVGImage) or video (LayoutVideo) objects that are
//   placeholders for displaying them.
//
//
// ***** LIFETIME *****
//
// LayoutObjects are fully owned by their associated DOM node. In other words,
// it's the DOM node's responsibility to free its LayoutObject, this is why
// LayoutObjects are not and SHOULD NOT be RefCounted.
//
// LayoutObjects are created during the DOM attachment. This phase computes
// the style and create the LayoutObject associated with the Node (see
// Node::attachLayoutTree). LayoutObjects are destructed during detachment (see
// Node::detachLayoutTree), which can happen when the DOM node is removed from
// the
// DOM tree, during page tear down or when the style is changed to contain
// 'display: none'.
//
// Anonymous LayoutObjects are owned by their enclosing DOM node. This means
// that if the DOM node is detached, it has to destroy any anonymous
// descendants. This is done in LayoutObject::destroy().
//
// Note that for correctness, destroy() is expected to clean any anonymous
// wrappers as sequences of insertion / removal could make them visible to
// the page. This is done by LayoutObject::destroyAndCleanupAnonymousWrappers()
// which is the preferred way to destroy an object.
//
//
// ***** INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS *****
// The preferred logical widths are the intrinsic sizes of this element
// (https://drafts.csswg.org/css-sizing-3/#intrinsic). Intrinsic sizes depend
// mostly on the content and a limited set of style properties (e.g. any
// font-related property for text, 'min-width'/'max-width',
// 'min-height'/'max-height').
//
// Those widths are used to determine the final layout logical width, which
// depends on the layout algorithm used and the available logical width.
//
// LayoutObject only has getters for the widths (MinPreferredLogicalWidth and
// MaxPreferredLogicalWidth). However the storage for them is in LayoutBox (see
// min_preferred_logical_width_ and max_preferred_logical_width_). This is
// because only boxes implementing the full box model have a need for them.
// Because LayoutBlockFlow's intrinsic widths rely on the underlying text
// content, LayoutBlockFlow may call LayoutText::ComputePreferredLogicalWidths.
//
// The 2 widths are computed lazily during layout when the getters are called.
// The computation is done by calling ComputePreferredLogicalWidths() behind the
// scene. The boolean used to control the lazy recomputation is
// PreferredLogicalWidthsDirty.
//
// See the individual getters below for more details about what each width is.
class CORE_EXPORT LayoutObject : public ImageResourceObserver,
                                 public DisplayItemClient {
  friend class LayoutObjectChildList;
  FRIEND_TEST_ALL_PREFIXES(LayoutObjectTest, MutableForPaintingClearPaintFlags);
  friend class VisualRectMappingTest;

 public:
  // Anonymous objects should pass the document as their node, and they will
  // then automatically be marked as anonymous in the constructor.
  explicit LayoutObject(Node*);
  ~LayoutObject() override;

  // Returns the name of the layout object.
  virtual const char* GetName() const = 0;

  // Returns the decorated name used by run-layout-tests. The name contains the
  // name of the object along with extra information about the layout object
  // state (e.g. positioning).
  String DecoratedName() const;

  // This is an inexact determination of whether the display of this objects is
  // altered or obscured by CSS effects.
  bool HasDistortingVisualEffects() const;

  // Returns false iff this object or one of its ancestors has opacity:0.
  bool HasNonZeroEffectiveOpacity() const;

 protected:
  void EnsureIdForTesting() { fragment_.EnsureIdForTesting(); };

 private:
  // DisplayItemClient methods.

  // Hide DisplayItemClient's methods whose names are too generic for
  // LayoutObjects. Should use LayoutObject's methods instead.
  using DisplayItemClient::Invalidate;
  using DisplayItemClient::IsValid;
  using DisplayItemClient::GetPaintInvalidationReason;

  // Do not call VisualRect directly outside of the DisplayItemClient
  // interface, use a per-fragment one on FragmentData instead.
  LayoutRect VisualRect() const final;

  void ClearPartialInvalidationVisualRect() const final {
    return GetMutableForPainting()
        .FirstFragment()
        .SetPartialInvalidationVisualRect(LayoutRect());
  }

 public:
  LayoutRect PartialInvalidationVisualRect() const final {
    return FirstFragment().PartialInvalidationVisualRect();
  }

  String DebugName() const final;

  // End of DisplayItemClient methods.

  LayoutObject* Parent() const { return parent_; }
  bool IsDescendantOf(const LayoutObject*) const;

  LayoutObject* PreviousSibling() const { return previous_; }
  LayoutObject* NextSibling() const { return next_; }

  DISABLE_CFI_PERF
  LayoutObject* SlowFirstChild() const {
    if (const LayoutObjectChildList* children = VirtualChildren())
      return children->FirstChild();
    return nullptr;
  }
  LayoutObject* SlowLastChild() const {
    if (const LayoutObjectChildList* children = VirtualChildren())
      return children->LastChild();
    return nullptr;
  }

  // See comment in the class description as to why there is no child.
  virtual LayoutObjectChildList* VirtualChildren() { return nullptr; }
  virtual const LayoutObjectChildList* VirtualChildren() const {
    return nullptr;
  }

  LayoutObject* NextInPreOrder() const;
  LayoutObject* NextInPreOrder(const LayoutObject* stay_within) const;
  LayoutObject* NextInPreOrderAfterChildren() const;
  LayoutObject* NextInPreOrderAfterChildren(
      const LayoutObject* stay_within) const;
  LayoutObject* PreviousInPreOrder() const;
  LayoutObject* PreviousInPreOrder(const LayoutObject* stay_within) const;

  LayoutObject* LastLeafChild() const;

  // The following functions are used when the layout tree hierarchy changes to
  // make sure layers get properly added and removed. Since containership can be
  // implemented by any subclass, and since a hierarchy can contain a mixture of
  // boxes and other object types, these functions need to be in the base class.
  PaintLayer* EnclosingLayer() const;
  void AddLayers(PaintLayer* parent_layer);
  void RemoveLayers(PaintLayer* parent_layer);
  void MoveLayers(PaintLayer* old_parent, PaintLayer* new_parent);
  PaintLayer* FindNextLayer(PaintLayer* parent_layer,
                            LayoutObject* start_point,
                            bool check_parent = true);

  // Returns the layer that will paint this object. During paint invalidation,
  // we should use the faster PaintInvalidatorContext::painting_layer instead.
  PaintLayer* PaintingLayer() const;

  bool IsFixedPositionObjectInPagedMedia() const;

  // Takes the given rect, assumed to be in absolute coordinates, and scrolls
  // this Element and all it's containers such that the child content of this
  // Element at that rect is visible in the viewport. Returns the new absolute
  // rect of the target rect after all scrolls are completed, in the coordinate
  // space of the local root frame.
  LayoutRect ScrollRectToVisible(const LayoutRect&,
                                 const WebScrollIntoViewParams&);

  // Convenience function for getting to the nearest enclosing box of a
  // LayoutObject.
  LayoutBox* EnclosingBox() const;

  LayoutBox* EnclosingScrollableBox() const;

  // Returns the containing block flow if it's a LayoutNGBlockFlow, or nullptr
  // otherwise. Note that the semantics is different from |EnclosingBox| for
  // atomic inlines that this function returns the container, while
  // |EnclosingBox| returns the atomic inline itself.
  LayoutBlockFlow* ContainingNGBlockFlow() const;

  // Returns |NGPhysicalBoxFragment| for |ContainingNGBlockFlow()| or nullptr
  // otherwise.
  const NGPhysicalBoxFragment* ContainingBlockFlowFragment() const;

  // Function to return our enclosing flow thread if we are contained inside
  // one. This function follows the containing block chain.
  LayoutFlowThread* FlowThreadContainingBlock() const {
    if (!IsInsideFlowThread())
      return nullptr;
    return LocateFlowThreadContainingBlock();
  }

#if DCHECK_IS_ON()
  void SetHasAXObject(bool flag) { has_ax_object_ = flag; }
  bool HasAXObject() const { return has_ax_object_; }

  // Helper class forbidding calls to setNeedsLayout() during its lifetime.
  class SetLayoutNeededForbiddenScope {
   public:
    explicit SetLayoutNeededForbiddenScope(LayoutObject&);
    ~SetLayoutNeededForbiddenScope();

   private:
    LayoutObject& layout_object_;
    bool preexisting_forbidden_;
  };

  void AssertLaidOut() const {
#ifndef NDEBUG
    if (NeedsLayout())
      ShowLayoutTreeForThis();
#endif
    SECURITY_DCHECK(!NeedsLayout());
  }

  void AssertSubtreeIsLaidOut() const {
    for (const LayoutObject* layout_object = this; layout_object;
         layout_object = layout_object->NextInPreOrder())
      layout_object->AssertLaidOut();
  }

  void AssertClearedPaintInvalidationFlags() const {
#ifndef NDEBUG
    if (PaintInvalidationStateIsDirty()) {
      ShowLayoutTreeForThis();
      NOTREACHED();
    }
#endif
  }

  void AssertSubtreeClearedPaintInvalidationFlags() const {
    for (const LayoutObject* layout_object = this; layout_object;
         layout_object = layout_object->NextInPreOrder())
      layout_object->AssertClearedPaintInvalidationFlags();
  }

#endif

  // LayoutObject tree manipulation
  //////////////////////////////////////////
  DISABLE_CFI_PERF virtual bool CanHaveChildren() const {
    return VirtualChildren();
  }
  virtual bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const {
    return true;
  }

  // This function is called whenever a child is inserted under |this|.
  //
  // The main purpose of this function is to generate a consistent layout
  // tree, which means generating the missing anonymous objects. Most of the
  // time there'll be no anonymous objects to generate.
  //
  // The following invariants are true on the input:
  // - |newChild->node()| is a child of |node()|, if |this| is not
  //   anonymous. If |this| is anonymous, the invariant holds with the
  //   enclosing non-anonymous LayoutObject.
  // - |beforeChild->node()| (if |beforeChild| is provided and not anonymous)
  //   is a sibling of |newChild->node()| (if |newChild| is not anonymous).
  //
  // The reason for these invariants is that insertions are performed on the
  // DOM tree. Because the layout tree may insert extra anonymous renderers,
  // the previous invariants are only guaranteed for the DOM tree. In
  // particular, |beforeChild| may not be a direct child when it's wrapped in
  // anonymous wrappers.
  //
  // Classes inserting anonymous LayoutObjects in the tree are expected to
  // check for the anonymous wrapper case with:
  //                    beforeChild->parent() != this
  //
  // The usage of |child/parent/sibling| in this comment actually means
  // |child/parent/sibling| in a flat tree because a layout tree is generated
  // from a structure of a flat tree if Shadow DOM is used.
  // See LayoutTreeBuilderTraversal and FlatTreeTraversal.
  //
  // See LayoutTable::addChild and LayoutBlock::addChild.
  // TODO(jchaffraix): |newChild| cannot be nullptr and should be a reference.
  virtual void AddChild(LayoutObject* new_child,
                        LayoutObject* before_child = nullptr);
  virtual void AddChildIgnoringContinuation(
      LayoutObject* new_child,
      LayoutObject* before_child = nullptr) {
    return AddChild(new_child, before_child);
  }
  virtual void RemoveChild(LayoutObject*);
  virtual bool CreatesAnonymousWrapper() const { return false; }
  //////////////////////////////////////////

  // Sets the parent of this object but doesn't add it as a child of the parent.
  void SetDangerousOneWayParent(LayoutObject*);

  UniqueObjectId UniqueId() const { return fragment_.UniqueId(); }

  inline bool ShouldApplyPaintContainment() const {
    return StyleRef().ContainsPaint() &&
           (!IsInline() || IsAtomicInlineLevel()) && !IsRubyText() &&
           (!IsTablePart() || IsLayoutBlockFlow());
  }

  inline bool ShouldApplyLayoutContainment() const {
    return StyleRef().ContainsLayout() &&
           (!IsInline() || IsAtomicInlineLevel()) && !IsRubyText() &&
           (!IsTablePart() || IsLayoutBlockFlow());
  }

  inline bool ShouldApplySizeContainment() const {
    return StyleRef().ContainsSize() &&
           (!IsInline() || IsAtomicInlineLevel()) && !IsRubyText() &&
           (!IsTablePart() || IsTableCaption()) && !IsTable();
  }

 private:
  //////////////////////////////////////////
  // Helper functions. Dangerous to use!
  void SetPreviousSibling(LayoutObject* previous) { previous_ = previous; }
  void SetNextSibling(LayoutObject* next) { next_ = next; }
  void SetParent(LayoutObject* parent) {
    parent_ = parent;

    // Only update if our flow thread state is different from our new parent and
    // if we're not a LayoutFlowThread.
    // A LayoutFlowThread is always considered to be inside itself, so it never
    // has to change its state in response to parent changes.
    bool inside_flow_thread = parent && parent->IsInsideFlowThread();
    if (inside_flow_thread != IsInsideFlowThread() && !IsLayoutFlowThread())
      SetIsInsideFlowThreadIncludingDescendants(inside_flow_thread);
  }

  //////////////////////////////////////////
 private:
#if DCHECK_IS_ON()
  bool IsSetNeedsLayoutForbidden() const { return set_needs_layout_forbidden_; }
  void SetNeedsLayoutIsForbidden(bool flag) {
    set_needs_layout_forbidden_ = flag;
  }
#endif

  void AddAbsoluteRectForLayer(IntRect& result);
  bool RequiresAnonymousTableWrappers(const LayoutObject*) const;

 public:
  // Dump this layout object to the specified string builder.
  void DumpLayoutObject(StringBuilder&,
                        bool dump_address,
                        unsigned show_tree_character_offset) const;

#ifndef NDEBUG
  void ShowTreeForThis() const;
  void ShowLayoutTreeForThis() const;
  void ShowLineTreeForThis() const;
  void ShowLayoutObject() const;

  // Dump the subtree established by this layout object to the specified string
  // builder. There will be one object per line, and descendants will be
  // indented according to their tree level. The optional "marked_foo"
  // parameters can be used to mark up to two objects in the subtree with a
  // label.
  void DumpLayoutTreeAndMark(StringBuilder&,
                             const LayoutObject* marked_object1 = nullptr,
                             const char* marked_label1 = nullptr,
                             const LayoutObject* marked_object2 = nullptr,
                             const char* marked_label2 = nullptr,
                             unsigned depth = 0) const;
#endif

  // This function is used to create the appropriate LayoutObject based
  // on the style, in particular 'display' and 'content'.
  // "display: none" or "display: contents" are the only times this function
  // will return nullptr.
  //
  // For renderer creation, the inline-* values create the same renderer
  // as the non-inline version. The difference is that inline-* sets
  // is_inline_ during initialization. This means that
  // "display: inline-table" creates a LayoutTable, like "display: table".
  //
  // Ideally every Element::createLayoutObject would call this function to
  // respond to 'display' but there are deep rooted assumptions about
  // which LayoutObject is created on a fair number of Elements. This
  // function also doesn't handle the default association between a tag
  // and its renderer (e.g. <iframe> creates a LayoutIFrame even if the
  // initial 'display' value is inline).
  static LayoutObject* CreateObject(Element*, const ComputedStyle&);

  // LayoutObjects are allocated out of the rendering partition.
  void* operator new(size_t);
  void operator delete(void*);

  bool IsPseudoElement() const {
    return GetNode() && GetNode()->IsPseudoElement();
  }

  virtual bool IsBoxModelObject() const { return false; }
  bool IsBR() const { return IsOfType(kLayoutObjectBr); }
  bool IsCanvas() const { return IsOfType(kLayoutObjectCanvas); }
  bool IsCounter() const { return IsOfType(kLayoutObjectCounter); }
  bool IsDetailsMarker() const { return IsOfType(kLayoutObjectDetailsMarker); }
  bool IsEmbeddedObject() const {
    return IsOfType(kLayoutObjectEmbeddedObject);
  }
  bool IsFieldset() const { return IsOfType(kLayoutObjectFieldset); }
  bool IsLayoutNGFieldset() const { return IsOfType(kLayoutObjectNGFieldset); }
  bool IsFieldsetIncludingNG() const {
    return IsFieldset() || IsLayoutNGFieldset();
  }
  bool IsFileUploadControl() const {
    return IsOfType(kLayoutObjectFileUploadControl);
  }
  bool IsFrame() const { return IsOfType(kLayoutObjectFrame); }
  bool IsFrameSet() const { return IsOfType(kLayoutObjectFrameSet); }
  bool IsLayoutNGBlockFlow() const {
    return IsOfType(kLayoutObjectNGBlockFlow);
  }
  bool IsLayoutNGFlexibleBox() const {
    return IsOfType(kLayoutObjectNGFlexibleBox);
  }
  bool IsLayoutNGMixin() const { return IsOfType(kLayoutObjectNGMixin); }
  bool IsLayoutNGListItem() const { return IsOfType(kLayoutObjectNGListItem); }
  bool IsLayoutNGListMarker() const {
    return IsOfType(kLayoutObjectNGListMarker);
  }
  bool IsLayoutNGListMarkerImage() const {
    return IsOfType(kLayoutObjectNGListMarkerImage);
  }
  bool IsLayoutNGText() const { return IsOfType(kLayoutObjectNGText); }
  bool IsLayoutTableCol() const {
    return IsOfType(kLayoutObjectLayoutTableCol);
  }
  bool IsListBox() const { return IsOfType(kLayoutObjectListBox); }
  bool IsListItem() const { return IsOfType(kLayoutObjectListItem); }
  bool IsListMarker() const { return IsOfType(kLayoutObjectListMarker); }
  bool IsMedia() const { return IsOfType(kLayoutObjectMedia); }
  bool IsMenuList() const { return IsOfType(kLayoutObjectMenuList); }
  bool IsProgress() const { return IsOfType(kLayoutObjectProgress); }
  bool IsQuote() const { return IsOfType(kLayoutObjectQuote); }
  bool IsLayoutButton() const { return IsOfType(kLayoutObjectLayoutButton); }
  bool IsLayoutCustom() const { return IsOfType(kLayoutObjectLayoutCustom); }
  bool IsLayoutGrid() const { return IsOfType(kLayoutObjectLayoutGrid); }
  bool IsLayoutIFrame() const { return IsOfType(kLayoutObjectLayoutIFrame); }
  bool IsLayoutImage() const { return IsOfType(kLayoutObjectLayoutImage); }
  bool IsLayoutMultiColumnSet() const {
    return IsOfType(kLayoutObjectLayoutMultiColumnSet);
  }
  bool IsLayoutMultiColumnSpannerPlaceholder() const {
    return IsOfType(kLayoutObjectLayoutMultiColumnSpannerPlaceholder);
  }
  bool IsLayoutReplaced() const {
    return IsOfType(kLayoutObjectLayoutReplaced);
  }
  bool IsLayoutScrollbarPart() const {
    return IsOfType(kLayoutObjectLayoutScrollbarPart);
  }
  bool IsLayoutView() const { return IsOfType(kLayoutObjectLayoutView); }
  bool IsRuby() const { return IsOfType(kLayoutObjectRuby); }
  bool IsRubyBase() const { return IsOfType(kLayoutObjectRubyBase); }
  bool IsRubyRun() const { return IsOfType(kLayoutObjectRubyRun); }
  bool IsRubyText() const { return IsOfType(kLayoutObjectRubyText); }
  bool IsSlider() const { return IsOfType(kLayoutObjectSlider); }
  bool IsSliderThumb() const { return IsOfType(kLayoutObjectSliderThumb); }
  bool IsTable() const { return IsOfType(kLayoutObjectTable); }
  bool IsTableCaption() const { return IsOfType(kLayoutObjectTableCaption); }
  bool IsTableCell() const { return IsOfType(kLayoutObjectTableCell); }
  bool IsTableRow() const { return IsOfType(kLayoutObjectTableRow); }
  bool IsTableSection() const { return IsOfType(kLayoutObjectTableSection); }
  bool IsTextArea() const { return IsOfType(kLayoutObjectTextArea); }
  bool IsTextControl() const { return IsOfType(kLayoutObjectTextControl); }
  bool IsTextField() const { return IsOfType(kLayoutObjectTextField); }
  bool IsVideo() const { return IsOfType(kLayoutObjectVideo); }
  bool IsWidget() const { return IsOfType(kLayoutObjectWidget); }

  virtual bool IsImage() const { return false; }

  virtual bool IsInlineBlockOrInlineTable() const { return false; }
  virtual bool IsLayoutBlock() const { return false; }
  virtual bool IsLayoutBlockFlow() const { return false; }
  virtual bool IsLayoutFlowThread() const { return false; }
  virtual bool IsLayoutInline() const { return false; }
  virtual bool IsLayoutEmbeddedContent() const { return false; }
  virtual bool IsLayoutNGObject() const { return false; }

  bool IsDocumentElement() const {
    return GetDocument().documentElement() == node_;
  }
  // isBody is called from LayoutBox::styleWillChange and is thus quite hot.
  bool IsBody() const {
    return GetNode() && GetNode()->HasTagName(HTMLNames::bodyTag);
  }
  bool IsHR() const;

  bool IsTablePart() const {
    return IsTableCell() || IsLayoutTableCol() || IsTableCaption() ||
           IsTableRow() || IsTableSection();
  }

  inline bool IsBeforeContent() const;
  inline bool IsAfterContent() const;
  inline bool IsBeforeOrAfterContent() const;
  static inline bool IsAfterContent(const LayoutObject* obj) {
    return obj && obj->IsAfterContent();
  }

  bool HasCounterNodeMap() const { return bitfields_.HasCounterNodeMap(); }
  void SetHasCounterNodeMap(bool has_counter_node_map) {
    bitfields_.SetHasCounterNodeMap(has_counter_node_map);
  }

  bool IsTruncated() const { return bitfields_.IsTruncated(); }
  void SetIsTruncated(bool is_truncated) {
    bitfields_.SetIsTruncated(is_truncated);
  }

  bool EverHadLayout() const { return bitfields_.EverHadLayout(); }

  bool ChildrenInline() const { return bitfields_.ChildrenInline(); }
  void SetChildrenInline(bool b) { bitfields_.SetChildrenInline(b); }

  bool AlwaysCreateLineBoxesForLayoutInline() const {
    DCHECK(IsLayoutInline());
    return bitfields_.AlwaysCreateLineBoxesForLayoutInline();
  }
  void SetAlwaysCreateLineBoxesForLayoutInline(bool always_create_line_boxes) {
    DCHECK(IsLayoutInline());
    bitfields_.SetAlwaysCreateLineBoxesForLayoutInline(
        always_create_line_boxes);
  }

  bool AncestorLineBoxDirty() const {
    return bitfields_.AncestorLineBoxDirty();
  }
  void SetAncestorLineBoxDirty(bool value = true) {
    bitfields_.SetAncestorLineBoxDirty(value);
    if (value)
      SetNeedsLayoutAndFullPaintInvalidation(
          LayoutInvalidationReason::kLineBoxesChanged);
  }

  void SetIsInsideFlowThreadIncludingDescendants(bool);

  bool IsInsideFlowThread() const { return bitfields_.IsInsideFlowThread(); }
  void SetIsInsideFlowThread(bool inside_flow_thread) {
    bitfields_.SetIsInsideFlowThread(inside_flow_thread);
  }

  // FIXME: Until all SVG layoutObjects can be subclasses of
  // LayoutSVGModelObject we have to add SVG layoutObject methods to
  // LayoutObject with an NOTREACHED() default implementation.
  bool IsSVG() const { return IsOfType(kLayoutObjectSVG); }
  bool IsSVGRoot() const { return IsOfType(kLayoutObjectSVGRoot); }
  bool IsSVGChild() const { return IsSVG() && !IsSVGRoot(); }
  bool IsSVGContainer() const { return IsOfType(kLayoutObjectSVGContainer); }
  bool IsSVGTransformableContainer() const {
    return IsOfType(kLayoutObjectSVGTransformableContainer);
  }
  bool IsSVGViewportContainer() const {
    return IsOfType(kLayoutObjectSVGViewportContainer);
  }
  bool IsSVGHiddenContainer() const {
    return IsOfType(kLayoutObjectSVGHiddenContainer);
  }
  bool IsSVGShape() const { return IsOfType(kLayoutObjectSVGShape); }
  bool IsSVGText() const { return IsOfType(kLayoutObjectSVGText); }
  bool IsSVGTextPath() const { return IsOfType(kLayoutObjectSVGTextPath); }
  bool IsSVGInline() const { return IsOfType(kLayoutObjectSVGInline); }
  bool IsSVGInlineText() const { return IsOfType(kLayoutObjectSVGInlineText); }
  bool IsSVGImage() const { return IsOfType(kLayoutObjectSVGImage); }
  bool IsSVGForeignObject() const {
    return IsOfType(kLayoutObjectSVGForeignObject);
  }
  bool IsSVGResourceContainer() const {
    return IsOfType(kLayoutObjectSVGResourceContainer);
  }
  bool IsSVGResourceFilter() const {
    return IsOfType(kLayoutObjectSVGResourceFilter);
  }
  bool IsSVGResourceFilterPrimitive() const {
    return IsOfType(kLayoutObjectSVGResourceFilterPrimitive);
  }

  // FIXME: Those belong into a SVG specific base-class for all layoutObjects
  // (see above). Unfortunately we don't have such a class yet, because it's not
  // possible for all layoutObjects to inherit from LayoutSVGObject ->
  // LayoutObject (some need LayoutBlock inheritance for instance)
  virtual void SetNeedsTransformUpdate() {}
  virtual void SetNeedsBoundariesUpdate();

  // Per the spec, mix-blend-mode applies to all non-SVG elements, and SVG
  // elements that are container elements, graphics elements or graphics
  // referencing elements.
  // https://www.w3.org/TR/compositing-1/#propdef-mix-blend-mode
  bool IsBlendingAllowed() const {
    return !IsSVG() || IsSVGShape() || IsSVGImage() || IsSVGText() ||
           IsSVGInline() || IsSVGRoot() || IsSVGForeignObject() ||
           // Blending does not apply to non-renderable elements such as
           // patterns (see: https://github.com/w3c/fxtf-drafts/issues/309).
           (IsSVGContainer() && !IsSVGHiddenContainer());
  }
  virtual bool HasNonIsolatedBlendingDescendants() const {
    // This is only implemented for layout objects that containt SVG flow.
    // For HTML/CSS layout objects, use the PaintLayer version instead.
    DCHECK(IsSVG());
    return false;
  }
  enum DescendantIsolationState {
    kDescendantIsolationRequired,
    kDescendantIsolationNeedsUpdate,
  };
  virtual void DescendantIsolationRequirementsChanged(
      DescendantIsolationState) {}

  // Per SVG 1.1 objectBoundingBox ignores clipping, masking, filter effects,
  // opacity and stroke-width.
  // This is used for all computation of objectBoundingBox relative units and by
  // SVGGraphicsElement::getBBox().
  // NOTE: Markers are not specifically ignored here by SVG 1.1 spec, but we
  // ignore them since stroke-width is ignored (and marker size can depend on
  // stroke-width). objectBoundingBox is returned local coordinates.
  // The name objectBoundingBox is taken from the SVG 1.1 spec.
  virtual FloatRect ObjectBoundingBox() const;
  virtual FloatRect StrokeBoundingBox() const;

  // Returns the smallest rectangle enclosing all of the painted content
  // respecting clipping, masking, filters, opacity, stroke-width and markers.
  // The local SVG coordinate space is the space where localSVGTransform
  // applies. For SVG objects defining viewports (e.g.
  // LayoutSVGViewportContainer and  LayoutSVGResourceMarker), the local SVG
  // coordinate space is the viewport space.
  virtual FloatRect VisualRectInLocalSVGCoordinates() const;

  // This returns the transform applying to the local SVG coordinate space,
  // which combines the CSS transform properties and animation motion transform.
  // See SVGElement::calculateTransform().
  // Most callsites want localToSVGParentTransform() instead.
  virtual AffineTransform LocalSVGTransform() const;

  // Returns the full transform mapping from local coordinates to parent's local
  // coordinates. For most SVG objects, this is the same as localSVGTransform.
  // For SVG objects defining viewports (see visualRectInLocalSVGCoordinates),
  // this includes any viewport transforms and x/y offsets as well as
  // localSVGTransform.
  virtual AffineTransform LocalToSVGParentTransform() const {
    return LocalSVGTransform();
  }

  // End of SVG-specific methods.

  bool IsAnonymous() const { return bitfields_.IsAnonymous(); }
  bool IsAnonymousBlock() const {
    // This function is kept in sync with anonymous block creation conditions in
    // LayoutBlock::createAnonymousBlock(). This includes creating an anonymous
    // LayoutBlock having a BLOCK or BOX display. Other classes such as
    // LayoutTextFragment are not LayoutBlocks and will return false.
    // See https://bugs.webkit.org/show_bug.cgi?id=56709.
    return IsAnonymous() &&
           (StyleRef().Display() == EDisplay::kBlock ||
            StyleRef().Display() == EDisplay::kWebkitBox) &&
           StyleRef().StyleType() == kPseudoIdNone && IsLayoutBlock() &&
           !IsListMarker() && !IsLayoutFlowThread() &&
           !IsLayoutMultiColumnSet();
  }
  bool IsElementContinuation() const {
    return GetNode() && GetNode()->GetLayoutObject() != this;
  }
  bool IsInlineElementContinuation() const {
    return IsElementContinuation() && IsInline();
  }
  virtual LayoutBoxModelObject* VirtualContinuation() const { return nullptr; }

  bool IsFloating() const { return bitfields_.Floating(); }

  bool IsFloatingWithNonContainingBlockParent() const {
    return IsFloating() && Parent() && !Parent()->IsLayoutBlockFlow();
  }

  // absolute or fixed positioning
  bool IsOutOfFlowPositioned() const {
    return bitfields_.IsOutOfFlowPositioned();
  }
  // relative or sticky positioning
  bool IsInFlowPositioned() const { return bitfields_.IsInFlowPositioned(); }
  bool IsRelPositioned() const { return bitfields_.IsRelPositioned(); }
  bool IsStickyPositioned() const { return bitfields_.IsStickyPositioned(); }
  bool IsFixedPositioned() const {
    return IsOutOfFlowPositioned() &&
           StyleRef().GetPosition() == EPosition::kFixed;
  }
  bool IsAbsolutePositioned() const {
    return IsOutOfFlowPositioned() &&
           StyleRef().GetPosition() == EPosition::kAbsolute;
  }
  bool IsPositioned() const { return bitfields_.IsPositioned(); }

  bool IsText() const { return bitfields_.IsText(); }
  bool IsBox() const { return bitfields_.IsBox(); }
  bool IsInline() const { return bitfields_.IsInline(); }  // inline object
  bool IsInLayoutNGInlineFormattingContext() const {
    return bitfields_.IsInLayoutNGInlineFormattingContext();
  }
  bool IsAtomicInlineLevel() const { return bitfields_.IsAtomicInlineLevel(); }
  bool IsHorizontalWritingMode() const {
    return bitfields_.HorizontalWritingMode();
  }
  bool HasFlippedBlocksWritingMode() const {
    return StyleRef().IsFlippedBlocksWritingMode();
  }

  bool HasLayer() const { return bitfields_.HasLayer(); }

  // This may be different from StyleRef().hasBoxDecorationBackground() because
  // some objects may have box decoration background other than from their own
  // style.
  bool HasBoxDecorationBackground() const {
    return bitfields_.HasBoxDecorationBackground();
  }

  bool BackgroundIsKnownToBeObscured() const;

  bool NeedsLayout() const {
    return bitfields_.SelfNeedsLayout() ||
           bitfields_.NormalChildNeedsLayout() ||
           bitfields_.PosChildNeedsLayout() ||
           bitfields_.NeedsSimplifiedNormalFlowLayout() ||
           bitfields_.NeedsPositionedMovementLayout();
  }

  bool SelfNeedsLayout() const { return bitfields_.SelfNeedsLayout(); }
  bool NeedsPositionedMovementLayout() const {
    return bitfields_.NeedsPositionedMovementLayout();
  }

  bool PosChildNeedsLayout() const { return bitfields_.PosChildNeedsLayout(); }
  bool NeedsSimplifiedNormalFlowLayout() const {
    return bitfields_.NeedsSimplifiedNormalFlowLayout();
  }
  bool NormalChildNeedsLayout() const {
    return bitfields_.NormalChildNeedsLayout();
  }
  bool NeedsCollectInlines() const { return bitfields_.NeedsCollectInlines(); }

  bool PreferredLogicalWidthsDirty() const {
    return bitfields_.PreferredLogicalWidthsDirty();
  }

  bool NeedsOverflowRecalc() const {
    return SelfNeedsOverflowRecalc() || ChildNeedsOverflowRecalc();
  }
  bool SelfNeedsOverflowRecalc() const {
    return bitfields_.SelfNeedsLayoutOverflowRecalc() ||
           bitfields_.SelfNeedsVisualOverflowRecalc();
  }
  bool ChildNeedsOverflowRecalc() const {
    return bitfields_.ChildNeedsLayoutOverflowRecalc() ||
           bitfields_.ChildNeedsVisualOverflowRecalc();
  }

  bool NeedsLayoutOverflowRecalc() const {
    return bitfields_.SelfNeedsLayoutOverflowRecalc() ||
           bitfields_.ChildNeedsLayoutOverflowRecalc();
  }
  bool SelfNeedsLayoutOverflowRecalc() const {
    return bitfields_.SelfNeedsLayoutOverflowRecalc();
  }
  bool ChildNeedsLayoutOverflowRecalc() const {
    return bitfields_.ChildNeedsLayoutOverflowRecalc();
  }
  void SetSelfNeedsLayoutOverflowRecalc() {
    bitfields_.SetSelfNeedsLayoutOverflowRecalc(true);
  }
  void SetChildNeedsLayoutOverflowRecalc() {
    bitfields_.SetChildNeedsLayoutOverflowRecalc(true);
  }
  void ClearSelfNeedsLayoutOverflowRecalc() {
    bitfields_.SetSelfNeedsLayoutOverflowRecalc(false);
  }
  void ClearChildNeedsLayoutOverflowRecalc() {
    bitfields_.SetChildNeedsLayoutOverflowRecalc(false);
  }

  bool NeedsVisualOverflowRecalc() const {
    return bitfields_.SelfNeedsVisualOverflowRecalc() ||
           bitfields_.ChildNeedsVisualOverflowRecalc();
  }
  bool SelfNeedsVisualOverflowRecalc() const {
    return bitfields_.SelfNeedsVisualOverflowRecalc();
  }
  bool ChildNeedsVisualOverflowRecalc() const {
    return bitfields_.ChildNeedsVisualOverflowRecalc();
  }
  void SetSelfNeedsVisualOverflowRecalc() {
    bitfields_.SetSelfNeedsVisualOverflowRecalc(true);
  }
  void SetChildNeedsVisualOverflowRecalc() {
    bitfields_.SetChildNeedsVisualOverflowRecalc(true);
  }
  void ClearSelfNeedsVisualOverflowRecalc() {
    bitfields_.SetSelfNeedsVisualOverflowRecalc(false);
  }
  void ClearChildNeedsVisualOverflowRecalc() {
    bitfields_.SetChildNeedsVisualOverflowRecalc(false);
  }

  // CSS clip only applies when position is absolute or fixed. Prefer this check
  // over !StyleRef().HasAutoClip().
  bool HasClip() const {
    return IsOutOfFlowPositioned() && !StyleRef().HasAutoClip();
  }
  bool HasOverflowClip() const { return bitfields_.HasOverflowClip(); }
  bool ShouldClipOverflow() const { return bitfields_.ShouldClipOverflow(); }
  bool HasClipRelatedProperty() const;

  bool HasTransformRelatedProperty() const {
    return bitfields_.HasTransformRelatedProperty();
  }
  bool IsTransformApplicable() const { return IsBox() || IsSVG(); }
  bool HasMask() const { return StyleRef().HasMask(); }
  bool HasClipPath() const { return StyleRef().ClipPath(); }
  bool HasHiddenBackface() const {
    return StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden;
  }
  bool HasBackdropFilter() const { return StyleRef().HasBackdropFilter(); }

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  // Not calling style()->hasFilterInducingProperty because some objects force
  // to ignore reflection style (e.g. LayoutInline).
  bool HasFilterInducingProperty() const {
    return StyleRef().HasFilter() || HasReflection();
  }

  bool HasShapeOutside() const { return StyleRef().ShapeOutside(); }

  // Return true if the given object is the effective root scroller in its
  // Document. See |effective root scroller| in page/scrolling/README.md.
  // Note: a root scroller always establishes a PaintLayer.
  // This bit is updated in
  // RootScrollerController::RecomputeEffectiveRootScroller in the LayoutClean
  // document lifecycle phase.
  bool IsEffectiveRootScroller() const {
    return bitfields_.IsEffectiveRootScroller();
  }

  // Return true if this is the "rendered legend" of a fieldset. They get
  // special treatment, in that they establish a new formatting context, and
  // shrink to fit if no logical width is specified.
  bool IsRenderedLegend() const;

  // The pseudo element style can be cached or uncached.  Use the cached method
  // if the pseudo element doesn't respect any pseudo classes (and therefore
  // has no concept of changing state).
  const ComputedStyle* GetCachedPseudoStyle(
      PseudoId,
      const ComputedStyle* parent_style = nullptr) const;
  scoped_refptr<ComputedStyle> GetUncachedPseudoStyle(
      const PseudoStyleRequest&,
      const ComputedStyle* parent_style = nullptr) const;

  LayoutView* View() const { return GetDocument().GetLayoutView(); }
  LocalFrameView* GetFrameView() const { return GetDocument().View(); }

  bool IsRooted() const;

  Node* GetNode() const { return IsAnonymous() ? nullptr : node_; }

  Node* NonPseudoNode() const {
    return IsPseudoElement() ? nullptr : GetNode();
  }

  void ClearNode() { node_ = nullptr; }

  // Returns the styled node that caused the generation of this layoutObject.
  // This is the same as node() except for layoutObjects of :before, :after and
  // :first-letter pseudo elements for which their parent node is returned.
  Node* GeneratingNode() const {
    return IsPseudoElement() ? GetNode()->ParentOrShadowHostNode() : GetNode();
  }

  Document& GetDocument() const {
    DCHECK(node_ || Parent());  // crbug.com/402056
    return node_ ? node_->GetDocument() : Parent()->GetDocument();
  }
  LocalFrame* GetFrame() const { return GetDocument().GetFrame(); }

  virtual LayoutMultiColumnSpannerPlaceholder* SpannerPlaceholder() const {
    return nullptr;
  }
  bool IsColumnSpanAll() const {
    return StyleRef().GetColumnSpan() == EColumnSpan::kAll &&
           SpannerPlaceholder();
  }

  // We include isLayoutButton() in this check, because buttons are
  // implemented using flex box but should still support things like
  // first-line, first-letter and text-overflow.
  // The flex box and grid specs require that flex box and grid do not
  // support first-line|first-letter, though.
  // When LayoutObject and display do not agree, allow first-line|first-letter
  // only when both indicate it's a block container.
  // TODO(cbiesinger): Remove when buttons are implemented with align-items
  // instead of flex box. crbug.com/226252.
  bool BehavesLikeBlockContainer() const {
    return (IsLayoutBlockFlow() && StyleRef().IsDisplayBlockContainer()) ||
           IsLayoutButton();
  }

  // May be optionally passed to container() and various other similar methods
  // that search the ancestry for some sort of containing block. Used to
  // determine if we skipped certain objects while walking the ancestry.
  class AncestorSkipInfo {
   public:
    AncestorSkipInfo(const LayoutObject* ancestor,
                     bool check_for_filters = false)
        : ancestor_(ancestor), check_for_filters_(check_for_filters) {}

    // Update skip info output based on the layout object passed.
    void Update(const LayoutObject& object) {
      if (&object == ancestor_)
        ancestor_skipped_ = true;
      if (check_for_filters_ && object.HasFilterInducingProperty())
        filter_skipped_ = true;
    }

    // TODO(mstensho): Get rid of this. It's just a temporary thing to retain
    // old behavior in LayoutObject::container().
    void ResetOutput() {
      ancestor_skipped_ = false;
      filter_skipped_ = false;
    }

    bool AncestorSkipped() const { return ancestor_skipped_; }
    bool FilterSkipped() const {
      DCHECK(check_for_filters_);
      return filter_skipped_;
    }

   private:
    // Input: A potential ancestor to look for. If we walk past this one while
    // walking the ancestry in search of some containing block, ancestorSkipped
    // will be set to true.
    const LayoutObject* ancestor_;
    // Input: When set, we'll check if we skip objects with filter inducing
    // properties.
    bool check_for_filters_;

    // Output: Set to true if |ancestor| was walked past while walking the
    // ancestry.
    bool ancestor_skipped_ = false;
    // Output: Set to true if we walked past a filter object. This will be set
    // regardless of the value of |ancestor|.
    bool filter_skipped_ = false;
  };

  // This function returns the containing block of the object.
  // Due to CSS being inconsistent, a containing block can be a relatively
  // positioned inline, thus we can't return a LayoutBlock from this function.
  //
  // This method is extremely similar to containingBlock(), but with a few
  // notable exceptions.
  // (1) For normal flow elements, it just returns the parent.
  // (2) For absolute positioned elements, it will return a relative
  //     positioned inline. containingBlock() simply skips relpositioned inlines
  //     and lets an enclosing block handle the layout of the positioned object.
  //     This does mean that computePositionedLogicalWidth and
  //     computePositionedLogicalHeight have to use container().
  //
  // Note that floating objects don't belong to either of the above exceptions.
  //
  // This function should be used for any invalidation as it would correctly
  // walk the containing block chain. See e.g. markContainerChainForLayout.
  // It is also used for correctly sizing absolutely positioned elements
  // (point 3 above).
  LayoutObject* Container(AncestorSkipInfo* = nullptr) const;
  // Finds the container as if this object is fixed-position.
  LayoutObject* ContainerForAbsolutePosition(AncestorSkipInfo* = nullptr) const;
  // Finds the container as if this object is absolute-position.
  LayoutObject* ContainerForFixedPosition(AncestorSkipInfo* = nullptr) const;

  // Returns ContainerForAbsolutePosition() if it's a LayoutBlock, or the
  // containing LayoutBlock of it.
  LayoutBlock* ContainingBlockForAbsolutePosition(
      AncestorSkipInfo* = nullptr) const;
  // Returns ContainerForFixedPosition() if it's a LayoutBlock, or the
  // containing LayoutBlock of it.
  LayoutBlock* ContainingBlockForFixedPosition(
      AncestorSkipInfo* = nullptr) const;

  bool CanContainOutOfFlowPositionedElement(EPosition position) const {
    DCHECK(position == EPosition::kAbsolute || position == EPosition::kFixed);
    return (position == EPosition::kAbsolute &&
            CanContainAbsolutePositionObjects()) ||
           (position == EPosition::kFixed && CanContainFixedPositionObjects());
  }

  virtual LayoutObject* HoverAncestor() const { return Parent(); }

  Element* OffsetParent(const Element* = nullptr) const;

  void MarkContainerNeedsCollectInlines();
  void ClearNeedsCollectInlines() { SetNeedsCollectInlines(false); }

  void MarkContainerChainForLayout(bool schedule_relayout = true,
                                   SubtreeLayoutScope* = nullptr);
  void SetNeedsLayout(LayoutInvalidationReasonForTracing,
                      MarkingBehavior = kMarkContainerChain,
                      SubtreeLayoutScope* = nullptr);
  void SetNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing,
      MarkingBehavior = kMarkContainerChain,
      SubtreeLayoutScope* = nullptr);
  void ClearNeedsLayout();
  void SetChildNeedsLayout(MarkingBehavior = kMarkContainerChain,
                           SubtreeLayoutScope* = nullptr);
  void SetNeedsPositionedMovementLayout();
  void SetPreferredLogicalWidthsDirty(MarkingBehavior = kMarkContainerChain);
  void ClearPreferredLogicalWidthsDirty();

  void SetNeedsLayoutAndPrefWidthsRecalc(
      LayoutInvalidationReasonForTracing reason) {
    SetNeedsLayout(reason);
    SetPreferredLogicalWidthsDirty();
  }
  void SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReasonForTracing reason) {
    SetNeedsLayoutAndFullPaintInvalidation(reason);
    SetPreferredLogicalWidthsDirty();
  }

  void SetPositionState(EPosition position) {
    DCHECK(
        (position != EPosition::kAbsolute && position != EPosition::kFixed) ||
        IsBox());
    bitfields_.SetPositionedState(position);
  }
  void ClearPositionedState() { bitfields_.ClearPositionedState(); }

  void SetFloating(bool is_floating) { bitfields_.SetFloating(is_floating); }
  void SetInline(bool is_inline) { bitfields_.SetIsInline(is_inline); }

  void SetIsInLayoutNGInlineFormattingContext(bool);
  virtual NGPaintFragment* FirstInlineFragment() const { return nullptr; }
  virtual void SetFirstInlineFragment(NGPaintFragment*) {}

  void SetHasBoxDecorationBackground(bool);

  enum BackgroundObscurationState {
    kBackgroundObscurationStatusInvalid,
    kBackgroundKnownToBeObscured,
    kBackgroundMayBeVisible,
  };
  void InvalidateBackgroundObscurationStatus();
  virtual bool ComputeBackgroundIsKnownToBeObscured() const { return false; }

  void SetIsText() { bitfields_.SetIsText(true); }
  void SetIsBox() { bitfields_.SetIsBox(true); }
  void SetIsAtomicInlineLevel(bool is_atomic_inline_level) {
    bitfields_.SetIsAtomicInlineLevel(is_atomic_inline_level);
  }
  void SetHorizontalWritingMode(bool has_horizontal_writing_mode) {
    bitfields_.SetHorizontalWritingMode(has_horizontal_writing_mode);
  }
  void SetHasOverflowClip(bool has_overflow_clip) {
    bitfields_.SetHasOverflowClip(has_overflow_clip);
  }
  void SetShouldClipOverflow(bool should_clip_overflow) {
    bitfields_.SetShouldClipOverflow(should_clip_overflow);
  }
  void SetHasLayer(bool has_layer) { bitfields_.SetHasLayer(has_layer); }
  void SetHasTransformRelatedProperty(bool has_transform) {
    bitfields_.SetHasTransformRelatedProperty(has_transform);
  }
  void SetHasReflection(bool has_reflection) {
    bitfields_.SetHasReflection(has_reflection);
  }
  void SetCanContainFixedPositionObjects(bool can_contain_fixed_position) {
    bitfields_.SetCanContainFixedPositionObjects(can_contain_fixed_position);
  }
  void SetIsEffectiveRootScroller(bool is_effective_root_scroller) {
    bitfields_.SetIsEffectiveRootScroller(is_effective_root_scroller);
  }

  virtual void Paint(const PaintInfo&) const;

  virtual bool RecalcOverflow();

  // Subclasses must reimplement this method to compute the size and position
  // of this object and all its descendants.
  //
  // By default, layout only lays out the children that are marked for layout.
  // In some cases, layout has to force laying out more children. An example is
  // when the width of the LayoutObject changes as this impacts children with
  // 'width' set to auto.
  virtual void UpdateLayout() = 0;
  virtual bool UpdateImageLoadingPriorities() { return false; }

  void HandleSubtreeModifications();
  virtual void SubtreeDidChange() {}

  // Flags used to mark if an object consumes subtree change notifications.
  bool ConsumesSubtreeChangeNotification() const {
    return bitfields_.ConsumesSubtreeChangeNotification();
  }
  void SetConsumesSubtreeChangeNotification() {
    bitfields_.SetConsumesSubtreeChangeNotification(true);
  }

  // Flags used to mark if a descendant subtree of this object has changed.
  void NotifyOfSubtreeChange();
  void NotifyAncestorsOfSubtreeChange();
  bool WasNotifiedOfSubtreeChange() const {
    return bitfields_.NotifiedOfSubtreeChange();
  }

  // Flags used to signify that a layoutObject needs to be notified by its
  // descendants that they have had their child subtree changed.
  void RegisterSubtreeChangeListenerOnDescendants(bool);
  bool HasSubtreeChangeListenerRegistered() const {
    return bitfields_.SubtreeChangeListenerRegistered();
  }

  /* This function performs a layout only if one is needed. */
  DISABLE_CFI_PERF void LayoutIfNeeded() {
    if (NeedsLayout())
      UpdateLayout();
  }

  void ForceLayout();
  void ForceChildLayout();

  // Used for element state updates that cannot be fixed with a paint
  // invalidation and do not need a relayout.
  virtual void UpdateFromElement() {}

  virtual void AddAnnotatedRegions(Vector<AnnotatedRegionValue>&);

  CompositingState GetCompositingState() const;
  virtual CompositingReasons AdditionalCompositingReasons() const;

  virtual bool HitTestAllPhases(HitTestResult&,
                                const HitTestLocation& location_in_container,
                                const LayoutPoint& accumulated_offset,
                                HitTestFilter = kHitTestAll);
  // Returns the node that is ultimately added to the hit test result. Some
  // objects report a hit testing node that is not their own (such as
  // continuations and some psuedo elements) and it is important that the
  // node be consistent between point- and list-based hit test results.
  virtual Node* NodeForHitTest() const;
  virtual void UpdateHitTestResult(HitTestResult&, const LayoutPoint&) const;
  virtual bool NodeAtPoint(HitTestResult&,
                           const HitTestLocation& location_in_container,
                           const LayoutPoint& accumulated_offset,
                           HitTestAction);

  virtual PositionWithAffinity PositionForPoint(const LayoutPoint&) const;
  PositionWithAffinity CreatePositionWithAffinity(int offset,
                                                  TextAffinity) const;
  PositionWithAffinity CreatePositionWithAffinity(int offset) const;
  PositionWithAffinity CreatePositionWithAffinity(const Position&) const;

  virtual void DirtyLinesFromChangedChild(
      LayoutObject*,
      MarkingBehavior marking_behaviour = kMarkContainerChain);

  // Set the style of the object and update the state of the object accordingly.
  void SetStyle(scoped_refptr<ComputedStyle>);

  // Set the style of the object if it's generated content.
  void SetPseudoStyle(scoped_refptr<ComputedStyle>);

  // Updates only the local style ptr of the object.  Does not update the state
  // of the object, and so only should be called when the style is known not to
  // have changed (or from setStyle).
  void SetStyleInternal(scoped_refptr<ComputedStyle> style) {
    style_ = std::move(style);
  }

  void SetStyleWithWritingModeOf(scoped_refptr<ComputedStyle>,
                                 LayoutObject* parent);
  void SetStyleWithWritingModeOfParent(scoped_refptr<ComputedStyle>);

  void FirstLineStyleDidChange(const ComputedStyle& old_style,
                               const ComputedStyle& new_style);

  void ClearBaseComputedStyle();

  // This function returns an enclosing non-anonymous LayoutBlock for this
  // element. This function is not always returning the containing block as
  // defined by CSS. In particular:
  // - if the CSS containing block is a relatively positioned inline,
  //   the function returns the inline's enclosing non-anonymous LayoutBlock.
  //   This means that a LayoutInline would be skipped (expected as it's not a
  //   LayoutBlock) but so would be an inline LayoutTable or LayoutBlockFlow.
  //   TODO(jchaffraix): Is that REALLY what we want here?
  // - if the CSS containing block is anonymous, we find its enclosing
  //   non-anonymous LayoutBlock.
  //   Note that in the previous examples, the returned LayoutBlock has no
  //   logical relationship to the original element.
  //
  // LayoutBlocks are the one that handle laying out positioned elements,
  // thus this function is important during layout, to insert the positioned
  // elements into the correct LayoutBlock.
  //
  // See container() for the function that returns the containing block.
  // See LayoutBlock.h for some extra explanations on containing blocks.
  LayoutBlock* ContainingBlock(AncestorSkipInfo* = nullptr) const;

  const LayoutBlock* InclusiveContainingBlock() const;

  bool CanContainAbsolutePositionObjects() const {
    return style_->CanContainAbsolutePositionObjects() ||
           CanContainFixedPositionObjects();
  }
  bool CanContainFixedPositionObjects() const {
    return bitfields_.CanContainFixedPositionObjects();
  }

  // Convert the given local point to absolute coordinates
  // FIXME: Temporary. If UseTransforms is true, take transforms into account.
  // Eventually localToAbsolute() will always be transform-aware.
  FloatPoint LocalToAbsolute(const FloatPoint& local_point = FloatPoint(),
                             MapCoordinatesFlags = 0) const;

  // If the LayoutBoxModelObject ancestor is non-null, the input point is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input point is in the
  //   space of the local root frame.
  //   Otherwise, the input point is in the space of the containing frame.
  FloatPoint AncestorToLocal(LayoutBoxModelObject*,
                             const FloatPoint&,
                             MapCoordinatesFlags = 0) const;
  FloatPoint AbsoluteToLocal(const FloatPoint& point,
                             MapCoordinatesFlags mode = 0) const {
    return AncestorToLocal(nullptr, point, mode);
  }

  // Convert a local quad to absolute coordinates, taking transforms into
  // account.
  FloatQuad LocalToAbsoluteQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return LocalToAncestorQuad(quad, nullptr, mode);
  }

  // Convert a quad in ancestor coordinates to local coordinates.
  // If the LayoutBoxModelObject ancestor is non-null, the input quad is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input quad is in the
  //   space of the local root frame.
  //   Otherwise, the input quad is in the space of the containing frame.
  FloatQuad AncestorToLocalQuad(LayoutBoxModelObject*,
                                const FloatQuad&,
                                MapCoordinatesFlags mode = 0) const;
  FloatQuad AbsoluteToLocalQuad(const FloatQuad& quad,
                                MapCoordinatesFlags mode = 0) const {
    return AncestorToLocalQuad(nullptr, quad, mode);
  }

  // Convert a local quad into the coordinate system of container, taking
  // transforms into account.
  // If the LayoutBoxModelObject ancestor is non-null, the result will be in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the result will be in the
  //   space of the local root frame.
  //   Otherwise, the result will be in the space of the containing frame.
  FloatQuad LocalToAncestorQuad(const FloatQuad&,
                                const LayoutBoxModelObject* ancestor,
                                MapCoordinatesFlags = 0) const;
  FloatPoint LocalToAncestorPoint(const FloatPoint&,
                                  const LayoutBoxModelObject* ancestor,
                                  MapCoordinatesFlags = 0) const;
  void LocalToAncestorRects(Vector<LayoutRect>&,
                            const LayoutBoxModelObject* ancestor,
                            const LayoutPoint& pre_offset,
                            const LayoutPoint& post_offset) const;

  // Convert a local quad into the coordinate system of container, not
  // include transforms. See localToAncestorQuad for details.
  FloatQuad LocalToAncestorQuadWithoutTransforms(
      const FloatQuad&,
      const LayoutBoxModelObject* ancestor,
      MapCoordinatesFlags = 0) const;

  // Return the transformation matrix to map points from local to the coordinate
  // system of a container, taking transforms into account.
  // Passing null for |ancestor| behaves the same as localToAncestorQuad.
  TransformationMatrix LocalToAncestorTransform(
      const LayoutBoxModelObject* ancestor,
      MapCoordinatesFlags = 0) const;
  TransformationMatrix LocalToAbsoluteTransform(
      MapCoordinatesFlags mode = 0) const {
    return LocalToAncestorTransform(nullptr, mode);
  }

  // Return the offset from the container() layoutObject (excluding transforms
  // and multicol).
  LayoutSize OffsetFromContainer(const LayoutObject*,
                                 bool ignore_scroll_offset = false) const;
  // Return the offset from an object from the ancestor. The ancestor need
  // not be on the containing block chain of |this|.
  LayoutSize OffsetFromAncestor(const LayoutObject*) const;

  virtual void AbsoluteRects(Vector<IntRect>&, const LayoutPoint&) const {}

  FloatRect AbsoluteBoundingBoxFloatRect(MapCoordinatesFlags = 0) const;
  // This returns an IntRect enclosing this object. If this object has an
  // integral size and the position has fractional values, the resultant
  // IntRect can be larger than the integral size.
  IntRect AbsoluteBoundingBoxRect(MapCoordinatesFlags = 0) const;
  // FIXME: This function should go away eventually
  IntRect AbsoluteBoundingBoxRectIgnoringTransforms() const;
  // These two handles inline anchors without content as well.
  LayoutRect AbsoluteBoundingBoxRectHandlingEmptyAnchor() const;
  // This returns an IntRect expanded from
  // AbsoluteBoundingBoxRectHandlingEmptyAnchor by ScrollMargin.
  LayoutRect AbsoluteBoundingBoxRectForScrollIntoView() const;

  // Build an array of quads in absolute coords for line boxes
  virtual void AbsoluteQuads(Vector<FloatQuad>&,
                             MapCoordinatesFlags mode = 0) const {}

  static FloatRect AbsoluteBoundingBoxRectForRange(const EphemeralRange&);

  // The bounding box (see: absoluteBoundingBoxRect) including all descendant
  // bounding boxes.
  IntRect AbsoluteBoundingBoxRectIncludingDescendants() const;

  // For accessibility, we want the bounding box rect of this element
  // in local coordinates, which can then be converted to coordinates relative
  // to any ancestor using, e.g., localToAncestorTransform.
  virtual FloatRect LocalBoundingBoxRectForAccessibility() const = 0;

  // This function returns the minimal logical width this object can have
  // without overflowing. This means that all the opportunities for wrapping
  // have been taken.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
  //
  // CSS 2.1 calls this width the "preferred minimum width" (thus this name)
  // and "minimum content width" (for table).
  // However CSS 3 calls it the "min-content inline size".
  // https://drafts.csswg.org/css-sizing-3/#min-content-inline-size
  // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
  virtual LayoutUnit MinPreferredLogicalWidth() const { return LayoutUnit(); }

  // This function returns the maximum logical width this object can have.
  //
  // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above.
  //
  // CSS 2.1 calls this width the "preferred width". However CSS 3 calls it
  // the "max-content inline size".
  // https://drafts.csswg.org/css-sizing-3/#max-content-inline-size
  // TODO(jchaffraix): We will probably want to rename it to match CSS 3.
  virtual LayoutUnit MaxPreferredLogicalWidth() const { return LayoutUnit(); }

  const ComputedStyle* Style() const { return style_.get(); }
  ComputedStyle* MutableStyle() const { return style_.get(); }

  // style_ can only be nullptr before the first style is set, thus most
  // callers will never see a nullptr style and should use StyleRef().
  const ComputedStyle& StyleRef() const { return MutableStyleRef(); }
  ComputedStyle& MutableStyleRef() const {
    DCHECK(style_);
    return *style_;
  }

  /* The following methods are inlined in LayoutObjectInlines.h */
  inline const ComputedStyle* FirstLineStyle() const;
  inline const ComputedStyle& FirstLineStyleRef() const;
  inline const ComputedStyle* Style(bool first_line) const;
  inline const ComputedStyle& StyleRef(bool first_line) const;

  static inline Color ResolveColor(const ComputedStyle& style_to_use,
                                   const CSSProperty& color_property) {
    return style_to_use.VisitedDependentColor(color_property);
  }

  inline Color ResolveColor(const CSSProperty& color_property) const {
    return StyleRef().VisitedDependentColor(color_property);
  }

  virtual CursorDirective GetCursor(const LayoutPoint&, Cursor&) const;

  // Return the LayoutBoxModelObject in the container chain which is responsible
  // for painting this object. The function crosses frames boundaries so the
  // returned value can be in a different document.
  //
  // This is the container that should be passed to the '*forPaintInvalidation'
  // methods.
  const LayoutBoxModelObject& ContainerForPaintInvalidation() const;

  bool IsPaintInvalidationContainer() const;

  // Invalidate the raster of a specific sub-rectangle within the object. The
  // rect is in the object's local coordinate space. This is useful e.g. when
  // a small region of a canvas changes.
  void InvalidatePaintRectangle(const LayoutRect&);

  // Returns the rect that should have paint invalidated whenever this object
  // changes. The rect is in the coordinate space of the document's scrolling
  // contents. This method deals with outlines and overflow.
  virtual LayoutRect VisualRectInDocument() const;

  // Returns the rect that should have raster invalidated whenever this object
  // changes. The rect is in the object's local coordinate space. This is for
  // non-SVG objects and LayoutSVGRoot only. SVG objects (except LayoutSVGRoot)
  // should use VisualRectInLocalSVGCoordinates() and map with SVG transforms
  // instead.
  LayoutRect LocalVisualRect() const {
    if (StyleRef().Visibility() != EVisibility::kVisible &&
        VisualRectRespectsVisibility())
      return LayoutRect();
    return LocalVisualRectIgnoringVisibility();
  }

  // Given a rect in the object's coordinate space, mutates the rect into one
  // representing the size of its visual painted output as if |ancestor| was the
  // root of the page: the rect is modified by any intervening clips, transforms
  // and scrolls between |this| and |ancestor| (not inclusive of |ancestor|),
  // but not any above |ancestor|.
  // The output is in the physical, painted coordinate pixel space of
  // |ancestor|.
  // Overflow clipping, CSS clipping and scrolling is *not* applied for
  // |ancestor| itself if |ancestor| scrolls overflow.
  // The output rect is suitable for purposes such as paint invalidation.
  //
  // The ancestor can be nullptr which, if |this| is not the root view, will map
  // the rect to the main frame's space which includes the root view's scroll
  // and clip. This is even true if the main frame is remote.
  //
  // If visualRectFlags has the EdgeInclusive bit set, clipping operations will
  // use LayoutRect::InclusiveIntersect, and the return value of
  // InclusiveIntersect will be propagated to the return value of this method.
  // Otherwise, clipping operations will use LayoutRect::intersect, and the
  // return value will be true only if the clipped rect has non-zero area.
  // See the documentation for LayoutRect::InclusiveIntersect for more
  // information.
  bool MapToVisualRectInAncestorSpace(
      const LayoutBoxModelObject* ancestor,
      LayoutRect&,
      VisualRectFlags = kDefaultVisualRectFlags) const;

  // Do not call this method directly. Call mapToVisualRectInAncestorSpace
  // instead.
  virtual bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const;

  // Do a rect-based hit test with this object as the stop node.
  HitTestResult HitTestForOcclusion(const LayoutRect&) const;
  HitTestResult HitTestForOcclusion() const {
    return HitTestForOcclusion(VisualRectInDocument());
  }

  // Return the offset to the column in which the specified point (in
  // flow-thread coordinates) lives. This is used to convert a flow-thread point
  // to a point in the containing coordinate space.
  virtual LayoutSize ColumnOffset(const LayoutPoint&) const {
    return LayoutSize();
  }

  virtual unsigned length() const { return 1; }

  bool IsFloatingOrOutOfFlowPositioned() const {
    return (IsFloating() || IsOutOfFlowPositioned());
  }

  bool HasReflection() const { return bitfields_.HasReflection(); }

  // The current selection state for an object.  For blocks, the state refers to
  // the state of the leaf descendants (as described above in the SelectionState
  // enum declaration).
  SelectionState GetSelectionState() const {
    return bitfields_.GetSelectionState();
  }
  void SetSelectionState(SelectionState state) {
    bitfields_.SetSelectionState(state);
  }
  bool CanUpdateSelectionOnRootLineBoxes() const;

  // A single rectangle that encompasses all of the selected objects within this
  // object. Used to determine the tightest possible bounding box for the
  // selection. The rect returned is in the object's local coordinate space.
  virtual LayoutRect LocalSelectionRect() const { return LayoutRect(); }

  LayoutRect AbsoluteSelectionRect() const;

  bool CanBeSelectionLeaf() const;
  bool IsSelected() const;
  bool IsSelectable() const;

  /**
     * Returns the local coordinates of the caret within this layout object.
     * @param caretOffset zero-based offset determining position within the
   * layout object.
     * @param extraWidthToEndOfLine optional out arg to give extra width to end
   * of line -
     * useful for character range rect computations
     */
  virtual LayoutRect LocalCaretRect(
      const InlineBox*,
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const;

  // When performing a global document tear-down, the layoutObject of the
  // document is cleared. We use this as a hook to detect the case of document
  // destruction and don't waste time doing unnecessary work.
  bool DocumentBeingDestroyed() const;

  void DestroyAndCleanupAnonymousWrappers();

  // While the destroy() method is virtual, this should only be overriden in
  // very rare circumstances.
  // You want to override willBeDestroyed() instead unless you explicitly need
  // to stop this object from being destroyed (for example,
  // LayoutEmbeddedContent overrides destroy() for this purpose).
  virtual void Destroy();

  // Virtual function helpers for the deprecated Flexible Box Layout (display:
  // -webkit-box).
  virtual bool IsDeprecatedFlexibleBox() const { return false; }

  // Virtual function helper for the new FlexibleBox Layout (display:
  // -webkit-flex).
  virtual bool IsFlexibleBox() const { return false; }

  bool IsFlexibleBoxIncludingDeprecated() const {
    return IsFlexibleBox() || IsDeprecatedFlexibleBox();
  }

  bool IsListItemIncludingNG() const {
    return IsListItem() || IsLayoutNGListItem();
  }

  bool IsListMarkerIncludingNG() const {
    return IsListMarker() || IsLayoutNGListMarker();
  }

  virtual bool IsCombineText() const { return false; }

  virtual int CaretMinOffset() const;
  virtual int CaretMaxOffset() const;

  // ImageResourceObserver override.
  void ImageChanged(ImageResourceContent*, CanDeferInvalidation) final;
  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override {}
  bool WillRenderImage() final;
  bool GetImageAnimationPolicy(ImageAnimationPolicy&) final;

  void Remove() {
    if (Parent())
      Parent()->RemoveChild(this);
  }

  bool VisibleToHitTestRequest(const HitTestRequest& request) const {
    return StyleRef().Visibility() == EVisibility::kVisible &&
           (request.IgnorePointerEventsNone() ||
            StyleRef().PointerEvents() != EPointerEvents::kNone) &&
           !IsInert();
  }

  // Warning: inertness can change without causing relayout.
  bool VisibleToHitTesting() const {
    return StyleRef().VisibleToHitTesting() && !IsInert();
  }

  // Map points and quads through elements, potentially via 3d transforms. You
  // should never need to call these directly; use localToAbsolute/
  // absoluteToLocal methods instead.
  virtual void MapLocalToAncestor(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const;
  // If the LayoutBoxModelObject ancestor is non-null, the input quad is in the
  // space of the ancestor.
  // Otherwise:
  //   If TraverseDocumentBoundaries is specified, the input quad is in the
  //   space of the local root frame.
  //   Otherwise, the input quad is in the space of the containing frame.
  virtual void MapAncestorToLocal(
      const LayoutBoxModelObject*,
      TransformState&,
      MapCoordinatesFlags = kApplyContainerFlip) const;

  // Pushes state onto LayoutGeometryMap about how to map coordinates from this
  // layoutObject to its container, or ancestorToStopAt (whichever is
  // encountered first). Returns the layoutObject which was mapped to (container
  // or ancestorToStopAt).
  virtual const LayoutObject* PushMappingToContainer(
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&) const;

  bool ShouldUseTransformFromContainer(const LayoutObject* container) const;
  void GetTransformFromContainer(const LayoutObject* container,
                                 const LayoutSize& offset_in_container,
                                 TransformationMatrix&) const;

  bool CreatesGroup() const {
    return StyleRef().HasOpacity() || HasMask() || HasClipPath() ||
           HasFilterInducingProperty() || StyleRef().HasBlendMode();
  }

  // Collects rectangles that the outline of this object would be drawing along
  // the outside of, even if the object isn't styled with a outline for now. The
  // rects also cover continuations.
  virtual void AddOutlineRects(Vector<LayoutRect>&,
                               const LayoutPoint& additional_offset,
                               NGOutlineType) const {}

  // For history and compatibility reasons, we draw outline:auto (for focus
  // rings) and normal style outline differently.
  // Focus rings enclose block visual overflows (of line boxes and descendants),
  // while normal outlines don't.
  NGOutlineType OutlineRectsShouldIncludeBlockVisualOverflow() const {
    return StyleRef().OutlineStyleIsAuto()
               ? NGOutlineType::kIncludeBlockVisualOverflow
               : NGOutlineType::kDontIncludeBlockVisualOverflow;
  }

  // Collects rectangles enclosing visual overflows of the DOM subtree under
  // this object.
  // The rects also cover continuations which may be not in the layout subtree
  // of this object.
  // TODO(crbug.com/614781): Currently the result rects don't cover list markers
  // and outlines.
  void AddElementVisualOverflowRects(
      Vector<LayoutRect>& rects,
      const LayoutPoint& additional_offset) const {
    AddOutlineRects(rects, additional_offset,
                    NGOutlineType::kIncludeBlockVisualOverflow);
  }

  // Compute a list of hit-test rectangles per layer rooted at this
  // layoutObject with at most the given touch action.
  virtual void ComputeLayerHitTestRects(LayerHitTestRects&, TouchAction) const;

  static RespectImageOrientationEnum ShouldRespectImageOrientation(
      const LayoutObject*);

  bool IsRelayoutBoundaryForInspector() const;

  // The visual rect, in the the space of the paint invalidation container
  // (*not* the graphics layer that paints this object).
  LayoutRect VisualRectIncludingCompositedScrolling(
      const LayoutBoxModelObject& paint_invalidation_container) const;

  // Called when the previous visual rect(s) is no longer valid.
  virtual void ClearPreviousVisualRects();

  PaintInvalidationReason FullPaintInvalidationReason() const {
    return full_paint_invalidation_reason_;
  }
  bool ShouldDoFullPaintInvalidation() const {
    if (!ShouldDelayFullPaintInvalidation() &&
        full_paint_invalidation_reason_ != PaintInvalidationReason::kNone) {
      DCHECK(IsFullPaintInvalidationReason(full_paint_invalidation_reason_));
      DCHECK(ShouldCheckForPaintInvalidation());
      return true;
    }
    return false;
  }
  // Indicates that the paint of the object should be fully invalidated.
  // We will repaint the object, and reraster the area on the composited layer
  // where the object shows. Note that this function doesn't automatically
  // cause invalidation of background painted on the scrolling contents layer
  // because we don't want to invalidate the whole scrolling contents layer on
  // non-background changes. It's also not safe to specially handle
  // PaintInvalidationReason::kBackground in paint invalidator because we don't
  // track paint invalidation reasons separately. To indicate that the
  // background needs full invalidation, use
  // SetBackgroundNeedsFullPaintInvalidation().
  void SetShouldDoFullPaintInvalidation(
      PaintInvalidationReason = PaintInvalidationReason::kFull);
  void SetShouldDoFullPaintInvalidationWithoutGeometryChange(
      PaintInvalidationReason = PaintInvalidationReason::kFull);

  void ClearPaintInvalidationFlags();

  bool ShouldCheckForPaintInvalidation() const {
    return bitfields_.ShouldCheckForPaintInvalidation();
  }
  void SetShouldCheckForPaintInvalidation();
  void SetShouldCheckForPaintInvalidationWithoutGeometryChange();

  bool SubtreeShouldCheckForPaintInvalidation() const {
    return bitfields_.SubtreeShouldCheckForPaintInvalidation();
  }
  void SetSubtreeShouldCheckForPaintInvalidation();

  bool NeedsPaintOffsetAndVisualRectUpdate() const {
    return bitfields_.NeedsPaintOffsetAndVisualRectUpdate();
  }
  bool DescendantNeedsPaintOffsetAndVisualRectUpdate() const {
    return bitfields_.DescendantNeedsPaintOffsetAndVisualRectUpdate();
  }

  bool MayNeedPaintInvalidationAnimatedBackgroundImage() const {
    return bitfields_.MayNeedPaintInvalidationAnimatedBackgroundImage();
  }
  void SetMayNeedPaintInvalidationAnimatedBackgroundImage();

  void SetSubtreeShouldDoFullPaintInvalidation(
      PaintInvalidationReason reason = PaintInvalidationReason::kSubtree);
  bool SubtreeShouldDoFullPaintInvalidation() const {
    DCHECK(!bitfields_.SubtreeShouldDoFullPaintInvalidation() ||
           ShouldDoFullPaintInvalidation());
    return bitfields_.SubtreeShouldDoFullPaintInvalidation();
  }

  // If true, it means that invalidation and repainting of the object can be
  // delayed until a future frame. This can be the case for an object whose
  // content is not visible to the user.
  bool ShouldDelayFullPaintInvalidation() const {
    return bitfields_.ShouldDelayFullPaintInvalidation();
  }
  void SetShouldDelayFullPaintInvalidation();

  bool ShouldInvalidateSelection() const {
    return bitfields_.ShouldInvalidateSelection();
  }
  void SetShouldInvalidateSelection();

  virtual LayoutRect ViewRect() const;

  // Called by PaintInvalidator during PrePaint. Checks paint invalidation flags
  // and other changes that will cause different painting, and invalidate
  // display item clients for painting if needed.
  virtual void InvalidatePaint(const PaintInvalidatorContext&) const;

  // When this object is invalidated for paint, this method is called to
  // invalidate any DisplayItemClients owned by this object, including the
  // object itself, LayoutText/LayoutInline line boxes, etc.,
  // not including children which will be invalidated normally during
  // invalidateTreeIfNeeded() and parts which are invalidated separately (e.g.
  // scrollbars). The caller should ensure the painting layer has been
  // setNeedsRepaint before calling this function.
  virtual void InvalidateDisplayItemClients(PaintInvalidationReason) const;

  virtual bool HasNonCompositedScrollbars() const { return false; }

  // Called before setting style for existing/new anonymous child. Override to
  // set custom styles for the child. For new anonymous child, |child| is null.
  virtual void UpdateAnonymousChildStyle(const LayoutObject* child,
                                         ComputedStyle& style) const {}

  // Returns a rect corresponding to this LayoutObject's bounds for use in
  // debugging output
  virtual LayoutRect DebugRect() const;

  // Each LayoutObject has one or more painting fragments (exactly one
  // in the absence of multicol/pagination).
  // See ../paint/README.md for more on fragments.
  const FragmentData& FirstFragment() const { return fragment_; }

  // Returns the bounding box of the visual rects of all fragments.
  LayoutRect FragmentsVisualRectBoundingBox() const;

  void SetNeedsOverflowRecalc();

  void InvalidateClipPathCache();

  // Call |SetShouldDoFullPaintInvalidation| for LayoutNG or
  // |SetShouldInvalidateSelection| on all selected children.
  void InvalidateSelectedChildrenOnStyleChange();

  // The whitelisted touch action is the union of the effective touch action
  // (from style) and blocking touch event handlers.
  TouchAction EffectiveWhitelistedTouchAction() const {
    if (InsideBlockingTouchEventHandler())
      return TouchAction::kTouchActionNone;
    return StyleRef().GetEffectiveTouchAction();
  }

  // Whether this object's Node has a blocking touch event handler on itself
  // or an ancestor.
  bool InsideBlockingTouchEventHandler() const {
    return bitfields_.InsideBlockingTouchEventHandler();
  }
  // Mark this object as having a |EffectiveWhitelistedTouchAction| changed, and
  // mark all ancestors as having a descendant that changed. This will cause a
  // PrePaint tree walk to update effective whitelisted touch action.
  void MarkEffectiveWhitelistedTouchActionChanged();
  bool EffectiveWhitelistedTouchActionChanged() const {
    return bitfields_.EffectiveWhitelistedTouchActionChanged();
  }
  bool DescendantEffectiveWhitelistedTouchActionChanged() const {
    return bitfields_.DescendantEffectiveWhitelistedTouchActionChanged();
  }
  void UpdateInsideBlockingTouchEventHandler(bool inside) {
    bitfields_.SetInsideBlockingTouchEventHandler(inside);
  }

  // Painters can use const methods only, except for these explicitly declared
  // methods.
  class CORE_EXPORT MutableForPainting {
   public:
    // Convenience mutator that clears paint invalidation flags and this object
    // and its descendants' needs-paint-property-update flags.
    void ClearPaintFlags() {
      DCHECK_EQ(layout_object_.GetDocument().Lifecycle().GetState(),
                DocumentLifecycle::kInPrePaint);
      layout_object_.ClearPaintInvalidationFlags();
      layout_object_.bitfields_.SetNeedsPaintPropertyUpdate(false);
      layout_object_.bitfields_.ResetSubtreePaintPropertyUpdateReasons();
      layout_object_.bitfields_.SetDescendantNeedsPaintPropertyUpdate(false);
      layout_object_.bitfields_.SetEffectiveWhitelistedTouchActionChanged(
          false);
      layout_object_.bitfields_
          .SetDescendantEffectiveWhitelistedTouchActionChanged(false);
    }
    void SetShouldCheckForPaintInvalidation() {
      layout_object_.SetShouldCheckForPaintInvalidation();
    }
    void SetShouldDoFullPaintInvalidation(PaintInvalidationReason reason) {
      layout_object_.SetShouldDoFullPaintInvalidation(reason);
    }
    void SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason reason) {
      layout_object_.SetShouldDoFullPaintInvalidationWithoutGeometryChange(
          reason);
    }
    void SetBackgroundNeedsFullPaintInvalidation() {
      layout_object_.SetBackgroundNeedsFullPaintInvalidation();
    }
    void SetShouldDelayFullPaintInvalidation() {
      layout_object_.SetShouldDelayFullPaintInvalidation();
    }
    void EnsureIsReadyForPaintInvalidation() {
      layout_object_.EnsureIsReadyForPaintInvalidation();
    }

    // The following setters store the current values as calculated during the
    // pre-paint tree walk. TODO(wangxianzhu): Add check of lifecycle states.
    void SetVisualRect(const LayoutRect& r) {
      layout_object_.fragment_.SetVisualRect(r);
    }

    void SetSelectionVisualRect(const LayoutRect& r) {
      layout_object_.fragment_.SetSelectionVisualRect(r);
    }

    void SetPreviousBackgroundObscured(bool b) {
      layout_object_.SetPreviousBackgroundObscured(b);
    }
    void UpdatePreviousOutlineMayBeAffectedByDescendants() {
      layout_object_.SetPreviousOutlineMayBeAffectedByDescendants(
          layout_object_.OutlineMayBeAffectedByDescendants());
    }

    void ClearPreviousVisualRects() {
      layout_object_.ClearPreviousVisualRects();
    }
    void SetNeedsPaintPropertyUpdate() {
      layout_object_.SetNeedsPaintPropertyUpdate();
    }
    void AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason reason) {
      layout_object_.AddSubtreePaintPropertyUpdateReason(reason);
    }

    void SetPartialInvalidationVisualRect(const LayoutRect& r) {
      DCHECK_EQ(layout_object_.GetDocument().Lifecycle().GetState(),
                DocumentLifecycle::kInPrePaint);
      FirstFragment().SetPartialInvalidationVisualRect(r);
    }

    void InvalidateClipPathCache() { layout_object_.InvalidateClipPathCache(); }

    void UpdateInsideBlockingTouchEventHandler(bool inside) {
      layout_object_.UpdateInsideBlockingTouchEventHandler(inside);
    }

#if DCHECK_IS_ON()
    // Same as setNeedsPaintPropertyUpdate() but does not mark ancestors as
    // having a descendant needing a paint property update.
    void SetOnlyThisNeedsPaintPropertyUpdateForTesting() {
      layout_object_.bitfields_.SetNeedsPaintPropertyUpdate(true);
    }
    void ClearNeedsPaintPropertyUpdateForTesting() {
      layout_object_.bitfields_.SetNeedsPaintPropertyUpdate(false);
    }
#endif

    FragmentData& FirstFragment() { return layout_object_.fragment_; }

   protected:
    friend class LayoutBoxModelObject;
    friend class LayoutScrollbar;
    friend class PaintInvalidator;
    friend class PaintPropertyTreeBuilder;
    friend class PrePaintTreeWalk;
    FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                             canStartElementOnCompositorTransformSPv2);
    FRIEND_TEST_ALL_PREFIXES(AnimationCompositorAnimationsTest,
                             canStartElementOnCompositorEffectSPv2);
    FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
    FRIEND_TEST_ALL_PREFIXES(LayoutObjectTest, VisualRect);
    FRIEND_TEST_ALL_PREFIXES(BoxPaintInvalidatorTest,
                             ComputePaintInvalidationReasonBasic);

    friend class LayoutObject;
    MutableForPainting(const LayoutObject& layout_object)
        : layout_object_(const_cast<LayoutObject&>(layout_object)) {}

    LayoutObject& layout_object_;
  };
  MutableForPainting GetMutableForPainting() const {
    return MutableForPainting(*this);
  }

  // Paint properties (see: |ObjectPaintProperties|) are built from an object's
  // state (location, transform, etc) as well as properties from ancestors.
  // When these inputs change, SetNeedsPaintPropertyUpdate will cause a property
  // tree update during the next document lifecycle update.
  //
  // In addition to tracking if an object needs its own paint properties
  // updated, SetNeedsPaintPropertyUpdate marks all ancestors as having a
  // descendant needing a paint property update too.
  void SetNeedsPaintPropertyUpdate();
  bool NeedsPaintPropertyUpdate() const {
    return bitfields_.NeedsPaintPropertyUpdate();
  }

  void AddSubtreePaintPropertyUpdateReason(
      SubtreePaintPropertyUpdateReason reason) {
    bitfields_.AddSubtreePaintPropertyUpdateReason(reason);
    SetNeedsPaintPropertyUpdate();
  }
  unsigned SubtreePaintPropertyUpdateReasons() const {
    return bitfields_.SubtreePaintPropertyUpdateReasons();
  }
  bool DescendantNeedsPaintPropertyUpdate() const {
    return bitfields_.DescendantNeedsPaintPropertyUpdate();
  }
  // Main thread scrolling reasons require fully updating paint propeties of all
  // ancestors (see: ScrollPaintPropertyNode.h).
  void SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();

  void SetIsScrollAnchorObject() { bitfields_.SetIsScrollAnchorObject(true); }
  // Clears the IsScrollAnchorObject bit if and only if no ScrollAnchors still
  // reference this LayoutObject.
  void MaybeClearIsScrollAnchorObject();

  bool ScrollAnchorDisablingStyleChanged() {
    return bitfields_.ScrollAnchorDisablingStyleChanged();
  }
  void SetScrollAnchorDisablingStyleChanged(bool changed) {
    bitfields_.SetScrollAnchorDisablingStyleChanged(changed);
  }


  bool CompositedScrollsWithRespectTo(
      const LayoutBoxModelObject& paint_invalidation_container) const;

  bool PreviousBackgroundObscured() const {
    return bitfields_.PreviousBackgroundObscured();
  }
  void SetPreviousBackgroundObscured(bool b) {
    bitfields_.SetPreviousBackgroundObscured(b);
  }

  bool IsBackgroundAttachmentFixedObject() const {
    return bitfields_.IsBackgroundAttachmentFixedObject();
  }

  bool BackgroundNeedsFullPaintInvalidation() const {
    return !ShouldDelayFullPaintInvalidation() &&
           bitfields_.BackgroundNeedsFullPaintInvalidation();
  }
  void SetBackgroundNeedsFullPaintInvalidation() {
    SetShouldDoFullPaintInvalidationWithoutGeometryChange(
        PaintInvalidationReason::kBackground);
    bitfields_.SetBackgroundNeedsFullPaintInvalidation(true);
  }

  bool OutlineMayBeAffectedByDescendants() const {
    return bitfields_.OutlineMayBeAffectedByDescendants();
  }
  bool PreviousOutlineMayBeAffectedByDescendants() const {
    return bitfields_.PreviousOutlineMayBeAffectedByDescendants();
  }

  LayoutRect SelectionVisualRect() const {
    return fragment_.SelectionVisualRect();
  }
  LayoutRect PartialInvalidationLocalRect() const {
    return fragment_.PartialInvalidationLocalRect();
  }

  void InvalidateIfControlStateChanged(ControlState);

  bool ContainsInlineWithOutlineAndContinuation() const {
    return bitfields_.ContainsInlineWithOutlineAndContinuation();
  }

  void SetOutlineMayBeAffectedByDescendants(bool b) {
    bitfields_.SetOutlineMayBeAffectedByDescendants(b);
  }

 protected:
  enum LayoutObjectType {
    kLayoutObjectBr,
    kLayoutObjectCanvas,
    kLayoutObjectFieldset,
    kLayoutObjectCounter,
    kLayoutObjectDetailsMarker,
    kLayoutObjectEmbeddedObject,
    kLayoutObjectFileUploadControl,
    kLayoutObjectFrame,
    kLayoutObjectFrameSet,
    kLayoutObjectLayoutTableCol,
    kLayoutObjectListBox,
    kLayoutObjectListItem,
    kLayoutObjectListMarker,
    kLayoutObjectMedia,
    kLayoutObjectMenuList,
    kLayoutObjectNGBlockFlow,
    kLayoutObjectNGFieldset,
    kLayoutObjectNGFlexibleBox,
    kLayoutObjectNGMixin,
    kLayoutObjectNGListItem,
    kLayoutObjectNGListMarker,
    kLayoutObjectNGListMarkerImage,
    kLayoutObjectNGText,
    kLayoutObjectProgress,
    kLayoutObjectQuote,
    kLayoutObjectLayoutButton,
    kLayoutObjectLayoutCustom,
    kLayoutObjectLayoutFlowThread,
    kLayoutObjectLayoutGrid,
    kLayoutObjectLayoutIFrame,
    kLayoutObjectLayoutImage,
    kLayoutObjectLayoutInline,
    kLayoutObjectLayoutMultiColumnSet,
    kLayoutObjectLayoutMultiColumnSpannerPlaceholder,
    kLayoutObjectLayoutEmbeddedContent,
    kLayoutObjectLayoutReplaced,
    kLayoutObjectLayoutScrollbarPart,
    kLayoutObjectLayoutView,
    kLayoutObjectRuby,
    kLayoutObjectRubyBase,
    kLayoutObjectRubyRun,
    kLayoutObjectRubyText,
    kLayoutObjectSlider,
    kLayoutObjectSliderThumb,
    kLayoutObjectTable,
    kLayoutObjectTableCaption,
    kLayoutObjectTableCell,
    kLayoutObjectTableRow,
    kLayoutObjectTableSection,
    kLayoutObjectTextArea,
    kLayoutObjectTextControl,
    kLayoutObjectTextField,
    kLayoutObjectVideo,
    kLayoutObjectWidget,

    kLayoutObjectSVG, /* Keep by itself? */
    kLayoutObjectSVGRoot,
    kLayoutObjectSVGContainer,
    kLayoutObjectSVGTransformableContainer,
    kLayoutObjectSVGViewportContainer,
    kLayoutObjectSVGHiddenContainer,
    kLayoutObjectSVGShape,
    kLayoutObjectSVGText,
    kLayoutObjectSVGTextPath,
    kLayoutObjectSVGInline,
    kLayoutObjectSVGInlineText,
    kLayoutObjectSVGImage,
    kLayoutObjectSVGForeignObject,
    kLayoutObjectSVGResourceContainer,
    kLayoutObjectSVGResourceFilter,
    kLayoutObjectSVGResourceFilterPrimitive,
  };
  virtual bool IsOfType(LayoutObjectType type) const { return false; }

  // Overrides should call the superclass at the end. style_ will be 0 the
  // first time this function will be called.
  virtual void StyleWillChange(StyleDifference, const ComputedStyle& new_style);
  // Overrides should call the superclass at the start. |oldStyle| will be 0 the
  // first time this function is called.
  virtual void StyleDidChange(StyleDifference, const ComputedStyle* old_style);
  void PropagateStyleToAnonymousChildren();
  // Return true for objects that don't want style changes automatically
  // propagated via propagateStyleToAnonymousChildren(), but rather rely on
  // other custom mechanisms (if they need to be notified of parent style
  // changes at all).
  virtual bool AnonymousHasStylePropagationOverride() { return false; }

  virtual void InLayoutNGInlineFormattingContextWillChange(bool) {}

  // A fast path for MapToVisualRectInAncestorSpace for when GeometryMapper
  // can be used. |intersects| is set to whether the input rect intersected
  // (see documentation of return value of MapToVisualRectInAncestorSpace).
  //
  // The return value of this method is whether the fast path could be used.
  bool MapToVisualRectInAncestorSpaceInternalFastPath(
      const LayoutBoxModelObject* ancestor,
      LayoutRect&,
      VisualRectFlags,
      bool& intersects) const;

  // This function is called before calling the destructor so that some clean-up
  // can happen regardless of whether they call a virtual function or not. As a
  // rule of thumb, this function should be preferred to the destructor. See
  // destroy() that is the one calling willBeDestroyed().
  //
  // There are 2 types of destructions: regular destructions and tree tear-down.
  // Regular destructions happen when the renderer is not needed anymore (e.g.
  // 'display' changed or the DOM Node was removed).
  // Tree tear-down is when the whole tree destroyed during navigation. It is
  // handled in the code by checking if documentBeingDestroyed() returns 'true'.
  // In this case, the code skips some unneeded expensive operations as we know
  // the tree is not reused (e.g. avoid clearing the containing block's line
  // box).
  virtual void WillBeDestroyed();

  virtual void InsertedIntoTree();
  virtual void WillBeRemovedFromTree();

  void SetDocumentForAnonymous(Document* document) {
    DCHECK(IsAnonymous());
    node_ = document;
  }

  // Add hit-test rects for the layout tree rooted at this node to the provided
  // collection on a per-Layer basis.
  // currentLayer must be the enclosing layer, and layerOffset is the current
  // offset within this layer. Subclass implementations will add any offset for
  // this layoutObject within it's container, so callers should provide only the
  // offset of the container within it's layer.
  // containerRect is a rect that has already been added for the currentLayer
  // which is likely to be a container for child elements. Any rect wholly
  // contained by containerRect can be skipped.
  virtual void AddLayerHitTestRects(
      LayerHitTestRects&,
      const PaintLayer* current_layer,
      const LayoutPoint& layer_offset,
      TouchAction supported_fast_actions,
      const LayoutRect& container_rect,
      TouchAction container_whitelisted_touch_action) const;

  // Add hit-test rects for this layoutObject only to the provided list.
  // layerOffset is the offset of this layoutObject within the current layer
  // that should be used for each result.
  virtual void ComputeSelfHitTestRects(Vector<LayoutRect>&,
                                       const LayoutPoint& layer_offset) const {}

#if DCHECK_IS_ON()
  virtual bool PaintInvalidationStateIsDirty() const;
#endif

  // Called before paint invalidation.
  virtual void EnsureIsReadyForPaintInvalidation() { DCHECK(!NeedsLayout()); }

  void SetIsBackgroundAttachmentFixedObject(bool);

  void SetEverHadLayout() { bitfields_.SetEverHadLayout(true); }

  // Remove this object and all descendants from the containing
  // LayoutFlowThread.
  void RemoveFromLayoutFlowThread();

  void SetContainsInlineWithOutlineAndContinuation(bool b) {
    bitfields_.SetContainsInlineWithOutlineAndContinuation(b);
  }

  void SetPreviousOutlineMayBeAffectedByDescendants(bool b) {
    bitfields_.SetPreviousOutlineMayBeAffectedByDescendants(b);
  }

  virtual bool VisualRectRespectsVisibility() const { return true; }
  virtual LayoutRect LocalVisualRectIgnoringVisibility() const;

  virtual bool CanBeSelectionLeafInternal() const { return false; }

  virtual LayoutSize OffsetFromContainerInternal(
      const LayoutObject*,
      bool ignore_scroll_offset) const;
  LayoutSize OffsetFromScrollableContainer(const LayoutObject*,
                                           bool ignore_scroll_offset) const;

 private:
  // Used only by applyFirstLineChanges to get a first line style based off of a
  // given new style, without accessing the cache.
  scoped_refptr<const ComputedStyle> UncachedFirstLineStyle() const;

  // Adjusts a visual rect in the space of |visual_rect| to be in the space of
  // the |paint_invalidation_container|, if needed. They can be different only
  // if |paint_invalidation_container| is a composited scroller.
  void AdjustVisualRectForCompositedScrolling(
      LayoutRect& visual_rect,
      const LayoutBoxModelObject& paint_invalidation_container) const;

  FloatQuad LocalToAncestorQuadInternal(const FloatQuad&,
                                        const LayoutBoxModelObject* ancestor,
                                        MapCoordinatesFlags = 0) const;

  void ClearLayoutRootIfNeeded() const;

  bool IsInert() const;

  void UpdateImage(StyleImage*, StyleImage*);

  void ScheduleRelayout();

  void UpdateShapeImage(const ShapeValue*, const ShapeValue*);
  void UpdateFillImages(const FillLayer* old_layers,
                        const FillLayer& new_layers);
  void UpdateCursorImages(const CursorList* old_cursors,
                          const CursorList* new_cursors);
  void CheckCounterChanges(const ComputedStyle* old_style,
                           const ComputedStyle* new_style);

  // Walk up the parent chain and find the first scrolling block to disable
  // scroll anchoring on.
  void SetScrollAnchorDisablingStyleChangedOnAncestor();

  inline void MarkContainerChainForOverflowRecalcIfNeeded();

  inline void SetNeedsPaintOffsetAndVisualRectUpdate();

  inline void InvalidateContainerPreferredLogicalWidths();

  const LayoutBoxModelObject* EnclosingCompositedContainer() const;

  LayoutFlowThread* LocateFlowThreadContainingBlock() const;
  void RemoveFromLayoutFlowThreadRecursive(LayoutFlowThread*);

  const ComputedStyle* CachedFirstLineStyle() const;
  StyleDifference AdjustStyleDifference(StyleDifference) const;

  void RemoveShapeImageClient(ShapeValue*);
  void RemoveCursorImageClient(const CursorList*);

  // These are helper functions for AbsoluteBoudingBoxRectHandlingEmptyAnchor()
  // and AbsoluteBoundingBoxRectForScrollIntoView().
  enum class ExpandScrollMargin { kExpand, kIgnore };
  LayoutRect AbsoluteBoundingBoxRectHelper(ExpandScrollMargin) const;
  bool GetUpperLeftCorner(ExpandScrollMargin, FloatPoint&) const;
  bool GetLowerRightCorner(ExpandScrollMargin, FloatPoint&) const;

#if DCHECK_IS_ON()
  void CheckBlockPositionedObjectsNeedLayout();
#endif

  bool IsTextOrSVGChild() const { return IsText() || IsSVGChild(); }

  static bool IsAllowedToModifyLayoutTreeStructure(Document&);

  // Returns the parent LayoutObject, or nullptr. This has a special case for
  // LayoutView to return the owning LayoutObject in the containing frame.
  inline LayoutObject* ParentCrossingFrames() const;

  void ApplyPseudoStyleChanges(const ComputedStyle& old_style);
  void ApplyFirstLineChanges(const ComputedStyle& old_style);

  LayoutRect VisualRectForInlineBox() const {
    return AdjustVisualRectForInlineBox(VisualRect());
  }
  LayoutRect PartialInvalidationVisualRectForInlineBox() const {
    return AdjustVisualRectForInlineBox(PartialInvalidationVisualRect());
  }
  LayoutRect AdjustVisualRectForInlineBox(const LayoutRect&) const;

  // This is set by Set[Subtree]ShouldDoFullPaintInvalidation, and cleared
  // during PrePaint in this object's InvalidatePaint(). It's different from
  // DisplayItemClient::GetPaintInvalidationReason() which is set during
  // PrePaint and cleared in PaintController::FinishCycle().
  // It's defined as the first field so that it can use the memory gap between
  // DisplayItemClient and LayoutObject's other fields.
  PaintInvalidationReason full_paint_invalidation_reason_;

  scoped_refptr<ComputedStyle> style_;

  // Oilpan: This untraced pointer to the owning Node is considered safe.
  UntracedMember<Node> node_;

  LayoutObject* parent_;
  LayoutObject* previous_;
  LayoutObject* next_;

#if DCHECK_IS_ON()
  unsigned has_ax_object_ : 1;
  unsigned set_needs_layout_forbidden_ : 1;
#endif

#define ADD_BOOLEAN_BITFIELD(field_name_, MethodNameBase) \
 private:                                                 \
  unsigned field_name_ : 1;                               \
                                                          \
 public:                                                  \
  bool MethodNameBase() const { return field_name_; }     \
  void Set##MethodNameBase(bool new_value) { field_name_ = new_value; }

  class LayoutObjectBitfields {
    enum PositionedState {
      kIsStaticallyPositioned = 0,
      kIsRelativelyPositioned = 1,
      kIsOutOfFlowPositioned = 2,
      kIsStickyPositioned = 3,
    };

   public:
    // LayoutObjectBitfields holds all the boolean values for LayoutObject.
    //
    // This is done to promote better packing on LayoutObject (at the expense of
    // preventing bit field packing for the subclasses). Classes concerned about
    // packing and memory use should hoist their boolean to this class. See
    // below the field from sub-classes (e.g. childrenInline).
    //
    // Some of those booleans are caches of ComputedStyle values (e.g.
    // positionState). This enables better memory locality and thus better
    // performance.
    //
    // This class is an artifact of the WebKit era where LayoutObject wasn't
    // allowed to grow and each sub-class was strictly monitored for memory
    // increase. Our measurements indicate that the size of LayoutObject and
    // subsequent classes do not impact memory or speed in a significant
    // manner. This is based on growing LayoutObject in
    // https://codereview.chromium.org/44673003 and subsequent relaxations
    // of the memory constraints on layout objects.
    LayoutObjectBitfields(Node* node)
        : self_needs_layout_(false),
          needs_positioned_movement_layout_(false),
          normal_child_needs_layout_(false),
          pos_child_needs_layout_(false),
          needs_simplified_normal_flow_layout_(false),
          self_needs_layout_overflow_recalc_(false),
          child_needs_layout_overflow_recalc_(false),
          self_needs_visual_overflow_recalc_(false),
          child_needs_visual_overflow_recalc_(false),
          preferred_logical_widths_dirty_(false),
          needs_collect_inlines_(false),
          should_check_for_paint_invalidation_(true),
          subtree_should_check_for_paint_invalidation_(false),
          should_delay_full_paint_invalidation_(false),
          subtree_should_do_full_paint_invalidation_(false),
          may_need_paint_invalidation_animated_background_image_(false),
          should_invalidate_selection_(false),
          needs_paint_offset_and_visual_rect_update_(true),
          descendant_needs_paint_offset_and_visual_rect_update_(true),
          needs_paint_property_update_(true),
          descendant_needs_paint_property_update_(true),
          floating_(false),
          is_anonymous_(!node),
          is_text_(false),
          is_box_(false),
          is_inline_(true),
          is_in_layout_ng_inline_formatting_context_(false),
          is_atomic_inline_level_(false),
          horizontal_writing_mode_(true),
          has_layer_(false),
          has_overflow_clip_(false),
          should_clip_overflow_(false),
          has_transform_related_property_(false),
          has_reflection_(false),
          can_contain_fixed_position_objects_(false),
          has_counter_node_map_(false),
          ever_had_layout_(false),
          ancestor_line_box_dirty_(false),
          is_inside_flow_thread_(false),
          subtree_change_listener_registered_(false),
          notified_of_subtree_change_(false),
          consumes_subtree_change_notification_(false),
          children_inline_(false),
          contains_inline_with_outline_and_continuation_(false),
          always_create_line_boxes_for_layout_inline_(false),
          previous_background_obscured_(false),
          is_background_attachment_fixed_object_(false),
          is_scroll_anchor_object_(false),
          scroll_anchor_disabling_style_changed_(false),
          has_box_decoration_background_(false),
          background_needs_full_paint_invalidation_(true),
          outline_may_be_affected_by_descendants_(false),
          previous_outline_may_be_affected_by_descendants_(false),
          is_truncated_(false),
          inside_blocking_touch_event_handler_(false),
          effective_whitelisted_touch_action_changed_(true),
          descendant_effective_whitelisted_touch_action_changed_(false),
          is_effective_root_scroller_(false),
          positioned_state_(kIsStaticallyPositioned),
          selection_state_(static_cast<unsigned>(SelectionState::kNone)),
          background_obscuration_state_(kBackgroundObscurationStatusInvalid),
          subtree_paint_property_update_reasons_(
              static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone)) {}

    // Self needs layout means that this layout object is marked for a full
    // layout. This is the default layout but it is expensive as it recomputes
    // everything. For CSS boxes, this includes the width (laying out the line
    // boxes again), the margins (due to block collapsing margins), the
    // positions, the height and the potential overflow.
    ADD_BOOLEAN_BITFIELD(self_needs_layout_, SelfNeedsLayout);

    // A positioned movement layout is a specialized type of layout used on
    // positioned objects that only visually moved. This layout is used when
    // changing 'top'/'left' on a positioned element or margins on an
    // out-of-flow one. Because the following operations don't impact the size
    // of the object or sibling LayoutObjects, this layout is very lightweight.
    //
    // Positioned movement layout is implemented in
    // LayoutBlock::simplifiedLayout.
    ADD_BOOLEAN_BITFIELD(needs_positioned_movement_layout_,
                         NeedsPositionedMovementLayout);

    // This boolean is set when a normal flow ('position' == static || relative)
    // child requires layout (but this object doesn't). Due to the nature of
    // CSS, laying out a child can cause the parent to resize (e.g., if 'height'
    // is auto).
    ADD_BOOLEAN_BITFIELD(normal_child_needs_layout_, NormalChildNeedsLayout);

    // This boolean is set when an out-of-flow positioned ('position' == fixed
    // || absolute) child requires layout (but this object doesn't).
    ADD_BOOLEAN_BITFIELD(pos_child_needs_layout_, PosChildNeedsLayout);

    // Simplified normal flow layout only relayouts the normal flow children,
    // ignoring the out-of-flow descendants.
    //
    // The implementation of this layout is in
    // LayoutBlock::simplifiedNormalFlowLayout.
    ADD_BOOLEAN_BITFIELD(needs_simplified_normal_flow_layout_,
                         NeedsSimplifiedNormalFlowLayout);

    ADD_BOOLEAN_BITFIELD(self_needs_layout_overflow_recalc_,
                         SelfNeedsLayoutOverflowRecalc);

    ADD_BOOLEAN_BITFIELD(child_needs_layout_overflow_recalc_,
                         ChildNeedsLayoutOverflowRecalc);

    ADD_BOOLEAN_BITFIELD(self_needs_visual_overflow_recalc_,
                         SelfNeedsVisualOverflowRecalc);

    ADD_BOOLEAN_BITFIELD(child_needs_visual_overflow_recalc_,
                         ChildNeedsVisualOverflowRecalc);

    // This boolean marks preferred logical widths for lazy recomputation.
    //
    // See INTRINSIC SIZES / PREFERRED LOGICAL WIDTHS above about those
    // widths.
    ADD_BOOLEAN_BITFIELD(preferred_logical_widths_dirty_,
                         PreferredLogicalWidthsDirty);

    // This flag is set on inline container boxes that need to run the
    // Pre-layout phase in LayoutNG. See NGInlineNode::CollectInlines().
    // Also maybe set to inline boxes to optimize the propagation.
    ADD_BOOLEAN_BITFIELD(needs_collect_inlines_, NeedsCollectInlines);

    // Paint related dirty bits.
    ADD_BOOLEAN_BITFIELD(should_check_for_paint_invalidation_,
                         ShouldCheckForPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(subtree_should_check_for_paint_invalidation_,
                         SubtreeShouldCheckForPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(should_delay_full_paint_invalidation_,
                         ShouldDelayFullPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(subtree_should_do_full_paint_invalidation_,
                         SubtreeShouldDoFullPaintInvalidation);
    ADD_BOOLEAN_BITFIELD(may_need_paint_invalidation_animated_background_image_,
                         MayNeedPaintInvalidationAnimatedBackgroundImage);
    ADD_BOOLEAN_BITFIELD(should_invalidate_selection_,
                         ShouldInvalidateSelection);
    // Whether the paint offset and visual rect need to be updated for this
    // object.
    ADD_BOOLEAN_BITFIELD(needs_paint_offset_and_visual_rect_update_,
                         NeedsPaintOffsetAndVisualRectUpdate);
    // Whether the paint offset and visual rect need to be updated for the
    // descendants of this object.
    ADD_BOOLEAN_BITFIELD(descendant_needs_paint_offset_and_visual_rect_update_,
                         DescendantNeedsPaintOffsetAndVisualRectUpdate);
    // Whether the paint properties need to be updated. For more details, see
    // LayoutObject::NeedsPaintPropertyUpdate().
    ADD_BOOLEAN_BITFIELD(needs_paint_property_update_,
                         NeedsPaintPropertyUpdate);
    // Whether the paint properties of a descendant need to be updated. For more
    // details, see LayoutObject::DescendantNeedsPaintPropertyUpdate().
    ADD_BOOLEAN_BITFIELD(descendant_needs_paint_property_update_,
                         DescendantNeedsPaintPropertyUpdate);
    // End paint related dirty bits.

    // This boolean is the cached value of 'float'
    // (see ComputedStyle::isFloating).
    ADD_BOOLEAN_BITFIELD(floating_, Floating);

    ADD_BOOLEAN_BITFIELD(is_anonymous_, IsAnonymous);
    ADD_BOOLEAN_BITFIELD(is_text_, IsText);
    ADD_BOOLEAN_BITFIELD(is_box_, IsBox);

    // This boolean represents whether the LayoutObject is 'inline-level'
    // (a CSS concept). Inline-level boxes are laid out inside a line. If
    // unset, the box is 'block-level' and thus stack on top of its
    // siblings (think of paragraphs).
    ADD_BOOLEAN_BITFIELD(is_inline_, IsInline);

    // This boolean is set when this LayoutObject is in LayoutNG inline
    // formatting context. Note, this LayoutObject itself may be laid out by
    // legacy.
    ADD_BOOLEAN_BITFIELD(is_in_layout_ng_inline_formatting_context_,
                         IsInLayoutNGInlineFormattingContext);

    // This boolean is set if the element is an atomic inline-level box.
    //
    // In CSS, atomic inline-level boxes are laid out on a line but they
    // are opaque from the perspective of line layout. This means that they
    // can't be split across lines like normal inline boxes (LayoutInline).
    // Examples of atomic inline-level elements: inline tables, inline
    // blocks and replaced inline elements.
    // See http://www.w3.org/TR/CSS2/visuren.html#inline-boxes.
    //
    // Our code is confused about the use of this boolean and confuses it
    // with being replaced (see LayoutReplaced about this).
    // TODO(jchaffraix): We should inspect callers and clarify their use.
    // TODO(jchaffraix): We set this boolean for replaced elements that are
    // not inline but shouldn't (crbug.com/567964). This should be enforced.
    ADD_BOOLEAN_BITFIELD(is_atomic_inline_level_, IsAtomicInlineLevel);
    ADD_BOOLEAN_BITFIELD(horizontal_writing_mode_, HorizontalWritingMode);

    ADD_BOOLEAN_BITFIELD(has_layer_, HasLayer);

    // This boolean is set if overflow != 'visible'.
    // This means that this object may need an overflow clip to be applied
    // at paint time to its visual overflow (see OverflowModel for more
    // details). Only set for LayoutBoxes and descendants.
    ADD_BOOLEAN_BITFIELD(has_overflow_clip_, HasOverflowClip);

    // Returns whether content which overflows should be clipped. This is not
    // just because of overflow clip, but other types of clip as well, such as
    // control clips or contain: paint.
    ADD_BOOLEAN_BITFIELD(should_clip_overflow_, ShouldClipOverflow);

    // This boolean is the cached value from
    // ComputedStyle::hasTransformRelatedProperty.
    ADD_BOOLEAN_BITFIELD(has_transform_related_property_,
                         HasTransformRelatedProperty);
    ADD_BOOLEAN_BITFIELD(has_reflection_, HasReflection);

    // This boolean is used to know if this LayoutObject is a container for
    // fixed position descendants.
    ADD_BOOLEAN_BITFIELD(can_contain_fixed_position_objects_,
                         CanContainFixedPositionObjects);

    // This boolean is used to know if this LayoutObject has one (or more)
    // associated CounterNode(s).
    // See class comment in LayoutCounter.h for more detail.
    ADD_BOOLEAN_BITFIELD(has_counter_node_map_, HasCounterNodeMap);

    ADD_BOOLEAN_BITFIELD(ever_had_layout_, EverHadLayout);
    ADD_BOOLEAN_BITFIELD(ancestor_line_box_dirty_, AncestorLineBoxDirty);

    ADD_BOOLEAN_BITFIELD(is_inside_flow_thread_, IsInsideFlowThread);

    ADD_BOOLEAN_BITFIELD(subtree_change_listener_registered_,
                         SubtreeChangeListenerRegistered);
    ADD_BOOLEAN_BITFIELD(notified_of_subtree_change_, NotifiedOfSubtreeChange);
    ADD_BOOLEAN_BITFIELD(consumes_subtree_change_notification_,
                         ConsumesSubtreeChangeNotification);

    // from LayoutBlock
    ADD_BOOLEAN_BITFIELD(children_inline_, ChildrenInline);

    // from LayoutBlockFlow
    ADD_BOOLEAN_BITFIELD(contains_inline_with_outline_and_continuation_,
                         ContainsInlineWithOutlineAndContinuation);

    // from LayoutInline
    ADD_BOOLEAN_BITFIELD(always_create_line_boxes_for_layout_inline_,
                         AlwaysCreateLineBoxesForLayoutInline);

    // Background obscuration status of the previous frame.
    ADD_BOOLEAN_BITFIELD(previous_background_obscured_,
                         PreviousBackgroundObscured);

    ADD_BOOLEAN_BITFIELD(is_background_attachment_fixed_object_,
                         IsBackgroundAttachmentFixedObject);
    ADD_BOOLEAN_BITFIELD(is_scroll_anchor_object_, IsScrollAnchorObject);

    // Whether changes in this LayoutObject's CSS properties since the last
    // layout should suppress any adjustments that would be made during the next
    // layout by ScrollAnchor objects for which this LayoutObject is on the path
    // from the anchor node to the scroller.
    // See http://bit.ly/sanaclap for more info.
    ADD_BOOLEAN_BITFIELD(scroll_anchor_disabling_style_changed_,
                         ScrollAnchorDisablingStyleChanged);

    ADD_BOOLEAN_BITFIELD(has_box_decoration_background_,
                         HasBoxDecorationBackground);

    ADD_BOOLEAN_BITFIELD(background_needs_full_paint_invalidation_,
                         BackgroundNeedsFullPaintInvalidation);

    // Whether shape of outline may be affected by any descendants. This is
    // updated before paint invalidation, checked during paint invalidation.
    ADD_BOOLEAN_BITFIELD(outline_may_be_affected_by_descendants_,
                         OutlineMayBeAffectedByDescendants);
    // The outlineMayBeAffectedByDescendants status of the last paint
    // invalidation.
    ADD_BOOLEAN_BITFIELD(previous_outline_may_be_affected_by_descendants_,
                         PreviousOutlineMayBeAffectedByDescendants);

    ADD_BOOLEAN_BITFIELD(is_truncated_, IsTruncated);

    // Whether this object's Node has a blocking touch event handler on itself
    // or an ancestor. This is updated during the PrePaint phase.
    ADD_BOOLEAN_BITFIELD(inside_blocking_touch_event_handler_,
                         InsideBlockingTouchEventHandler);

    // Set when |EffectiveWhitelistedTouchAction| changes (i.e., blocking touch
    // event handlers change or effective touch action style changes). This only
    // needs to be set on the object that changes as the PrePaint walk will
    // ensure descendants are updated.
    ADD_BOOLEAN_BITFIELD(effective_whitelisted_touch_action_changed_,
                         EffectiveWhitelistedTouchActionChanged);

    // Set when a descendant's |EffectiveWhitelistedTouchAction| changes. This
    // is used to ensure the PrePaint tree walk processes objects with
    // |effective_whitelisted_touch_action_changed_|.
    ADD_BOOLEAN_BITFIELD(descendant_effective_whitelisted_touch_action_changed_,
                         DescendantEffectiveWhitelistedTouchActionChanged);

    ADD_BOOLEAN_BITFIELD(is_effective_root_scroller_, IsEffectiveRootScroller);

   private:
    // This is the cached 'position' value of this object
    // (see ComputedStyle::position).
    unsigned positioned_state_ : 2;  // PositionedState
    unsigned selection_state_ : 3;   // SelectionState
    // Mutable for getter which lazily update this field.
    mutable unsigned
        background_obscuration_state_ : 2;  // BackgroundObscurationState

    // Reasons for the full subtree invalidation.
    unsigned subtree_paint_property_update_reasons_
        : kSubtreePaintPropertyUpdateReasonsBitfieldWidth;

   public:
    bool IsOutOfFlowPositioned() const {
      return positioned_state_ == kIsOutOfFlowPositioned;
    }
    bool IsRelPositioned() const {
      return positioned_state_ == kIsRelativelyPositioned;
    }
    bool IsStickyPositioned() const {
      return positioned_state_ == kIsStickyPositioned;
    }
    bool IsInFlowPositioned() const {
      return positioned_state_ == kIsRelativelyPositioned ||
             positioned_state_ == kIsStickyPositioned;
    }
    bool IsPositioned() const {
      return positioned_state_ != kIsStaticallyPositioned;
    }

    void SetPositionedState(EPosition position_state) {
      // This maps FixedPosition and AbsolutePosition to
      // IsOutOfFlowPositioned, saving one bit.
      switch (position_state) {
        case EPosition::kStatic:
          positioned_state_ = kIsStaticallyPositioned;
          break;
        case EPosition::kRelative:
          positioned_state_ = kIsRelativelyPositioned;
          break;
        case EPosition::kAbsolute:
        case EPosition::kFixed:
          positioned_state_ = kIsOutOfFlowPositioned;
          break;
        case EPosition::kSticky:
          positioned_state_ = kIsStickyPositioned;
          break;
        default:
          NOTREACHED();
          break;
      }
    }
    void ClearPositionedState() {
      positioned_state_ = kIsStaticallyPositioned;
    }

    ALWAYS_INLINE SelectionState GetSelectionState() const {
      return static_cast<SelectionState>(selection_state_);
    }
    ALWAYS_INLINE void SetSelectionState(SelectionState selection_state) {
      selection_state_ = static_cast<unsigned>(selection_state);
    }

    ALWAYS_INLINE unsigned SubtreePaintPropertyUpdateReasons() const {
      return subtree_paint_property_update_reasons_;
    }
    ALWAYS_INLINE void AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason reason) {
      DCHECK_LE(static_cast<unsigned>(reason),
                1u << (kSubtreePaintPropertyUpdateReasonsBitfieldWidth - 1));
      subtree_paint_property_update_reasons_ |= static_cast<unsigned>(reason);
    }
    ALWAYS_INLINE void ResetSubtreePaintPropertyUpdateReasons() {
      subtree_paint_property_update_reasons_ =
          static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone);
    }

    ALWAYS_INLINE BackgroundObscurationState
    GetBackgroundObscurationState() const {
      return static_cast<BackgroundObscurationState>(
          background_obscuration_state_);
    }
    ALWAYS_INLINE void SetBackgroundObscurationState(
        BackgroundObscurationState s) const {
      background_obscuration_state_ = s;
    }
  };

#undef ADD_BOOLEAN_BITFIELD

  LayoutObjectBitfields bitfields_;

  void SetSelfNeedsLayout(bool b) { bitfields_.SetSelfNeedsLayout(b); }
  void SetNeedsPositionedMovementLayout(bool b) {
    bitfields_.SetNeedsPositionedMovementLayout(b);
  }
  void SetNormalChildNeedsLayout(bool b) {
    bitfields_.SetNormalChildNeedsLayout(b);
  }
  void SetPosChildNeedsLayout(bool b) { bitfields_.SetPosChildNeedsLayout(b); }
  void SetNeedsSimplifiedNormalFlowLayout(bool b) {
    bitfields_.SetNeedsSimplifiedNormalFlowLayout(b);
  }
  void SetNeedsCollectInlines(bool b) { bitfields_.SetNeedsCollectInlines(b); }

 private:
  friend class LineLayoutItem;

  // Store state between styleWillChange and styleDidChange
  static bool affects_parent_block_;

  FragmentData fragment_;
  DISALLOW_COPY_AND_ASSIGN(LayoutObject);
};

// Allow equality comparisons of LayoutObjects by reference or pointer,
// interchangeably.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(LayoutObject)

inline bool LayoutObject::DocumentBeingDestroyed() const {
  return GetDocument().Lifecycle().GetState() >= DocumentLifecycle::kStopping;
}

inline bool LayoutObject::IsBeforeContent() const {
  if (StyleRef().StyleType() != kPseudoIdBefore)
    return false;
  // Text nodes don't have their own styles, so ignore the style on a text node.
  if (IsText() && !IsBR())
    return false;
  return true;
}

inline bool LayoutObject::IsAfterContent() const {
  if (StyleRef().StyleType() != kPseudoIdAfter)
    return false;
  // Text nodes don't have their own styles, so ignore the style on a text node.
  if (IsText() && !IsBR())
    return false;
  return true;
}

inline bool LayoutObject::IsBeforeOrAfterContent() const {
  return IsBeforeContent() || IsAfterContent();
}

// setNeedsLayout() won't cause full paint invalidations as
// setNeedsLayoutAndFullPaintInvalidation() does. Otherwise the two methods are
// identical.
inline void LayoutObject::SetNeedsLayout(
    LayoutInvalidationReasonForTracing reason,
    MarkingBehavior mark_parents,
    SubtreeLayoutScope* layouter) {
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
#endif
  bool already_needed_layout = bitfields_.SelfNeedsLayout();
  SetSelfNeedsLayout(true);
  MarkContainerNeedsCollectInlines();
  if (!already_needed_layout) {
    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
        "LayoutInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorLayoutInvalidationTrackingEvent::Data(this, reason));
    if (mark_parents == kMarkContainerChain &&
        (!layouter || layouter->Root() != this))
      MarkContainerChainForLayout(!layouter, layouter);
  }
}

inline void LayoutObject::SetNeedsLayoutAndFullPaintInvalidation(
    LayoutInvalidationReasonForTracing reason,
    MarkingBehavior mark_parents,
    SubtreeLayoutScope* layouter) {
  SetNeedsLayout(reason, mark_parents, layouter);
  SetShouldDoFullPaintInvalidation();
}

inline void LayoutObject::ClearNeedsLayout() {
  // Set flags for later stages/cycles.
  SetEverHadLayout();
  SetShouldCheckForPaintInvalidation();

  // Clear needsLayout flags.
  SetSelfNeedsLayout(false);
  SetPosChildNeedsLayout(false);
  SetNeedsSimplifiedNormalFlowLayout(false);
  SetNormalChildNeedsLayout(false);
  SetNeedsPositionedMovementLayout(false);
  SetAncestorLineBoxDirty(false);

#if DCHECK_IS_ON()
  CheckBlockPositionedObjectsNeedLayout();
#endif

  SetScrollAnchorDisablingStyleChanged(false);
}

inline void LayoutObject::SetChildNeedsLayout(MarkingBehavior mark_parents,
                                              SubtreeLayoutScope* layouter) {
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
#endif
  bool already_needed_layout = NormalChildNeedsLayout();
  SetNormalChildNeedsLayout(true);
  MarkContainerNeedsCollectInlines();
  // FIXME: Replace MarkOnlyThis with the SubtreeLayoutScope code path and
  // remove the MarkingBehavior argument entirely.
  if (!already_needed_layout && mark_parents == kMarkContainerChain &&
      (!layouter || layouter->Root() != this))
    MarkContainerChainForLayout(!layouter, layouter);
}

inline void LayoutObject::SetNeedsPositionedMovementLayout() {
  bool already_needed_layout = NeedsPositionedMovementLayout();
  SetNeedsPositionedMovementLayout(true);
#if DCHECK_IS_ON()
  DCHECK(!IsSetNeedsLayoutForbidden());
#endif
  if (!already_needed_layout)
    MarkContainerChainForLayout();
}

inline void LayoutObject::SetIsInLayoutNGInlineFormattingContext(
    bool new_value) {
  if (IsInLayoutNGInlineFormattingContext() == new_value)
    return;
  InLayoutNGInlineFormattingContextWillChange(new_value);
  bitfields_.SetIsInLayoutNGInlineFormattingContext(new_value);
}

inline void LayoutObject::SetHasBoxDecorationBackground(bool b) {
  if (b == bitfields_.HasBoxDecorationBackground())
    return;

  bitfields_.SetHasBoxDecorationBackground(b);
  InvalidateBackgroundObscurationStatus();
}

inline void LayoutObject::InvalidateBackgroundObscurationStatus() {
  bitfields_.SetBackgroundObscurationState(kBackgroundObscurationStatusInvalid);
}

DISABLE_CFI_PERF
inline bool LayoutObject::BackgroundIsKnownToBeObscured() const {
  if (bitfields_.GetBackgroundObscurationState() ==
      kBackgroundObscurationStatusInvalid) {
    BackgroundObscurationState state = ComputeBackgroundIsKnownToBeObscured()
                                           ? kBackgroundKnownToBeObscured
                                           : kBackgroundMayBeVisible;
    bitfields_.SetBackgroundObscurationState(state);
  }
  return bitfields_.GetBackgroundObscurationState() ==
         kBackgroundKnownToBeObscured;
}

inline void MakeMatrixRenderable(TransformationMatrix& matrix,
                                 bool has3d_rendering) {
  if (!has3d_rendering)
    matrix.MakeAffine();
}

enum class LayoutObjectSide {
  kRemainingTextIfOnBoundary,
  kFirstLetterIfOnBoundary
};
CORE_EXPORT const LayoutObject* AssociatedLayoutObjectOf(
    const Node&,
    int offset_in_node,
    LayoutObjectSide = LayoutObjectSide::kRemainingTextIfOnBoundary);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const LayoutObject*);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const LayoutObject&);

#define DEFINE_LAYOUT_OBJECT_TYPE_CASTS(thisType, predicate)           \
  DEFINE_TYPE_CASTS(thisType, LayoutObject, object, object->predicate, \
                    object.predicate)

}  // namespace blink

#ifndef NDEBUG
// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showTree(const blink::LayoutObject*);
CORE_EXPORT void showLineTree(const blink::LayoutObject*);
CORE_EXPORT void showLayoutTree(const blink::LayoutObject* object1);
// We don't make object2 an optional parameter so that showLayoutTree
// can be called from gdb easily.
CORE_EXPORT void showLayoutTree(const blink::LayoutObject* object1,
                                const blink::LayoutObject* object2);

#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_OBJECT_H_
