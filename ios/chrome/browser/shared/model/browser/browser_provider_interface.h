// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_

#import <Foundation/Foundation.h>

class Browser;
@protocol BrowserProvider;

// A BrowserProviderInterface is an abstraction that exposes the available
// Browsers for the Chrome UI.
@protocol BrowserProviderInterface
// One browser must be designated as being the "current" browser. It is
// usually either the incognito or the main one.
@property(nonatomic, readonly) Browser* currentBrowser;
// The "main" (meaning non-incognito -- the nomenclature is legacy) browser.
@property(nonatomic, readonly) Browser* mainBrowser;
// The incognito Browser.
@property(nonatomic, readonly) Browser* incognitoBrowser;
// YES iff `incognitoBrowser` is already created.
@property(nonatomic, assign, readonly) BOOL hasIncognitoBrowser;

// Deprecated: Use `currentBrowser`, `mainBrowser`, `incognitoBrowser`, and
// `hasIncognitoBrowser` instead.
@property(nonatomic, weak, readonly) id<BrowserProvider> currentBrowserProvider;
@property(nonatomic, readonly) id<BrowserProvider> mainBrowserProvider;
@property(nonatomic, readonly) id<BrowserProvider> incognitoBrowserProvider;
@property(nonatomic, assign, readonly) BOOL hasIncognitoBrowserProvider;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_
