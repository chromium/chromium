// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/swapped_out_messages.h"

#include "content/common/accessibility_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_client.h"

namespace content {

bool SwappedOutMessages::CanSendWhileSwappedOut(const IPC::Message* msg) {
  // We filter out most IPC messages when swapped out.  However, some are
  // important (e.g., ACKs) for keeping the browser and renderer state
  // consistent in case we later return to the same renderer.
  switch (msg->type()) {
    // Handled by RenderViewHost.
    case FrameHostMsg_RenderProcessGone::ID:
    case ViewHostMsg_ClosePage_ACK::ID:
    case ViewHostMsg_Focus::ID:
    case ViewHostMsg_ShowFullscreenWidget::ID:
    case ViewHostMsg_ShowWidget::ID:
    case ViewHostMsg_UpdateTargetURL::ID:
    // Send page scale factor reset notification upon cross-process navigations.
    case ViewHostMsg_PageScaleFactorChanged::ID:
    // Allow history.back() in OOPIFs - https://crbug.com/845923.
    case ViewHostMsg_GoToEntryAtOffset::ID:
    // Allow cross-process JavaScript calls.
    case WidgetHostMsg_RouteCloseEvent::ID:
      return true;
    default:
      break;
  }

  // Check with the embedder as well.
  ContentClient* client = GetContentClient();
  return client->CanSendWhileSwappedOut(msg);
}

bool SwappedOutMessages::CanHandleWhileSwappedOut(
    const IPC::Message& msg) {
  // Any message the renderer is allowed to send while swapped out should
  // be handled by the browser.
  if (CanSendWhileSwappedOut(&msg))
    return true;

  // We drop most other messages that arrive from a swapped out renderer.
  // However, some are important (e.g., ACKs) for keeping the browser and
  // renderer state consistent in case we later return to the renderer.
  // Note that synchronous messages that are not handled will receive an
  // error reply instead, to avoid leaving the renderer in a stuck state.
  switch (msg.type()) {
    // We allow closing even if we are in the process of swapping out.
    case WidgetHostMsg_Close::ID:
    // Sends an ACK.
    case WidgetHostMsg_RequestSetBounds::ID:
    // Sends an ACK.
    case AccessibilityHostMsg_EventBundle::ID:
      return true;
    default:
      break;
  }

  return false;
}

}  // namespace content
