// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/test_expectations.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "build/build_config.h"
#import "ui/base/device_form_factor.h"

namespace {

// Message used by EarlGrey in `_XCTFailureHandler` when halting a test due to
// an assertion failure.
NSString* const kEarlGreyHaltExecutionMessage =
    @"Immediately halt execution of testcase";

// Exception name used by EarlGrey when a host application crash occurs.
NSString* const kEarlGreyInterruptExceptionName =
    @"EarlGreyInternalTestInterruptException";

}  // namespace

@implementation TestExpectationEntry

- (NSString*)expectedOutcomeDescription {
  NSMutableArray<NSString*>* outcomes = [NSMutableArray array];
  if (self.type & TestExpectationTypePass) {
    [outcomes addObject:@"pass"];
  }
  if (self.type & TestExpectationTypeFailure) {
    [outcomes addObject:@"fail"];
  }
  if (self.type & TestExpectationTypeCrash) {
    [outcomes addObject:@"crash"];
  }
  if (self.type & TestExpectationTypeSkip) {
    [outcomes addObject:@"skip"];
  }

  if (outcomes.count == 0) {
    return @"pass";
  }
  NSMutableString* result = [NSMutableString string];
  for (NSUInteger i = 0; i < outcomes.count; ++i) {
    if (i > 0) {
      if (i == outcomes.count - 1) {
        [result appendString:(outcomes.count > 2) ? @", or " : @" or "];
      } else {
        [result appendString:@", "];
      }
    }
    [result appendString:outcomes[i]];
  }
  return result;
}

// Returns a formatted string describing the line number and file path.
- (NSString*)locationDescription {
  if (self.filePath.length > 0) {
    return [NSString stringWithFormat:@"line %lu in %@",
                                      (unsigned long)self.lineNumber,
                                      self.filePath];
  }
  return
      [NSString stringWithFormat:@"line %lu", (unsigned long)self.lineNumber];
}

- (NSString*)documentationMessage {
  NSString* outcome = [self expectedOutcomeDescription];

  return [NSString stringWithFormat:@"This test is expected to %@ (%@).",
                                    outcome, [self locationDescription]];
}

- (NSString*)unmetExpectationMessageWithActualOutcome:(NSString*)actualOutcome {
  NSString* expectedOutcome = [self expectedOutcomeDescription];

  return [NSString
      stringWithFormat:@"Unmet test expectation (%@): expected %@, actual %@.",
                       [self locationDescription], expectedOutcome,
                       actualOutcome];
}

- (TestExpectationMatchResult)matchesIssueType:(XCTIssueType)issueType
                            compactDescription:(NSString*)compactDescription {
  if (issueType == XCTIssueTypeUnmatchedExpectedFailure) {
    return TestExpectationMatchResult::kUnmatched;
  }

  BOOL didCrash = (issueType == XCTIssueTypeUncaughtException ||
                   issueType == XCTIssueTypeThrownError);

  if (didCrash) {
    if ([compactDescription containsString:kEarlGreyHaltExecutionMessage] &&
        ![compactDescription containsString:kEarlGreyInterruptExceptionName]) {
      didCrash = NO;
    }
  }

  BOOL matches = didCrash ? ((self.type & TestExpectationTypeCrash) != 0)
                          : ((self.type & TestExpectationTypeFailure) != 0);
  if (!matches) {
    NSString* actualOutcome = didCrash ? @"Crash" : @"Failure";
    NSLog(@"%@", [self unmetExpectationMessageWithActualOutcome:actualOutcome]);
    return TestExpectationMatchResult::kMismatched;
  }
  return TestExpectationMatchResult::kMatched;
}

@end

@interface TestExpectations ()
- (instancetype)initWithFilePath:(NSString*)path;
// Evaluates whether the expectation's tag string matches active tags.
- (BOOL)doTagsMatch:(NSString*)tagsStr activeTags:(NSSet<NSString*>*)activeTags;
@end

namespace {
TestExpectations* g_shared_instance = nil;

enum class TagCategory {
  kOS,
  kDevice,
  kOther,
};

TagCategory GetTagCategory(NSString* tag) {
  if ([tag hasPrefix:@"ipad"] || [tag hasPrefix:@"iphone"]) {
    return TagCategory::kDevice;
  }
  if ([tag hasPrefix:@"ios"] || [tag hasPrefix:@"build-"]) {
    return TagCategory::kOS;
  }
  return TagCategory::kOther;
}
}  // namespace

