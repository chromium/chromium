// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_

#import <Foundation/Foundation.h>
#import "base/feature_list.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

class PromosManager;

// Feature flag that enables version 2 of What's New.
BASE_DECLARE_FEATURE(kWhatsNewIOSM116);

// Returns whether What's New was used in the overflow menu. This is used to
// decide on the location of the What's New entry point in the overflow menu.
bool WasWhatsNewUsed();

// Set that What's New was used in the overflow menu.
void SetWhatsNewUsed(PromosManager* promosManager);

// Set that What's New has been registered in the promo manager.
void setWhatsNewPromoRegistration();

// Returns whether What's New promo should be registered in the promo manager.
// This is used to avoid registering the What's New promo in the promo manager
// more than once.
bool ShouldRegisterWhatsNewPromo();

// Returns whether What's New M116 is enabled.
bool IsWhatsNewM116Enabled();

// Returns a string version of WhatsNewType.
const char* WhatsNewTypeToString(WhatsNewType type);

// Returns a string version of WhatsNewType only for M116 content.
const char* WhatsNewTypeToStringM116(WhatsNewType type);

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_UTIL_H_
