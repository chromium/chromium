// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_QUERY_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_QUERY_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

enum class LensEntrypoint;

// Query parameters used to open Lens.
@interface LensQuery : NSObject

// The current identity associated with the browser.
@property(nonatomic, strong) UIImage* image;

// Whether or not the browser is currently in incognito mode.
@property(nonatomic, assign) BOOL isIncognito;

// The entry point from which Lens was entered.
@property(nonatomic, assign) LensEntrypoint entrypoint;

// The serialized viewport state to send to Lens. Can be nil.
@property(nonatomic, strong) NSString* serializedViewportState;

// The webview size.
@property(nonatomic, assign) CGSize webviewSize;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_QUERY_H_
