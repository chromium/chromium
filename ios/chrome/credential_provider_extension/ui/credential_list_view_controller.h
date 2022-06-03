// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_VIEW_CONTROLLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"

@interface CredentialListViewController
    : UITableViewController <CredentialListConsumer>
@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_VIEW_CONTROLLER_H_