@implementation TestExpectations {
  NSMutableDictionary<NSString*, TestExpectationEntry*>* _expectations;

  // Override for active tags (used in tests).
  NSSet<NSString*>* _activeTagsOverride;

  // Store original content for reparsing if tags are overridden.
  NSString* _content;

  // Path to the expectations file.
  NSString* _filePath;
}

- (instancetype)initWithFilePath:(NSString*)path {
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  CHECK(content && !error) << "Error reading expectations file at " << path
                           << ": " << error;
  return [self initWithContent:content filePath:path];
}

+ (instancetype)sharedInstance {
  if (!g_shared_instance) {
    NSBundle* bundle = [NSBundle bundleForClass:[TestExpectations class]];
    NSString* path = [bundle pathForResource:@"test_expectations"
                                      ofType:@"txt"];
    CHECK(path) << "Failed to find test_expectations.txt in bundle " << bundle;
    g_shared_instance = [[TestExpectations alloc] initWithFilePath:path];
  }
  return g_shared_instance;
}

- (instancetype)initWithContent:(NSString*)content {
  return [self initWithContent:content filePath:nil];
}

- (instancetype)initWithContent:(NSString*)content
                       filePath:(NSString*)filePath {
  self = [super init];
  if (self) {
    _content = [content copy];
    _filePath = [filePath copy];
    _expectations = [NSMutableDictionary dictionary];
    [self parseExpectations:_content];
  }
  return self;
}

- (NSSet<NSString*>*)activeTags {
  if (_activeTagsOverride) {
    return _activeTagsOverride;
  }

  NSMutableSet<NSString*>* tags = [NSMutableSet set];
  [tags addObject:@"ios"];

  NSString* systemVersion = [UIDevice currentDevice].systemVersion;
  NSArray<NSString*>* components =
      [systemVersion componentsSeparatedByString:@"."];
  if (components.count > 0) {
    NSString* major = components[0];
    [tags addObject:[NSString stringWithFormat:@"ios%@", major]];
    if (components.count > 1) {
      NSString* minor = components[1];
      [tags addObject:[NSString stringWithFormat:@"ios%@.%@", major, minor]];
      if (components.count > 2) {
        NSString* patch = components[2];
        [tags addObject:[NSString stringWithFormat:@"ios%@.%@.%@", major, minor,
                                                   patch]];
      }
    }
  }

  // Add build number
  std::string buildVersion = base::SysInfo::OperatingSystemBuildVersion();
  if (!buildVersion.empty()) {
    NSString* buildNSString = base::SysUTF8ToNSString(buildVersion);
    NSString* prefixedBuild =
        [NSString stringWithFormat:@"build-%@", buildNSString];
    [tags addObject:prefixedBuild.lowercaseString];
  }

#if TARGET_OS_SIMULATOR
  [tags addObject:@"simulator"];
#else
  [tags addObject:@"device"];
#endif

#if defined(ADDRESS_SANITIZER)
  [tags addObject:@"asan"];
#endif

#if BUILDFLAG(IS_IOS_MACCATALYST)
  [tags addObject:@"catalyst"];
#endif

#if defined(NDEBUG)
  [tags addObject:@"release"];
#else
  [tags addObject:@"debug"];
#endif

  std::string hardwareModel = base::SysInfo::HardwareModelName();
  NSString* modelNSString = base::SysUTF8ToNSString(hardwareModel);
  NSSet<NSString*>* deviceTags =
      [TestExpectations deviceTagsForHardwareModel:modelNSString
                                        formFactor:ui::GetDeviceFormFactor()];
  [tags unionSet:deviceTags];

  return [tags copy];
}

