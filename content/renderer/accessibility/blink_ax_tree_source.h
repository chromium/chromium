// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_

#include <stdint.h>

#include <set>
#include <string>
#include <unordered_map>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace content {

class AXImageAnnotator;
class BlinkAXTreeSource;
class RenderFrameImpl;

// Create this on the stack to freeze BlinkAXTreeSource and automatically
// un-freeze it when it goes out of scope.
class ScopedFreezeBlinkAXTreeSource {
 public:
  explicit ScopedFreezeBlinkAXTreeSource(BlinkAXTreeSource* tree_source);

  ScopedFreezeBlinkAXTreeSource(const ScopedFreezeBlinkAXTreeSource&) = delete;
  ScopedFreezeBlinkAXTreeSource& operator=(
      const ScopedFreezeBlinkAXTreeSource&) = delete;

  ~ScopedFreezeBlinkAXTreeSource();

 private:
  BlinkAXTreeSource* tree_source_;
};

class CONTENT_EXPORT BlinkAXTreeSource
    : public ui::AXTreeSource<blink::WebAXObject> {
 public:
  BlinkAXTreeSource(RenderFrameImpl* render_frame, ui::AXMode mode);
  ~BlinkAXTreeSource() override;

  // Freeze caches the document, accessibility root, and current focused
  // object for fast retrieval during a batch of operations. Use
  // ScopedFreezeBlinkAXTreeSource on the stack rather than calling
  // these directly.
  void Freeze();
  void Thaw();

  // It may be necessary to call SetRoot if you're using a WebScopedAXContext,
  // because BlinkAXTreeSource can't get the root of the tree from the
  // WebDocument if accessibility isn't enabled globally.
  void SetRoot(blink::WebAXObject root);

#if defined(AX_FAIL_FAST_BUILD)
  // Walks up the ancestor chain to see if this is a descendant of the root.
  // TODO(accessibility) Remove once it's clear this never triggers.
  bool IsInTree(blink::WebAXObject node) const;
#endif

  ui::AXMode accessibility_mode() { return accessibility_mode_; }
  void SetAccessibilityMode(ui::AXMode new_mode);

  bool exclude_offscreen() const { return exclude_offscreen_; }
  void set_exclude_offscreen(bool exclude) { exclude_offscreen_ = exclude; }

  // Set the id of the node to fetch image data for. Normally the content
  // of images is not part of the accessibility tree, but one node at a
  // time can be designated as the image data node, which will send the
  // contents of the image with each accessibility update until another
  // node is designated.
  int image_data_node_id() { return image_data_node_id_; }
  void set_image_data_node_id(int id) { image_data_node_id_ = id; }

  void set_max_image_data_size(const gfx::Size& size) {
    max_image_data_size_ = size;
  }

  // The following methods add or remove an image annotator which is used to
  // provide automatic labels for images.
  void AddBlinkImageAnnotator(AXImageAnnotator* const annotator) {
    image_annotator_ = annotator;
  }
  void RemoveBlinkImageAnnotator() {
    image_annotator_ = nullptr;
    first_unlabeled_image_id_ = absl::nullopt;
  }

  // Query or update a set of IDs for which we should load inline text boxes.
  bool ShouldLoadInlineTextBoxes(const blink::WebAXObject& obj) const;
  void SetLoadInlineTextBoxesForId(int32_t id);

  void PopulateAXRelativeBounds(blink::WebAXObject obj,
                                ui::AXRelativeBounds* bounds,
                                bool* clips_children = nullptr) const;

  // Cached bounding boxes.
  bool HasCachedBoundingBox(int32_t id) const;
  const ui::AXRelativeBounds& GetCachedBoundingBox(int32_t id) const;
  void SetCachedBoundingBox(int32_t id, const ui::AXRelativeBounds& bounds);
  size_t GetCachedBoundingBoxCount() const;

  // AXTreeSource implementation.
  bool GetTreeData(ui::AXTreeData* tree_data) const override;
  blink::WebAXObject GetRoot() const override;
  blink::WebAXObject GetFromId(int32_t id) const override;
  int32_t GetId(blink::WebAXObject node) const override;
  void GetChildren(
      blink::WebAXObject node,
      std::vector<blink::WebAXObject>* out_children) const override;
  blink::WebAXObject GetParent(blink::WebAXObject node) const override;
  void SerializeNode(blink::WebAXObject node,
                     ui::AXNodeData* out_data) const override;
  bool IsIgnored(blink::WebAXObject node) const override;
  bool IsValid(blink::WebAXObject node) const override;
  bool IsEqual(blink::WebAXObject node1,
               blink::WebAXObject node2) const override;
  blink::WebAXObject GetNull() const override;
  std::string GetDebugString(blink::WebAXObject node) const override;
  void SerializerClearedNode(int32_t node_id) override;

  blink::WebDocument GetMainDocument() const;

  // Ignore code that limits based on the protocol (like https, file, etc.)
  // to enable tests to run.
  static void IgnoreProtocolChecksForTesting();

 private:
  const blink::WebDocument& document() const {
    DCHECK(frozen_);
    return document_;
  }
  const blink::WebAXObject& root() const {
    DCHECK(frozen_);
    return root_;
  }
  const blink::WebAXObject& focus() const {
    DCHECK(frozen_);
    return focus_;
  }

  void SerializeBoundingBoxAttributes(blink::WebAXObject src,
                                      ui::AXNodeData* dst) const;
  void SerializeOtherScreenReaderAttributes(blink::WebAXObject src,
                                            ui::AXNodeData* dst) const;
  blink::WebAXObject ComputeRoot() const;

  // Max length for attributes such as aria-label.
  static const uint32_t kMaxStringAttributeLength = 10000;
  // Max length for a static text name.
  // Length of War and Peace (http://www.gutenberg.org/files/2600/2600-0.txt).
  static const uint32_t kMaxStaticTextLength = 3227574;
  void TruncateAndAddStringAttribute(
      ui::AXNodeData* dst,
      ax::mojom::StringAttribute attribute,
      const std::string& value,
      uint32_t max_len = kMaxStringAttributeLength) const;

  void AddImageAnnotations(blink::WebAXObject& src, ui::AXNodeData* dst) const;

  RenderFrameImpl* render_frame_;

  ui::AXMode accessibility_mode_;

  // If true, excludes nodes and their entire subtrees if they're entirely
  // offscreen. This is only meant to be used when snapshotting the
  // accessibility tree.
  bool exclude_offscreen_ = false;

  // An explicit root to use, otherwise it's taken from the WebDocument.
  blink::WebAXObject explicit_root_;

  // A set of IDs for which we should always load inline text boxes.
  std::set<int32_t> load_inline_text_boxes_ids_;

  // The ID of the object to fetch image data for.
  int image_data_node_id_ = -1;

  gfx::Size max_image_data_size_;

  // The class instance that retrieves and manages automatic labels for images.
  AXImageAnnotator* image_annotator_ = nullptr;

  // Whether we should highlight annotation results visually on the page
  // for debugging.
  bool image_annotation_debugging_ = false;

  // The AxID of the first unlabeled image we have encountered in this tree.
  //
  // Used to ensure that the tutor message that explains to screen reader users
  // how to turn on automatic image labels is provided only once.
  mutable absl::optional<int32_t> first_unlabeled_image_id_ = absl::nullopt;

  // Current bounding box of every object, so we can detect when it moves.
  mutable std::unordered_map<int, ui::AXRelativeBounds> cached_bounding_boxes_;

  // These are updated when calling |Freeze|.
  bool frozen_ = false;
  blink::WebDocument document_;
  blink::WebAXObject root_;
  blink::WebAXObject focus_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
