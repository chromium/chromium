// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/notification/notification_message.h"

namespace remoting {

NotificationMessage::NotificationMessage() = default;
NotificationMessage::NotificationMessage(const NotificationMessage&) = default;
NotificationMessage::NotificationMessage(NotificationMessage&&) = default;
NotificationMessage::~NotificationMessage() = default;
NotificationMessage& NotificationMessage::operator=(
    const NotificationMessage&) = default;
NotificationMessage& NotificationMessage::operator=(NotificationMessage&&) =
    default;

}  // namespace remoting
