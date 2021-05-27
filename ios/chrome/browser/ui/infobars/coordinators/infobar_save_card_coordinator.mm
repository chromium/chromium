// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_save_card_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#import "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "ios/chrome/browser/infobars/infobar_controller_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/ui/infobars/infobar_container.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarSaveCardCoordinator () <InfobarCoordinatorImplementation,
                                          InfobarSaveCardModalDelegate>

// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// InfobarSaveCardTableViewController owned by this Coordinator.
@property(nonatomic, strong)
    InfobarSaveCardTableViewController* modalViewController;
// Consumer that is configured by this coordinator.
@property(nonatomic, weak) id<InfobarSaveCardModalConsumer> modalConsumer;
// Delegate that holds the Infobar information and actions.
@property(nonatomic, readonly)
    autofill::AutofillSaveCardInfoBarDelegateMobile* saveCardInfoBarDelegate;
// YES if the Infobar has been Accepted.
@property(nonatomic, assign) BOOL infobarAccepted;

// TODO(crbug.com/1014652): Move these to future Mediator since these properties
// don't belong in the Coordinator. Cardholder Name to be saved by
// |saveCardInfoBarDelegate|.
@property(nonatomic, copy) NSString* cardholderName;
// Card Expiration month to be saved by |saveCardInfoBarDelegate|.
@property(nonatomic, copy) NSString* expirationMonth;
// Card Expiration year to be saved by |saveCardInfoBarDelegate|.
@property(nonatomic, copy) NSString* expirationYear;

@end

@implementation InfobarSaveCardCoordinator
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;
// Synthesize since readonly property from superclass is changed to readwrite.
@synthesize modalViewController = _modalViewController;

- (instancetype)initWithInfoBarDelegate:
    (autofill::AutofillSaveCardInfoBarDelegateMobile*)saveCardInfoBarDelegate {
  self = [super initWithInfoBarDelegate:saveCardInfoBarDelegate
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeSaveCard];
  if (self) {
    _saveCardInfoBarDelegate = saveCardInfoBarDelegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.infobarAccepted = NO;
    self.bannerViewController = [[InfobarBannerViewController alloc]
        initWithDelegate:self
           presentsModal:self.hasBadge
                    type:InfobarType::kInfobarTypeSaveCard];

    [self.bannerViewController
        setButtonText:self.saveCardInfoBarDelegate->upload()
                          ? l10n_util::GetNSString(
                                IDS_IOS_AUTOFILL_SAVE_ELLIPSIS)
                          : base::SysUTF16ToNSString(
                                self.saveCardInfoBarDelegate->GetButtonLabel(
                                    ConfirmInfoBarDelegate::BUTTON_OK))];
    [self.bannerViewController
        setTitleText:base::SysUTF16ToNSString(
                         self.saveCardInfoBarDelegate->GetMessageText())];
    [self.bannerViewController
        setSubtitleText:base::SysUTF16ToNSString(
                            self.saveCardInfoBarDelegate->card_label())];
    self.bannerViewController.iconImage =
        [UIImage imageNamed:@"infobar_save_card_icon"];

    self.cardholderName = base::SysUTF16ToNSString(
        self.saveCardInfoBarDelegate->cardholder_name());
    self.expirationMonth = base::SysUTF16ToNSString(
        self.saveCardInfoBarDelegate->expiration_date_month());
    self.expirationYear = base::SysUTF16ToNSString(
        self.saveCardInfoBarDelegate->expiration_date_year());
  }
}

