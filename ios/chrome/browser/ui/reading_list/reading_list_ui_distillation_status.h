// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UI_DISTILLATION_STATUS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UI_DISTILLATION_STATUS_H_

#import <Foundation/Foundation.h>

// Enum describing the distillation state for reading list UI.
typedef NS_ENUM(NSInteger, ReadingListUIDistillationStatus) {
  ReadingListUIDistillationStatusPending,
  ReadingListUIDistillationStatusSuccess,
  ReadingListUIDistillationStatusFailure
};

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_UI_DISTILLATION_STATUS_H_
