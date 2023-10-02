// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_MANAGER_H_
#define CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_MANAGER_H_

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class CloseListenerHost;

// CloseListenerManager tracks whether its WebContents' focused frame has an
// active CloseWatcher. Updates when a CloseWatcher is added/remove and when
// the focused frame changes. Notifies WebContentsDelegate when the overall
// state changes. This is necessary for embedders (i.e., android) that need to
// know ahead of time whether there is a CloseWatcher that should intercept and
// consume a back gesture.
class CONTENT_EXPORT CloseListenerManager
    : public WebContentsUserData<CloseListenerManager> {
 public:
  ~CloseListenerManager() override;
  CloseListenerManager(const CloseListenerManager&) = delete;
  CloseListenerManager& operator=(const CloseListenerManager&) = delete;

  static void DidChangeFocusedFrame(WebContents* web_contents);

  // `host_being_updated` may not be associated with the currently focused
  // frame, in which case nothing will be updated.
  void MaybeUpdateInterceptStatus(CloseListenerHost* host_being_updated);

 private:
  explicit CloseListenerManager(WebContents* web_contents);
  friend class WebContentsUserData<CloseListenerManager>;

  bool should_intercept_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_CLOSEWATCHER_CLOSE_LISTENER_MANAGER_H_
