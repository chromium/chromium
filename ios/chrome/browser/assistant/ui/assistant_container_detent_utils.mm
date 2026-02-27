// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_detent_utils.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"

namespace {

// The ratio for the medium detent.
constexpr CGFloat kMediumDetentRatio = 0.5;

}  // namespace

NSString* const kAssistantContainerMediumDetentIdentifier =
    @"kAssistantContainerMediumDetentIdentifier";
NSString* const kAssistantContainerLargeDetentIdentifier =
    @"kAssistantContainerLargeDetentIdentifier";
NSString* const kAssistantContainerMinimizedDetentIdentifier =
    @"kAssistantContainerMinimizedDetentIdentifier";

AssistantContainerDetent* AssistantContainerMediumDetent(UIView* baseView) {
  __weak UIView* weakView = baseView;
  return [[AssistantContainerDetent alloc]
      initWithIdentifier:kAssistantContainerMediumDetentIdentifier
           valueResolver:^NSInteger {
             return round(weakView.safeAreaLayoutGuide.layoutFrame.size.height *
                          kMediumDetentRatio);
           }];
}

AssistantContainerDetent* AssistantContainerLargeDetent(UIView* baseView) {
  __weak UIView* weakView = baseView;
  return [[AssistantContainerDetent alloc]
      initWithIdentifier:kAssistantContainerLargeDetentIdentifier
           valueResolver:^NSInteger {
             return round(weakView.safeAreaLayoutGuide.layoutFrame.size.height);
           }];
}

AssistantContainerDetent* AssistantContainerFixedDetent(NSInteger height,
                                                        NSString* identifier) {
  return [[AssistantContainerDetent alloc] initWithIdentifier:identifier
                                                valueResolver:^NSInteger {
                                                  return height;
                                                }];
}
