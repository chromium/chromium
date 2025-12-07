// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/shared/public/commands/generate_qr_code_command.h"

@implementation GenerateQRCodeCommand {
  GURL _URL;
}

- (instancetype)initWithURL:(const GURL&)URL title:(NSString*)title {
  if ((self = [super init])) {
    _URL = URL;
    _title = title;
  }
  return self;
}
@end