- (void)parseExpectations:(NSString*)content {
  NSSet<NSString*>* activeTags = [self activeTags];

  NSError* error = nil;
  NSRegularExpression* regex =
      [NSRegularExpression regularExpressionWithPattern:
                               @"^(?:([^\\s\\[]\\S*)\\s+)?(?:\\[([^\\]]+)\\]"
                               @"\\s+)?(\\S+)\\s+\\[([^\\]]+)\\](?:\\s*#.*)?$"
                                                options:0
                                                  error:&error];
  CHECK(!error) << "Failed to compile regex: " << error;

  NSArray<NSString*>* lines = [content componentsSeparatedByString:@"\n"];
  NSUInteger lineNumber = 0;
  for (NSString* line in lines) {
    lineNumber++;
    NSString* trimmed =
        [line stringByTrimmingCharactersInSet:[NSCharacterSet
                                                  whitespaceCharacterSet]];
    if (trimmed.length == 0 || [trimmed hasPrefix:@"#"]) {
      continue;
    }

    NSTextCheckingResult* match =
        [regex firstMatchInString:trimmed
                          options:0
                            range:NSMakeRange(0, trimmed.length)];
    if (!match) {
      continue;
    }

    NSString* bug = nil;
    if ([match rangeAtIndex:1].location != NSNotFound) {
      bug = [trimmed substringWithRange:[match rangeAtIndex:1]];
    }

    NSString* tagsStr = nil;
    if ([match rangeAtIndex:2].location != NSNotFound) {
      tagsStr = [trimmed substringWithRange:[match rangeAtIndex:2]];
    }

    NSString* testId = nil;
    if ([match rangeAtIndex:3].location != NSNotFound) {
      testId = [trimmed substringWithRange:[match rangeAtIndex:3]];
    }

    NSString* expectationsStr = nil;
    if ([match rangeAtIndex:4].location != NSNotFound) {
      expectationsStr = [trimmed substringWithRange:[match rangeAtIndex:4]];
    }

    // Validate tags
    if (![self doTagsMatch:tagsStr activeTags:activeTags]) {
      continue;
    }

    // Validate expectations
    TestExpectationType type = TestExpectationTypeNone;
    if (expectationsStr) {
      NSArray<NSString*>* expectations =
          [expectationsStr componentsSeparatedByString:@" "];
      for (NSString* exp in expectations) {
        NSString* trimmedExp =
            [exp stringByTrimmingCharactersInSet:[NSCharacterSet
                                                     whitespaceCharacterSet]];
        NSString* lowercaseExp = [trimmedExp lowercaseString];
        if ([lowercaseExp isEqualToString:@"failure"]) {
          type |= TestExpectationTypeFailure;
        } else if ([lowercaseExp isEqualToString:@"pass"]) {
          type |= TestExpectationTypePass;
        } else if ([lowercaseExp isEqualToString:@"skip"]) {
          type |= TestExpectationTypeSkip;
        } else if ([lowercaseExp isEqualToString:@"crash"]) {
          type |= TestExpectationTypeCrash;
        }
      }
    }

    if (type == TestExpectationTypeNone) {
      type = TestExpectationTypePass;
    }

    // Normalize test ID and store
    NSString* normalizedTestId = [self normalizeTestIdentifier:testId];
    TestExpectationEntry* entry = [[TestExpectationEntry alloc] init];
    entry.bug = bug ? bug : @"Expected failure";
    entry.type = type;
    entry.lineNumber = lineNumber;
    entry.filePath = _filePath;
    _expectations[normalizedTestId] = entry;
  }
}

- (TestExpectationEntry*)expectationEntryForTestCase:(NSString*)testClassName
                                          methodName:(NSString*)methodName {
  NSString* methodId =
      [NSString stringWithFormat:@"%@/%@", testClassName, methodName];
  NSString* normalizedMethodId = [self normalizeTestIdentifier:methodId];

  TestExpectationEntry* entry = _expectations[normalizedMethodId];
  if (entry) {
    return entry;
  }

  // Try class-level expectation
  NSString* normalizedClassId = [self normalizeTestIdentifier:testClassName];
  return _expectations[normalizedClassId];
}

- (NSString*)normalizeTestIdentifier:(NSString*)identifier {
  return [identifier stringByReplacingOccurrencesOfString:@"." withString:@"/"];
}

// Evaluates whether the expectation's tag string matches active tags.
//
// Note on tag matching semantics:
// - OS and Device categories use "ANY" (OR) semantics: if an expectation
// specifies
//   multiple tags within the OS or Device category (e.g., [ ios-17 ios-18 ] or
//   [ iphone ipad ]), matching ANY tag within that category satisfies the
//   requirement.
// - Other tags use "ALL" (AND) semantics: EVERY tag in the kOther category
// present in the
//   expectation (e.g., [ debug asan ]) must be present in the active tags for a
//   match.
//
// This divergence between ANY and ALL semantics is intentional to allow
// expectations that span multiple OS versions or device families while
// requiring specific build configurations.
- (BOOL)doTagsMatch:(NSString*)tagsStr
         activeTags:(NSSet<NSString*>*)activeTags {
  if (!tagsStr) {
    return YES;
  }

  NSArray<NSString*>* tags = [tagsStr componentsSeparatedByString:@" "];
  bool expectation_has_os_tag = false;
  bool os_matched = false;
  bool expectation_has_device_tag = false;
  bool device_matched = false;

  for (NSString* tag in tags) {
    NSString* trimmedTag =
        [tag stringByTrimmingCharactersInSet:[NSCharacterSet
                                                 whitespaceCharacterSet]];
    if (trimmedTag.length > 0) {
      NSString* lowercaseTag = [trimmedTag lowercaseString];
      bool is_match = [activeTags containsObject:lowercaseTag];
      TagCategory category = GetTagCategory(lowercaseTag);
      switch (category) {
        case TagCategory::kOS:
          expectation_has_os_tag = true;
          if (is_match) {
            os_matched = true;
          }
          break;
        case TagCategory::kDevice:
          expectation_has_device_tag = true;
          if (is_match) {
            device_matched = true;
          }
          break;
        case TagCategory::kOther:
          if (!is_match) {
            return NO;
          }
          break;
      }
    }
  }

  // For the expectation to match, all kOther tags must have matched in the loop
  // above (ALL semantics), and if any OS or Device tags were specified, at
  // least one tag from each specified category must match (ANY semantics).
  return (os_matched || !expectation_has_os_tag) &&
         (device_matched || !expectation_has_device_tag);
}

