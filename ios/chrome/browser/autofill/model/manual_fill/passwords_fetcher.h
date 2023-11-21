// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_PASSWORDS_FETCHER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_PASSWORDS_FETCHER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"

@class PasswordFetcher;

namespace password_manager {
class PasswordStoreInterface;
struct PasswordForm;
}  // namespace password_manager

class GURL;

// Protocol to receive the passwords fetched asynchronously.
@protocol PasswordFetcherDelegate

// Saved passwords has been fetched or updated.
- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
              passwords;

@end

@interface PasswordFetcher : NSObject

// The designated initializer. `profilePasswordStore` must not be nil. The
// passwords will be filtered by the passed `origin`, pass an empty GURL to
// avoid filtering.
// TODO(crbug.com/1374242); DCHECK accountPasswordStore too and document the
// precondition after launch.
- (instancetype)
    initWithProfilePasswordStore:
        (scoped_refptr<password_manager::PasswordStoreInterface>)
            profilePasswordStore
            accountPasswordStore:
                (scoped_refptr<password_manager::PasswordStoreInterface>)
                    accountPasswordStore
                        delegate:(id<PasswordFetcherDelegate>)delegate
                             URL:(const GURL&)URL NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MODEL_MANUAL_FILL_PASSWORDS_FETCHER_H_
