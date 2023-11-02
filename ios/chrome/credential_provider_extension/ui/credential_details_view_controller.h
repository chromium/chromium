// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/credential_provider_extension/ui/credential_details_consumer.h"

// View controller for credential details. Similar to chrome settings password
// details (i/c/b/u/s/p/password_details_table_view_controller).
@interface CredentialDetailsViewController
    : UITableViewController <CredentialDetailsConsumer>
@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_DETAILS_VIEW_CONTROLLER_H_
