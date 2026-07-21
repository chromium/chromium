// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/test/at_memory_app_interface.h"

#import <memory>

#import "ios/chrome/browser/autofill/atmemory/coordinator/scoped_at_memory_search_provider_override.h"
#import "ios/chrome/browser/autofill/atmemory/test/fake_at_memory_search_provider.h"

@implementation AtMemoryAppInterface {
  std::unique_ptr<ScopedAtMemorySearchProviderOverride> _searchProviderOverride;
  FakeAtMemorySearchProvider* _fakeSearchProvider;
}

+ (AtMemoryAppInterface*)sharedInstance {
  static AtMemoryAppInterface* instance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    instance = [[AtMemoryAppInterface alloc] init];
  });
  return instance;
}

+ (void)setUpFakeSearchProviderWithSearchResults:
            (NSArray<NSDictionary*>*)searchResults
                               granularFillItems:
                                   (NSArray<NSDictionary*>*)granularFillItems {
  AtMemoryAppInterface* appInterface = [AtMemoryAppInterface sharedInstance];
  appInterface->_fakeSearchProvider = [[FakeAtMemorySearchProvider alloc] init];
  [appInterface->_fakeSearchProvider setSearchResults:searchResults
                                    granularFillItems:granularFillItems];
  appInterface->_searchProviderOverride =
      ScopedAtMemorySearchProviderOverride::MakeAndArmForTesting(
          appInterface->_fakeSearchProvider);
}

+ (void)tearDownFakeSearchProvider {
  AtMemoryAppInterface* appInterface = [AtMemoryAppInterface sharedInstance];
  appInterface->_searchProviderOverride.reset();
  appInterface->_fakeSearchProvider = nil;
}

@end
