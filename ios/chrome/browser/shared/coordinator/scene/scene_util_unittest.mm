// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/platform_test.h"

@interface FakeSceneSession : NSObject

- (instancetype)initWithIdentifier:(NSString*)identifier;

@property(nonatomic, copy, readonly) NSString* persistentIdentifier;

@end

@implementation FakeSceneSession {
  __strong NSString* _identifier;
}

- (instancetype)initWithIdentifier:(NSString*)identifier {
  if ((self = [super init])) {
    _identifier = [identifier copy];
  }
  return self;
}

- (NSString*)persistentIdentifier {
  return _identifier;
}

@end

@interface FakeScene : NSObject

- (instancetype)initWithSession:(id)session;

@property(nonatomic, strong, readonly) FakeSceneSession* session;

@end

@implementation FakeScene {
  __strong FakeSceneSession* _session;
}

- (instancetype)initWithSession:(FakeSceneSession*)session {
  if ((self = [super init])) {
    _session = session;
  }
  return self;
}

- (FakeSceneSession*)session {
  return _session;
}

@end

namespace {

// Returns a fake UIScene with `identifier` as session persistent identifier
// when running on iOS 13+ or nil otherwise. The fake object implements just
// enough API for SessionIdentifierForScene().
id FakeSceneWithIdentifier(NSString* identifier) {
  return [[FakeScene alloc]
      initWithSession:[[FakeSceneSession alloc] initWithIdentifier:identifier]];
}

}  // anonymous namespace

using SceneUtilTest = PlatformTest;

// Tests that the identifier returned by SessionIdentifierForScene() for a
// Scene is either the UIKit identifier (if multi-scene is supported by the
// device) or a known constants otherwise.
TEST_F(SceneUtilTest, SessionIdentifierForScene) {
  NSString* identifier = [[NSUUID UUID] UUIDString];
  id scene = FakeSceneWithIdentifier(identifier);

  std::string expected = "{SyntheticIdentifier}";
  if (base::ios::IsMultipleScenesSupported()) {
    expected = base::SysNSStringToUTF8(identifier);
  }

  EXPECT_EQ(expected, SessionIdentifierForScene(scene));
}
