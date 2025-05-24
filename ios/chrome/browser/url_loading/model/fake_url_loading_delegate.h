// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_DELEGATE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@protocol URLLoadingDelegate;

// Fake object that implements URLLoadingDelegate.
@interface FakeURLLoadingDelegate : NSObject <URLLoadingDelegate>
@end

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_FAKE_URL_LOADING_DELEGATE_H_
