// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_mediator.h"

namespace password_manager {
class FormFetcher;
}  // namespace password_manager

// Testing category exposing private methods of ManualFillPasswordMediator for
// tests.
@interface ManualFillPasswordMediator (Testing)

// Sets the form fetcher.
- (void)setFormFetcher:
    (std::unique_ptr<password_manager::FormFetcher>)formFetcher;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_PASSWORD_MEDIATOR_TESTING_H_
