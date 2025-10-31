// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/credential_exchange/ui/credential_import_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

// Main screen for credential import, displaying different stages of import.
@interface CredentialImportViewController
    : PromoStyleViewController <CredentialImportConsumer>

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_UI_CREDENTIAL_IMPORT_VIEW_CONTROLLER_H_
