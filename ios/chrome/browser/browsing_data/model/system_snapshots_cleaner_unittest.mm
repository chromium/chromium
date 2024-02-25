// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/system_snapshots_cleaner.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

bool RegexMatchesOneSnapshotPath(NSString* regexString) {
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:regexString
                                                options:0
                                                  error:nullptr];
  std::vector<base::FilePath> snapshotsPaths;
  GetSnapshotsPaths(&snapshotsPaths);
  int numberOfMatches = 0;
  for (const base::FilePath& path : snapshotsPaths) {
    NSString* string =
        [NSString stringWithCString:path.value().c_str()
                           encoding:[NSString defaultCStringEncoding]];
    if ([regex numberOfMatchesInString:string
                               options:0
                                 range:NSMakeRange(0, [string length])]) {
      numberOfMatches++;
    }
  }
  return numberOfMatches == 1;
}

using SnapshotsUtilTest = PlatformTest;

TEST_F(SnapshotsUtilTest, TestSnapshotList) {
  NSString* scaleModifier = @"";
  CGFloat scale = [UIScreen mainScreen].scale;
  if (scale > 1) {
    scaleModifier = [NSString stringWithFormat:@"@%.0fx", scale];
  }
  NSString* path = base::SysUTF8ToNSString(base::apple::BaseBundleID());
  NSString* filename = @"UIApplicationAutomaticSnapshotDefault-LandscapeRight";
  NSString* regex = [NSString
      stringWithFormat:@".*/%@/%@%@.png$", path, filename, scaleModifier];
  EXPECT_FALSE(RegexMatchesOneSnapshotPath(@".*"));
  EXPECT_FALSE(RegexMatchesOneSnapshotPath(@"foo"));
  EXPECT_TRUE(RegexMatchesOneSnapshotPath(@".*LandscapeRight.*"));
  EXPECT_TRUE(RegexMatchesOneSnapshotPath(regex));
}

}  // anonymous namespace
