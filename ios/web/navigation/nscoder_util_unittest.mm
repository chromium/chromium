// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/nscoder_util.h"

#import <Foundation/Foundation.h>

#import <array>
#import <string_view>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {
namespace {

using NSCoderStdStringTest = PlatformTest;

constexpr auto kTestStrings = std::to_array<std::string_view>({
    "Arf",
    "",
    "This is working™",
    "古池や蛙飛込む水の音\nふるいけやかわずとびこむみずのおと",
    "ἀγεωμέτρητος μηδεὶς εἰσίτω",
    "Bang!\t\n",
});

TEST_F(NSCoderStdStringTest, encodeDecode) {
  for (std::string_view test_string : kTestStrings) {
    NSKeyedArchiver* archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    nscoder_util::EncodeString(archiver, @"test", test_string);

    NSKeyedUnarchiver* unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:[archiver encodedData]
                                                    error:nil];
    const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

    EXPECT_EQ(decoded, test_string);
  }
}

TEST_F(NSCoderStdStringTest, decodeEmpty) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:[archiver encodedData]
                                                  error:nil];
  const std::string decoded = nscoder_util::DecodeString(unarchiver, @"test");

  EXPECT_EQ(decoded, "");
}

}  // namespace
}  // namespace web
