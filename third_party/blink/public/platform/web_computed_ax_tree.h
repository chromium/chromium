// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPUTED_AX_TREE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPUTED_AX_TREE_H_

#include "third_party/blink/public/platform/web_string.h"

namespace blink {

enum WebAOMIntAttribute {
  AOM_INT_ATTRIBUTE_NONE,
  AOM_ATTR_COLUMN_COUNT,
  AOM_ATTR_COLUMN_INDEX,
  AOM_ATTR_COLUMN_SPAN,
  AOM_ATTR_HIERARCHICAL_LEVEL,
  AOM_ATTR_POS_IN_SET,
  AOM_ATTR_ROW_COUNT,
  AOM_ATTR_ROW_INDEX,
  AOM_ATTR_ROW_SPAN,
  AOM_ATTR_SET_SIZE,
};

enum WebAOMFloatAttribute {
  AOM_FLOAT_ATTRIBUTE_NONE,
  AOM_ATTR_VALUE_MIN,
  AOM_ATTR_VALUE_MAX,
  AOM_ATTR_VALUE_NOW,
};

enum WebAOMStringAttribute {
  AOM_STRING_ATTRIBUTE_NONE,
  AOM_ATTR_AUTOCOMPLETE,
  AOM_ATTR_KEY_SHORTCUTS,
  AOM_ATTR_NAME,
  AOM_ATTR_PLACEHOLDER,
  AOM_ATTR_ROLE_DESCRIPTION,
  AOM_ATTR_VALUE_TEXT,
};

enum WebAOMBoolAttribute {
  AOM_BOOL_ATTRIBUTE_NONE,
  AOM_ATTR_ATOMIC,
  AOM_ATTR_BUSY,
  AOM_ATTR_DISABLED,
  AOM_ATTR_EXPANDED,
  AOM_ATTR_MODAL,
  AOM_ATTR_MULTILINE,
  AOM_ATTR_MULTISELECTABLE,
  AOM_ATTR_READONLY,
  AOM_ATTR_REQUIRED,
  AOM_ATTR_SELECTED,
};

class WebComputedAXTree {
 public:
  virtual ~WebComputedAXTree() {}

  // Compute the accessibility tree as a snapshot. Returns true if the snapshot
  // could be unserialized into a tree successfully.
  virtual bool ComputeAccessibilityTree() = 0;

  // Accessibility Property Accessors.

  // Get the specified attribute for a given AXID, returning true if the
  // node exists in the tree and contains the attribute, and stores the result
  // in |out_param|.
  virtual bool GetBoolAttributeForAXNode(int32_t ax_id,
                                         WebAOMBoolAttribute,
                                         bool* out_param) = 0;
  virtual bool GetIntAttributeForAXNode(int32_t ax_id,
                                        WebAOMIntAttribute,
                                        int32_t* out_param) = 0;
  virtual bool GetStringAttributeForAXNode(int32_t ax_id,
                                           WebAOMStringAttribute,
                                           WebString* out_param) = 0;

  virtual bool GetFloatAttributeForAXNode(int32_t ax_id,
                                          WebAOMFloatAttribute,
                                          float* out_param) = 0;

  // The role is stored seperately from other attributes in the AXNode, so we
  // expose a seperate method for retrieving this.
  virtual bool GetRoleForAXNode(int32_t ax_id, blink::WebString* out_param) = 0;
  virtual bool GetCheckedStateForAXNode(int32_t ax_id,
                                        blink::WebString* out_param) = 0;
  virtual bool GetParentIdForAXNode(int32_t ax_id, int32_t* out_param) = 0;
  virtual bool GetFirstChildIdForAXNode(int32_t ax_id, int32_t* out_param) = 0;
  virtual bool GetLastChildIdForAXNode(int32_t ax_id, int32_t* out_param) = 0;
  virtual bool GetPreviousSiblingIdForAXNode(int32_t ax_id,
                                             int32_t* out_param) = 0;
  virtual bool GetNextSiblingIdForAXNode(int32_t ax_id, int32_t* out_param) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_COMPUTED_AX_TREE_H_
