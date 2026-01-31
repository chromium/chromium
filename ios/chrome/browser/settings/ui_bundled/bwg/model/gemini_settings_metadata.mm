// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_metadata.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_context.h"

@implementation GeminiSettingsMetadata

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                      context:(GeminiSettingsContext)context {
  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _context = context;
  }
  return self;
}

@end
