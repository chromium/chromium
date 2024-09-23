// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "components/keyed_service/core/keyed_service.h"
#import "url/gurl.h"

@class ContentNotificationNAUConfiguration;

// Service responsible for interacting with the content notification service.
class ContentNotificationService : public KeyedService {
 public:
  ContentNotificationService();
  ~ContentNotificationService() override;

  // Returns a destination URL from an unparsed content notification payload.
  virtual GURL GetDestinationUrl(NSDictionary<NSString*, id>* payload) = 0;

  // Returns a payload to be sent for feedback from a content notification
  // payload.
  virtual NSDictionary<NSString*, NSString*>* GetFeedbackPayload(
      NSDictionary<NSString*, id>* payload) = 0;

  // Completion handler indicates the success of the NAU request for a content
  // notification.
  virtual void SendNAUForConfiguration(
      ContentNotificationNAUConfiguration* configuration) = 0;
};

#endif  // IOS_CHROME_BROWSER_CONTENT_NOTIFICATION_MODEL_CONTENT_NOTIFICATION_SERVICE_H_
