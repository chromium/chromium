// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

#import <Foundation/Foundation.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#pragma mark - Test handlers

@protocol ShowProtocol <NSObject>
- (void)show;
- (void)showMore;
@end

@protocol HideProtocol
- (void)hide;
- (void)hideMore;
@end

@protocol CompositeProtocolWithMethods <HideProtocol>
- (void)doCompositeThings;
@end

@protocol EmptyContainerProtocol <CompositeProtocolWithMethods, ShowProtocol>
@end

// A handler with methods that take no arguments.
@interface CommandDispatcherTestSimpleTarget : NSObject <ShowProtocol>

// Will be set to YES when the `-show` method is called.
@property(nonatomic, assign) BOOL showCalled;

// Will be set to YES when the `-showMore` method is called.
@property(nonatomic, assign) BOOL showMoreCalled;

// Will be set to YES when the `-hide` method is called.
@property(nonatomic, assign) BOOL hideCalled;

// Resets the above properties to NO.
- (void)resetProperties;

// Handler methods.
- (void)hide;

@end

@implementation CommandDispatcherTestSimpleTarget

@synthesize showCalled = _showCalled;
@synthesize showMoreCalled = _showMoreCalled;
@synthesize hideCalled = _hideCalled;

- (void)resetProperties {
  self.showCalled = NO;
  self.showMoreCalled = NO;
  self.hideCalled = NO;
}

- (void)show {
  self.showCalled = YES;
}

- (void)showMore {
  self.showMoreCalled = YES;
}

- (void)hide {
  self.hideCalled = YES;
}

@end

// A handler with methods that take various types of arguments.
@interface CommandDispatcherTestTargetWithArguments : NSObject

// Set to YES when `-methodWithInt:` is called.
@property(nonatomic, assign) BOOL intMethodCalled;

// The argument passed to the most recent call of `-methodWithInt:`.
@property(nonatomic, assign) int intArgument;

// Set to YES when `-methodWithObject:` is called.
@property(nonatomic, assign) BOOL objectMethodCalled;

// The argument passed to the most recent call of `-methodWithObject:`.
@property(nonatomic, strong) NSObject* objectArgument;

// Resets the above properties to NO or nil.
- (void)resetProperties;

// Handler methods.
- (void)methodWithInt:(int)arg;
- (void)methodWithObject:(NSObject*)arg;
- (int)methodToAddFirstArgument:(int)first toSecond:(int)second;

@end

@implementation CommandDispatcherTestTargetWithArguments

@synthesize intMethodCalled = _intMethodCalled;
@synthesize intArgument = _intArgument;
@synthesize objectMethodCalled = _objectMethodCalled;
@synthesize objectArgument = _objectArgument;

- (void)resetProperties {
  self.intMethodCalled = NO;
  self.intArgument = 0;
  self.objectMethodCalled = NO;
  self.objectArgument = nil;
}

- (void)methodWithInt:(int)arg {
  self.intMethodCalled = YES;
  self.intArgument = arg;
}

- (void)methodWithObject:(NSObject*)arg {
  self.objectMethodCalled = YES;
  self.objectArgument = arg;
}

- (int)methodToAddFirstArgument:(int)first toSecond:(int)second {
  return first + second;
}

@end

#pragma mark - Tests

using CommandDispatcherTest = PlatformTest;

// Tests handler methods with no arguments.
TEST_F(CommandDispatcherTest, SimpleTarget) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target forSelector:@selector(show)];
  [dispatcher startDispatchingToTarget:target forSelector:@selector(hide)];

  [dispatcher show];
  EXPECT_TRUE(target.showCalled);
  EXPECT_FALSE(target.hideCalled);

  [target resetProperties];
  [dispatcher hide];
  EXPECT_FALSE(target.showCalled);
  EXPECT_TRUE(target.hideCalled);
}

