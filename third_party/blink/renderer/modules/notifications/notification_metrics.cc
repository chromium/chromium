// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/notifications/notification_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace blink {

void RecordPersistentNotificationDisplayResult(
    PersistentNotificationDisplayResult reason) {
  base::UmaHistogramEnumeration(
      "Notifications.PersistentNotificationDisplayResult", reason);
}

}  // namespace blink
