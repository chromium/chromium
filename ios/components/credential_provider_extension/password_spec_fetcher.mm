// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_spec_fetcher.h"

#import <string_view>

#import "base/base64.h"
#import "components/autofill/core/browser/proto/password_requirements.pb.h"

using autofill::DomainSuggestions;
using autofill::PasswordRequirementsSpec;

namespace {

// URL of the fetching endpoint.
NSString* const kPasswordSpecURL =
    @"https://content-autofill.googleapis.com/v1/domainSuggestions/";
// Header field name for the API key.
NSString* const kApiKeyHeaderField = @"X-Goog-Api-Key";
// Encoding requested from the server.
NSString* const kEncodeKeyValue = @"base64";
// Header field name for the encoding.
NSString* const kEncodeKeyHeaderField = @"X-Goog-Encode-Response-If-Executable";
// Query parameter name to for the type of response.
NSString* const kAltQueryName = @"alt";
// Query parameter value for a bits response (compared to a JSON response).
NSString* const kAltQueryValue = @"proto";
// Timeout for the spec fetching request.
const NSTimeInterval kPasswordSpecTimeout = 10;

}

@interface PasswordSpecFetcher ()

// Host that identifies the spec to be fetched.
@property(nonatomic, copy) NSString* host;

// API key to query the spec.
@property(nonatomic, copy) NSString* APIKey;

// Data task for fetching the spec.
@property(nonatomic, copy) NSURLSessionDataTask* task;

// Completion to be called once there is a response.
@property(nonatomic, copy) FetchSpecCompletionBlock completion;

// The spec if ready or an empty one if fetch hasn't happened.
@property(nonatomic, readwrite) PasswordRequirementsSpec spec;

@end

@implementation PasswordSpecFetcher

- (instancetype)initWithHost:(NSString*)host APIKey:(NSString*)APIKey {
  self = [super init];
  if (self) {
    // Replace a nil host with the empty string.
    if (!host) {
      host = @"";
    }
    _host = [host stringByAddingPercentEncodingWithAllowedCharacters:
                      NSCharacterSet.URLQueryAllowedCharacterSet];
    _APIKey = APIKey;
  }
  return self;
}

- (BOOL)didFetchSpec {
  return self.task.state == NSURLSessionTaskStateCompleted;
}

- (void)fetchSpecWithCompletion:(FetchSpecCompletionBlock)completion {
  self.completion = completion;

  if (self.task) {
    return;
  }
  NSString* finalURL = [kPasswordSpecURL stringByAppendingString:self.host];
  NSURLComponents* URLComponents =
      [NSURLComponents componentsWithString:finalURL];
  NSURLQueryItem* queryAltItem =
      [NSURLQueryItem queryItemWithName:kAltQueryName value:kAltQueryValue];
  URLComponents.queryItems = @[ queryAltItem ];
  NSMutableURLRequest* request =
      [NSMutableURLRequest requestWithURL:URLComponents.URL
                              cachePolicy:NSURLRequestUseProtocolCachePolicy
                          timeoutInterval:kPasswordSpecTimeout];
  [request setValue:self.APIKey forHTTPHeaderField:kApiKeyHeaderField];
  [request setValue:kEncodeKeyValue forHTTPHeaderField:kEncodeKeyHeaderField];

  __weak __typeof__(self) weakSelf = self;
  NSURLSession* session = [NSURLSession sharedSession];
  self.task =
      [session dataTaskWithRequest:request
                 completionHandler:^(NSData* data, NSURLResponse* response,
                                     NSError* error) {
                   [weakSelf onReceivedData:data response:response error:error];
                 }];
  [self.task resume];
}

- (void)onReceivedData:(NSData*)data
              response:(NSURLResponse*)response
                 error:(NSError*)error {
  // Return early if there is an error.
  if (error) {
    [self executeCompletion];
    return;
  }

  // Parse the proto and execute completion.
  std::string decoded;
  const std::string_view encoded_bytes(static_cast<const char*>([data bytes]),
                                       [data length]);
  if (base::Base64Decode(encoded_bytes, &decoded)) {
    DomainSuggestions suggestions;
    suggestions.ParseFromString(decoded);
    if (suggestions.has_password_requirements()) {
      self.spec = suggestions.password_requirements();
    }
  }
  [self executeCompletion];
}

// Executes the completion if present. And releases it after.
- (void)executeCompletion {
  if (self.completion) {
    auto completion = self.completion;
    self.completion = nil;
    dispatch_async(dispatch_get_main_queue(), ^{
      completion(self.spec);
    });
  }
}

@end
