// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/test_args.h"

@implementation TestArgs

+ (BOOL)shouldStorePageContextLocallyFromTestArgs {
  NSString* value = [TestArgs readTestArgument:kStorePageContextLocally];
  if (value == nil) {
    return NO;
  }
  return YES;
}

+ (NSString*)readInputDirFromTestArgs {
  return [TestArgs readTestArgument:kInputDir];
}

#pragma mark - Helper
+ (NSString*)readTestArgument:(NSString*)argumentName {
  NSArray<NSString*>* arguments = [[NSProcessInfo processInfo] arguments];
  for (NSString* arg in arguments) {
    if ([arg hasPrefix:argumentName]) {
      return [arg substringFromIndex:[argumentName length]];
    }
  }
  return nil;
}

@end