@end

@implementation TestExpectations (Testing)

- (void)setOverrideActiveTagsForTesting:(NSSet<NSString*>*)tags {
  // Store tags normalized to lowercase for testing consistency.
  NSMutableSet<NSString*>* normalizedTags = [NSMutableSet set];
  for (NSString* tag in tags) {
    [normalizedTags addObject:[tag lowercaseString]];
  }
  _activeTagsOverride = [normalizedTags copy];

  // Reparse expectations with the new tags.
  [_expectations removeAllObjects];
  if (_content) {
    [self parseExpectations:_content];
  }
}

+ (instancetype)sharedInstanceForTesting:(NSString*)content {
  CHECK(g_shared_instance == nil);
  g_shared_instance = [[TestExpectations alloc] initWithContent:content];
  return g_shared_instance;
}

+ (void)resetForTesting {
  g_shared_instance = nil;
}

// Generates device tags given a hardware model string and UI device form
// factor.
+ (NSSet<NSString*>*)deviceTagsForHardwareModel:(NSString*)hardwareModel
                                     formFactor:
                                         (ui::DeviceFormFactor)formFactor {
  NSMutableSet<NSString*>* tags = [NSMutableSet set];
  NSString* lowercaseModel = nil;

  if (hardwareModel.length > 0) {
    NSString* modelNSString = hardwareModel;
    // On iOS simulators, base::SysInfo::HardwareModelName() returns a
    // formatted string like "iOS Simulator (iPhone16,1)". Extract the actual
    // model identifier inside the parentheses so that tag matching and
    // classification work consistently across both simulator builds and
    // physical hardware.
    NSRange openParen = [modelNSString rangeOfString:@"("];
    NSRange closeParen = [modelNSString rangeOfString:@")"
                                              options:NSBackwardsSearch];
    if (openParen.location != NSNotFound && closeParen.location != NSNotFound &&
        closeParen.location > openParen.location + 1) {
      NSRange modelRange = NSMakeRange(
          openParen.location + 1, closeParen.location - openParen.location - 1);
      modelNSString = [modelNSString substringWithRange:modelRange];
    }

    lowercaseModel = [modelNSString lowercaseString];
    if (lowercaseModel.length > 0) {
      // Add both the exact hardware model identifier and its general model
      // prefix (stripped of the revision/generation suffix after the comma).
      // For example, if the extracted model name is "iPhone16,1", this
      // generates and adds both "iphone16,1" and "iphone16" as active device
      // tags.
      [tags addObject:lowercaseModel];
      NSRange commaRange = [lowercaseModel rangeOfString:@","];
      if (commaRange.location != NSNotFound) {
        [tags addObject:[lowercaseModel substringToIndex:commaRange.location]];
      }
    }
  }

  // Derive the general device family tag directly from the model identifier.
  if ([lowercaseModel hasPrefix:@"ipad"]) {
    [tags addObject:@"ipad"];
  } else if ([lowercaseModel hasPrefix:@"iphone"]) {
    [tags addObject:@"iphone"];
  } else {
    // Fallback if model string was unavailable or unrecognized.
    if (formFactor == ui::DEVICE_FORM_FACTOR_TABLET) {
      [tags addObject:@"ipad"];
    } else if (formFactor == ui::DEVICE_FORM_FACTOR_PHONE) {
      [tags addObject:@"iphone"];
    }
  }

  return [tags copy];
}

@end
