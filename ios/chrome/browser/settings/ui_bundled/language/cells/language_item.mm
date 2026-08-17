// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/language/cells/language_item.h"

#import <optional>

@implementation LanguageItem {
  std::optional<base::i18n::LanguageTag> _languageTag;
}

- (instancetype)initWithType:(NSInteger)type
                 languageTag:(base::i18n::LanguageTag)languageTag {
  self = [super initWithType:type];
  if (self) {
    _languageTag = languageTag;
  }
  return self;
}

- (base::i18n::LanguageTag)languageTag {
  return _languageTag.value_or(base::i18n::GetKnownLanguageTag("und"));
}

- (void)setLanguageTag:(base::i18n::LanguageTag)languageTag {
  _languageTag = languageTag;
}

@end
