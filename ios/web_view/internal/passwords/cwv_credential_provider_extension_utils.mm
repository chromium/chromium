// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_credential_provider_extension_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#import "ios/components/credential_provider_extension/password_spec_fetcher.h"
#import "ios/components/credential_provider_extension/password_util.h"

@implementation CWVCredentialProviderExtensionUtils

+ (nullable NSString*)retrievePasswordForKeychainIdentifier:
    (NSString*)keychainIdentifier {
  return credential_provider_extension::PasswordWithKeychainIdentifier(
      keychainIdentifier);
}

+ (BOOL)storePasswordForKeychainIdentifier:(NSString*)keychainIdentifier
                                  password:(NSString*)password {
  return credential_provider_extension::StorePasswordInKeychain(
      password, keychainIdentifier);
}

+ (void)generateRandomPasswordForHost:(NSString*)host
                               APIKey:(NSString*)APIKey
                    completionHandler:(void (^)(NSString* generatedPassword))
                                          completionHandler {
  __block PasswordSpecFetcher* fetcher =
      [[PasswordSpecFetcher alloc] initWithHost:host APIKey:APIKey];
  [fetcher fetchSpecWithCompletion:^(autofill::PasswordRequirementsSpec spec) {
    std::u16string password = autofill::GeneratePassword(spec);
    completionHandler(base::SysUTF16ToNSString(password));

    // This guarantees that |fetcher| will not be deallocated until after this
    // block is called.
    fetcher = nil;
  }];
}

@end
