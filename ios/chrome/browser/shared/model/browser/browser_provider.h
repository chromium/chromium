// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_

#import <Foundation/Foundation.h>

class Browser;

// Deprecated: Use `BrowserProviderInterface` properties (`currentBrowser`,
// `mainBrowser`, `incognitoBrowser`) directly instead.
@protocol BrowserProvider

// The active browser. This can never be nullptr once the UI initialization has
// completed, i.e. after the ProfileState has passed ProfileInitStage::kUIReady.
@property(nonatomic, readonly) Browser* browser;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_H_
