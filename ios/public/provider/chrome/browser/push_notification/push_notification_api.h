// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_

namespace ios {
namespace provider {

// Initializes the device to handle push notifications
void InitializeConfiguration();

// Registers the device with the server to receive push notifications
void RegisterDevice();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PUSH_NOTIFICATION_PUSH_NOTIFICATION_API_H_
