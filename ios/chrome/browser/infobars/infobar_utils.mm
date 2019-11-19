// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_utils.h"

#include <memory>
#include <utility>

#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_confirm_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  if (IsConfirmInfobarMessagesUIEnabled()) {
    // TODO(crbug.com/927064): Coordinators shouldn't be created at this level,
    // we should probably send only the delegate and have the presenting
    // Coordinator create the right Coordinator using that delegate.
    InfobarConfirmCoordinator* coordinator = [[InfobarConfirmCoordinator alloc]
        initWithInfoBarDelegate:delegate.get()
                   badgeSupport:NO
                           type:InfobarType::kInfobarTypeConfirm];
    return std::make_unique<InfoBarIOS>(coordinator, std::move(delegate));
  } else {
    ConfirmInfoBarController* controller = [[ConfirmInfoBarController alloc]
        initWithInfoBarDelegate:delegate.get()];
    return std::make_unique<InfoBarIOS>(controller, std::move(delegate));
  }
}

std::unique_ptr<infobars::InfoBar> CreateHighPriorityConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  DCHECK(IsInfobarUIRebootEnabled());
  // TODO(crbug.com/927064): Coordinators shouldn't be created at this level,
  // we should probably send only the delegate and have the presenting
  // Coordinator create the right Coordinator using that delegate.
  InfobarConfirmCoordinator* coordinator = [[InfobarConfirmCoordinator alloc]
      initWithInfoBarDelegate:delegate.get()
                 badgeSupport:NO
                         type:InfobarType::kInfobarTypeConfirm];
  coordinator.highPriorityPresentation = YES;
  return std::make_unique<InfoBarIOS>(coordinator, std::move(delegate));
}
