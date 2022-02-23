// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_

#include "third_party/blink/public/common/common_export.h"

namespace blink {

// The type of pageshow events. These values must be synced with
// EventPageShowPersisted in enum.xml. Do not renumber these values.
enum class EventPageShowPersisted {
  // The pageshow event is recorded without persisted flag in renderer.
  kNoInRenderer = 0,

  // The pageshow event is recorded with persisted flag in renderer.
  kYesInRenderer = 1,

  // Browser triggers a pageshow event with persisted flag. The recorded count
  // should be almost the same as kYesInRenderer. See crbug.com/1234634.
  kYesInBrowser = 2,

  // TODO(https://crbug.com/1234634): Below here is for tracking down the
  // mismatch. Remove these when debugging is complete.

  // Browser triggers a pageshow event with persisted flag, counted in the
  // back-forward cache code. The recorded count should be almost the same as
  // kYesInRenderer. See crbug.com/1234634.
  kYesInBrowser_BackForwardCache_WillCommitNavigationToCachedEntry = 3,
  kYesInBrowser_BackForwardCache_RestoreEntry_Attempt = 4,
  kYesInBrowser_BackForwardCache_RestoreEntry_Succeed = 5,
  kYesInBrowser_RenderFrameHostManager_CommitPending = 6,

  // Renderer has a received a state with
  // `should_dispatch_pageshow_for_debugging` set to true.
  kBrowserYesInRenderer = 7,

  // As kBrowserYesInRenderer but we have confirmed that we have a Page object.
  kBrowserYesInRendererWithPage = 8,

  // Browser received an ACK after sending state with
  // `should_dispatch_pageshow_for_debugging` set to true.
  kYesInBrowserAck = 9,

  // Mojo interface was not connected when the IPC was being sent.
  kYesInBrowserDisconnected = 10,

  // RenderView was not live when the IPC was being sent.
  kYesInBrowserRenderViewNotLive = 11,

  // There is not kNoInBrowser as we don't have to compare the counts of
  // pageshow events without persisted between browser and renderer so far.

  kMaxValue = kYesInBrowserRenderViewNotLive,
};

BLINK_COMMON_EXPORT void RecordUMAEventPageShowPersisted(
    EventPageShowPersisted value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_EVENT_PAGE_SHOW_PERSISTED_H_
