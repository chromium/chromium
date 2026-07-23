// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/test_expectations.h"

#import <vector>

#import "build/build_config.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

class TestExpectationsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [TestExpectations resetForTesting];
  }
  void TearDown() override {
    [TestExpectations resetForTesting];
    PlatformTest::TearDown();
  }
};

TEST_F(TestExpectationsTest, ParseSimpleExpectation) {
  NSString* content = @"MyTestCase/testMethod [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod"];
  EXPECT_TRUE(entry != nil);
  EXPECT_NSEQ(@"Expected failure", entry.bug);
  EXPECT_EQ(TestExpectationTypeFailure, entry.type);

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"otherMethod"] == nil);
}

TEST_F(TestExpectationsTest, ParseWithBugIdentifier) {
  NSString* content = @"crbug.com/12345 MyTestCase/testMethod [ Failure ]\n"
                      @"b/98765 MyTestCase/testOtherMethod [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod"];
  EXPECT_TRUE(entry != nil);
  EXPECT_NSEQ(@"crbug.com/12345", entry.bug);
  EXPECT_EQ(TestExpectationTypeFailure, entry.type);

  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testOtherMethod"];
  EXPECT_TRUE(entry != nil);
  EXPECT_NSEQ(@"b/98765", entry.bug);
  EXPECT_EQ(TestExpectationTypeFailure, entry.type);
}

TEST_F(TestExpectationsTest, ParseWithMatchingTags) {
  NSString* content = @"[ iOS26 Simulator ] MyTestCase/testMethod [ Failure ]\n"
                      @"[ iOS18 ] MyTestCase/testOtherMethod [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS", @"iOS26",
                                                            @"Simulator", nil]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod"] != nil);

  // iOS18 tag doesn't match active tags (iOS26), so the expectation is ignored.
  EXPECT_TRUE([expectations
                  expectationEntryForTestCase:@"MyTestCase"
                                   methodName:@"testOtherMethod"] == nil);
}

TEST_F(TestExpectationsTest, ClassLevelExpectation) {
  NSString* content = @"MyTestCase [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod"] != nil);
  EXPECT_TRUE([expectations
                  expectationEntryForTestCase:@"MyTestCase"
                                   methodName:@"anotherMethod"] != nil);
}

TEST_F(TestExpectationsTest, Normalization) {
  NSString* content = @"MyTestCase.testMethod [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod"] != nil);
}

TEST_F(TestExpectationsTest, CommentsAndBlankLines) {
  NSString* content =
      @"# This is a comment\n"
      @"\n"
      @"crbug.com/123 MyTestCase/testMethod [ Failure ] # inline comment\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod"];
  EXPECT_TRUE(entry != nil);
  EXPECT_NSEQ(@"crbug.com/123", entry.bug);
}

TEST_F(TestExpectationsTest, CaseInsensitiveMatching) {
  NSString* content =
      @"[ ios26 simulator ] MyTestCase/testMethod1 [ failure ]\n"
      @"[ build-23f5067a ] MyTestCase/testMethod2 [ FAILURE ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS26",
                                                            @"Simulator",
                                                            @"build-23F5067a",
                                                            nil]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod1"] != nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod2"] != nil);
}

TEST_F(TestExpectationsTest, MinorAndPatchOSVersionMatching) {
  NSString* content = @"[ iOS18.2 ] MyTestCase/testMethod1 [ Failure ]\n"
                      @"[ iOS18.2.1 ] MyTestCase/testMethod2 [ Failure ]\n"
                      @"[ iOS18.3 ] MyTestCase/testMethod3 [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS", @"iOS18",
                                                            @"iOS18.2",
                                                            @"iOS18.2.1", nil]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod1"] != nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod2"] != nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod3"] == nil);
}

TEST_F(TestExpectationsTest, BuildNumberMatching) {
  NSString* content = @"[ build-17F42 ] MyTestCase/testMethod1 [ Failure ]\n"
                      @"[ build-18A5301 ] MyTestCase/testMethod2 [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS26",
                                                            @"build-17F42",
                                                            nil]];

  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod1"] != nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod2"] == nil);
}

