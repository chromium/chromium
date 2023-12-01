// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/170921.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace content {

enum NotificationType {
  NOTIFICATION_CONTENT_START = 0,

  // NavigationController ----------------------------------------------------

  // A new non-pending navigation entry has been created. This will
  // correspond to one NavigationController entry being created (in the case
  // of new navigations) or renavigated to (for back/forward navigations).
  //
  // The source will be the navigation controller doing the commit. The
  // details will be NavigationController::LoadCommittedDetails.
  // DEPRECATED: Use WebContentsObserver::NavigationEntryCommitted()
  // TODO(https://crbug.com/1174760): Remove.
  NOTIFICATION_NAV_ENTRY_COMMITTED = NOTIFICATION_CONTENT_START,

  // WebContents ---------------------------------------------------------------

  // Indicates a RenderWidgetHost has been hidden or restored. The source is
  // the RWH whose visibility changed, the details is a bool set to true if
  // the new state is "visible."
  //
  // DEPRECATED:
  // Use RenderWidgetHostObserver::RenderWidgetHostVisibilityChanged()
  // TODO(https://crbug.com/1174771): Remove.
  NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,

  // Custom notifications used by the embedder should start from here.
  NOTIFICATION_CONTENT_END,
};

}  // namespace content

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/170921.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_TYPES_H_
