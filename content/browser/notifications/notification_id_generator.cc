// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_id_generator.h"

#include <sstream>
#include <string_view>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kNotificationTagSeparator = '#';
const char kPersistentNotificationPrefix = 'p';
const char kNonPersistentNotificationPrefix = 'n';
const char kNotificationShownByBrowserFlag = 'b';

}  // namespace

// static
bool NotificationIdGenerator::IsPersistentNotification(
    const std::string_view& notification_id) {
  return notification_id.length() > 0 &&
         notification_id.front() == kPersistentNotificationPrefix;
}

// static
bool NotificationIdGenerator::IsNonPersistentNotification(
    const std::string_view& notification_id) {
  return notification_id.length() > 0 &&
         notification_id.front() == kNonPersistentNotificationPrefix;
}

// Notification Id is of the following format:
// p[b]#<origin>#[1|0][<developer_tag>|persistent_notification_id]
std::string NotificationIdGenerator::GenerateForPersistentNotification(
    const GURL& origin,
    const std::string& tag,
    bool is_shown_by_browser,
    int64_t persistent_notification_id) const {
  DCHECK(origin.is_valid());
  DCHECK_EQ(origin, origin.DeprecatedGetOriginAsURL());

  std::stringstream stream;

  stream << kPersistentNotificationPrefix;

  if (is_shown_by_browser)
    stream << kNotificationShownByBrowserFlag;

  stream << kNotificationTagSeparator;
  stream << origin;
  stream << kNotificationTagSeparator;

  stream << base::NumberToString(!tag.empty());
  if (tag.size())
    stream << tag;
  else
    stream << base::NumberToString(persistent_notification_id);

  return stream.str();
}

// Notification Id is of the following format:
// n#<origin>#<token>
std::string NotificationIdGenerator::GenerateForNonPersistentNotification(
    const url::Origin& origin,
    const std::string& token) const {
  DCHECK(!origin.opaque());
  DCHECK(!token.empty());
  return base::StringPrintf(
      "%c%c%s%c%s", kNonPersistentNotificationPrefix, kNotificationTagSeparator,
      origin.Serialize().c_str(), kNotificationTagSeparator, token.c_str());
}

}  // namespace content
