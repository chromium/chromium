// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/uma/uma_table_view_controller_model_delegate.h"

// Mediator for the UMA dialog.
@interface UMAMediator : NSObject <UMATableViewControllerModelDelegate>

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_MEDIATOR_H_