// Tests handler methods that take arguments.
TEST_F(CommandDispatcherTest, TargetWithArguments) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestTargetWithArguments* target =
      [[CommandDispatcherTestTargetWithArguments alloc] init];

  [dispatcher startDispatchingToTarget:target
                           forSelector:@selector(methodWithInt:)];
  [dispatcher startDispatchingToTarget:target
                           forSelector:@selector(methodWithObject:)];
  [dispatcher startDispatchingToTarget:target
                           forSelector:@selector(methodToAddFirstArgument:
                                                                 toSecond:)];

  const int int_argument = 4;
  [dispatcher methodWithInt:int_argument];
  EXPECT_TRUE(target.intMethodCalled);
  EXPECT_FALSE(target.objectMethodCalled);
  EXPECT_EQ(int_argument, target.intArgument);

  [target resetProperties];
  NSObject* object_argument = [[NSObject alloc] init];
  [dispatcher methodWithObject:object_argument];
  EXPECT_FALSE(target.intMethodCalled);
  EXPECT_TRUE(target.objectMethodCalled);
  EXPECT_EQ(object_argument, target.objectArgument);

  [target resetProperties];
  EXPECT_EQ(13, [dispatcher methodToAddFirstArgument:7 toSecond:6]);
  EXPECT_FALSE(target.intMethodCalled);
  EXPECT_FALSE(target.objectMethodCalled);
}

// Tests that messages are routed to the proper handler when multiple targets
// are registered.
TEST_F(CommandDispatcherTest, MultipleTargets) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* showTarget =
      [[CommandDispatcherTestSimpleTarget alloc] init];
  CommandDispatcherTestSimpleTarget* hideTarget =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:showTarget forSelector:@selector(show)];
  [dispatcher startDispatchingToTarget:hideTarget forSelector:@selector(hide)];

  [dispatcher show];
  EXPECT_TRUE(showTarget.showCalled);
  EXPECT_FALSE(hideTarget.showCalled);

  [showTarget resetProperties];
  [dispatcher hide];
  EXPECT_FALSE(showTarget.hideCalled);
  EXPECT_TRUE(hideTarget.hideCalled);
}

// Tests handlers registered via protocols.
TEST_F(CommandDispatcherTest, ProtocolRegistration) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(ShowProtocol)];

  [dispatcher show];
  EXPECT_TRUE(target.showCalled);
  [dispatcher showMore];
  EXPECT_TRUE(target.showCalled);
}

// Tests that handlers are no longer forwarded messages after selector
// deregistration.
TEST_F(CommandDispatcherTest, SelectorDeregistration) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target forSelector:@selector(show)];
  [dispatcher startDispatchingToTarget:target forSelector:@selector(hide)];

  [dispatcher show];
  EXPECT_TRUE(target.showCalled);
  EXPECT_FALSE(target.hideCalled);

  [target resetProperties];
  [dispatcher stopDispatchingForSelector:@selector(show)];
  bool exception_caught = false;
  @try {
    [dispatcher show];
  } @catch (NSException* exception) {
    EXPECT_EQ(NSInvalidArgumentException, [exception name]);
    exception_caught = true;
  }
  EXPECT_TRUE(exception_caught);

  [dispatcher hide];
  EXPECT_FALSE(target.showCalled);
  EXPECT_TRUE(target.hideCalled);
}

