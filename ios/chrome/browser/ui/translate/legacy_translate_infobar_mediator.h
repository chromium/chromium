// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace translate {
class TranslateInfoBarDelegate;
}

@class LanguageSelectionContext;
@protocol LanguageSelectionHandler;
@protocol PopupMenuConsumer;
@protocol TranslateNotificationHandler;
@protocol TranslateOptionSelectionHandler;
class WebStateList;

// Translate popup menu types.
typedef NS_ENUM(NSInteger, TranslatePopupMenuType) {
  TranslatePopupMenuTypeLanguageSelection,
  TranslatePopupMenuTypeTranslateOptionSelection,
};

// Mediator responsible for installing UI handlers for and providing data for
// the translate infobar's language selection popup menu as well as translate
// options popup menus. Also installs the UI handler for the translate options
// notfications.
@interface LegacyTranslateInfobarMediator : NSObject

// |selectionHandler| presents and dismisses the language selection UI as well
// as the translate option selection UI. |notificationHandler| presents and
// dismisses translate related notification UI.
- (instancetype)
    initWithSelectionHandler:
        (id<LanguageSelectionHandler, TranslateOptionSelectionHandler>)
            selectionHandler
         notificationHandler:
             (id<TranslateNotificationHandler>)notificationHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Type of the translate popup menu.
@property(nonatomic, assign) TranslatePopupMenuType type;

// The WebStateList that this mediator observes.
@property(nonatomic, assign) WebStateList* webStateList;

// Provides a list of available languages as well as the current source and
// target languages.
@property(nonatomic, assign)
    const translate::TranslateInfoBarDelegate* infobarDelegate;

// Index of the language unavailable for selection depending on if the language
// selection UI is being populated for the source or the target language.
@property(nonatomic, assign) NSUInteger unavailableLanguageIndex;

// The consumer to be configured with this mediator.
@property(nonatomic, weak) id<PopupMenuConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_MEDIATOR_H_
