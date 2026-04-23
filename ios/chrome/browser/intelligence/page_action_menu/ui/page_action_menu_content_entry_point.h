// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONTENT_ENTRY_POINT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONTENT_ENTRY_POINT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"

// Model class for an element in the footer displaying information on entry
// points that aren't available.
@interface ContentEntryPointUnavailabilityItem : NSObject

// Information about the unavailability reason.
@property(nonatomic, copy) NSString* text;
//  Optional icon which can be displayed next to the text.
@property(nonatomic, strong) UIImage* icon;
// Optional "url" associated with the link in the text; this is not a valid
// url per say and only gets used to uniquely identify the link during handling.
@property(nonatomic, copy) NSString* actionIdentifier;
// Identifier used to log the impression of this unavailability item.
@property(nonatomic, assign) IOSPageActionMenuFooterReason metricIdentifier;

// Initializers are intentionally unavailable. Use factory methods instead.
- (instancetype)init NS_UNAVAILABLE;
// Factory for an item linked to enterprise policies with Gemini.
+ (instancetype)geminiEnterprise;
// Factory for an item linked to enterprise policies with Lens.
+ (instancetype)lensEnterprise;
// Factory for an item linked to the default search engine with Lens.
+ (instancetype)lensSearchEngine;
@end

// Model class for a main entry point in the page tools menu. When not enabled,
// we optionally provide a disclaimer in the footer for some of the reasons of
// unavailability.
@interface PageActionMenuContentEntryPoint : NSObject

// Whether the entry point is eligible and available.
@property(nonatomic, readonly) BOOL enabled;
// Optional model entity for the footer element associated with the entry point.
@property(nonatomic, readonly)
    ContentEntryPointUnavailabilityItem* unavailabilityItem;

- (instancetype)init NS_UNAVAILABLE;
// Convenience initializer for entry points with no disclaimers in the footer.
- (instancetype)initWithEnabled:(BOOL)enabled;
// Designated initializer for entry points with an associated item describing
// the disclaimer for ineligibility.
- (instancetype)initWithEnabled:(BOOL)enabled
                     footerItem:(ContentEntryPointUnavailabilityItem*)item
    NS_DESIGNATED_INITIALIZER;
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_CONTENT_ENTRY_POINT_H_
