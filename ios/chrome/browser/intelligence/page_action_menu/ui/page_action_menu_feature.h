// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_FEATURE_H_

#import <UIKit/UIKit.h>

// Types of features that can appear in the Page Action Menu.
typedef NS_ENUM(NSInteger, PageActionMenuFeatureType) {
  PageActionMenuTranslate,
  PageActionMenuPopupBlocker,
  PageActionMenuCameraPermission,
  PageActionMenuMicrophonePermission,
  PageActionMenuPriceTracking,
};

// Types of actions a feature perform.
typedef NS_ENUM(NSInteger, PageActionMenuFeatureActionType) {
  // Shows toggle switch (e.g., Camera permission on/off).
  PageActionMenuToggleAction,
  // Shows action button (e.g., "Show original" for Translate).
  PageActionMenuButtonAction,
  // Shows chevron for settings navigation.
  PageActionMenuSettingsAction,
};

// Data model for a feature in the Page Action Menu.
@interface PageActionMenuFeature : NSObject

// The type of feature.
@property(nonatomic, readonly) PageActionMenuFeatureType featureType;

// Primary title displayed in the feature
@property(nonatomic, copy, readonly) NSString* title;

// Optional subtitle displayed in the feature.
@property(nonatomic, copy) NSString* subtitle;

// Icon displayed on the left side for the feature.
@property(nonatomic, strong, readonly) UIImage* icon;

// Type of action this feature supports.
@property(nonatomic, assign) PageActionMenuFeatureActionType actionType;

// Text for action button.
@property(nonatomic, copy) NSString* actionText;

// For toggle-type features, whether the feature is currently active.
@property(nonatomic, assign) BOOL toggleState;

// Accessibility label for the feature.
@property(nonatomic, copy) NSString* accessibilityLabel;

// Convenience initializer for basic feature.
- (instancetype)initWithFeatureType:(PageActionMenuFeatureType)featureType
                              title:(NSString*)title
                               icon:(UIImage*)icon
                         actionType:(PageActionMenuFeatureActionType)actionType;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_ACTION_MENU_UI_PAGE_ACTION_MENU_FEATURE_H_
