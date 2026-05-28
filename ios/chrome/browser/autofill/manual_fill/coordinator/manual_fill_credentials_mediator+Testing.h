// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_CREDENTIALS_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_CREDENTIALS_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/autofill/manual_fill/coordinator/manual_fill_credentials_mediator.h"

namespace password_manager {
class FormFetcher;
}  // namespace password_manager

// Testing category exposing private methods of ManualFillCredentialsMediator
// for tests.
@interface ManualFillCredentialsMediator (Testing)

// Sets the form fetcher.
- (void)setFormFetcher:
    (std::unique_ptr<password_manager::FormFetcher>)formFetcher;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUAL_FILL_COORDINATOR_MANUAL_FILL_CREDENTIALS_MEDIATOR_TESTING_H_
