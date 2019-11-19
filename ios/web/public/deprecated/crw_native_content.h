// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_H_
#define IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_H_

#import <UIKit/UIKit.h>

#include "url/gurl.h"

namespace web {
struct ContextMenuParams;
}  // namespace web;

@protocol CRWNativeContentDelegate;

// Abstract methods needed for manipulating native content in the web content
// area.
@protocol CRWNativeContent <NSObject>

// The page title, meant for display to the user. Will return nil if not
// available.
- (NSString*)title;

// The URL represented by the content being displayed.
- (const GURL&)url;

// The view to insert into the content area displayed to the user.
- (UIView*)view;

// Returns YES if there is currently a live view in the tab (e.g., the view
// hasn't been discarded due to low memory).
// NOTE: This should be used for metrics-gathering only; for any other purpose
// callers should not know or care whether the view is live.
- (BOOL)isViewAlive;

// Reload any displayed data to ensure the view is up to date.
- (void)reload;

@optional

// Optional method that allows to set CRWNativeContent delegate.
- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate;

// Notifies the CRWNativeContent that it has been shown.
- (void)wasShown;

// Notifies the CRWNativeContent that it has been hidden.
- (void)wasHidden;

// Executes JavaScript on the native view. |handler| is called with the results
// of the evaluation. If the native view cannot evaluate JS at the moment,
// |handler| is called with an NSError.
- (void)executeJavaScript:(NSString*)script
        completionHandler:(void (^)(id, NSError*))handler;

// A native content controller should do any clean up at this time when
// WebController closes.
- (void)close;

// Enables or disables usage of web views inside the native controller.
- (void)setWebUsageEnabled:(BOOL)webUsageEnabled;

// Enables or disables the scrolling in the native view (when available).
- (void)setScrollEnabled:(BOOL)enabled;

// Called when a snapshot of the content will be taken.
- (void)willUpdateSnapshot;

// Notifies the CRWNativeContent that it will be removed from superview.
- (void)willBeDismissed;

// The URL that will be displayed to the user when presenting this native
// content.
- (const GURL&)virtualURL;

// The content inset and offset of this native view.
- (CGPoint)contentOffset;
- (UIEdgeInsets)contentInset;

@end

// CRWNativeContent delegate protocol.
@protocol CRWNativeContentDelegate <NSObject>

@optional
// Called when the content supplies a new title.
- (void)nativeContent:(id)content titleDidChange:(NSString*)title;

// Called when the content triggers a context menu.
- (void)nativeContent:(id)content
    handleContextMenu:(const web::ContextMenuParams&)params;

@end

#endif  // IOS_WEB_PUBLIC_DEPRECATED_CRW_NATIVE_CONTENT_H_
