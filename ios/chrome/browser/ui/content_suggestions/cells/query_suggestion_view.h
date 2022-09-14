// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_QUERY_SUGGESTION_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_QUERY_SUGGESTION_VIEW_H_

#import <UIKit/UIKit.h>

class GURL;

@interface QuerySuggestionConfig : NSObject

// Query text.
@property(nonatomic, copy) NSString* query;

// Destination URL for this query.
@property(nonatomic, assign) GURL URL;

// Index position of this suggestion in the module.
@property(nonatomic, assign) int index;

@end

// View showing a query suggestion
@interface QuerySuggestionView : UIView

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:(QuerySuggestionConfig*)config;

@property(nonatomic, strong, readonly) QuerySuggestionConfig* config;

// If called, this view will show a separator along it's bottom border.
- (void)addBottomSeparator;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_QUERY_SUGGESTION_VIEW_H_