TEST_F(TestExpectationsTest, IPadIPhoneTagsMatching) {
  NSString* content = @"[ ipad ] MyTestCase/testMethod1 [ Failure ]\n"
                      @"[ iphone ] MyTestCase/testMethod2 [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];

  // Test with 'ipad' tag.
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"ipad"]];
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod1"] != nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod2"] == nil);

  // Test with 'iphone' tag.
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iphone"]];
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod1"] == nil);
  EXPECT_TRUE([expectations expectationEntryForTestCase:@"MyTestCase"
                                             methodName:@"testMethod2"] != nil);
}

TEST_F(TestExpectationsTest, ActiveTagsIncludesAsanIfDefined) {
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:@""];
  NSSet<NSString*>* tags = [expectations activeTags];
#if defined(ADDRESS_SANITIZER)
  EXPECT_TRUE([tags containsObject:@"asan"]);
#else
  EXPECT_FALSE([tags containsObject:@"asan"]);
#endif
}

TEST_F(TestExpectationsTest, ActiveTagsIncludesCatalystIfDefined) {
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:@""];
  NSSet<NSString*>* tags = [expectations activeTags];
#if BUILDFLAG(IS_IOS_MACCATALYST)
  EXPECT_TRUE([tags containsObject:@"catalyst"]);
#else
  EXPECT_FALSE([tags containsObject:@"catalyst"]);
#endif
}

TEST_F(TestExpectationsTest, ActiveTagsIncludesDebugOrRelease) {
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:@""];
  NSSet<NSString*>* tags = [expectations activeTags];
#if defined(NDEBUG)
  EXPECT_TRUE([tags containsObject:@"release"]);
  EXPECT_FALSE([tags containsObject:@"debug"]);
#else
  EXPECT_TRUE([tags containsObject:@"debug"]);
  EXPECT_FALSE([tags containsObject:@"release"]);
#endif
}

TEST_F(TestExpectationsTest, SkipExpectation) {
  NSString* content = @"NotABug MyTestCase/testMethod1 [ Skip ]\n"
                      @"crbug.com/98765 MyTestCase/testMethod2 [ Skip ]\n"
                      @"MyTestCase/testMethod3 [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"ios"]];

  // testMethod1: Skip -> outReason is "NotABug"
  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod1"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypeSkip, entry.type);
  EXPECT_NSEQ(@"NotABug", entry.bug);

  // testMethod2: Skip -> outReason is "crbug.com/98765"
  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testMethod2"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypeSkip, entry.type);
  EXPECT_NSEQ(@"crbug.com/98765", entry.bug);
}

TEST_F(TestExpectationsTest, CrashExpectation) {
  NSString* content = @"NotABug MyTestCase/testMethod1 [ Crash ]\n"
                      @"crbug.com/54321 MyTestCase/testMethod2 [ Crash ]\n"
                      @"MyTestCase/testMethod3 [ Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"ios"]];

  // testMethod1: Crash -> outReason is "NotABug"
  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod1"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypeCrash, entry.type);
  EXPECT_NSEQ(@"NotABug", entry.bug);

  // testMethod2: Crash -> outReason is "crbug.com/54321"
  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testMethod2"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypeCrash, entry.type);
  EXPECT_NSEQ(@"crbug.com/54321", entry.bug);
}

TEST_F(TestExpectationsTest, MultipleExpectationsCombinations) {
  NSString* content =
      @"crbug.com/111 MyTestCase/testMethod1 [ Failure Crash ]\n"
      @"crbug.com/222 MyTestCase/testMethod2 [ Pass Failure Crash ]\n"
      @"crbug.com/333 MyTestCase/testMethod3 [ Pass Crash ]\n"
      @"crbug.com/444 MyTestCase/testMethod4 [ ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"ios"]];

  // testMethod1: Failure Crash
  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod1"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypeFailure | TestExpectationTypeCrash, entry.type);
  EXPECT_NSEQ(@"crbug.com/111", entry.bug);

  // testMethod2: Pass Failure Crash
  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testMethod2"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypePass | TestExpectationTypeFailure |
                TestExpectationTypeCrash,
            entry.type);
  EXPECT_NSEQ(@"crbug.com/222", entry.bug);

  // testMethod3: Pass Crash
  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testMethod3"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypePass | TestExpectationTypeCrash, entry.type);
  EXPECT_NSEQ(@"crbug.com/333", entry.bug);

  // testMethod4: [ ] -> no outcomes => pass only
  entry = [expectations expectationEntryForTestCase:@"MyTestCase"
                                         methodName:@"testMethod4"];
  EXPECT_TRUE(entry != nil);
  EXPECT_EQ(TestExpectationTypePass, entry.type);
  EXPECT_NSEQ(@"crbug.com/444", entry.bug);
}

