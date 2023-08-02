// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Chromium code relies on NSRegularExpression class to match regular
// expressions.  Any subtle changes in behavior can lead to hard to diagnose
// problems.  This files tests how NSRegularExpression handles various regular
// expression features.

namespace {

// Checks that capture groups from `testString` substituted into
// `templateString` matches `expected`.
void ExpectRegexMatched(NSRegularExpression* regex,
                        NSString* testString,
                        NSString* templateString,
                        NSString* expected) {
  NSRange testRange = NSMakeRange(0, [testString length]);
  NSString* outputString =
      [regex stringByReplacingMatchesInString:testString
                                      options:0
                                        range:testRange
                                 withTemplate:templateString];
  EXPECT_TRUE(outputString && [outputString isEqualToString:expected])
      << "ExpectRegexMatched: '" << base::SysNSStringToUTF8(expected) << "' != "
      << (!outputString ? "(nil)"
                        : "'" + base::SysNSStringToUTF8(outputString) + "'");
}

// Checks that `testString` is not matched by `regex`.
void ExpectRegexNotMatched(NSRegularExpression* regex, NSString* testString) {
  __block BOOL matched = NO;
  NSRange testRange = NSMakeRange(0, [testString length]);
  [regex enumerateMatchesInString:testString
                          options:0
                            range:testRange
                       usingBlock:^(NSTextCheckingResult* result,
                                    NSMatchingFlags flags, BOOL* stop) {
                         if (NSEqualRanges([result range], testRange)) {
                           *stop = YES;
                           matched = YES;
                         }
                       }];
  EXPECT_FALSE(matched) << "ExpectRegexNotMatched: '"
                        << base::SysNSStringToUTF8(testString) << "' "
                        << "pattern:"
                        << base::SysNSStringToUTF8([regex pattern]);
}

// Returns an autoreleased NSRegularExpression object from the regular
// expression `pattern`.
NSRegularExpression* MakeRegularExpression(NSString* pattern) {
  NSError* error = nil;
  return [NSRegularExpression regularExpressionWithPattern:pattern
                                                   options:0
                                                     error:&error];
}

using NSRegularExpressionTest = PlatformTest;

TEST_F(NSRegularExpressionTest, TestSimpleRegex) {
  NSRegularExpression* regex = MakeRegularExpression(@"foo(.*)bar(.*)");
  ExpectRegexMatched(regex, @"fooONEbarTWO", @"first $1, second $2",
                     @"first ONE, second TWO");
}

TEST_F(NSRegularExpressionTest, TestComplexRegex) {
  NSString* expression = @"^http[s]?://"
                          "(?:"
                          "(?:youtu\\.be/)|"
                          "(?:.*\\.youtube\\.com/watch\\?v=)|"
                          "(?:.*\\.youtube\\.com/index\\?)"
                          ")"
                          "([^&]*)[\\&]?(?:.*)$";
  NSString* templateString = @"vnd.youtube://$1";
  NSString* expectedOutput = @"vnd.youtube://ndRXe3tTnsA";
  NSRegularExpression* regex = MakeRegularExpression(expression);
  ExpectRegexMatched(regex, @"http://youtu.be/ndRXe3tTnsA", templateString,
                     expectedOutput);
  ExpectRegexMatched(regex, @"http://www.youtube.com/watch?v=ndRXe3tTnsA",
                     templateString, expectedOutput);
  ExpectRegexNotMatched(regex, @"http://www.google.com");
  ExpectRegexNotMatched(regex, @"http://www.youtube.com/embed/GkOZ8DfO248");
}

TEST_F(NSRegularExpressionTest, TestSimpleAlternation) {
  // This test verifies how NSRegularExpression works.
  // Regex 'ab|c' matches 'ab', 'ac', or 'c'. Does not match 'abc', 'a', or 'b'.
  NSRegularExpression* regex = MakeRegularExpression(@"^ab|c$");
  ExpectRegexMatched(regex, @"ab", @"$0", @"ab");
  ExpectRegexMatched(regex, @"c", @"$0", @"c");
  ExpectRegexMatched(regex, @"ac", @"$0", @"ac");
  ExpectRegexNotMatched(regex, @"abc");
  ExpectRegexNotMatched(regex, @"a");
  ExpectRegexNotMatched(regex, @"b");

  // Tests for '(?:ab)|(?:c)', which is slightly different from 'ab|c'.
  regex = MakeRegularExpression(@"^(?:ab)|(?:c)$");
  ExpectRegexMatched(regex, @"ab", @"$0", @"ab");
  ExpectRegexMatched(regex, @"c", @"$0", @"c");
  ExpectRegexNotMatched(regex, @"ac");
  ExpectRegexNotMatched(regex, @"abc");

  // This other regex: 'a(b|c)' matches either 'ab' or 'ac'.
  regex = MakeRegularExpression(@"^a(?:b|c)$");
  ExpectRegexMatched(regex, @"ab", @"$0", @"ab");
  ExpectRegexMatched(regex, @"ac", @"$0", @"ac");
  ExpectRegexNotMatched(regex, @"a");
  ExpectRegexNotMatched(regex, @"abc");
}

TEST_F(NSRegularExpressionTest, TestUberCaptureGroup) {
  // The absence of an uber-capture group caused NSRegularExpression to crash on
  // iOS 5.x. This tests to make sure that it is not happening on iOS 6+
  // environments.
  NSRegularExpression* regex = MakeRegularExpression(@"^(ab|cd|ef)ghij$");
  ExpectRegexMatched(regex, @"abghij", @"$0", @"abghij");
  ExpectRegexMatched(regex, @"cdghij", @"$0", @"cdghij");
  ExpectRegexMatched(regex, @"efghij", @"$0", @"efghij");
  ExpectRegexNotMatched(regex, @"abcdefghij");

  regex = MakeRegularExpression(@"^ab|cd|efghij$");
  ExpectRegexMatched(regex, @"ab", @"$0", @"ab");
  ExpectRegexMatched(regex, @"cd", @"$0", @"cd");
  ExpectRegexMatched(regex, @"efghij", @"$0", @"efghij");
  ExpectRegexNotMatched(regex, @"abcdefghij");
  ExpectRegexNotMatched(regex, @"abghij");
  ExpectRegexNotMatched(regex, @"cdghij");
}

}  // namespace
