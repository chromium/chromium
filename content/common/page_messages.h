// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_MESSAGES_H_
#define CONTENT_COMMON_PAGE_MESSAGES_H_

#include "content/public/common/common_param_traits.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/screen_info.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/platform/web_text_autosizer_page_info.h"
#include "ui/gfx/geometry/rect.h"

// IPC messages for page-level actions.
// TODO(https://crbug.com/775827): Convert to mojo.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START PageMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED1(PageMsg_VisibilityChanged, content::PageVisibilityState)

// Sent when the history for this page is altered from another process. The
// history list should be reset to |history_length| length, and the offset
// should be reset to |history_offset|.
IPC_MESSAGE_ROUTED2(PageMsg_SetHistoryOffsetAndLength,
                    int /* history_offset */,
                    int /* history_length */)

IPC_MESSAGE_ROUTED1(PageMsg_AudioStateChanged, bool /* is_audio_playing */)

// Sent to all renderers, instructing them to freeze or unfreeze all frames that
// belongs to this page.
IPC_MESSAGE_ROUTED1(PageMsg_SetPageFrozen, bool /* frozen */)

// Sent to all renderers to freeze all frames and dispatch page visibility
// events for bfcache.
IPC_MESSAGE_ROUTED0(PageMsg_PutPageIntoBackForwardCache)

// Sent to all renderers to resume all frames and dispatch page visibility
// events for bfcache.
IPC_MESSAGE_ROUTED1(PageMsg_RestorePageFromBackForwardCache,
                    base::TimeTicks /* navigation_start */)

// Sent to all renderers when the mainframe state required by
// blink::TextAutosizer changes in the main frame's renderer.
IPC_MESSAGE_ROUTED1(PageMsg_UpdateTextAutosizerPageInfoForRemoteMainFrames,
                    blink::WebTextAutosizerPageInfo /* page_info */)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(PageMsg_SetRendererPrefs, blink::mojom::RendererPreferences)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Adding a new message? Stick to the sort order above: first platform
// independent PageMsg, then ifdefs for platform specific PageMsg, then platform
// independent PageHostMsg, then ifdefs for platform specific PageHostMsg.

#endif  // CONTENT_COMMON_PAGE_MESSAGES_H_
