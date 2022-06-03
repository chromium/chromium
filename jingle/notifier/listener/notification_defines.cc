// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/notification_defines.h"

#include <cstddef>

#include "base/json/string_escape.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace notifier {

Subscription::Subscription() {}
Subscription::~Subscription() {}

bool Subscription::Equals(const Subscription& other) const {
  return channel == other.channel && from == other.from;
}

namespace {

template <typename T>
bool ListsEqual(const T& t1, const T& t2) {
  if (t1.size() != t2.size()) {
    return false;
  }
  for (size_t i = 0; i < t1.size(); ++i) {
    if (!t1[i].Equals(t2[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool SubscriptionListsEqual(const SubscriptionList& subscriptions1,
                            const SubscriptionList& subscriptions2) {
  return ListsEqual(subscriptions1, subscriptions2);
}

Recipient::Recipient() {}
Recipient::~Recipient() {}

bool Recipient::Equals(const Recipient& other) const {
  return to == other.to && user_specific_data == other.user_specific_data;
}

bool RecipientListsEqual(const RecipientList& recipients1,
                         const RecipientList& recipients2) {
  return ListsEqual(recipients1, recipients2);
}

Notification::Notification() {}
Notification::Notification(const Notification& other) = default;
Notification::~Notification() {}

bool Notification::Equals(const Notification& other) const {
  return
      channel == other.channel &&
      data == other.data &&
      RecipientListsEqual(recipients, other.recipients);
}

std::string Notification::ToString() const {
  // |channel| or |data| could hold binary data, so convert all non-ASCII
  // characters to escape sequences.
  const std::string& printable_channel =
      base::EscapeBytesAsInvalidJSONString(channel, true /* put_in_quotes */);
  const std::string& printable_data =
      base::EscapeBytesAsInvalidJSONString(data, true /* put_in_quotes */);
  return
      "{ channel: " + printable_channel + ", data: " + printable_data + " }";
}

}  // namespace notifier
