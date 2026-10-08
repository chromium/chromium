// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/posted_observer_list.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* const kRegisteredEvent = @"registered";
NSString* const kReRegisteredEvent = @"re-registered";
NSString* const kFirstEvent = @"first";
NSString* const kSecondEvent = @"second";
NSString* const kThirdEvent = @"third";

}  // namespace

// Protocol notified through the `PostedObserverList` under test.
@protocol PostedObserverListTestObserver <NSObject>
- (void)didReceiveEvent:(NSString*)event;
@end

// Records the events it receives, in order.
@interface FakePostedObserverListTestObserver
    : NSObject <PostedObserverListTestObserver>
@property(nonatomic, readonly) NSMutableArray<NSString*>* events;
// Event that triggers `onTriggerEvent`, after it is recorded.
@property(nonatomic, copy) NSString* triggerEvent;
// Called with the receiver when it receives `triggerEvent`, e.g. to re-enter
// the list from within a notification.
@property(nonatomic, copy) void (^onTriggerEvent)
    (FakePostedObserverListTestObserver* receiver);
@end

@implementation FakePostedObserverListTestObserver

- (instancetype)init {
  if ((self = [super init])) {
    _events = [NSMutableArray array];
  }
  return self;
}

- (void)didReceiveEvent:(NSString*)event {
  [_events addObject:event];
  if (_onTriggerEvent && [event isEqualToString:_triggerEvent]) {
    _onTriggerEvent(self);
  }
}

@end

class PostedObserverListTest : public PlatformTest {
 protected:
  PostedObserverListTest() {
    list_ = [[PostedObserverList alloc]
        initWithProtocol:@protocol(PostedObserverListTestObserver)];
  }

  // Posts `event` to the observers of `list_`.
  void Notify(NSString* event) {
    [list_ postNotification:^(id<PostedObserverListTestObserver> observers) {
      [observers didReceiveEvent:event];
    }];
  }

  // Registers `observer` with a callback that records `registered_event`.
  void AddObserverWithRegisteredEvent(
      FakePostedObserverListTestObserver* observer,
      NSString* registered_event = kRegisteredEvent) {
    [list_ addObserver:observer
          onRegistered:^(id<PostedObserverListTestObserver> registered) {
            [registered didReceiveEvent:registered_event];
          }];
  }

  base::test::TaskEnvironment task_environment_;
  PostedObserverList<id<PostedObserverListTestObserver>>* list_ = nil;
};

// Tests that notifications are delivered asynchronously.
TEST_F(PostedObserverListTest, NotifyIsPosted) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];
  [list_ addObserver:observer];
  actor::FlushCurrentSequence();

  Notify(kFirstEvent);
  EXPECT_EQ(0u, observer.events.count);

  actor::FlushCurrentSequence();
  EXPECT_NSEQ(@[ kFirstEvent ], observer.events);
}

// Tests that an observer receives the notifications posted after it was added,
// and not the ones already queued, even if they were not delivered yet.
TEST_F(PostedObserverListTest, ObserverSkipsNotificationsPostedBeforeAdd) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  Notify(kFirstEvent);
  [list_ addObserver:observer];
  Notify(kSecondEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ(@[ kSecondEvent ], observer.events);
}

// Tests that the registration callback runs asynchronously, before the
// notifications posted after the observer was added.
TEST_F(PostedObserverListTest, RegisteredBlockRunsBeforeLaterNotifications) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  EXPECT_EQ(0u, observer.events.count);

  actor::FlushCurrentSequence();
  EXPECT_NSEQ((@[ kRegisteredEvent, kFirstEvent ]), observer.events);
}

// Tests that removing an observer before its registration completes cancels
// the registration.
TEST_F(PostedObserverListTest, RemoveCancelsPendingRegistration) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  [list_ removeObserver:observer];
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_EQ(0u, observer.events.count);
}

// Tests that removing an observer cancels the notifications already queued for
// it.
TEST_F(PostedObserverListTest, RemoveCancelsPendingNotifications) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];
  [list_ addObserver:observer];
  actor::FlushCurrentSequence();

  Notify(kFirstEvent);
  [list_ removeObserver:observer];
  actor::FlushCurrentSequence();

  EXPECT_EQ(0u, observer.events.count);
}

// Tests that an observer removed and re-added before its first registration
// completes only receives the second registration and the notifications posted
// after it, not the stale registration or the notifications posted before.
TEST_F(PostedObserverListTest, ReAddBeforeRegistrationCompletesStartsFresh) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  [list_ removeObserver:observer];
  Notify(kSecondEvent);
  AddObserverWithRegisteredEvent(observer, kReRegisteredEvent);
  Notify(kThirdEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kReRegisteredEvent, kThirdEvent ]), observer.events);
}

// Tests that an observer removed and re-added after its registration completed
// receives a fresh registration, and misses the notifications posted while it
// was removed.
TEST_F(PostedObserverListTest, ReAddAfterRegistrationCompletesStartsFresh) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];
  AddObserverWithRegisteredEvent(observer);
  actor::FlushCurrentSequence();

  [list_ removeObserver:observer];
  Notify(kFirstEvent);
  AddObserverWithRegisteredEvent(observer, kReRegisteredEvent);
  Notify(kSecondEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kRegisteredEvent, kReRegisteredEvent, kSecondEvent ]),
              observer.events);
}