// Tests that handlers are no longer forwarded messages after protocol
// deregistration.
TEST_F(CommandDispatcherTest, ProtocolDeregistration) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(ShowProtocol)];
  [dispatcher startDispatchingToTarget:target forSelector:@selector(hide)];

  [dispatcher show];
  EXPECT_TRUE(target.showCalled);
  EXPECT_FALSE(target.showMoreCalled);
  EXPECT_FALSE(target.hideCalled);
  [target resetProperties];
  [dispatcher showMore];
  EXPECT_FALSE(target.showCalled);
  EXPECT_TRUE(target.showMoreCalled);
  EXPECT_FALSE(target.hideCalled);

  [target resetProperties];
  [dispatcher stopDispatchingForProtocol:@protocol(ShowProtocol)];
  bool exception_caught = false;
  @try {
    [dispatcher show];
  } @catch (NSException* exception) {
    EXPECT_EQ(NSInvalidArgumentException, [exception name]);
    exception_caught = true;
  }
  EXPECT_TRUE(exception_caught);
  exception_caught = false;
  @try {
    [dispatcher showMore];
  } @catch (NSException* exception) {
    EXPECT_EQ(NSInvalidArgumentException, [exception name]);
    exception_caught = true;
  }
  EXPECT_TRUE(exception_caught);

  [dispatcher hide];
  EXPECT_FALSE(target.showCalled);
  EXPECT_FALSE(target.showMoreCalled);
  EXPECT_TRUE(target.hideCalled);
}

// Tests that handlers are no longer forwarded messages after target
// deregistration.
TEST_F(CommandDispatcherTest, TargetDeregistration) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* showTarget =
      [[CommandDispatcherTestSimpleTarget alloc] init];
  CommandDispatcherTestSimpleTarget* hideTarget =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:showTarget forSelector:@selector(show)];
  [dispatcher startDispatchingToTarget:hideTarget forSelector:@selector(hide)];

  [dispatcher show];
  EXPECT_TRUE(showTarget.showCalled);
  EXPECT_FALSE(hideTarget.showCalled);

  [dispatcher stopDispatchingToTarget:showTarget];
  bool exception_caught = false;
  @try {
    [dispatcher show];
  } @catch (NSException* exception) {
    EXPECT_EQ(NSInvalidArgumentException, [exception name]);
    exception_caught = true;
  }
  EXPECT_TRUE(exception_caught);

  [dispatcher hide];
  EXPECT_FALSE(showTarget.hideCalled);
  EXPECT_TRUE(hideTarget.hideCalled);
}

// Tests that an exception is thrown when there is no registered handler for a
// given selector.
TEST_F(CommandDispatcherTest, NoTargetRegisteredForSelector) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target forSelector:@selector(show)];

  bool exception_caught = false;
  @try {
    [dispatcher hide];
  } @catch (NSException* exception) {
    EXPECT_EQ(NSInvalidArgumentException, [exception name]);
    exception_caught = true;
  }

  EXPECT_TRUE(exception_caught);
}

// Tests that an exception is not thrown when prepareForShutdown was called
// before the target was removed.
TEST_F(CommandDispatcherTest,
       PrepareForShutdownLetsStopDispatchingForSelectorFailSilently) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  // Check stopDispatchingForSelector:
  [dispatcher startDispatchingToTarget:target forSelector:@selector(show)];
  [dispatcher prepareForShutdown];
  [dispatcher stopDispatchingForSelector:@selector(show)];
  bool exception_caught = false;
  @try {
    [dispatcher show];
  } @catch (NSException* exception) {
    exception_caught = true;
  }

  EXPECT_FALSE(exception_caught);
}

TEST_F(CommandDispatcherTest,
       PrepareForShutdownLetsStopDispatchingForProtocolFailSilently) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  // Check stopDispatchingForProtocol:
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(HideProtocol)];
  [dispatcher prepareForShutdown];
  [dispatcher stopDispatchingForProtocol:@protocol(HideProtocol)];
  bool exception_caught = false;
  @try {
    [dispatcher hide];
  } @catch (NSException* exception) {
    exception_caught = true;
  }

  EXPECT_FALSE(exception_caught);
}

TEST_F(CommandDispatcherTest,
       PrepareForShutdownLetsStopDispatchingToTargetFailSilently) {
  id dispatcher = [[CommandDispatcher alloc] init];
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  // Check stopDispatchingToTarget:
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(HideProtocol)];
  [dispatcher prepareForShutdown];
  [dispatcher stopDispatchingToTarget:target];
  bool exception_caught = false;
  @try {
    [dispatcher hide];
  } @catch (NSException* exception) {
    exception_caught = true;
  }

  EXPECT_FALSE(exception_caught);
}

