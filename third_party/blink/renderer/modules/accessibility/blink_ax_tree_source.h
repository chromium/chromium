// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_

#include <stdint.h>

#include <set>
#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_source.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT BlinkAXTreeSource
    : public GarbageCollected<BlinkAXTreeSource>,
      public ui::AXTreeSource<AXObject*> {
 public:
  explicit BlinkAXTreeSource(AXObjectCacheImpl& ax_object_cache);
  ~BlinkAXTreeSource() override;

  static BlinkAXTreeSource* Create(AXObjectCacheImpl& ax_object_cache) {
    return MakeGarbageCollected<BlinkAXTreeSource>(ax_object_cache);
  }

  // AXTreeSource implementation.
  bool GetTreeData(ui::AXTreeData* tree_data) const override;
  AXObject* GetRoot() const override;
  AXObject* GetFromId(int32_t id) const override;
  int32_t GetId(AXObject* node) const override;
  void CacheChildrenIfNeeded(AXObject*) override {}
  size_t GetChildCount(AXObject* node) const override;
  AXObject* ChildAt(AXObject* node, size_t) const override;
  void ClearChildCache(AXObject*) override {}
  AXObject* GetParent(AXObject* node) const override;
  void SerializeNode(AXObject* node, ui::AXNodeData* out_data) const override;
  bool IsIgnored(AXObject* node) const override;
  bool IsEqual(AXObject* node1, AXObject* node2) const override;
  AXObject* GetNull() const override;
  std::string GetDebugString(AXObject* node) const override;

  // Set the id of the node to fetch image data for. Normally the content
  // of images is not part of the accessibility tree, but one node at a
  // time can be designated as the image data node, which will send the
  // contents of the image with each accessibility update until another
  // node is designated.
  int image_data_node_id() { return image_data_node_id_; }
  void set_image_data_node_id(int id, const gfx::Size& max_size) {
    image_data_node_id_ = id;
    max_image_data_size_ = max_size;
  }

  // Ignore code that limits based on the protocol (like https, file, etc.)
  // to enable tests to run.
  static void IgnoreProtocolChecksForTesting();

  void Trace(Visitor*) const;

  void OnLoadInlineTextBoxes(AXObject& obj);

  AXObject* GetPluginRoot();

  void Freeze();

  void Thaw();

 private:
  void SetLoadInlineTextBoxesForId(int32_t id);

  void Selection(const AXObject* obj,
                 bool& is_selection_backward,
                 AXObject** anchor_object,
                 int& anchor_offset,
                 ax::mojom::blink::TextAffinity& anchor_affinity,
                 AXObject** focus_object,
                 int& focus_offset,
                 ax::mojom::blink::TextAffinity& focus_affinity) const;

  AXObject* GetFocusedObject() const;

  // The ID of the object to fetch image data for.
  int image_data_node_id_ = -1;

  gfx::Size max_image_data_size_;

  // Whether we should highlight annotation results visually on the page
  // for debugging.
  bool image_annotation_debugging_ = false;

  Member<AXObjectCacheImpl> ax_object_cache_;

  // These are updated when calling |Freeze|.
  bool frozen_ = false;
  Member<AXObject> root_ = nullptr;
  Member<AXObject> focus_ = nullptr;

  // The AxID of the first unlabeled image we have encountered in this tree.
  //
  // Used to ensure that the tutor message that explains to screen reader users
  // how to turn on automatic image labels is provided only once.
  mutable absl::optional<int32_t> first_unlabeled_image_id_ = absl::nullopt;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_BLINK_AX_TREE_SOURCE_H_
