// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREATE_NAVIGATION_ITEM_TITLE_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREATE_NAVIGATION_ITEM_TITLE_VIEW_H_

#import <UIKit/UIKit.h>

namespace credential_provider_extension {

// Creates a branded title view for the keychain navigation controller.
UIView* CreateNavigationItemTitleView(UIFont* font);

}  // namespace credential_provider_extension

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREATE_NAVIGATION_ITEM_TITLE_VIEW_H_
