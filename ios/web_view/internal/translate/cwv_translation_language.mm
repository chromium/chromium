// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"

#import <string>

#import "base/strings/sys_string_conversions.h"

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
