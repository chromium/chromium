// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/notification_method.h"

#include "base/logging.h"

namespace notifier {

const NotificationMethod kDefaultNotificationMethod = NOTIFICATION_SERVER;

std::string NotificationMethodToString(
    NotificationMethod notification_method) {
  switch (notification_method) {
    case NOTIFICATION_P2P:
      return "NOTIFICATION_P2P";
    case NOTIFICATION_SERVER:
      return "NOTIFICATION_SERVER";
    default:
      LOG(WARNING) << "Unknown value for notification method: "
                   << notification_method;
      break;
  }
  return "<unknown notification method>";
}

NotificationMethod StringToNotificationMethod(const std::string& str) {
  if (str == "p2p") {
    return NOTIFICATION_P2P;
  } else if (str == "server") {
    return NOTIFICATION_SERVER;
  }
  LOG(WARNING) << "Unknown notification method \"" << str
               << "\"; using method "
               << NotificationMethodToString(kDefaultNotificationMethod);
  return kDefaultNotificationMethod;
}

}  // namespace notifier
