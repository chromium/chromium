// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/frame_tree_node_id_registry.h"

#include "base/bind_helpers.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/public/browser/web_contents.h"

namespace content {

using WebContentsGetter = FrameTreeNodeIdRegistry::WebContentsGetter;
using IsMainFrameGetter = FrameTreeNodeIdRegistry::IsMainFrameGetter;

namespace {

base::Optional<bool> IsMainFrameFromFrameTreeNodeId(int frame_tree_node_id) {
  if (FrameTreeNode* frame_tree_node =
          FrameTreeNode::GloballyFindByID(frame_tree_node_id)) {
    return frame_tree_node->IsMainFrame();
  }
  return base::nullopt;
}

}  // namespace

// static
FrameTreeNodeIdRegistry* FrameTreeNodeIdRegistry::GetInstance() {
  static base::NoDestructor<FrameTreeNodeIdRegistry> instance;
  return instance.get();
}

void FrameTreeNodeIdRegistry::Add(const base::UnguessableToken& id,
                                  const int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = (map_.emplace(id, frame_tree_node_id)).second;
  CHECK(inserted);
}

void FrameTreeNodeIdRegistry::Remove(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  map_.erase(id);
}

WebContentsGetter FrameTreeNodeIdRegistry::GetWebContentsGetter(
    const base::UnguessableToken& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = map_.find(id);
  if (iter == map_.end())
    return base::NullCallback();
  return base::BindRepeating(&WebContents::FromFrameTreeNodeId, iter->second);
}

IsMainFrameGetter FrameTreeNodeIdRegistry::GetIsMainFrameGetter(
    const base::UnguessableToken& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = map_.find(id);
  if (iter == map_.end()) {
    return base::NullCallback();
  }
  return base::BindRepeating(&IsMainFrameFromFrameTreeNodeId, iter->second);
}

FrameTreeNodeIdRegistry::FrameTreeNodeIdRegistry() = default;

FrameTreeNodeIdRegistry::~FrameTreeNodeIdRegistry() = default;

}  // namespace content
