// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_SPEC_FETCHER_H_
#define IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_SPEC_FETCHER_H_

#import <Foundation/Foundation.h>

namespace autofill {
class PasswordRequirementsSpec;
}

// Type of the block invoked when spec fetch is complete.
using FetchSpecCompletionBlock =
    void (^)(autofill::PasswordRequirementsSpec spec);

// Can fetch a password specification for the given host.
@interface PasswordSpecFetcher : NSObject

// `host` indicates which spec should be fetched from the service.
// `APIKey` is the API key used to fetch the service.
- (instancetype)initWithHost:(NSString*)host APIKey:(NSString*)APIKey;

// Indicates if the spec has been fetched already.
@property(nonatomic, readonly) BOOL didFetchSpec;

// The spec if ready or an empty one if fetch hasn't happened.
@property(nonatomic, readonly) autofill::PasswordRequirementsSpec spec;

// Fetches the spec and executes `completion` in the main thread. If called
// multiple times only the last completion is executed. An empty spec is
// returned in case there is any error or it is not found.
- (void)fetchSpecWithCompletion:(FetchSpecCompletionBlock)completion;

@end

#endif  // IOS_COMPONENTS_CREDENTIAL_PROVIDER_EXTENSION_PASSWORD_SPEC_FETCHER_H_
