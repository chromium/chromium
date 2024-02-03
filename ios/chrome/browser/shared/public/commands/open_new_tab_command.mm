// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"

#import "ios/web/public/navigation/referrer.h"
#import "url/gurl.h"

@implementation OpenNewTabCommand {
  GURL _URL;
  GURL _virtualURL;
  web::Referrer _referrer;
}

@synthesize inIncognito = _inIncognito;
@synthesize inBackground = _inBackground;
@synthesize originPoint = _originPoint;
@synthesize fromChrome = _fromChrome;
@synthesize appendTo = _appendTo;
@synthesize userInitiated = _userInitiated;
@synthesize shouldFocusOmnibox = _shouldFocusOmnibox;

- (instancetype)initInIncognito:(BOOL)inIncognito
                   inBackground:(BOOL)inBackground {
  if ((self = [super init])) {
    _inIncognito = inIncognito;
    _inBackground = inBackground;
    _userInitiated = YES;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                 virtualURL:(const GURL&)virtualURL
                   referrer:(const web::Referrer&)referrer
                inIncognito:(BOOL)inIncognito
               inBackground:(BOOL)inBackground
                   appendTo:(OpenPosition)append {
  if ((self = [self initInIncognito:inIncognito inBackground:inBackground])) {
    _URL = URL;
    _virtualURL = virtualURL;
    _referrer = referrer;
    _appendTo = append;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                   referrer:(const web::Referrer&)referrer
                inIncognito:(BOOL)inIncognito
               inBackground:(BOOL)inBackground
                   appendTo:(OpenPosition)append {
  return [self initWithURL:URL
                virtualURL:GURL()
                  referrer:referrer
               inIncognito:inIncognito
              inBackground:inBackground
                  appendTo:append];
}

- (instancetype)initFromChrome:(const GURL&)URL inIncognito:(BOOL)inIncognito {
  self = [self initWithURL:URL
                  referrer:web::Referrer()
               inIncognito:inIncognito
              inBackground:NO
                  appendTo:OpenPosition::kLastTab];
  if (self) {
    _fromChrome = YES;
  }
  return self;
}

+ (instancetype)commandWithIncognito:(BOOL)incognito
                         originPoint:(CGPoint)origin {
  OpenNewTabCommand* command = [[self alloc] initInIncognito:incognito
                                                inBackground:NO];
  command.originPoint = origin;
  return command;
}

+ (instancetype)commandWithIncognito:(BOOL)incognito {
  return [[self alloc] initInIncognito:incognito inBackground:NO];
}

+ (instancetype)command {
  return [self commandWithIncognito:NO];
}

+ (instancetype)incognitoTabCommand {
  return [self commandWithIncognito:YES];
}

+ (instancetype)commandWithURLFromChrome:(const GURL&)URL
                             inIncognito:(BOOL)inIncognito {
  return [[self alloc] initFromChrome:URL inIncognito:inIncognito];
}

+ (instancetype)commandWithURLFromChrome:(const GURL&)URL {
  return [[self alloc] initFromChrome:URL inIncognito:NO];
}

- (const GURL&)URL {
  return _URL;
}

- (const GURL&)virtualURL {
  return _virtualURL;
}

- (const web::Referrer&)referrer {
  return _referrer;
}

@end