// Tests that -respondsToSelector returns YES for methods once they are
// dispatched for.
// Tests handler methods with no arguments.
TEST_F(CommandDispatcherTest, RespondsToSelector) {
  id dispatcher = [[CommandDispatcher alloc] init];

  EXPECT_FALSE([dispatcher respondsToSelector:@selector(show)]);
  CommandDispatcherTestSimpleTarget* target =
      [[CommandDispatcherTestSimpleTarget alloc] init];

  [dispatcher startDispatchingToTarget:target forSelector:@selector(show)];
  EXPECT_TRUE([dispatcher respondsToSelector:@selector(show)]);

  [dispatcher stopDispatchingForSelector:@selector(show)];
  EXPECT_FALSE([dispatcher respondsToSelector:@selector(show)]);

  // Actual dispatcher methods should still always advertise that they are
  // responded to.
  EXPECT_TRUE([dispatcher
      respondsToSelector:@selector(startDispatchingToTarget:forSelector:)]);
  EXPECT_TRUE(
      [dispatcher respondsToSelector:@selector(stopDispatchingForSelector:)]);
}

TEST_F(CommandDispatcherTest, DispatchingForProtocol) {
  id dispatcher = [[CommandDispatcher alloc] init];
  NSObject* target = [[NSObject alloc] init];

  // Check that -dispatchingForProtocol tracks simple stop/start.
  EXPECT_FALSE([dispatcher dispatchingForProtocol:@protocol(HideProtocol)]);
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(HideProtocol)];
  EXPECT_TRUE([dispatcher dispatchingForProtocol:@protocol(HideProtocol)]);
  [dispatcher stopDispatchingForProtocol:@protocol(HideProtocol)];
  EXPECT_FALSE([dispatcher dispatchingForProtocol:@protocol(HideProtocol)]);

  // Check that -dispatchingForProtocol handles a conformed protocol.
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(CompositeProtocolWithMethods)];
  EXPECT_FALSE([dispatcher
      dispatchingForProtocol:@protocol(CompositeProtocolWithMethods)]);
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(HideProtocol)];
  EXPECT_TRUE([dispatcher
      dispatchingForProtocol:@protocol(CompositeProtocolWithMethods)]);

  // Check that -dispatchingForProtocol doesn't have a problem with a protocol
  // that also conforms to NSObject.
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(ShowProtocol)];
  EXPECT_TRUE([dispatcher dispatchingForProtocol:@protocol(ShowProtocol)]);

  // Check that conforming to all of the conformed protocols in a protocol with
  // no methods is the same as conforming to that protocol.
  EXPECT_TRUE(
      [dispatcher dispatchingForProtocol:@protocol(EmptyContainerProtocol)]);

  // Check that stopping dispatch to a protocol doesn't stop dispatch to its
  // conformed protocols.
  [dispatcher
      stopDispatchingForProtocol:@protocol(CompositeProtocolWithMethods)];
  EXPECT_TRUE([dispatcher dispatchingForProtocol:@protocol(HideProtocol)]);
}

TEST_F(CommandDispatcherTest, HandlerForProtocol) {
  CommandDispatcher* dispatcher = [[CommandDispatcher alloc] init];
  NSObject* target = [[NSObject alloc] init];

  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(ShowProtocol)];
  id<ShowProtocol> handler = HandlerForProtocol(dispatcher, ShowProtocol);
  EXPECT_EQ(handler, dispatcher);

  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(HideProtocol)];
  [dispatcher startDispatchingToTarget:target
                           forProtocol:@protocol(CompositeProtocolWithMethods)];
  id<EmptyContainerProtocol> container_handler =
      HandlerForProtocol(dispatcher, EmptyContainerProtocol);
  EXPECT_EQ(container_handler, dispatcher);
}