TEST_F(TestExpectationsTest, LineNumberAndFeedbackMessages) {
  NSString* content = @"# Header comment\n"
                      @"\n"
                      @"crbug.com/123 MyTestCase/testCrash [ Crash ]\n"
                      @"crbug.com/456 MyTestCase/testFlaky [ Pass Failure ]\n";
  TestExpectations* expectations =
      [TestExpectations sharedInstanceForTesting:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"ios"]];

  TestExpectationEntry* crashEntry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testCrash"];
  ASSERT_NE(nil, crashEntry);
  EXPECT_EQ(3u, crashEntry.lineNumber);
  EXPECT_NSEQ(@"crash", [crashEntry expectedOutcomeDescription]);
  EXPECT_NSEQ(@"This test is expected to crash (line 3).",
              [crashEntry documentationMessage]);
  EXPECT_NSEQ(@"Unmet test expectation (line 3): expected crash, actual Pass.",
              [crashEntry unmetExpectationMessageWithActualOutcome:@"Pass"]);

  crashEntry.filePath = @"/path/to/test_expectations.txt";
  EXPECT_NSEQ(@"This test is expected to crash (line 3 in "
              @"/path/to/test_expectations.txt).",
              [crashEntry documentationMessage]);
  EXPECT_NSEQ(
      @"Unmet test expectation (line 3 in /path/to/test_expectations.txt): "
      @"expected crash, actual Pass.",
      [crashEntry unmetExpectationMessageWithActualOutcome:@"Pass"]);

  TestExpectationEntry* flakyEntry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testFlaky"];
  ASSERT_NE(nil, flakyEntry);
  EXPECT_EQ(4u, flakyEntry.lineNumber);
  EXPECT_NSEQ(@"pass or fail", [flakyEntry expectedOutcomeDescription]);
  EXPECT_NSEQ(@"This test is expected to pass or fail (line 4).",
              [flakyEntry documentationMessage]);
}

TEST_F(TestExpectationsTest, DefaultOutcomeDescription) {
  TestExpectationEntry* entry = [[TestExpectationEntry alloc] init];
  entry.type = TestExpectationTypeNone;
  EXPECT_NSEQ(@"pass", [entry expectedOutcomeDescription]);
}

TEST_F(TestExpectationsTest, FourOutcomesDescription) {
  TestExpectationEntry* entry = [[TestExpectationEntry alloc] init];
  entry.type = TestExpectationTypePass | TestExpectationTypeFailure |
               TestExpectationTypeCrash | TestExpectationTypeSkip;
  EXPECT_NSEQ(@"pass, fail, crash, or skip",
              [entry expectedOutcomeDescription]);
}

struct TagMatchingTestCase {
  const char* test_name;
  const char* content;
  std::vector<const char*> active_tags;
  bool expect_match;
};

class TestExpectationsMatchingTest
    : public PlatformTest,
      public testing::WithParamInterface<TagMatchingTestCase> {};