- (void)stop {
  [super stop];
  if (self.started) {
    self.started = NO;
    // RemoveInfoBar() will delete the InfobarIOS that owns this Coordinator
    // from memory.
    if (self.delegate) {
      self.delegate->RemoveInfoBar();
    }
    _saveCardInfoBarDelegate = nil;
    [self.infobarContainer childCoordinatorStopped:self];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)isInfobarAccepted {
  return self.infobarAccepted;
}

- (BOOL)infobarBannerActionWillPresentModal {
  return self.saveCardInfoBarDelegate->upload();
}

- (void)performInfobarAction {
  // Display the modal (thus the ToS) if the card will be uploaded, this is a
  // legal requirement and shouldn't be changed.
  if (!self.modalViewController && self.saveCardInfoBarDelegate->upload()) {
    [self presentInfobarModalFromBanner];
    return;
  }
  // Ignore the Accept() return value since it always returns YES.
  DCHECK(self.cardholderName);
  DCHECK(self.expirationMonth);
  DCHECK(self.expirationYear);
  self.saveCardInfoBarDelegate->UpdateAndAccept(
      base::SysNSStringToUTF16(self.cardholderName),
      base::SysNSStringToUTF16(self.expirationMonth),
      base::SysNSStringToUTF16(self.expirationYear));
  self.infobarAccepted = YES;
}

- (void)infobarWasDismissed {
  // Release these strong ViewControllers at the time of infobar dismissal.
  self.bannerViewController = nil;
  self.modalViewController = nil;
}

#pragma mark Banner

- (void)infobarBannerWasPresented {
  // TODO(crbug.com/1014652): Record metrics here if there's a distinction
  // between automatic and manual presentation.
}

- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (BOOL)infobarActionInProgress {
  return NO;
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // TODO(crbug.com/1014652): Record metrics here if there's a distinction
  // between ignoring or dismissing the Infobar.
}

#pragma mark Modal

- (BOOL)configureModalViewController {
  // Return early if there's no delegate. e.g. A Modal presentation has been
  // triggered after the Infobar was destroyed, but before the badge/banner
  // were dismissed.
  if (!self.saveCardInfoBarDelegate)
    return NO;

  self.modalViewController =
      [[InfobarSaveCardTableViewController alloc] initWithModalDelegate:self];
  self.modalViewController.title =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_SAVE_CARD);
  self.modalConsumer = self.modalViewController;

  NSString* cardNumber = [NSString
      stringWithFormat:@"•••• %@",
                       base::SysUTF16ToNSString(self.saveCardInfoBarDelegate
                                                    ->card_last_four_digits())];
  // Only allow editing if the card will be uploaded and it hasn't been
  // previously saved.
  BOOL supportsEditing =
      self.saveCardInfoBarDelegate->upload() && !self.infobarAccepted;

  // Convert gfx::Image to UIImage. The NSDictionary below doesn't support nil,
  // so NSNull must be used.
  const gfx::Image& avatar_gfx =
      self.saveCardInfoBarDelegate->displayed_target_account_avatar();
  NSObject* avatar =
      avatar_gfx.IsEmpty() ? [NSNull null] : avatar_gfx.ToUIImage();

  NSDictionary* prefs = @{
    kCardholderNamePrefKey : self.cardholderName,
    kCardIssuerIconNamePrefKey :
        NativeImage(self.saveCardInfoBarDelegate->issuer_icon_id()),
    kCardNumberPrefKey : cardNumber,
    kExpirationMonthPrefKey : self.expirationMonth,
    kExpirationYearPrefKey : self.expirationYear,
    kLegalMessagesPrefKey : [self legalMessagesForModal],
    kCurrentCardSavedPrefKey : @(self.infobarAccepted),
    kSupportsEditingPrefKey : @(supportsEditing),
    kDisplayedTargetAccountEmailPrefKey : base::SysUTF16ToNSString(
        self.saveCardInfoBarDelegate->displayed_target_account_email()),
    kDisplayedTargetAccountAvatarPrefKey : avatar,
  };
  [self.modalConsumer setupModalViewControllerWithPrefs:prefs];

  return YES;
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // TODO(crbug.com/1014652): Check if there's a metric that should be recorded
  // here, or if there's a need to keep track of the presented state of the
  // Infobar for recording metrics on de-alloc. (See equivalent method on
  // InfobarPasswordCoordinator).
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  UITableView* tableView = self.modalViewController.tableView;
  // Update the tableView frame to then layout its content for |width|.
  tableView.frame = CGRectMake(0, 0, width, tableView.frame.size.height);
  [tableView setNeedsLayout];
  [tableView layoutIfNeeded];

  // Since the TableView is contained in a NavigationController get the
  // navigation bar height.
  CGFloat navigationBarHeight = self.modalViewController.navigationController
                                    .navigationBar.frame.size.height;

  return tableView.contentSize.height + navigationBarHeight;
}

#pragma mark - InfobarSaveCardModalDelegate

- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year {
  self.cardholderName = cardholderName;
  self.expirationMonth = month;
  self.expirationYear = year;
  [self modalInfobarButtonWasAccepted:self];
}

- (void)dismissModalAndOpenURL:(const GURL&)linkURL {
  // Before passing the URL to the block, make sure the block has a copy of
  // the URL and not just a reference.
  const GURL URL(linkURL);
  [self dismissInfobarModalAnimated:YES
                         completion:^{
                           self.saveCardInfoBarDelegate
                               ->OnLegalMessageLinkClicked(URL);
                         }];
}

#pragma mark - Private

// TODO(crbug.com/1014652): Move to a future Mediator since this doesn't belong
// in the Coordinator.
- (NSMutableArray<SaveCardMessageWithLinks*>*)legalMessagesForModal {
  NSMutableArray<SaveCardMessageWithLinks*>* legalMessages =
      [[NSMutableArray alloc] init];
  // Only display legal Messages if the card is being uploaded and there are
  // any.
  if (self.saveCardInfoBarDelegate->upload() &&
      !self.saveCardInfoBarDelegate->legal_message_lines().empty()) {
    for (const auto& line :
         self.saveCardInfoBarDelegate->legal_message_lines()) {
      SaveCardMessageWithLinks* message =
          [[SaveCardMessageWithLinks alloc] init];
      message.messageText = base::SysUTF16ToNSString(line.text());
      NSMutableArray* linkRanges = [[NSMutableArray alloc] init];
      std::vector<GURL> linkURLs;
      for (const auto& link : line.links()) {
        [linkRanges addObject:[NSValue valueWithRange:link.range.ToNSRange()]];
        linkURLs.push_back(link.url);
      }
      message.linkRanges = linkRanges;
      message.linkURLs = linkURLs;
      [legalMessages addObject:message];
    }
  }
  return legalMessages;
}

@end
