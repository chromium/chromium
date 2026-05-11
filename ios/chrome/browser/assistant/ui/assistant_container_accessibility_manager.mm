// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_accessibility_manager.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AssistantContainerAccessibilityManager {
  __weak AssistantGrabberButton* _grabberButton;
  __weak id<AssistantContainerAccessibilityManagerDelegate> _delegate;
  AssistantContainerDetent _currentDetent;
  std::vector<AssistantContainerDetent> _detents;
}

- (instancetype)
    initWithGrabberButton:(AssistantGrabberButton*)grabberButton
                 delegate:(id<AssistantContainerAccessibilityManagerDelegate>)
                              delegate {
  self = [super init];
  if (self) {
    _grabberButton = grabberButton;
    _delegate = delegate;
  }
  return self;
}

- (void)updateAccessibilityPropertiesWithCurrentDetent:
            (AssistantContainerDetent)currentDetent
                                      availableDetents:
                                          (const std::vector<
                                              AssistantContainerDetent>&)
                                              detents {
  _currentDetent = currentDetent;
  if (_detents != detents) {
    _detents = detents;
  }

  [self updateAccessibilityProperties];
}

#pragma mark - AssistantGrabberButtonAccessibilityDelegate

- (void)assistantGrabberButtonDidIncrement:(AssistantGrabberButton*)button {
  for (size_t i = 0; i < _detents.size() - 1; ++i) {
    if (_detents[i] == _currentDetent) {
      [_delegate accessibilityManagerDidRequestDetentChange:_detents[i + 1]];
      return;
    }
  }
}

- (void)assistantGrabberButtonDidDecrement:(AssistantGrabberButton*)button {
  for (size_t i = 1; i < _detents.size(); ++i) {
    if (_detents[i] == _currentDetent) {
      [_delegate accessibilityManagerDidRequestDetentChange:_detents[i - 1]];
      return;
    }
  }
}

#pragma mark - Private

// Updates the accessibility value and custom actions of the grabber button.
// The `_detents` array must be sorted in increasing order of size.
- (void)updateAccessibilityProperties {
  if (!_grabberButton) {
    return;
  }

  NSString* valueString = nil;
  switch (_currentDetent) {
    case AssistantContainerDetent::kMinimized:
      valueString =
          l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_MINIMIZED);
      break;
    case AssistantContainerDetent::kMedium:
      valueString =
          l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_MEDIUM);
      break;
    case AssistantContainerDetent::kLarge:
      valueString =
          l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_VALUE_LARGE);
      break;
  }
  _grabberButton.accessibilityValue = valueString;

  NSMutableArray* customActions = [[NSMutableArray alloc] init];

  for (AssistantContainerDetent detent : _detents) {
    if (detent == _currentDetent) {
      continue;
    }
    NSString* actionName = nil;
    switch (detent) {
      case AssistantContainerDetent::kMinimized:
        actionName =
            l10n_util::GetNSString(IDS_IOS_ASSISTANT_GRABBER_ACTION_MINIMIZE);
        break;
      case AssistantContainerDetent::kMedium:
        actionName = l10n_util::GetNSString(
            IDS_IOS_ASSISTANT_GRABBER_ACTION_EXPAND_MEDIUM);
        break;
      case AssistantContainerDetent::kLarge:
        actionName = l10n_util::GetNSString(
            IDS_IOS_ASSISTANT_GRABBER_ACTION_EXPAND_FULL);
        break;
    }

    UIAccessibilityCustomAction* action =
        [self customActionForDetent:detent name:actionName];
    [customActions addObject:action];
  }
  _grabberButton.accessibilityCustomActions = customActions;
}

// Creates a custom action for the given detent.
- (UIAccessibilityCustomAction*)customActionForDetent:
                                    (AssistantContainerDetent)detent
                                                 name:(NSString*)name {
  __weak __typeof(self) weakSelf = self;
  return [[UIAccessibilityCustomAction alloc]
       initWithName:name
      actionHandler:^BOOL(UIAccessibilityCustomAction* action) {
        CHECK(action);
        return [weakSelf requestDetentChange:detent];
      }];
}

// Requests a detent change from the delegate.
- (BOOL)requestDetentChange:(AssistantContainerDetent)detent {
  [_delegate accessibilityManagerDidRequestDetentChange:detent];
  return YES;
}

@end
