// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for UMATableViewController.
@protocol UMATableViewControllerModelDelegate <NSObject>

// Property to get and set the metrics reporting setting.
@property(nonatomic, assign) BOOL reportingMetricEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_UMA_UMA_TABLE_VIEW_CONTROLLER_MODEL_DELEGATE_H_
