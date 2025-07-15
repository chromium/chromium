// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/notification_metrics_recorder.h"

#import <UserNotifications/UserNotifications.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/cxx23_to_underlying.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@interface NotificationMetricsRecorder ()
// Stores (as the key) the identifier for notifications that have been
// delivered and (as the value) the type of notification. This mirrors the
// list of notifications returned by the notification center's
// `getDeliveredNotifications`.
@property(nonatomic, strong)
    NSDictionary<NSString*, NSNumber*>* deliveredNotifications;
// A reference to the local state prefs.
@property(nonatomic, readonly) PrefService* localState;
@end

@implementation NotificationMetricsRecorder {
  // The notification center used to get the list of delivered notifications.
  UNUserNotificationCenter* _notificationCenter;
}

@synthesize deliveredNotifications = _deliveredNotifications;
@synthesize localState = _localState;

- (instancetype)initWithNotificationCenter:
    (UNUserNotificationCenter*)notificationCenter {
  self = [super init];
  if (self) {
    _notificationCenter = notificationCenter;
  }
  return self;
}

- (void)handleDeliveredNotificationsWithClosure:(base::OnceClosure)closure {
  __weak __typeof(self) weakSelf = self;
  scoped_refptr<base::SequencedTaskRunner> taskRunner =
      base::SequencedTaskRunner::GetCurrentDefault();
  __block base::OnceClosure closureCopy = std::move(closure);
  [_notificationCenter getDeliveredNotificationsWithCompletionHandler:^(
                           NSArray<UNNotification*>* notifications) {
    taskRunner->PostTask(
        FROM_HERE, base::BindOnce(^{
                     [weakSelf processDeliveredNotifications:notifications];
                   }).Then(std::move(closureCopy)));
  }];
}

- (BOOL)wasDelivered:(UNNotification*)notification {
  return self.deliveredNotifications[notification.request.identifier] != nil;
}

- (void)recordReceived:(UNNotification*)notification {
  NotificationType type = [self.classifier classifyNotification:notification];
  [self addDeliveredNotification:notification.request.identifier type:type];
  [self recordTypeReceived:type];
}

- (void)recordInteraction:(UNNotification*)notification {
  if ([self wasDelivered:notification]) {
    NotificationType type = static_cast<NotificationType>(
        self.deliveredNotifications[notification.request.identifier].intValue);
    [self recordTypeInteraction:type];
    // Remove from the list so that it doesn't get marked "dismissed" the next
    // time the list is processed.
    [self removeDeliveredNotification:notification.request.identifier];
    return;
  }

  // This notification has not been previously handled.
  NotificationType type = [self.classifier classifyNotification:notification];
  [self recordTypeReceived:type];
  [self recordTypeInteraction:type];
}

#pragma mark - Accessors

- (NSDictionary<NSString*, NSNumber*>*)deliveredNotifications {
  if (!_deliveredNotifications) {
    // Load from local state prefs.
    const base::Value::Dict& dict =
        self.localState->GetDict(prefs::kHandledDeliveredNotificationIds);
    NSMutableDictionary<NSString*, NSNumber*>* newDict =
        [NSMutableDictionary dictionary];
    for (auto it : dict) {
      if (!it.second.is_int()) {
        continue;
      }
      newDict[base::SysUTF8ToNSString(it.first)] = @(it.second.GetInt());
    }
    _deliveredNotifications = newDict;
  }
  return _deliveredNotifications;
}

- (void)setDeliveredNotifications:
    (NSDictionary<NSString*, NSNumber*>*)notifications {
  // Persist to local state prefs.
  base::Value::Dict newDict;
  for (NSString* identifier in notifications) {
    newDict.Set(base::SysNSStringToUTF8(identifier),
                notifications[identifier].intValue);
  }
  self.localState->SetDict(prefs::kHandledDeliveredNotificationIds,
                           std::move(newDict));
  _deliveredNotifications = notifications;
}

- (PrefService*)localState {
  if (!_localState) {
    _localState = GetApplicationContext()->GetLocalState();
  }
  return _localState;
}

#pragma mark - Private

// Adds a notification that has been handled to the `deliveredNotifications`
// dictionary, or updates the `type`.
- (void)addDeliveredNotification:(NSString*)identifier
                            type:(NotificationType)type {
  NSMutableDictionary<NSString*, NSNumber*>* newDict =
      [self.deliveredNotifications mutableCopy];
  newDict[identifier] = @(base::to_underlying(type));
  self.deliveredNotifications = newDict;
}

// Removes a notification from the `deliveredNotifications` dictionary.
- (void)removeDeliveredNotification:(NSString*)identifier {
  NSMutableDictionary<NSString*, NSNumber*>* newDict =
      [self.deliveredNotifications mutableCopy];
  [newDict removeObjectForKey:identifier];
  self.deliveredNotifications = newDict;
}

// Processes the array of notifications returned from the UNNotificationCenter
// in order to record metrics about delivered or dismissed notifications.
- (void)processDeliveredNotifications:(NSArray<UNNotification*>*)notifications {
  NSMutableDictionary<NSString*, NSNumber*>* newDeliveredNotifications =
      [NSMutableDictionary dictionaryWithCapacity:notifications.count];

  for (UNNotification* notification in notifications) {
    NSString* identifier = notification.request.identifier;
    if ([self wasDelivered:notification]) {
      newDeliveredNotifications[identifier] =
          self.deliveredNotifications[identifier];
      continue;
    }
    NotificationType type = [self.classifier classifyNotification:notification];
    newDeliveredNotifications[identifier] = @(base::to_underlying(type));
    [self recordTypeReceived:type];
  }

  // Record the dismissed notifications.
  NSMutableDictionary<NSString*, NSNumber*>* dismissed =
      [self.deliveredNotifications mutableCopy];
  [dismissed removeObjectsForKeys:newDeliveredNotifications.allKeys];
  for (NSString* identifier in dismissed) {
    NotificationType type =
        static_cast<NotificationType>(dismissed[identifier].intValue);
    [self recordTypeDismissed:type];
  }

  self.deliveredNotifications = newDeliveredNotifications;
}

// Records a histogram for dismissed notifications.
- (void)recordTypeDismissed:(NotificationType)type {
  base::UmaHistogramEnumeration("IOS.Notification.Dismissed", type);
}

// Records a histogram for received notifications.
- (void)recordTypeReceived:(NotificationType)type {
  base::UmaHistogramEnumeration("IOS.Notification.Received", type);
}

// Records a histogram for notification interations.
- (void)recordTypeInteraction:(NotificationType)type {
  base::UmaHistogramEnumeration("IOS.Notification.Interaction", type);
}

@end
