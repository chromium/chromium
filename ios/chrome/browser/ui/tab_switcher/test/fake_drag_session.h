// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DRAG_SESSION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DRAG_SESSION_H_

#import <UIKit/UIKit.h>

// Test object that conforms to UIDragSession.
@interface FakeDragSession : NSObject <UIDragSession>

@property(nonatomic, readonly) NSArray<UIDragItem*>* items;
@property(nonatomic, readonly) BOOL allowsMoveOperation;
@property(nonatomic, readonly, getter=isRestrictedToDraggingApplication)
    BOOL restrictedToDraggingApplication;
@property(nonatomic, strong) id localContext;

- (instancetype)initWithItems:(NSArray<UIDragItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DRAG_SESSION_H_
