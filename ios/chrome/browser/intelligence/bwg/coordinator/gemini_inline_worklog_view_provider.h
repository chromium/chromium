// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_custom_ui_view_provider.h"

@protocol ActorTaskDataSource;
@protocol ActuationSuggestionsPresenter;

// Renders the worklog of agent tasks inside Gemini responses. The running
// task's worklog follows its data source, and past tasks are built from their
// blocks. Vended views may outlive the provider.
@interface GeminiInlineWorklogViewProvider
    : NSObject <GeminiCustomUIViewProvider>

// Initializes with `suggestionsPresenter`, which is asked to show the picker
// when a suggestion card is tapped in a vended view.
- (instancetype)initWithSuggestionsPresenter:
    (id<ActuationSuggestionsPresenter>)suggestionsPresenter
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Binds the running task's view to `dataSource`, replacing any previous
// binding.
- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource;

// Unbinds the running task's view, which keeps its final state.
- (void)stopObservingDataSource;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_INLINE_WORKLOG_VIEW_PROVIDER_H_
