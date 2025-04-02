// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/credential_store_util.h"

#import "base/barrier_callback.h"
#import "ios/chrome/common/credential_provider/credential_store.h"
namespace credential_store_util {

namespace {

// Wrapper around an NSArray of Credential, so it can be stored in a C++
// collection. This is useful for interacting with Callbacks.
struct CredentialFetchResult {
  NSArray<id<Credential>>* credentials;
};

// Given a list of lists of credentials, returns a single list containing the
// unique elements.
NSArray<id<Credential>>* Merge(
    const std::vector<CredentialFetchResult>& results) {
  // Compute total count so we can reserve space.
  NSUInteger total_count = 0;
  for (const CredentialFetchResult& result : results) {
    total_count += result.credentials.count;
  }

  NSMutableArray<id<Credential>>* merged_result =
      [[NSMutableArray alloc] initWithCapacity:total_count];

  for (const CredentialFetchResult& result : results) {
    [merged_result addObjectsFromArray:result.credentials];
  }
  return merged_result;
}

}  // namespace

void ReadFromMultipleCredentialStoresAsync(
    NSArray<id<CredentialStore>>* stores,
    base::OnceCallback<void(NSArray<id<Credential>>*)> completion) {
  // BarrierCallback collects the output of each individual read, and only
  // once all results are accumulated will it merge the results and finally
  // invoke `completion`. It is thread-safe, but `completion` will be run
  // synchronously on the thread where the last read occurred.
  auto accumulator = base::BarrierCallback<CredentialFetchResult>(
      stores.count, base::BindOnce(&Merge).Then(std::move(completion)));

  for (id<CredentialStore> store in stores) {
    // Create one single-use instance (copy) of `accumulator` per store/block.
    // These instances share state. Use __block to prevent a needless second
    // copy (once on this line, once when the block is created).
    __block base::OnceCallback<void(CredentialFetchResult)>
        accumulatorInstance = accumulator;
    [store
        getCredentialsWithCompletion:^(NSArray<id<Credential>>* credentials) {
          std::move(accumulatorInstance).Run({credentials});
        }];
  }
}

}  // namespace credential_store_util
