// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/platform/web_computed_ax_tree.h"

#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/web_string.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

class AomContentAxTree : public blink::WebComputedAXTree {
 public:
  explicit AomContentAxTree(RenderFrameImpl* render_frame);

  AomContentAxTree(const AomContentAxTree&) = delete;
  AomContentAxTree& operator=(const AomContentAxTree&) = delete;

  // blink::WebComputedAXTree implementation.
  bool ComputeAccessibilityTree() override;

  bool GetBoolAttributeForAXNode(int32_t ax_id,
                                 blink::WebAOMBoolAttribute,
                                 bool* out_param) override;
  bool GetIntAttributeForAXNode(int32_t ax_id,
                                blink::WebAOMIntAttribute,
                                int32_t* out_param) override;
  bool GetStringAttributeForAXNode(int32_t ax_id,
                                   blink::WebAOMStringAttribute,
                                   blink::WebString* out_param) override;
  bool GetFloatAttributeForAXNode(int32_t ax_id,
                                  blink::WebAOMFloatAttribute,
                                  float* out_param) override;
  bool GetRoleForAXNode(int32_t ax_id, blink::WebString* out_param) override;
  bool GetCheckedStateForAXNode(int32_t ax_id,
                                blink::WebString* out_param) override;
  bool GetParentIdForAXNode(int32_t ax_id, int32_t* out_param) override;
  bool GetFirstChildIdForAXNode(int32_t ax_id, int32_t* out_param) override;
  bool GetLastChildIdForAXNode(int32_t ax_id, int32_t* out_param) override;
  bool GetPreviousSiblingIdForAXNode(int32_t ax_id,
                                     int32_t* out_param) override;
  bool GetNextSiblingIdForAXNode(int32_t ax_id, int32_t* out_param) override;

 private:
  bool GetRestrictionAttributeForAXNode(int32_t,
                                        blink::WebAOMBoolAttribute,
                                        bool* out_param);
  bool GetStateAttributeForAXNode(int32_t,
                                  blink::WebAOMBoolAttribute,
                                  bool* out_param);
  ui::AXTree tree_;
  raw_ptr<RenderFrameImpl> render_frame_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_
