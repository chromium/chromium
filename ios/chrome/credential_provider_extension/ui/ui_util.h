// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_UI_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_UI_UTIL_H_

#import <Foundation/Foundation.h>

@class ASCredentialServiceIdentifier;

// Prompt for the top of the navigation controller telling what the current site
// is.
NSString* PromptForServiceIdentifiers(
    NSArray<ASCredentialServiceIdentifier*>* serviceIdentfiers);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_UI_UTIL_H_
