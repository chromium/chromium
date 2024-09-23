// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FRAME_TREE_NODE_ID_H_
#define CONTENT_PUBLIC_BROWSER_FRAME_TREE_NODE_ID_H_

#include "base/types/id_type.h"

namespace content {

// A FrameTreeNode ID is a browser-global value that uniquely identifies a
// browser-side concept of a frame (a "FrameTreeNode") that hosts content.
//
// When the FrameTreeNode is removed, the ID is not reused.
//
// A FrameTreeNode can outlive its current RenderFrameHost, and may be
// associated with multiple RenderFrameHosts over time. This happens because
// some navigations require a "RenderFrameHost swap" which causes a new
// RenderFrameHost to be housed in the FrameTreeNode. For example, this happens
// for any cross-process navigation, since a RenderFrameHost is tied to a
// process.
//
// In the other direction, a RenderFrameHost can also transfer to a different
// FrameTreeNode! Prior to the advent of prerendered pages
// (content/browser/preloading/prerender/README.md), that was not true, and it
// could be assumed that the return value of
// RenderFrameHost::GetFrameTreeNodeId() was constant over the lifetime of the
// RenderFrameHost. But with prerender activations, the main frame of the
// prerendered page transfers to a new FrameTreeNode, so newer code should no
// longer make that assumption. This transfer only happens for main frames
// (currently only during a prerender activation navigation) and never happens
// for subframes.
//
// Like all base::IdType types, this is default-constructed in the null state.
// The null state has an invalid value and will test `false` when converted to
// boolean. (Generation of new values is done with FrameTreeNodeId::Generator.)
using FrameTreeNodeId = base::IdType<class FrameTreeNodeIdTag,
                                     int32_t,
                                     /*kInvalidValue=*/-1,
                                     /*kFirstGeneratedId=*/1,
                                     /*kExtraInvalidValues=*/0>;

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FRAME_TREE_NODE_ID_H_
