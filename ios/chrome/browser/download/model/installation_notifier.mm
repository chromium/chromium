// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/installation_notifier.h"
#import "ios/chrome/browser/download/model/installation_notifier+Testing.h"

#import <UIKit/UIKit.h>

#import <stdint.h>
#import <memory>

#import "base/check_op.h"
#import "base/metrics/histogram_macros.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/base/backoff_entry.h"
#import "url/gurl.h"

namespace {
const net::BackoffEntry::Policy kPollingBackoffPolicy = {
    0,          // Number of errors to ignore.
    1 * 1000,   // Initial delay in milliseconds.
    1.5,        // Multiply factor.
    0.1,        // Jitter factor.
    60 * 1000,  // Maximum backoff in milliseconds.
    -1,         // Entry lifetime.
    false       // Always use initial delay.
};
}  // namespace

@interface DefaultDispatcher : NSObject <DispatcherProtocol>
@end

@implementation DefaultDispatcher
- (void)dispatchAfter:(int64_t)delayInNSec withBlock:(dispatch_block_t)block {
  dispatch_time_t dispatchTime = dispatch_time(DISPATCH_TIME_NOW, delayInNSec);
  dispatch_after(dispatchTime, dispatch_get_main_queue(), block);
}
@end

@interface InstallationNotifier ()
// Dispatched blocks cannot be cancelled. Instead, each block has a `blockId`.
// If `blockId` is different from `lastCreatedBlockId_`, then the block does
// not execute anything.
@property(nonatomic, readonly) int lastCreatedBlockId;
@end

@implementation InstallationNotifier {
  std::unique_ptr<net::BackoffEntry> _backoffEntry;
  id<DispatcherProtocol> _dispatcher;
  // Dictionary mapping URL schemes to mutable sets of observers.
  NSMutableDictionary* _installedAppObservers;
  __weak NSNotificationCenter* _notificationCenter;

  // This object can be a fake application in unittests.
  __weak UIApplication* _sharedApplication;
}

@synthesize lastCreatedBlockId = _lastCreatedBlockId;

+ (InstallationNotifier*)sharedInstance {
  static InstallationNotifier* instance = [[InstallationNotifier alloc] init];
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _lastCreatedBlockId = 0;
    _dispatcher = [[DefaultDispatcher alloc] init];
    _installedAppObservers = [[NSMutableDictionary alloc] init];
    _notificationCenter = [NSNotificationCenter defaultCenter];
    _backoffEntry.reset(new net::BackoffEntry([self backOffPolicy]));
  }
  return self;
}

- (void)registerForInstallationNotifications:(id)observer
                                withSelector:(SEL)notificationSelector
                                   forScheme:(NSString*)scheme {
  [self registerForInstallationNotifications:observer
                                withSelector:notificationSelector
                                   forScheme:scheme
                                startPolling:YES];
}

- (void)registerForInstallationNotifications:(id)observer
                                withSelector:(SEL)notificationSelector
                                   forScheme:(NSString*)scheme
                                startPolling:(BOOL)poll {
  // Workaround a crash caused by calls to this function with a nil `scheme`.
  if (![scheme length])
    return;
  DCHECK([observer respondsToSelector:notificationSelector]);
  DCHECK([scheme rangeOfString:@":"].location == NSNotFound);
  // A strong reference would prevent the observer from unregistering itself
  // from its dealloc method, because the dealloc itself would never be called.
  NSValue* weakReferenceToObserver =
      [NSValue valueWithNonretainedObject:observer];
  NSMutableSet* observers = [_installedAppObservers objectForKey:scheme];
  if (!observers)
    observers = [[NSMutableSet alloc] init];
  if ([observers containsObject:weakReferenceToObserver])
    return;
  [observers addObject:weakReferenceToObserver];
  [_installedAppObservers setObject:observers forKey:scheme];
  [_notificationCenter addObserver:observer
                          selector:notificationSelector
                              name:scheme
                            object:self];
  _backoffEntry->Reset();
  if (poll)
    [self dispatchInstallationNotifierBlock];
}

- (void)unregisterForNotifications:(id)observer {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  NSValue* weakReferenceToObserver =
      [NSValue valueWithNonretainedObject:observer];
  [_notificationCenter removeObserver:observer];
  for (NSString* scheme in [_installedAppObservers allKeys]) {
    DCHECK([scheme isKindOfClass:[NSString class]]);
    NSMutableSet* observers = [_installedAppObservers objectForKey:scheme];
    if ([observers containsObject:weakReferenceToObserver]) {
      [observers removeObject:weakReferenceToObserver];
      if ([observers count] == 0) {
        [_installedAppObservers removeObjectForKey:scheme];
      }
    }
  }
}

- (void)checkNow {
  // Reset the back off polling.
  _backoffEntry->Reset();
  [self pollForTheInstallationOfApps];
}

- (void)stopPolling {
  // Increment the queued block ID, making it higher than the block ID of any
  // currently queued block, which will prevent them from running (and from
  // queueing any new blocks).
  ++_lastCreatedBlockId;
}

- (void)dispatchInstallationNotifierBlock {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  int blockId = ++_lastCreatedBlockId;
  _backoffEntry->InformOfRequest(false);
  int64_t delayInNSec =
      _backoffEntry->GetTimeUntilRelease().InMicroseconds() * NSEC_PER_USEC;
  __weak InstallationNotifier* weakSelf = self;
  [_dispatcher dispatchAfter:delayInNSec
                   withBlock:^{
                     InstallationNotifier* strongSelf = weakSelf;
                     if (blockId == [strongSelf lastCreatedBlockId]) {
                       [strongSelf pollForTheInstallationOfApps];
                     }
                   }];
}

- (void)pollForTheInstallationOfApps {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  __block BOOL keepPolling = NO;
  NSMutableSet* keysToDelete = [NSMutableSet set];
  [_installedAppObservers enumerateKeysAndObjectsUsingBlock:^(
                              id scheme, id observers, BOOL* stop) {
    DCHECK([scheme isKindOfClass:[NSString class]]);
    DCHECK([observers isKindOfClass:[NSMutableSet class]]);
    DCHECK([observers count] > 0);
    NSURL* testSchemeURL =
        [NSURL URLWithString:[NSString stringWithFormat:@"%@:", scheme]];
    if ([[UIApplication sharedApplication] canOpenURL:testSchemeURL]) {
      [_notificationCenter postNotificationName:scheme object:self];
      for (id weakReferenceToObserver in observers) {
        id observer = [weakReferenceToObserver nonretainedObjectValue];
        [_notificationCenter removeObserver:observer name:scheme object:self];
      }
      if (![keysToDelete containsObject:scheme]) {
        [keysToDelete addObject:scheme];
      }
    } else {
      keepPolling = YES;
    }
  }];
  [_installedAppObservers removeObjectsForKeys:[keysToDelete allObjects]];
  if (keepPolling)
    [self dispatchInstallationNotifierBlock];
}

- (net::BackoffEntry::Policy const*)backOffPolicy {
  return &kPollingBackoffPolicy;
}

#pragma mark -
#pragma mark Testing setters

- (void)setDispatcher:(id<DispatcherProtocol>)dispatcher {
  _dispatcher = dispatcher;
}

- (void)resetDispatcher {
  _dispatcher = [[DefaultDispatcher alloc] init];
}

@end
