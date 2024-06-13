// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/perf/perf_test.h"
#import "testing/platform_test.h"

@interface TestObserver : NSObject<ChromeBroadcastObserver>
@property(nonatomic) BOOL lastObservedBool;
@property(nonatomic) CGFloat lastObservedCGFloat;
@property(nonatomic) CGSize lastObservedCGSize;
@property(nonatomic) UIEdgeInsets lastObservedUIEdgeInsets;
@property(nonatomic) NSInteger tabStripVisibleCallCount;
@property(nonatomic) NSInteger contentScrollOffsetCallCount;
@property(nonatomic) NSInteger scrollViewSizeCallCount;
@property(nonatomic) NSInteger contentSizeCallCount;
@property(nonatomic) NSInteger contentInsetCallCount;
@end

@implementation TestObserver
@synthesize lastObservedBool = _lastObservedBool;
@synthesize lastObservedCGFloat = _lastObservedCGFloat;
@synthesize lastObservedCGSize = _lastObservedCGSize;
@synthesize lastObservedUIEdgeInsets = _lastObservedUIEdgeInsets;
@synthesize tabStripVisibleCallCount = _tabStripVisibleCallCount;
@synthesize contentScrollOffsetCallCount = _contentScrollOffsetCallCount;
@synthesize scrollViewSizeCallCount = _scrollViewSizeCallCount;
@synthesize contentSizeCallCount = _contentSizeCallCount;
@synthesize contentInsetCallCount = _contentInsetCallCount;

- (void)broadcastScrollViewIsScrolling:(BOOL)visible {
  self.tabStripVisibleCallCount++;
  self.lastObservedBool = visible;
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  self.contentScrollOffsetCallCount++;
  self.lastObservedCGFloat = offset;
}

- (void)broadcastScrollViewSize:(CGSize)scrollViewSize {
  self.scrollViewSizeCallCount++;
  self.lastObservedCGSize = scrollViewSize;
}

- (void)broadcastScrollViewContentSize:(CGSize)contentSize {
  self.contentSizeCallCount++;
  self.lastObservedCGSize = contentSize;
}

- (void)broadcastScrollViewContentInset:(UIEdgeInsets)contentInset {
  self.contentInsetCallCount++;
  self.lastObservedUIEdgeInsets = contentInset;
}

@end

@interface TestObservable : NSObject
@property(nonatomic) BOOL observableBool;
@property(nonatomic) CGFloat observableCGFloat;
@property(nonatomic) CGSize observableCGSize;
@property(nonatomic) UIEdgeInsets observableUIEdgeInsets;
@end
@implementation TestObservable
@synthesize observableBool = _observableBool;
@synthesize observableCGFloat = _observableCGFloat;
@synthesize observableCGSize = _observableCGSize;
@synthesize observableUIEdgeInsets = _observableUIEdgeInsets;
@end

typedef PlatformTest ChromeBroadcasterTest;

