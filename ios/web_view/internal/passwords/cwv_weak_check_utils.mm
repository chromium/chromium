// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/weak_check_utility.h"
#import "ios/web_view/internal/passwords/cwv_password_internal.h"
#import "ios/web_view/internal/passwords/cwv_weak_check_utils_internal.h"

@implementation CWVWeakCheckUtils

- (instancetype)init {
  self = [super init];
  return self;
}

+ (BOOL)isPasswordWeak:(NSString*)password {
  std::u16string passwordToCheck = base::SysNSStringToUTF16(password);
  return password_manager::IsWeak(passwordToCheck).value();
}

@end
