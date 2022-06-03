// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#include "ios/chrome/browser/chrome_url_constants.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AppStartupParameters {
  GURL _externalURL;
  GURL _completeURL;
  std::vector<GURL> _URLs;
}

@synthesize externalURLParams = _externalURLParams;
@synthesize postOpeningAction = _postOpeningAction;
@synthesize launchInIncognito = _launchInIncognito;
// TODO(crbug.com/1021752): Remove this stub.
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

- (instancetype)initWithURLs:(const std::vector<GURL>&)URLs {
  if (URLs.empty()) {
    self = [self initWithExternalURL:GURL(kChromeUINewTabURL)
                         completeURL:GURL(kChromeUINewTabURL)];
  } else {
    self = [self initWithExternalURL:URLs.front() completeURL:URLs.front()];
  }

  if (self) {
    _URLs = URLs;
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

- (ApplicationModeForTabOpening)applicationMode {
  return self.launchInIncognito ? ApplicationModeForTabOpening::INCOGNITO
                                : ApplicationModeForTabOpening::NORMAL;
}

@end
