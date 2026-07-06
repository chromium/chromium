// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/test_expectations.h"

#import <TargetConditionals.h>
#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"

@implementation TestExpectationEntry
@end

namespace {
TestExpectations* g_shared_instance = nil;
}  // namespace

@interface TestExpectations ()
- (instancetype)initWithFilePath:(NSString*)path;
- (instancetype)initWithContent:(NSString*)content;
@end

@implementation TestExpectations {
  NSMutableDictionary<NSString*, TestExpectationEntry*>* _expectations;

  // Override for active tags (used in tests).
  NSSet<NSString*>* _activeTagsOverride;

  // Store original content for reparsing if tags are overridden.
  NSString* _content;
}

- (instancetype)initWithFilePath:(NSString*)path {
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  CHECK(content && !error) << "Error reading expectations file at " << path
                           << ": " << error;
  return [self initWithContent:content];
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
  self = [super init];
  if (self) {
    _content = [content copy];
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
  std::string buildNumber = base::SysInfo::GetIOSBuildNumber();
  if (!buildNumber.empty()) {
    NSString* buildNSString = base::SysUTF8ToNSString(buildNumber);
    [tags addObject:[buildNSString lowercaseString]];
  }

#if TARGET_OS_SIMULATOR
  [tags addObject:@"simulator"];
#else
  [tags addObject:@"device"];
#endif

  if ([UIDevice currentDevice].userInterfaceIdiom == UIUserInterfaceIdiomPad) {
    [tags addObject:@"ipad"];
  } else if ([UIDevice currentDevice].userInterfaceIdiom ==
             UIUserInterfaceIdiomPhone) {
    [tags addObject:@"iphone"];
  }

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
  for (NSString* line in lines) {
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
    BOOL tagsMatch = YES;
    if (tagsStr) {
      NSArray<NSString*>* tags = [tagsStr componentsSeparatedByString:@" "];
      for (NSString* tag in tags) {
        NSString* trimmedTag =
            [tag stringByTrimmingCharactersInSet:[NSCharacterSet
                                                     whitespaceCharacterSet]];
        if (trimmedTag.length > 0) {
          NSString* lowercaseTag = [trimmedTag lowercaseString];
          if (![activeTags containsObject:lowercaseTag]) {
            tagsMatch = NO;
            break;
          }
        }
      }
    }

    if (!tagsMatch) {
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
      continue;
    }

    // Normalize test ID and store
    NSString* normalizedTestId = [self normalizeTestIdentifier:testId];
    TestExpectationEntry* entry = [[TestExpectationEntry alloc] init];
    entry.bug = bug ? bug : @"Expected failure";
    entry.type = type;
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

@end
