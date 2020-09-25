// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_FRAME_NAVIGATE_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_FRAME_NAVIGATE_PARAMS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message_utils.h"
#include "net/base/host_port_pair.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace content {

// Struct used by WebContentsObserver.
// Note that we derived from IPC::NoParams here, because this struct is used in
// an IPC struct as a parent. Deriving from NoParams allows us to by-pass the
// out of line constructor checks in our clang plugins.
struct CONTENT_EXPORT FrameNavigateParams : public IPC::NoParams {
  FrameNavigateParams();
  FrameNavigateParams(const FrameNavigateParams& other);
  ~FrameNavigateParams();

  // The unique ID of the NavigationEntry for browser-initiated navigations.
  // This value was given to the render process in the HistoryNavigationParams
  // and is being returned by the renderer without it having any idea what it
  // means. If the navigation was renderer-initiated, this value is 0.
  int nav_entry_id;

  // The item sequence number identifies each stop in the session history.  It
  // is unique within the renderer process and makes a best effort to be unique
  // across browser sessions (using a renderer process timestamp).
  int64_t item_sequence_number;

  // The document sequence number is used to identify cross-document navigations
  // in session history.  It increments for each new document and is unique in
  // the same way as |item_sequence_number|.  In-page navigations get a new item
  // sequence number but the same document sequence number.
  int64_t document_sequence_number;

  // URL of the page being loaded.
  GURL url;

  // The base URL for the page's document when the frame was committed. Empty if
  // similar to 'url' above. Note that any base element in the page has not been
  // parsed yet and is therefore not reflected.
  // This is of interest when a MHTML file is loaded, as the base URL has been
  // set to original URL of the site the MHTML represents.
  GURL base_url;

  // URL of the referrer of this load. WebKit generates this based on the
  // source of the event that caused the load.
  content::Referrer referrer;

  // The type of transition.
  ui::PageTransition transition;

  // Lists the redirects that occurred on the way to the current page. This
  // vector has the same format as reported by the WebDataSource in the glue,
  // with the current page being the last one in the list (so even when
  // there's no redirect, there will be one entry in the list.
  std::vector<GURL> redirects;

  // Set to false if we want to update the session history but not update
  // the browser history.  E.g., on unreachable urls.
  bool should_update_history;

  // Contents MIME type of main frame.
  std::string contents_mime_type;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_FRAME_NAVIGATE_PARAMS_H_
