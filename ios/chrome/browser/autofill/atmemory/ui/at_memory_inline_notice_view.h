// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_INLINE_NOTICE_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_INLINE_NOTICE_VIEW_H_

#import <UIKit/UIKit.h>

@class AtMemoryInlineNoticeView;
@class AtMemoryInlineNoticeConfiguration;

// Delegate protocol for AtMemoryInlineNoticeView.
@protocol AtMemoryInlineNoticeViewDelegate <NSObject>

// Called when the user taps the OK button.
- (void)inlineNoticeViewDidTapOK:(AtMemoryInlineNoticeView*)view;

// Called when the user taps the settings link.
- (void)inlineNoticeViewDidTapSettings:(AtMemoryInlineNoticeView*)view;

@end

// View that displays the inline notice inside the AtMemory bottom sheet.
@interface AtMemoryInlineNoticeView : UIView <UIContentView>

// The design initializer conforming to UIContentView.
- (instancetype)initWithConfiguration:
    (AtMemoryInlineNoticeConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

// Configuration object for AtMemoryInlineNoticeView.
@interface AtMemoryInlineNoticeConfiguration : NSObject <UIContentConfiguration>

// The title of the notice.
@property(nonatomic, copy) NSString* title;

// The message body text of the notice.
@property(nonatomic, copy) NSString* message;

// Delegate to handle actions from the notice view.
@property(nonatomic, weak) id<AtMemoryInlineNoticeViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_INLINE_NOTICE_VIEW_H_
