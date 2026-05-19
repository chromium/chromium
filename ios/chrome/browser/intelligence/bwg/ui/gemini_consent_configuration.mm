// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"

@implementation GeminiConsentHeader

- (instancetype)initWithIcon:(UIImage*)icon title:(NSAttributedString*)title {
  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
  }
  return self;
}

@end

@implementation GeminiConsentRow

- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                        body:(NSAttributedString*)body {
  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
    _body = [body copy];
    _collapsed = YES;
  }
  return self;
}

@end

@implementation GeminiConsentConfiguration

- (instancetype)initWithRows:(NSArray<GeminiConsentRow*>*)rows
                    footnote:(NSAttributedString*)footnote
                      header:(GeminiConsentHeader*)header
                 collapsible:(BOOL)collapsible {
  self = [super init];
  if (self) {
    _rows = [rows copy];
    _footnote = [footnote copy];
    _header = header;
    _collapsible = collapsible;
  }
  return self;
}

@end
