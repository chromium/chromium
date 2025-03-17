// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"

@implementation CWVTranslationLanguage

@synthesize languageCode = _languageCode;
@synthesize localizedName = _localizedName;
@synthesize nativeName = _nativeName;

- (instancetype)initWithLanguageCode:(const std::string&)languageCode
                       localizedName:(const std::u16string&)localizedName
                          nativeName:(const std::u16string&)nativeName {
  self = [super init];
  if (self) {
    _languageCode = base::SysUTF8ToNSString(languageCode);
    _localizedName = base::SysUTF16ToNSString(localizedName);
    _nativeName = base::SysUTF16ToNSString(nativeName);
  }
  return self;
}

+ (instancetype)autoLanguageWithLocalizedName:(NSString*)localizedName
                                   nativeName:(NSString*)nativeName {
  std::u16string validLocalizedName = base::SysNSStringToUTF16(localizedName);
  std::u16string validNativeName = base::SysNSStringToUTF16(nativeName);
  return [[CWVTranslationLanguage alloc] initWithLanguageCode:"auto"
                                                localizedName:validLocalizedName
                                                   nativeName:validNativeName];
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[CWVTranslationLanguage class]]) {
    return NO;
  }

  CWVTranslationLanguage* otherLanguage = (CWVTranslationLanguage*)object;
  return [_languageCode isEqualToString:otherLanguage.languageCode];
}

- (NSUInteger)hash {
  return [_languageCode hash];
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"%@ name:%@(%@) code:%@", [super description],
                                 _localizedName, _nativeName, _languageCode];
}

@end
