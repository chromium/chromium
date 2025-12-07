// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_STORE_UTIL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_STORE_UTIL_H_

#import <vector>

#import "base/functional/callback.h"
#import "ios/chrome/common/credential_provider/credential.h"
#import "ios/chrome/common/credential_provider/credential_store.h"

namespace credential_store_util {

// Triggers asynchronous reads of each store in `stores`, merges and dedupes the
// results, and finally invokes `completion`. Note that `completion` is not
// guaranteed to run on the same thread where this method was invoked.
void ReadFromMultipleCredentialStoresAsync(
    NSArray<id<CredentialStore>>* stores,
    base::OnceCallback<void(NSArray<id<Credential>>*)> completion);

}  // namespace credential_store_util

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_STORE_UTIL_H_
