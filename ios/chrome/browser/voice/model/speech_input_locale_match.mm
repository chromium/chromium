// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/speech_input_locale_match.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"

namespace {

// Keys used in SpeechInputLocaleMatches.plist:
NSString* const kMatchedLocaleKey = @"Locale";
NSString* const kMatchingLocalesKey = @"MatchingLocales";
NSString* const kMatchingLanguagesKey = @"MatchingLanguages";

}  // namespace

@implementation SpeechInputLocaleMatch

@synthesize matchedLocale = _matchedLocale;
@synthesize matchingLocales = _matchingLocales;
@synthesize matchingLanguages = _matchingLanguages;

- (instancetype)initWithMatchedLocale:(NSString*)matchedLocale
                      matchingLocales:(NSArray<NSString*>*)matchingLocales
                    matchingLanguages:(NSArray<NSString*>*)matchingLanguages {
  if ((self = [super init])) {
    _matchedLocale = [matchedLocale copy];
    _matchingLocales = [matchingLocales copy];
    _matchingLanguages = [matchingLanguages copy];
  }
  return self;
}

- (instancetype)initWithDictionary:(NSDictionary*)dict {
  NSString* matchedLocale =
      base::apple::ObjCCastStrict<NSString>(dict[kMatchedLocaleKey]);

  NSArray* matchingLocales =
      base::apple::ObjCCastStrict<NSArray>(dict[kMatchingLocalesKey]);
  for (id machingLocale : matchingLocales) {
    DCHECK([machingLocale isKindOfClass:[NSString class]]);
  }

  NSArray* machingLanguages =
      base::apple::ObjCCastStrict<NSArray>(dict[kMatchingLanguagesKey]);
  for (id machingLanguage : machingLanguages) {
    DCHECK([machingLanguage isKindOfClass:[NSString class]]);
  }

  return [self initWithMatchedLocale:matchedLocale
                     matchingLocales:matchingLocales
                   matchingLanguages:machingLanguages];
}

@end

NSArray<SpeechInputLocaleMatch*>* LoadSpeechInputLocaleMatches() {
  NSString* path = [base::apple::FrameworkBundle()
      pathForResource:@"SpeechInputLocaleMatches"
               ofType:@"plist"
          inDirectory:@"gm-config/ANY"];

  NSMutableArray<SpeechInputLocaleMatch*>* matches = [NSMutableArray array];
  for (id item in [NSArray arrayWithContentsOfFile:path]) {
    NSDictionary* dict = base::apple::ObjCCastStrict<NSDictionary>(item);
    SpeechInputLocaleMatch* match =
        [[SpeechInputLocaleMatch alloc] initWithDictionary:dict];
    [matches addObject:match];
  }
  return matches;
}
