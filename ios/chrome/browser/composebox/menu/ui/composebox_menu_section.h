// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SECTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SECTION_H_

#import <UIKit/UIKit.h>

@class ComposeboxMenuItem;

enum class ComposeboxMenuSectionIdentifier {
  kAttachments = 0,
  kTools,
  kModels,
};

// Represents a section in the Composebox menu.
@interface ComposeboxMenuSection : NSObject

// The section title.
@property(nonatomic, copy, readonly) NSString* title;
// The list of items in the section.
@property(nonatomic, copy, readonly) NSArray<ComposeboxMenuItem*>* items;
// The section identifier.
@property(nonatomic, assign, readonly)
    ComposeboxMenuSectionIdentifier identifier;

- (instancetype)initWithTitle:(NSString*)title
                        items:(NSArray<ComposeboxMenuItem*>*)items
                   identifier:(ComposeboxMenuSectionIdentifier)identifier;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_UI_COMPOSEBOX_MENU_SECTION_H_
