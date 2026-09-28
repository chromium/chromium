// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_AI_MODE_BUTTON_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_AI_MODE_BUTTON_SERVICE_IOS_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

class TemplateURLService;

// Service providing properties of the AI Mode NTP button on iOS.
@interface AIModeButtonServiceIOS : NSObject

// The title for the AI Mode button.
@property(nonatomic, readonly) NSString* title;
// The icon for the AI Mode button.
@property(nonatomic, readonly) UIImage* icon;
// The URL to navigate to when the AI Mode button is tapped.
@property(nonatomic, readonly) GURL URL;

// Initializes the service with the given `templateURLService`.
- (instancetype)initWithTemplateURLService:
    (TemplateURLService*)templateURLService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end
#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_AI_MODE_BUTTON_SERVICE_IOS_H_
