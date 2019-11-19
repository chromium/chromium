// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
#define CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_

// IPC messages for accessibility.

#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/transform.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::AXContentIntAttribute,
                          content::AX_CONTENT_INT_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ax::mojom::Action, ax::mojom::Action::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(ax::mojom::ScrollAlignment,
                          ax::mojom::ScrollAlignment::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(ui::AXActionData)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(target_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(source_extension_id)
  IPC_STRUCT_TRAITS_MEMBER(target_node_id)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(flags)
  IPC_STRUCT_TRAITS_MEMBER(anchor_node_id)
  IPC_STRUCT_TRAITS_MEMBER(anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(focus_node_id)
  IPC_STRUCT_TRAITS_MEMBER(focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(custom_action_id)
  IPC_STRUCT_TRAITS_MEMBER(target_rect)
  IPC_STRUCT_TRAITS_MEMBER(target_point)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(hit_test_event_to_fire)
  IPC_STRUCT_TRAITS_MEMBER(horizontal_scroll_alignment)
  IPC_STRUCT_TRAITS_MEMBER(vertical_scroll_alignment)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(actions)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
  IPC_STRUCT_TRAITS_MEMBER(content_int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(relative_bounds)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeData)
  IPC_STRUCT_TRAITS_MEMBER(tree_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(focused_tree_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(mimetype)
  IPC_STRUCT_TRAITS_MEMBER(doctype)
  IPC_STRUCT_TRAITS_MEMBER(loaded)
  IPC_STRUCT_TRAITS_MEMBER(loading_progress)
  IPC_STRUCT_TRAITS_MEMBER(focus_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_is_backward)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_anchor_affinity)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_object_id)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_offset)
  IPC_STRUCT_TRAITS_MEMBER(sel_focus_affinity)
  IPC_STRUCT_TRAITS_MEMBER(routing_id)
  IPC_STRUCT_TRAITS_MEMBER(parent_routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::AXContentTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(has_tree_data)
  IPC_STRUCT_TRAITS_MEMBER(tree_data)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(root_id)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
  IPC_STRUCT_TRAITS_MEMBER(event_from)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventBundleParams)
  // Zero or more updates to the accessibility tree to apply first.
  IPC_STRUCT_MEMBER(std::vector<content::AXContentTreeUpdate>, updates)

  // Zero or more events to fire after the tree updates have been applied.
  IPC_STRUCT_MEMBER(std::vector<ui::AXEvent>, events)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_LocationChangeParams)
  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location info.
  IPC_STRUCT_MEMBER(ui::AXRelativeBounds, new_location)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_FindInPageResultParams)
  // The find in page request id.
  IPC_STRUCT_MEMBER(int, request_id)

  // The index of the result match.
  IPC_STRUCT_MEMBER(int, match_index)

  // The id of the accessibility object for the start of the match range.
  IPC_STRUCT_MEMBER(int, start_id)

  // The character offset into the text of the start object.
  IPC_STRUCT_MEMBER(int, start_offset)

  // The id of the accessibility object for the end of the match range.
  IPC_STRUCT_MEMBER(int, end_id)

  // The character offset into the text of the end object.
  IPC_STRUCT_MEMBER(int, end_offset)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Relay a request from assistive technology to perform an action,
// such as focusing or clicking on a node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_PerformAction,
                    ui::AXActionData  /* action parameters */)

// Tells the render view that a AccessibilityHostMsg_EventBundle
// message was processed and it can send additional updates. The argument
// must be the same as the ack_token passed to
// AccessibilityHostMsg_EventBundle.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_EventBundle_ACK, int /* ack_token */)

// Tell the renderer to reset and send a new accessibility tree from
// scratch because the browser is out of sync. It passes a sequential
// reset token. This should be rare, and if we need reset the same renderer
// too many times we just kill it. After sending a reset, the browser ignores
// incoming accessibility IPCs until it receives one with the matching reset
// token. Conversely, it ignores IPCs with a reset token if it was not
// expecting a reset.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_Reset,
                    int /* reset token */)

// Kill the renderer because we got a fatal error in the accessibility tree
// and we've already reset too many times.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_FatalError)

// Request a one-time snapshot of the accessibility tree without
// enabling accessibility if it wasn't already enabled. The passed id
// will be returned in the AccessibilityHostMsg_SnapshotResponse message.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_SnapshotTree,
                    int /* callback id */,
                    ui::AXMode /* ax_mode */)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_EventBundle_ACK with the same
// ack_token.
// The |reset_token| parameter is set if this IPC was sent in response
// to a reset request from the browser. When the browser requests a reset,
// it ignores incoming IPCs until it sees one with the correct reset token.
// Any other time, it ignores IPCs with a reset token.
IPC_MESSAGE_ROUTED3(AccessibilityHostMsg_EventBundle,
                    AccessibilityHostMsg_EventBundleParams /* params */,
                    int /* reset_token */,
                    int /* ack_token */)

// Sent to update the browser of the location of accessibility objects.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_LocationChanges,
    std::vector<AccessibilityHostMsg_LocationChangeParams>)

// Sent to update the browser of Find In Page results.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_FindInPageResult,
    AccessibilityHostMsg_FindInPageResultParams)

// Sent when a Find In Page operation is finished and all highlighted results
// are cleared.
IPC_MESSAGE_ROUTED0(AccessibilityHostMsg_FindInPageTermination)

// Sent in response to PerformAction with parameter kHitTest.
IPC_MESSAGE_ROUTED5(AccessibilityHostMsg_ChildFrameHitTestResult,
                    int /* action request id of initial caller */,
                    gfx::Point /* location tested */,
                    int /* routing id of child frame */,
                    int /* browser plugin instance id of child frame */,
                    ax::mojom::Event /* event to fire */)

// Sent in response to AccessibilityMsg_SnapshotTree. The callback id that was
// passed to the request will be returned in |callback_id|, along with
// a standalone snapshot of the accessibility tree.
IPC_MESSAGE_ROUTED2(AccessibilityHostMsg_SnapshotResponse,
                    int /* callback_id */,
                    content::AXContentTreeUpdate)

#endif  // CONTENT_COMMON_ACCESSIBILITY_MESSAGES_H_
