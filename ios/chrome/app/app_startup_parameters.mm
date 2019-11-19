// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#include "base/stl_util.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/payments/payment_request_constants.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppStartupParameters {
  GURL _externalURL;
  GURL _completeURL;
}

@synthesize externalURLParams = _externalURLParams;
@synthesize postOpeningAction = _postOpeningAction;
@synthesize launchInIncognito = _launchInIncognito;
@synthesize completePaymentRequest = _completePaymentRequest;
@synthesize textQuery = _textQuery;

- (const GURL&)externalURL {
  return _externalURL;
}

- (const GURL&)completeURL {
  return _completeURL;
}

- (instancetype)initWithExternalURL:(const GURL&)externalURL
                        completeURL:(const GURL&)completeURL {
  self = [super init];
  if (self) {
    _externalURL = externalURL;
    _completeURL = completeURL;
  }
  return self;
}

- (instancetype)initWithUniversalLink:(const GURL&)universalLink {
  // If a new tab with |_externalURL| needs to be opened after the App
  // was launched as the result of a Universal Link navigation, the only
  // supported possibility at this time is the New Tab Page.
  self = [self initWithExternalURL:GURL(kChromeUINewTabURL)
                       completeURL:GURL(kChromeUINewTabURL)];

  if (self) {
    std::map<std::string, std::string> parameters;
    net::QueryIterator query_iterator(universalLink);
    while (!query_iterator.IsAtEnd()) {
      parameters.insert(std::make_pair(query_iterator.GetKey(),
                                       query_iterator.GetUnescapedValue()));
      query_iterator.Advance();
    }

    // Currently only Payment Request parameters are supported.
    if (base::Contains(parameters, payments::kPaymentRequestIDExternal) &&
        base::Contains(parameters, payments::kPaymentRequestDataExternal)) {
      _externalURLParams = parameters;
      _completePaymentRequest = YES;
    }
  }

  return self;
}

- (NSString*)description {
  NSMutableString* description =
      [NSMutableString stringWithFormat:@"AppStartupParameters: %s",
                                        _externalURL.spec().c_str()];
  if (self.launchInIncognito) {
    [description appendString:@", should launch in incognito"];
  }

  switch (self.postOpeningAction) {
    case START_QR_CODE_SCANNER:
      [description appendString:@", should launch QR scanner"];
      break;
    case START_VOICE_SEARCH:
      [description appendString:@", should launch voice search"];
      break;
    case FOCUS_OMNIBOX:
      [description appendString:@", should focus omnibox"];
      break;
    default:
      break;
  }

  if (self.completePaymentRequest) {
    [description appendString:@", should complete payment request"];
  }

  return description;
}

@end
