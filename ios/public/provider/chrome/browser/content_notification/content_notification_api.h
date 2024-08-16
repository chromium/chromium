// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTENT_NOTIFICATION_CONTENT_NOTIFICATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTENT_NOTIFICATION_CONTENT_NOTIFICATION_API_H_

#import <memory>

@class ContentNotificationConfiguration;
class ContentNotificationService;

namespace ios {
namespace provider {

// Creates a new instance of ContentNotificationService using `config`.
std::unique_ptr<ContentNotificationService> CreateContentNotificationService(
    ContentNotificationConfiguration* config);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_CONTENT_NOTIFICATION_CONTENT_NOTIFICATION_API_H_
