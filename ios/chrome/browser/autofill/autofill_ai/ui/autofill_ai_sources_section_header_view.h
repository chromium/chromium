// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_SECTION_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_SECTION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

// Compact section header view displaying a source group name (e.g., "Gmail",
// "Photos") above each inset-grouped card in `AutofillAiSourcesViewController`.
@interface AutofillAiSourcesSectionHeaderView : UITableViewHeaderFooterView

// Whether this header belongs to the first section of the table view, which
// uses tighter top padding.
@property(nonatomic, assign, getter=isFirstSection) BOOL firstSection;

// Configures the header with `title`.
- (void)configureWithTitle:(NSString*)title;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SOURCES_SECTION_HEADER_VIEW_H_
