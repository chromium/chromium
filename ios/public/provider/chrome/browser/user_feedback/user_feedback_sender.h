// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_SENDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_SENDER_H_

// Indicates where is this feedback coming from.
enum class UserFeedbackSender {
  // Sent from tools overflow menu.
  ToolsMenu = 0,
  // Sent from a Sad Tab.
  SadTab,
  // Sent from Discover Feed.
  Feed,
  // Sent from a keyboard command.
  KeyCommand,
  // Sent from Mini Map.
  MiniMap,
  // Sent from Parcel Tracking.
  ParcelTracking,
  // Sent from Unit Conversion.
  UnitConversion,
  // Sent from a content notification.
  ContentNotification,
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_FEEDBACK_USER_FEEDBACK_SENDER_H_
