// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_startup_parameters.h"

#import "base/feature_list.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "net/base/mac/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

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
@synthesize applicationMode = _applicationMode;
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
                        completeURL:(const GURL&)completeURL
                    applicationMode:(ApplicationModeForTabOpening)mode {
  self = [super init];
  if (self) {
    _externalURL = externalURL;
    _completeURL = completeURL;
    _applicationMode = mode;
  }
  return self;
}

- (instancetype)initWithURLs:(const std::vector<GURL>&)URLs
             applicationMode:(ApplicationModeForTabOpening)mode {
  if (URLs.empty()) {
    self = [self initWithExternalURL:GURL(kChromeUINewTabURL)
                         completeURL:GURL(kChromeUINewTabURL)
                     applicationMode:mode];
  } else {
    self = [self initWithExternalURL:URLs.front()
                         completeURL:URLs.front()
                     applicationMode:mode];
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
  if (self.applicationMode == ApplicationModeForTabOpening::INCOGNITO) {
    [description appendString:@", should launch in incognito"];
  }

  switch (self.postOpeningAction) {
    case START_QR_CODE_SCANNER:
      [description appendString:@", should launch QR scanner"];
      break;
    case START_LENS:
      [description appendString:@", should launch Lens"];
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

- (void)setPostOpeningAction:(TabOpeningPostOpeningAction)action {
  // Only NO_ACTION or SHOW_DEFAULT_BROWSER_SETTINGS are allowed on non NTP.
  DCHECK(action == NO_ACTION || action == SHOW_DEFAULT_BROWSER_SETTINGS ||
         _externalURL == GURL(kChromeUINewTabURL));
  _postOpeningAction = action;
}

@end
