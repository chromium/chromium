// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"

@protocol ContentSuggestionsCommands;
@protocol ContentSuggestionsMenuProvider;
@protocol ContentSuggestionsViewControllerAudience;
class UrlLoadingBrowserAgent;

// CollectionViewController to display the suggestions items.
@interface ContentSuggestionsViewController
    : UIViewController <ContentSuggestionsConsumer>

// Initializes the new tab page view controller.
- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Returns the vertical space taken up by the Content Suggestions.
- (CGFloat)contentSuggestionsHeight;

// Handler for the commands sent by the ContentSuggestionsViewController.
@property(nonatomic, weak) id<ContentSuggestionsCommands>
    suggestionCommandHandler;
@property(nonatomic, weak) id<ContentSuggestionsViewControllerAudience>
    audience;
// Provider of menu configurations for the contentSuggestions component.
@property(nonatomic, weak) id<ContentSuggestionsMenuProvider> menuProvider;
@property(nonatomic, assign) UrlLoadingBrowserAgent* urlLoadingBrowserAgent;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_H_
