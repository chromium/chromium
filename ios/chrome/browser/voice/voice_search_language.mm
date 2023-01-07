// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_language.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation VoiceSearchLanguage

@synthesize identifier = _identifier;
@synthesize displayName = _displayName;
@synthesize localizationPreference = _localizationPreference;

- (instancetype)initWithIdentifier:(NSString*)identifier
                       displayName:(NSString*)displayName
            localizationPreference:(NSString*)localizationPreference {
  if ((self = [super init])) {
    _identifier = [identifier copy];
    _displayName = [displayName copy];
    _localizationPreference = [localizationPreference copy];
  }
  return self;
}

@end