TEST_P(TestExpectationsMatchingTest, MatchResult) {
  const TagMatchingTestCase& param = GetParam();
  NSString* content = [NSString stringWithUTF8String:param.content];
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];

  NSMutableSet<NSString*>* active_tags = [NSMutableSet set];
  for (const char* tag : param.active_tags) {
    [active_tags addObject:[NSString stringWithUTF8String:tag]];
  }
  [expectations setOverrideActiveTagsForTesting:active_tags];

  TestExpectationEntry* entry =
      [expectations expectationEntryForTestCase:@"MyTestCase"
                                     methodName:@"testMethod"];
  if (param.expect_match) {
    EXPECT_TRUE(entry != nil);
  } else {
    EXPECT_TRUE(entry == nil);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TestExpectationsMatchingTest,
    testing::Values(
        TagMatchingTestCase{
            "CategoryMatchingOS",
            "[ iOS18 iOS26 Simulator ] MyTestCase/testMethod [ Failure ]\n",
            {"iOS26", "Simulator"},
            true},
        TagMatchingTestCase{
            "CategoryMatchingDevice",
            "[ iPad iPhone Device ] MyTestCase/testMethod [ Failure ]\n",
            {"iPhone", "Device"},
            true},
        TagMatchingTestCase{
            "OtherCategoryMissingOne",
            "[ iOS18 Simulator Device ] MyTestCase/testMethod [ Failure ]\n",
            {"iOS18", "Simulator"},
            false},
        TagMatchingTestCase{
            "OtherCategoryAllMatch",
            "[ iOS18 Simulator Device ] MyTestCase/testMethod [ Failure ]\n",
            {"iOS18", "Simulator", "Device"},
            true},
        TagMatchingTestCase{"OtherCategoryCustomTag",
                            "[ custom_tag iOS26 Simulator ] "
                            "MyTestCase/testMethod [ Failure ]\n",
                            {"custom_tag", "iOS26", "Simulator"},
                            true},
        TagMatchingTestCase{"DeviceModelPrefix",
                            "[ iphone17 ] MyTestCase/testMethod [ Failure ]\n",
                            {"iphone", "iphone17", "iphone17,1"},
                            true},
        TagMatchingTestCase{
            "DeviceModelExact",
            "[ iphone17,1 ] MyTestCase/testMethod [ Failure ]\n",
            {"iphone", "iphone17", "iphone17,1"},
            true},
        TagMatchingTestCase{"AsanMatch",
                            "[ asan ] MyTestCase/testMethod [ Failure ]\n",
                            {"asan"},
                            true},
        TagMatchingTestCase{"AsanMismatch",
                            "[ asan ] MyTestCase/testMethod [ Failure ]\n",
                            {"ios"},
                            false},
        TagMatchingTestCase{"CatalystMatch",
                            "[ catalyst ] MyTestCase/testMethod [ Failure ]\n",
                            {"catalyst"},
                            true},
        TagMatchingTestCase{"CatalystMismatch",
                            "[ catalyst ] MyTestCase/testMethod [ Failure ]\n",
                            {"ios"},
                            false},
        TagMatchingTestCase{"DebugMatch",
                            "[ debug ] MyTestCase/testMethod [ Failure ]\n",
                            {"debug"},
                            true},
        TagMatchingTestCase{"DebugMismatch",
                            "[ debug ] MyTestCase/testMethod [ Failure ]\n",
                            {"release"},
                            false},
        TagMatchingTestCase{"ReleaseMatch",
                            "[ release ] MyTestCase/testMethod [ Failure ]\n",
                            {"release"},
                            true},
        TagMatchingTestCase{"ReleaseMismatch",
                            "[ release ] MyTestCase/testMethod [ Failure ]\n",
                            {"debug"},
                            false}),
    [](const testing::TestParamInfo<TagMatchingTestCase>& info) {
      return info.param.test_name;
    });

struct DeviceTagsTestCase {
  const char* test_name;
  const char* hardware_model;
  ui::DeviceFormFactor form_factor;
  std::vector<const char*> expected_tags;
  std::vector<const char*> unexpected_tags;
};

class TestExpectationsDeviceTagsTest
    : public PlatformTest,
      public testing::WithParamInterface<DeviceTagsTestCase> {};

TEST_P(TestExpectationsDeviceTagsTest, VerifyGeneratedTags) {
  const DeviceTagsTestCase& param = GetParam();
  NSString* hardware_model =
      [NSString stringWithUTF8String:param.hardware_model];
  NSSet<NSString*>* tags =
      [TestExpectations deviceTagsForHardwareModel:hardware_model
                                        formFactor:param.form_factor];

  for (const char* expected : param.expected_tags) {
    EXPECT_TRUE([tags containsObject:[NSString stringWithUTF8String:expected]])
        << "Expected tag missing: " << expected;
  }
  for (const char* unexpected : param.unexpected_tags) {
    EXPECT_FALSE(
        [tags containsObject:[NSString stringWithUTF8String:unexpected]])
        << "Unexpected tag present: " << unexpected;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TestExpectationsDeviceTagsTest,
    testing::Values(DeviceTagsTestCase{"SimulatorIPhone",
                                       "iOS Simulator (iPhone16,1)",
                                       ui::DEVICE_FORM_FACTOR_PHONE,
                                       {"iphone16,1", "iphone16", "iphone"},
                                       {"ipad"}},
                    DeviceTagsTestCase{"FallbackIPad",
                                       "UnknownModel",
                                       ui::DEVICE_FORM_FACTOR_TABLET,
                                       {"unknownmodel", "ipad"},
                                       {"iphone"}}),
    [](const testing::TestParamInfo<DeviceTagsTestCase>& info) {
      return info.param.test_name;
    });
