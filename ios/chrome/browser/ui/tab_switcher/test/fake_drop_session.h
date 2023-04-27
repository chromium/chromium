// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DROP_SESSION_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DROP_SESSION_H_

#import <UIKit/UIKit.h>

// Test object that conforms to UIDropSession.
@interface FakeDropSession : NSObject <UIDropSession>

@property(nonatomic, readonly) NSArray<UIDragItem*>* items;
@property(nonatomic) UIDropSessionProgressIndicatorStyle progressIndicatorStyle;
@property(nonatomic) BOOL allowsMoveOperation;
@property(nonatomic, strong) id<UIDragSession> localDragSession;
@property(nonatomic) NSProgress* progress;
@property(nonatomic, readonly, getter=isRestrictedToDraggingApplication)
    BOOL restrictedToDraggingApplication;

- (instancetype)initWithItems:(NSArray<UIDragItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_DROP_SESSION_H_
