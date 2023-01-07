// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_MESSAGE_H_
#define REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_MESSAGE_H_

#include <string>

namespace remoting {

struct NotificationMessage final {
  NotificationMessage();
  NotificationMessage(const NotificationMessage&);
  NotificationMessage(NotificationMessage&&);
  ~NotificationMessage();

  NotificationMessage& operator=(const NotificationMessage&);
  NotificationMessage& operator=(NotificationMessage&&);

  std::string message_id;
  std::string message_text;
  std::string link_text;
  std::string link_url;
  bool allow_silence;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_NOTIFICATION_NOTIFICATION_MESSAGE_H_
