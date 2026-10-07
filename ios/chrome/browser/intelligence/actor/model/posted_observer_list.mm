// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/posted_observer_list.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/location.h"
#import "base/memory/scoped_refptr.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"

namespace {

// Called with an observer once its registration completes.
using RegisteredBlock = void (^)(id observer);

// Returns whether `observer` is registered in `observers`.
bool ContainsObserver(CRBProtocolObservers* observers, id observer) {
  __block bool contains = false;
  [observers executeOnObservers:^(id candidate) {
    contains |= candidate == observer;
  }];
  return contains;
}

}  // namespace

@implementation PostedObserverList {
  // Ensures single-sequence use, which message ordering relies on.
  SEQUENCE_CHECKER(_sequenceChecker);

  // Runner for registrations and notifications. Being sequenced keeps them in
  // posting order, which the delivery contract relies on.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Registered observers, held weakly.
  CRBProtocolObservers* _observers;

  // Pending registrations, mapping each observer, held weakly and compared by
  // identity, to its registration ID. Removing an entry cancels it. Entries of
  // deallocated observers may linger until the next mutation, but never match.
  NSMapTable<id, NSNumber*>* _pendingObserverIDs;

  // Source of registration IDs, which tell a registration apart from a later
  // one of the same observer.
  NSUInteger _observerIDCounter;
}

- (instancetype)initWithProtocol:(Protocol*)protocol {
  if ((self = [super init])) {
    _taskRunner = base::SequencedTaskRunner::GetCurrentDefault();
    _observers = [CRBProtocolObservers observersWithProtocol:protocol];
    // Keys compare by pointer, not `-isEqual:`, to match `CRBProtocolObservers`
    // membership, so observers overriding `-isEqual:` stay distinct.
    _pendingObserverIDs = [[NSMapTable alloc]
        initWithKeyOptions:NSPointerFunctionsWeakMemory |
                           NSPointerFunctionsObjectPointerPersonality
              valueOptions:NSPointerFunctionsStrongMemory
                  capacity:0];
  }
  return self;
}

#pragma mark - Public

- (void)addObserver:(id)observer {
  [self addObserver:observer onRegistered:nil];
}

- (void)addObserver:(id)observer onRegistered:(RegisteredBlock)onRegistered {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Checked here because the add to `_observers` is posted, and nil would match
  // records of deallocated observers.
  CHECK(observer);
  CHECK([observer conformsToProtocol:_observers.protocol]);
  // A duplicate registration would send `onRegistered` twice.
  if (ContainsObserver(_observers, observer) ||
      [_pendingObserverIDs objectForKey:observer]) {
    return;
  }
  NSNumber* observerID = @(++_observerIDCounter);
  [_pendingObserverIDs setObject:observerID forKey:observer];
  // Weak, so a pending registration does not extend the observer's lifetime.
  __weak id weakObserver = observer;
  // Posted, so `observer` joins after already queued notifications and before
  // later ones. Strong `self` so pending work survives the owner releasing the
  // list.
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          [self completeRegistrationOfObserver:weakObserver
                                                    observerID:observerID
                                                  onRegistered:onRegistered];
                        }));
}

- (void)removeObserver:(id)observer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // nil would match records of deallocated observers.
  if (!observer) {
    return;
  }
  // Cancels a pending registration. Re-adding issues a new ID, so the cancelled
  // completion stays a no-op.
  [_pendingObserverIDs removeObjectForKey:observer];
  [_observers removeObserver:observer];
}

- (void)postNotification:(void (^)(id observers))notification {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK(notification);
  // Strong `self` so pending work survives the owner releasing the list.
  _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                          [self deliverNotification:notification];
                        }));
}

#pragma mark - Private

// Moves `observer` to `_observers` and calls `onRegistered`. No-op if the
// registration identified by `observerID` was cancelled, or if `observer` was
// deallocated.
- (void)completeRegistrationOfObserver:(id)observer
                            observerID:(NSNumber*)observerID
                          onRegistered:(RegisteredBlock)onRegistered {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!observer ||
      ![[_pendingObserverIDs objectForKey:observer] isEqual:observerID]) {
    return;
  }
  [_pendingObserverIDs removeObjectForKey:observer];
  [_observers addObserver:observer];
  if (onRegistered) {
    onRegistered(observer);
  }
}

// Calls `notification` with the observers registered at delivery time.
- (void)deliverNotification:(void (^)(id observers))notification {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  notification(_observers);
}

@end
