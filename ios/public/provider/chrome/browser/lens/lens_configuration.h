// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class PrefService;
enum class LensEntrypoint;
@protocol SingleSignOnService;
@protocol SystemIdentity;

// Configuration object used by the LensProvider.
@interface LensConfiguration : NSObject

// The current identity associated with the browser.
@property(nonatomic, strong) id<SystemIdentity> identity;

// Whether or not the browser is currently in incognito mode.
@property(nonatomic, assign) BOOL isIncognito;

// The SingleSignOnService instance to use by LensProvider.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

// PrefService used by Lens.
@property(nonatomic, assign) PrefService* localState;

// The entry point from which Lens was entered.
@property(nonatomic, assign) LensEntrypoint entrypoint;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_LENS_LENS_CONFIGURATION_H_
