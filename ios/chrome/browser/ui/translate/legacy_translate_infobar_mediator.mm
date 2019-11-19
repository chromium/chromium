// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/translate/legacy_translate_infobar_mediator.h"

#include <memory>

#include "base/logging.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/language_selection_context.h"
#import "ios/chrome/browser/translate/language_selection_handler.h"
#import "ios/chrome/browser/translate/translate_option_selection_handler.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_consumer.h"
#import "ios/chrome/browser/ui/translate/cells/select_language_popup_menu_item.h"
#import "ios/chrome/browser/ui/translate/cells/translate_popup_menu_item.h"
#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LegacyTranslateInfobarMediator () <WebStateListObserving> {
  // WebStateList observers.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
}

// Presents and dismisses the language selection UI as well as the translate
// option selection UI.
@property(nonatomic, weak)
    id<LanguageSelectionHandler, TranslateOptionSelectionHandler>
        selectionHandler;

// Presents and dismisses translate related notification UI.
@property(nonatomic, weak) id<TranslateNotificationHandler> notificationHandler;

@end

@implementation LegacyTranslateInfobarMediator

- (instancetype)
    initWithSelectionHandler:
        (id<LanguageSelectionHandler, TranslateOptionSelectionHandler>)
            selectionHandler
         notificationHandler:
             (id<TranslateNotificationHandler>)notificationHandler {
  DCHECK(selectionHandler);
  DCHECK(notificationHandler);
  if ((self = [super init])) {
    _selectionHandler = selectionHandler;
    _notificationHandler = notificationHandler;
  }
  return self;
}

#pragma mark - Public methods

- (void)disconnect {
  self.webStateList = nil;
}

#pragma mark - Properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList == webStateList)
    return;

  if (_webStateList) {
    [self removeWebStateListObserver];

    // Uninstall delegates for each WebState in WebStateList.
    for (int i = 0; i < self.webStateList->count(); i++) {
      [self uninstallDelegatesForWebState:self.webStateList->GetWebStateAt(i)];
    }
  }

  _webStateList = webStateList;

  if (_webStateList) {
    // Install delegates for each WebState in WebStateList.
    for (int i = 0; i < _webStateList->count(); i++) {
      [self installDelegatesForWebState:_webStateList->GetWebStateAt(i)];
    }

    [self addWebStateListObserver];
  }
}

- (void)setConsumer:(id<PopupMenuConsumer>)consumer {
  _consumer = consumer;
  [_consumer setPopupMenuItems:self.items];
}

#pragma mark - Private

// Returns the menu items for either the language selection popup menu or the
// translate option selection popup menu.
- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)items {
  if (self.type == TranslatePopupMenuTypeLanguageSelection) {
    return [self languageSelectionItems];
  }
  return [self translateOptionSelectionItems];
}

// Returns the menu items for the language selection popup menu.
- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)languageSelectionItems {
  NSMutableArray* items = [NSMutableArray array];
  for (size_t index = 0; index < self.infobarDelegate->num_languages();
       index++) {
    if (index == self.unavailableLanguageIndex)
      continue;

    SelectLanguagePopupMenuItem* item =
        [[SelectLanguagePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
    item.actionIdentifier = PopupMenuActionSelectLanguage;
    item.title =
        base::SysUTF16ToNSString(self.infobarDelegate->language_name_at(index));
    item.languageCode =
        base::SysUTF8ToNSString(self.infobarDelegate->language_code_at(index));
    [items addObject:item];
  }

  return @[ items ];
}

// Returns the menu items for the translate option selection popup menu.
- (NSArray<NSArray<TableViewItem<PopupMenuItem>*>*>*)
    translateOptionSelectionItems {
  base::string16 originalLanguageName =
      self.infobarDelegate->original_language_name();

  TranslatePopupMenuItem* selectTargetLanguageItem =
      [[TranslatePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
  selectTargetLanguageItem.actionIdentifier =
      PopupMenuActionChangeTargetLanguage;
  selectTargetLanguageItem.title = base::SysUTF16ToNSString(
      l10n_util::GetStringUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_MORE_LANGUAGE));

  TranslatePopupMenuItem* alwaysTranslateLanguageItem =
      [[TranslatePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
  alwaysTranslateLanguageItem.actionIdentifier =
      PopupMenuActionAlwaysTranslateSourceLanguage;
  alwaysTranslateLanguageItem.title =
      base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_OPTIONS_ALWAYS, originalLanguageName));
  alwaysTranslateLanguageItem.selected =
      self.infobarDelegate->ShouldAlwaysTranslate();

  TranslatePopupMenuItem* neverTranslateLanguageItem =
      [[TranslatePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
  neverTranslateLanguageItem.actionIdentifier =
      PopupMenuActionNeverTranslateSourceLanguage;
  neverTranslateLanguageItem.title =
      base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_LANG,
          originalLanguageName));

  TranslatePopupMenuItem* neverTranslateSiteItem =
      [[TranslatePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
  neverTranslateSiteItem.actionIdentifier = PopupMenuActionNeverTranslateSite;
  neverTranslateSiteItem.title =
      base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
          IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE));

  TranslatePopupMenuItem* selectSourceLanguageItem =
      [[TranslatePopupMenuItem alloc] initWithType:kItemTypeEnumZero];
  selectSourceLanguageItem.actionIdentifier =
      PopupMenuActionChangeSourceLanguage;
  selectSourceLanguageItem.title =
      base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
          IDS_TRANSLATE_INFOBAR_OPTIONS_NOT_SOURCE_LANGUAGE,
          originalLanguageName));

  return @[
    @[ selectTargetLanguageItem ],
    @[
      alwaysTranslateLanguageItem, neverTranslateLanguageItem,
      neverTranslateSiteItem, selectSourceLanguageItem
    ]
  ];
}

// Adds observer for WebStateList.
- (void)addWebStateListObserver {
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
  _scopedWebStateListObserver =
      std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
          _webStateListObserverBridge.get());
  _scopedWebStateListObserver->Add(self.webStateList);
}

// Removes observer for WebStateList.
- (void)removeWebStateListObserver {
  _scopedWebStateListObserver.reset();
  _webStateListObserverBridge.reset();
}

// Installs delegates for |webState|.
- (void)installDelegatesForWebState:(web::WebState*)webState {
  if (ChromeIOSTranslateClient::FromWebState(webState)) {
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_language_selection_handler(self.selectionHandler);
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_translate_option_selection_handler(self.selectionHandler);
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_translate_notification_handler(self.notificationHandler);
  }
}

// Uninstalls delegates for |webState|.
- (void)uninstallDelegatesForWebState:(web::WebState*)webState {
  if (ChromeIOSTranslateClient::FromWebState(webState)) {
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_language_selection_handler(nil);
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_translate_option_selection_handler(nil);
    ChromeIOSTranslateClient::FromWebState(webState)
        ->set_translate_notification_handler(nil);
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self installDelegatesForWebState:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self uninstallDelegatesForWebState:oldWebState];
  [self installDelegatesForWebState:newWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self uninstallDelegatesForWebState:webState];
}

@end
