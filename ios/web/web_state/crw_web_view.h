// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CRW_WEB_VIEW_H_
#define IOS_WEB_WEB_STATE_CRW_WEB_VIEW_H_

#import <WebKit/WebKit.h>

@protocol CRWEditMenuBuilder;
@protocol CRWInputViewProvider;

// Subclass of WKWebView which supports custom input views.
@interface CRWWebView : WKWebView

// Provider for custom input views and their respective view controllers.
@property(nonatomic, weak) id<CRWInputViewProvider> inputViewProvider;

// Customizer for the edit menu.
@property(nonatomic, weak) id<CRWEditMenuBuilder> editMenuBuilder;

@end

#endif  // IOS_WEB_WEB_STATE_CRW_WEB_VIEW_H_
