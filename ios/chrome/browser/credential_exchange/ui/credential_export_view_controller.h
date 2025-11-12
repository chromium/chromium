// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/credential_exchange/ui/credential_export_consumer.h"

@protocol CredentialExportViewControllerPresentationDelegate;

API_AVAILABLE(ios(26.0))
@interface CredentialExportViewController
    : UITableViewController <CredentialExportConsumer>

// Delegate for handling dismissal of the view.
@property(nonatomic, weak)
    id<CredentialExportViewControllerPresentationDelegate>
        delegate;

- (instancetype)init;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_EXPORT_VIEW_CONTROLLER_H_