TEST_F(ChromeBroadcasterTest, TestBroadcastBoolFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableBool = NO;

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewIsScrolling:)];

  observable.observableBool = YES;

  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewIsScrolling:)];
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  EXPECT_TRUE(observer.lastObservedBool);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestBroadcastFloatFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveBoolFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewIsScrolling:)];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableBool = YES;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewIsScrolling:)];
  EXPECT_TRUE(observer.lastObservedBool);
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveFloatFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(1.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);

  observable.observableCGFloat = 2.0;
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveScrollViewSizeFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.scrollViewSizeCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewSize:)];
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.scrollViewSizeCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  CGSize kScrollViewSize1 = CGSizeMake(100, 100);
  observable.observableCGSize = kScrollViewSize1;
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.scrollViewSizeCallCount);

  [broadcaster broadcastValue:@"observableCGSize"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewSize:)];
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, kScrollViewSize1));
  EXPECT_EQ(1, observer.scrollViewSizeCallCount);

  CGSize kScrollViewSize2 = CGSizeMake(200, 200);
  observable.observableCGSize = kScrollViewSize2;
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, kScrollViewSize2));
  EXPECT_EQ(2, observer.scrollViewSizeCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveContentSizeFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.contentSizeCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewContentSize:)];
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.contentSizeCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  CGSize kContentViewSize1 = CGSizeMake(100, 100);
  observable.observableCGSize = kContentViewSize1;
  EXPECT_TRUE(CGSizeEqualToSize(observer.lastObservedCGSize, CGSizeZero));
  EXPECT_EQ(0, observer.contentSizeCallCount);

  [broadcaster broadcastValue:@"observableCGSize"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewContentSize:)];
  EXPECT_TRUE(
      CGSizeEqualToSize(observer.lastObservedCGSize, kContentViewSize1));
  EXPECT_EQ(1, observer.contentSizeCallCount);

  CGSize kContentViewSize2 = CGSizeMake(200, 200);
  observable.observableCGSize = kContentViewSize2;
  EXPECT_TRUE(
      CGSizeEqualToSize(observer.lastObservedCGSize, kContentViewSize2));
  EXPECT_EQ(2, observer.contentSizeCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveContentInsetFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(observer.lastObservedUIEdgeInsets,
                                            UIEdgeInsetsZero));
  EXPECT_EQ(0, observer.contentInsetCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewContentInset:)];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(observer.lastObservedUIEdgeInsets,
                                            UIEdgeInsetsZero));
  EXPECT_EQ(0, observer.contentInsetCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  UIEdgeInsets kInsets1 = UIEdgeInsetsMake(1, 1, 1, 1);
  observable.observableUIEdgeInsets = kInsets1;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(observer.lastObservedUIEdgeInsets,
                                            UIEdgeInsetsZero));
  EXPECT_EQ(0, observer.contentInsetCallCount);

  [broadcaster broadcastValue:@"observableUIEdgeInsets"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewContentInset:)];
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(observer.lastObservedUIEdgeInsets,
                                            kInsets1));
  EXPECT_EQ(1, observer.contentInsetCallCount);

  UIEdgeInsets kInsets2 = UIEdgeInsetsMake(2, 2, 2, 2);
  observable.observableUIEdgeInsets = kInsets2;
  EXPECT_TRUE(UIEdgeInsetsEqualToEdgeInsets(observer.lastObservedUIEdgeInsets,
                                            kInsets2));
  EXPECT_EQ(2, observer.contentInsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestBroadcastManyFloats) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  NSMutableArray<TestObserver*>* observers = [[NSMutableArray alloc] init];
  for (size_t i = 0; i < 100; i++) {
    [observers addObject:[[TestObserver alloc] init]];
    [broadcaster addObserver:observers.lastObject
                 forSelector:@selector(broadcastContentScrollOffset:)];
  }

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];
  // All observers should have the initial value set.
  for (TestObserver* observer in observers) {
    EXPECT_EQ(1.0, observer.lastObservedCGFloat);
    EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  }

  // Change the value a thousand times.
  NSDate* start = [NSDate date];
  for (size_t i = 0; i < 1000; i++) {
    observable.observableCGFloat += 1.0;
  }
  NSTimeInterval elapsed = -[start timeIntervalSinceNow] * 1000.0 /* to ms */;

  // Log the elapsed time for performance tracking.
  perf_test::PrintResult("Broadcast", "", "100 observers, 1000 updates",
                         elapsed, "ms", true /* "important" */);

  EXPECT_EQ(1001.0, observable.observableCGFloat);
  for (TestObserver* observer in observers) {
    EXPECT_EQ(1001.0, observer.lastObservedCGFloat);
    EXPECT_EQ(1001, observer.contentScrollOffsetCallCount);
  }
}

TEST_F(ChromeBroadcasterTest, TestBroadcastDuplicateFloats) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 2.0;
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestSeparateObservers) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* boolObserver = [[TestObserver alloc] init];
  TestObserver* floatObserver = [[TestObserver alloc] init];

  TestObservable* observable = [[TestObservable alloc] init];

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  [broadcaster addObserver:boolObserver
               forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster addObserver:floatObserver
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_FALSE(boolObserver.lastObservedBool);
  EXPECT_EQ(1, boolObserver.tabStripVisibleCallCount);
  EXPECT_EQ(0, floatObserver.tabStripVisibleCallCount);
  EXPECT_EQ(0.0, floatObserver.lastObservedCGFloat);
  EXPECT_EQ(1, floatObserver.contentScrollOffsetCallCount);
  EXPECT_EQ(0, boolObserver.contentScrollOffsetCallCount);

  observable.observableCGFloat = 5.0;
  EXPECT_EQ(5.0, floatObserver.lastObservedCGFloat);
  EXPECT_EQ(2, floatObserver.contentScrollOffsetCallCount);
  EXPECT_EQ(0.0, boolObserver.lastObservedCGFloat);
  EXPECT_EQ(0, boolObserver.contentScrollOffsetCallCount);

  observable.observableBool = YES;
  EXPECT_TRUE(boolObserver.lastObservedBool);
  EXPECT_EQ(2, boolObserver.tabStripVisibleCallCount);
  EXPECT_FALSE(floatObserver.lastObservedBool);
  EXPECT_EQ(0, floatObserver.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestStopBroadcasting) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastContentScrollOffset:)];
  observable.observableCGFloat = 4.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestStopObserving) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;
  observable.observableBool = YES;
  TestObserver* observer = [[TestObserver alloc] init];

  [broadcaster addObserver:observer
               forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  EXPECT_TRUE(observer.lastObservedBool);
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  [broadcaster removeObserver:observer
                  forSelector:@selector(broadcastContentScrollOffset:)];
  observable.observableCGFloat = 4.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
  [broadcaster removeObserver:observer
                  forSelector:@selector(broadcastScrollViewIsScrolling:)];
  observable.observableBool = YES;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}
