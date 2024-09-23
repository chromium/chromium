// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"

@protocol ContentSuggestionsViewControllerAudience;
@class ContentSuggestionsMetricsRecorder;
class UrlLoadingBrowserAgent;

// CollectionViewController to display the suggestions items.
@interface ContentSuggestionsViewController
    : UIViewController <ContentSuggestionsConsumer>

// Initializes the new tab page view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    audience;
// Provider of menu configurations for the contentSuggestions component.
@property(nonatomic, assign) UrlLoadingBrowserAgent* urlLoadingBrowserAgent;

// Recorder for content suggestions metrics.
@property(nonatomic, weak)
    ContentSuggestionsMetricsRecorder* contentSuggestionsMetricsRecorder;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_
