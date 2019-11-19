// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node_blame_context.h"

#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/traced_value.h"
#include "content/browser/frame_host/frame_tree.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kFrameTreeNodeBlameContextCategory[] = "navigation";
const char kFrameTreeNodeBlameContextName[] = "FrameTreeNodeBlameContext";
const char kFrameTreeNodeBlameContextType[] = "FrameTreeNode";
const char kFrameTreeNodeBlameContextScope[] = "FrameTreeNode";
const char kRenderFrameBlameContextScope[] = "RenderFrame";

}  // namespace

FrameTreeNodeBlameContext::FrameTreeNodeBlameContext(int node_id,
                                                     FrameTreeNode* parent)
    : base::trace_event::BlameContext(
          kFrameTreeNodeBlameContextCategory,
          kFrameTreeNodeBlameContextName,
          kFrameTreeNodeBlameContextType,
          kFrameTreeNodeBlameContextScope,
          node_id,
          parent ? &parent->blame_context() : nullptr) {}

FrameTreeNodeBlameContext::~FrameTreeNodeBlameContext() {}

void FrameTreeNodeBlameContext::AsValueInto(
    base::trace_event::TracedValue* value) {
  BlameContext::AsValueInto(value);

  // id() is equal to the owner FrameTreeNode's id, as set in the constructor.
  FrameTreeNode* owner = FrameTreeNode::GloballyFindByID(id());
  DCHECK(owner);

  RenderFrameHostImpl* current_frame_host = owner->current_frame_host();
  if (!current_frame_host)
    return;

  int process_id = base::kNullProcessId;
  if (current_frame_host->GetProcess()->GetProcess().IsValid())
    process_id = current_frame_host->GetProcess()->GetProcess().Pid();

  if (process_id >= 0) {
    int routing_id = current_frame_host->GetRoutingID();
    DCHECK_NE(routing_id, MSG_ROUTING_NONE);

    value->BeginDictionary("renderFrame");
    value->SetInteger("pid_ref", process_id);
    value->SetString("id_ref", base::StringPrintf("0x%x", routing_id));
    value->SetString("scope", kRenderFrameBlameContextScope);
    value->EndDictionary();
  }

  GURL url = current_frame_host->GetLastCommittedURL();
  if (url.is_valid())
    value->SetString("url", url.spec());
}

}  // namespace content
