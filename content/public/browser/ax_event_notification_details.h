// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_

#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_relative_bounds.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {

// Use this object in conjunction with the
// |WebContentsObserver::AccessibilityEventReceived| method.
struct CONTENT_EXPORT AXEventNotificationDetails {
 public:
  AXEventNotificationDetails();
  AXEventNotificationDetails(const AXEventNotificationDetails& other);
  ~AXEventNotificationDetails();

  // The unique ID of the accessibility tree this event bundle applies to.
  ui::AXTreeID ax_tree_id;

  // Zero or more updates to the accessibility tree to apply first.
  std::vector<ui::AXTreeUpdate> updates;

  // Zero or more events to fire after the tree updates have been applied.
  std::vector<ui::AXEvent> events;
};

// Use this object in conjunction with the
// |WebContentsObserver::AccessibilityLocationChangeReceived| method.
struct CONTENT_EXPORT AXLocationChangeNotificationDetails {
 public:
  AXLocationChangeNotificationDetails();
  AXLocationChangeNotificationDetails(
      const AXLocationChangeNotificationDetails& other);
  ~AXLocationChangeNotificationDetails();

  int id;
  ui::AXTreeID ax_tree_id;
  ui::AXRelativeBounds new_location;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AX_EVENT_NOTIFICATION_DETAILS_H_
