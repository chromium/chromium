// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_event_notification_details.h"

#include "ui/accessibility/ax_event.h"

namespace content {

AXEventNotificationDetails::AXEventNotificationDetails()
    : ax_tree_id(ui::AXTreeIDUnknown()) {}

AXEventNotificationDetails::AXEventNotificationDetails(
    const AXEventNotificationDetails& other) = default;

AXEventNotificationDetails::~AXEventNotificationDetails() {}

AXLocationChangeNotificationDetails::AXLocationChangeNotificationDetails()
    : id(-1), ax_tree_id(ui::AXTreeIDUnknown()) {}

AXLocationChangeNotificationDetails::AXLocationChangeNotificationDetails(
    const AXLocationChangeNotificationDetails& other) = default;

AXLocationChangeNotificationDetails::~AXLocationChangeNotificationDetails() {}


}  // namespace content
