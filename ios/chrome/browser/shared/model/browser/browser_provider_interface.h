// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_

#import <Foundation/Foundation.h>

@protocol BrowserProvider;

// A BrowserProviderInterface is an abstraction that exposes the available
// BrowserProvider for the Chrome UI.
@protocol BrowserProviderInterface
// One provider must be designated as being the "current" provider. It is
// usually either the incognito or the main one.
@property(nonatomic, weak, readonly) id<BrowserProvider> currentBrowserProvider;
// The provider for the "main" (meaning non-incognito -- the nomenclature is
// legacy) browser.
@property(nonatomic, readonly) id<BrowserProvider> mainBrowserProvider;
// The provider for the incognito Browser.
@property(nonatomic, readonly) id<BrowserProvider> incognitoBrowserProvider;
// YES iff `incognitoBrowserProvider` is already created.
@property(nonatomic, assign, readonly) BOOL hasIncognitoBrowserProvider;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_PROVIDER_INTERFACE_H_
