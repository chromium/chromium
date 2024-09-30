// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_collection_view_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The corner radius of this container.
const float kCornerRadius = 24;

}  // namespace

@interface MagicStackModuleCollectionViewCell () <
    UIContextMenuInteractionDelegate>

@property(nonatomic, assign) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleCollectionViewCell {
  MagicStackModuleContainer* _moduleContainer;
  UIContextMenuInteraction* _contextMenuInteraction;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;
    self.clipsToBounds = YES;

    _moduleContainer =
        [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero];
    _moduleContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_moduleContainer];
    AddSameConstraints(_moduleContainer, self);
  }
  return self;
}

- (void)configureWithConfig:(MagicStackModule*)config {
  _type = config.type;
  if ([self allowsLongPress]) {
    if (!_contextMenuInteraction) {
      _contextMenuInteraction =
          [[UIContextMenuInteraction alloc] initWithDelegate:self];
      [self addInteraction:_contextMenuInteraction];
    }
  }
  [_moduleContainer configureWithConfig:config];
}

#pragma mark - Setters

- (void)setDelegate:(id<MagicStackModuleContainerDelegate>)delegate {
  _moduleContainer.delegate = delegate;
  _delegate = delegate;
}

#pragma mark UICollectionViewCell Overrides

- (void)prepareForReuse {
  [super prepareForReuse];
  if (_contextMenuInteraction) {
    [self removeInteraction:_contextMenuInteraction];
    _contextMenuInteraction = nil;
  }
  [_moduleContainer resetView];
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  CHECK([self allowsLongPress]);
  __weak MagicStackModuleCollectionViewCell* weakSelf = self;
  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        return [UIMenu menuWithTitle:[weakSelf contextMenuTitle]
                            children:[weakSelf contextMenuActions]];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Helpers

// Returns the list of actions for the long-press /  context menu.
- (NSArray<UIAction*>*)contextMenuActions {
  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];

  if ((IsSetUpListModuleType(_type) && IsIOSTipsNotificationsEnabled()) ||
      (_type == ContentSuggestionsModuleType::kSafetyCheck &&
       IsSafetyCheckNotificationsEnabled())) {
    [actions addObject:[self toggleNotificationsActionForModuleType:self.type]];
  }

  [actions addObject:[self hideAction]];

  [actions addObject:[self customizeCardAction]];

  return actions;
}

// Returns the menu action to hide this module type.
- (UIAction*)hideAction {
  __weak __typeof(self) weakSelf = self;
  UIAction* hideAction = [UIAction
      actionWithTitle:[self contextMenuHideDescription]
                image:DefaultSymbolWithPointSize(kHideActionSymbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate neverShowModuleType:weakSelf.type];
              }];
  hideAction.attributes = UIMenuElementAttributesDestructive;
  return hideAction;
}

// Returns the menu action to hide this module type.
- (UIAction*)customizeCardAction {
  __weak __typeof(self) weakSelf = self;
  UIAction* hideAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_MAGIC_STACK_CONTEXT_MENU_CUSTOMIZE_CARDS_TITLE)
                image:DefaultSymbolWithPointSize(kSliderHorizontalSymbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate customizeCardsWasTapped];
              }];
  return hideAction;
}

// `YES` if this container should show a context menu when the user performs a
// long-press gesture.
- (BOOL)allowsLongPress {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      return YES;
    default:
      return NO;
  }
}

// Returns the menu action to opt-in to Tips Notifications.
- (UIAction*)toggleNotificationsActionForModuleType:
    (ContentSuggestionsModuleType)moduleType {
  const PushNotificationClientId clientId =
      [self pushNotificationClientId:moduleType];

  BOOL optedIn = [self optedInToNotificationsForClient:clientId];

  __weak __typeof(self) weakSelf = self;

  NSString* title;
  NSString* symbol;

  int featureTitle = [self pushNotificationTitleMessageId:moduleType];

  if (optedIn) {
    title = l10n_util::GetNSStringF(
        IDS_IOS_TIPS_NOTIFICATIONS_CONTEXT_MENU_ITEM_OFF,
        l10n_util::GetStringUTF16(featureTitle));
    symbol = kBellSlashSymbol;
  } else {
    title =
        l10n_util::GetNSStringF(IDS_IOS_TIPS_NOTIFICATIONS_CONTEXT_MENU_ITEM,
                                l10n_util::GetStringUTF16(featureTitle));
    symbol = kBellSymbol;
  }

  return [UIAction
      actionWithTitle:title
                image:DefaultSymbolWithPointSize(symbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                if (optedIn) {
                  [weakSelf.delegate disableNotifications:weakSelf.type];
                } else {
                  [weakSelf.delegate enableNotifications:weakSelf.type];
                }
              }];
}

// Returns the `PushNotificationClientId` associated with the specified `type`.
// Currently, push notifications are exclusively supported by the Set Up List
// and Safety Check modules.
- (PushNotificationClientId)pushNotificationClientId:
    (ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return PushNotificationClientId::kSafetyCheck;
  }

  if (IsSetUpListModuleType(type)) {
    return PushNotificationClientId::kTips;
  }

  NOTREACHED();
}

// Retrieves the message ID for the push notification feature title associated
// with the specified `ContentSuggestionsModuleType`. Currently, push
// notifications are exclusively supported by the Set Up List and Safety Check
// modules.
- (int)pushNotificationTitleMessageId:(ContentSuggestionsModuleType)type {
  // This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return IDS_IOS_SAFETY_CHECK_TITLE;
  }

  if (IsSetUpListModuleType(type)) {
    return content_suggestions::SetUpListTitleStringID();
  }

  NOTREACHED();
}

// Returns YES if the user has already opted-in to notifications for the
// specified `clientId`.
- (BOOL)optedInToNotificationsForClient:(PushNotificationClientId)clientId {
  // Currently, push notifications are exclusively supported for the Set Up List
  // and Safety Check modules.
  CHECK(clientId == PushNotificationClientId::kTips ||
        clientId == PushNotificationClientId::kSafetyCheck);

  // IMPORTANT: Notifications for Set Up List and Safety Check are managed
  // through the app-wide notification settings. If a feature that utilizes
  // per-profile notification settings is being introduced, ensure a `gaia_id`
  // is passed to `GetMobileNotificationPermissionStatusForClient()` below.
  return push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(clientId, "");
}

// Title string for the context menu of this container.
- (NSString*)contextMenuTitle {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSafetyCheck:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kParcelTracking:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      return @"";
    default:
      NOTREACHED();
  }
}

// Descriptor string for hide action of the context menu of this container.
- (NSString*)contextMenuHideDescription {
  switch (_type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      return l10n_util::GetNSStringF(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_DESCRIPTION,
          l10n_util::GetStringUTF16(
              content_suggestions::SetUpListTitleStringID()));
    case ContentSuggestionsModuleType::kParcelTracking:
      return l10n_util::GetNSStringF(
          IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_DESCRIPTION,
          base::SysNSStringToUTF16(l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)));
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_HIDE_CARD);
    default:
      NOTREACHED();
  }
}

@end