// Tests that adding the same observer twice registers it once.
TEST_F(PostedObserverListTest, DuplicateAddRegistersOnce) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kRegisteredEvent, kFirstEvent ]), observer.events);
}

// Tests that adding an observer whose registration already completed does not
// call the registration callback again.
TEST_F(PostedObserverListTest, DuplicateAddAfterRegistrationIsIgnored) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  actor::FlushCurrentSequence();
  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kRegisteredEvent, kFirstEvent ]), observer.events);
}

// Tests that an observer removing itself from within a notification does not
// receive the notifications already queued.
TEST_F(PostedObserverListTest, ObserverRemovesItselfDuringNotification) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];
  observer.triggerEvent = kFirstEvent;
  observer.onTriggerEvent = ^(FakePostedObserverListTestObserver* receiver) {
    [list_ removeObserver:receiver];
  };
  [list_ addObserver:observer];
  actor::FlushCurrentSequence();

  Notify(kFirstEvent);
  Notify(kSecondEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ(@[ kFirstEvent ], observer.events);
}

// Tests that an observer removed from within a notification, before it was
// reached, does not receive that notification.
TEST_F(PostedObserverListTest, ObserverRemovedByAnotherDuringNotification) {
  FakePostedObserverListTestObserver* first_observer =
      [[FakePostedObserverListTestObserver alloc] init];
  FakePostedObserverListTestObserver* second_observer =
      [[FakePostedObserverListTestObserver alloc] init];
  first_observer.triggerEvent = kFirstEvent;
  first_observer.onTriggerEvent =
      ^(FakePostedObserverListTestObserver* receiver) {
        [list_ removeObserver:second_observer];
      };
  [list_ addObserver:first_observer];
  [list_ addObserver:second_observer];
  actor::FlushCurrentSequence();

  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ(@[ kFirstEvent ], first_observer.events);
  EXPECT_EQ(0u, second_observer.events.count);
}

// Tests that an observer removing itself from within its registration callback
// does not receive the notifications posted after it was added.
TEST_F(PostedObserverListTest, ObserverRemovesItselfDuringRegistration) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];
  observer.triggerEvent = kRegisteredEvent;
  observer.onTriggerEvent = ^(FakePostedObserverListTestObserver* receiver) {
    [list_ removeObserver:receiver];
  };

  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ(@[ kRegisteredEvent ], observer.events);
}

// Tests that an observer added from within a notification skips the
// notifications already queued at that point, and receives the later ones.
TEST_F(PostedObserverListTest, ObserverAddedDuringNotification) {
  FakePostedObserverListTestObserver* first_observer =
      [[FakePostedObserverListTestObserver alloc] init];
  FakePostedObserverListTestObserver* second_observer =
      [[FakePostedObserverListTestObserver alloc] init];
  first_observer.triggerEvent = kFirstEvent;
  first_observer.onTriggerEvent =
      ^(FakePostedObserverListTestObserver* receiver) {
        AddObserverWithRegisteredEvent(second_observer);
      };
  [list_ addObserver:first_observer];
  actor::FlushCurrentSequence();

  // `kSecondEvent` is queued before `second_observer` is added.
  Notify(kFirstEvent);
  Notify(kSecondEvent);
  actor::FlushCurrentSequence();
  Notify(kThirdEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kFirstEvent, kSecondEvent, kThirdEvent ]),
              first_observer.events);
  EXPECT_NSEQ((@[ kRegisteredEvent, kThirdEvent ]), second_observer.events);
}

// Tests that pending registrations and notifications complete after the owner
// releases the list, and that the list is deallocated once they have run.
TEST_F(PostedObserverListTest, PendingWorkCompletesAfterListReleased) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  Notify(kFirstEvent);
  __weak PostedObserverList* weak_list = list_;
  list_ = nil;
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kRegisteredEvent, kFirstEvent ]), observer.events);
  EXPECT_EQ(nil, weak_list);
}

// Tests that the list, including a pending registration, does not retain its
// observers.
TEST_F(PostedObserverListTest, ObserversAreHeldWeakly) {
  __weak FakePostedObserverListTestObserver* weak_observer = nil;
  @autoreleasepool {
    FakePostedObserverListTestObserver* observer =
        [[FakePostedObserverListTestObserver alloc] init];
    weak_observer = observer;
    AddObserverWithRegisteredEvent(observer);
  }
  EXPECT_EQ(nil, weak_observer);

  // Completing the registration of, and notifying, a deallocated observer is a
  // no-op.
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();
}

// Tests that removing a nil observer is a no-op and does not affect pending
// registrations.
TEST_F(PostedObserverListTest, RemoveNilObserverIsNoOp) {
  FakePostedObserverListTestObserver* observer =
      [[FakePostedObserverListTestObserver alloc] init];

  AddObserverWithRegisteredEvent(observer);
  [list_ removeObserver:nil];
  Notify(kFirstEvent);
  actor::FlushCurrentSequence();

  EXPECT_NSEQ((@[ kRegisteredEvent, kFirstEvent ]), observer.events);
}
