// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_mediator.h"

#import <memory>
#import <optional>
#import <tuple>
#import <utility>

#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_model/data_model_util.h"
#import "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/credit_card_save_metrics_ios.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/save_card_bottom_sheet_model.h"
#import "ios/chrome/browser/autofill/model/features.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_consumer.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/util/autofill_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/ui/util/accessibility_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using SaveCreditCardPromptOverlayType =
    autofill::autofill_metrics::SaveCreditCardPromptOverlayType;

// Bridges the C++ Observer interface to the Objective-C mediator. Scoped
// observation adds and removes this bridge as the observer of the
// SaveCardBottomSheetModel.
class ModelObserverBridge
    : public autofill::SaveCardBottomSheetModel::Observer {
 public:
  ModelObserverBridge(autofill::SaveCardBottomSheetModel* model,
                      __weak SaveCardBottomSheetMediator* mediator)
      : mediator_(mediator) {
    scoped_model_observation_.Observe(model);
  }
  ~ModelObserverBridge() override = default;
  void OnCreditCardUploadCompleted(bool card_saved) override {
    [mediator_ onCreditCardUploadCompleted:card_saved];
  }

 private:
  __weak SaveCardBottomSheetMediator* mediator_;
  base::ScopedObservation<autofill::SaveCardBottomSheetModel,
                          ModelObserverBridge>
      scoped_model_observation_{this};
};

std::pair<NSString*, NSString*> ParseExpirationDate(NSString* expirationDate) {
  std::u16string month;
  std::u16string year;
  if (autofill::data_util::ParseExpirationDate(
          base::SysNSStringToUTF16(expirationDate), &month, &year)) {
    return {base::SysUTF16ToNSString(month), base::SysUTF16ToNSString(year)};
  }
  return {@"", @""};
}

}  // namespace

@implementation SaveCardBottomSheetMediator {
  // `_modelObserverBridge` holds a scoped observation of the model for the
  // mediator and must be destroyed before the `_saveCardBottomSheetModel`

  // The model layer component providing resources and callbacks for
  // saving the card or rejecting the card upload.
  std::unique_ptr<autofill::SaveCardBottomSheetModel> _saveCardBottomSheetModel;
  raw_ptr<AutofillBottomSheetTabHelper> _tabHelper;
  __weak id<AutofillCommands> _autofillCommandsHandler;
  std::optional<ModelObserverBridge> _modelObserverBridge;

  // Timer that controls auto dismissal of bottomsheet after success
  // confirmation is shown.
  base::OneShotTimer _autoDismissConfirmationTimer;

  // Indicates whether bottom sheet is already in the process of dismissing.
  BOOL _dismissing;

  // Indicates whether dismissal was triggered by clicking a legal message link.
  BOOL _dismissedForLinkClick;

  // Stored values for the editable flow.
  NSString* _cardNumber;
  NSString* _expirationDate;
  NSString* _cardholderName;
  NSString* _cvc;
  NSString* _nickname;

  // Validity flags for the editable flow.
  BOOL _cardNumberValid;
  BOOL _expirationDateValid;
  BOOL _cardCVCValid;
  BOOL _nicknameValid;
}

- (instancetype)initWithUIModel:
                    (std::unique_ptr<autofill::SaveCardBottomSheetModel>)model
        autofillCommandsHandler:(id<AutofillCommands>)autofillCommandsHandler {
  self = [super init];
  if (self) {
    _saveCardBottomSheetModel = std::move(model);
    _autofillCommandsHandler = autofillCommandsHandler;
    _modelObserverBridge.emplace(_saveCardBottomSheetModel.get(), self);
  }
  return self;
}

- (instancetype)initWithTabHelper:(AutofillBottomSheetTabHelper*)tabHelper
          autofillCommandsHandler:
              (id<AutofillCommands>)autofillCommandsHandler {
  CHECK(tabHelper);
  self = [super init];
  if (self) {
    _tabHelper = tabHelper;
    _autofillCommandsHandler = autofillCommandsHandler;
    autofill::SaveCardBottomSheetModel* model = [self model];
    if (model) {
      _modelObserverBridge.emplace(model, self);
    }
  }
  return self;
}

- (void)disconnect {
  _autoDismissConfirmationTimer.Stop();
  _modelObserverBridge.reset();
  _saveCardBottomSheetModel.reset();
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn) &&
      !_dismissedForLinkClick && _tabHelper) {
    std::ignore = _tabHelper->TakeSaveCardBottomSheetModel();
  }
  _tabHelper = nullptr;
}

- (autofill::SaveCardBottomSheetModel*)model {
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn)) {
    return _tabHelper ? _tabHelper->GetSaveCardBottomSheetModel() : nullptr;
  }
  return _saveCardBottomSheetModel.get();
}

- (void)onBottomSheetDismissedWithLinkClicked:(BOOL)linkClicked {
  // `_dismissing` would be `YES` if the bottomsheet was dismissed due `No
  // thanks` button in offer state or autodismissed in confirmation state or was
  // dismissed on link clicked.
  if (_dismissing) {
    return;
  }
  if (base::FeatureList::IsEnabled(kAutofillReshowSavePromptOnTabReturn) &&
      linkClicked) {
    // User tapped legal terms link; suppress cancellation and prompt result
    // logging so the prompt can be restored when returning to this tab.
    _dismissedForLinkClick = YES;
    _dismissing = YES;
    return;
  }

  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  switch (model->save_card_state()) {
    case autofill::SaveCardBottomSheetModel::SaveCardState::kOffered:
      model->OnCanceled();
      autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
          linkClicked ? autofill::autofill_metrics::
                            SaveCreditCardPromptResultIOS::kLinkClicked
                      : autofill::autofill_metrics::
                            SaveCreditCardPromptResultIOS::kSwiped,
          model->is_for_upload(),
          model->save_card_delegate()->GetSaveCreditCardOptions(),
          SaveCreditCardPromptOverlayType::kBottomSheet);
      break;
    case autofill::SaveCardBottomSheetModel::SaveCardState::kSaveInProgress:
      CHECK(model->is_for_upload());
      autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
          autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed);
      break;
    case autofill::SaveCardBottomSheetModel::SaveCardState::kSaved:
      autofill::autofill_metrics::
          LogCreditCardUploadConfirmationViewResultMetric(
              autofill::autofill_metrics::LegacySaveCardPromptResult::kClosed,
              /*is_card_uploaded=*/model->is_for_upload());
      break;
    case autofill::SaveCardBottomSheetModel::SaveCardState::kFailed:
      NOTREACHED();
  }
  _dismissing = YES;
}

- (BOOL)isDismissingForTesting {
  return _dismissing;
}

- (void)setConsumer:(id<SaveCardBottomSheetConsumer>)consumer {
  _consumer = consumer;

  if (!_consumer) {
    return;
  }

  autofill::SaveCardBottomSheetModel* model = [self model];
  if (!model) {
    return;
  }

  [self.consumer setAboveTitleImage:NativeImage(model->logo_icon_id())];
  [self.consumer
      setAboveTitleImageDescription:base::SysUTF16ToNSString(
                                        model->logo_icon_description())];
  [self.consumer setTitle:base::SysUTF16ToNSString(model->title())];
  [self.consumer setSubtitle:base::SysUTF16ToNSString(model->subtitle())];
  [self.consumer setAcceptActionText:base::SysUTF16ToNSString(
                                         model->accept_button_text())];
  [self.consumer setCancelActionText:base::SysUTF16ToNSString(
                                         model->cancel_button_text())];

  if (model->is_for_upload()) {
    [self.consumer setLegalMessages:[AutofillLegalMessageLine
                                        convertFrom:model->legal_messages()]];
  }

  [self.consumer
      setCardNameAndLastFourDigits:base::SysUTF16ToNSString(
                                       model->card_name_last_four_digits())
                withCardExpiryDate:base::SysUTF16ToNSString(
                                       model->card_expiry_date())
                       andCardIcon:NativeImage(model->issuer_icon_id())
         andCardAccessibilityLabel:base::SysUTF16ToNSString(
                                       model
                                           ->card_accessibility_description())];

  // Log kShown strictly once; suppress on tab-return reshow.
  if (!model->has_been_shown()) {
    model->set_has_been_shown(true);
    autofill::autofill_metrics::LogSaveCreditCardPromptOfferMetricIos(
        autofill::autofill_metrics::SaveCardPromptOffer::kShown,
        model->is_for_upload(),
        model->save_card_delegate()->GetSaveCreditCardOptions(),
        SaveCreditCardPromptOverlayType::kBottomSheet);

    autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
        autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kShown,
        model->is_for_upload(),
        model->save_card_delegate()->GetSaveCreditCardOptions(),
        SaveCreditCardPromptOverlayType::kBottomSheet);
  }
  [self updateSaveButtonStatus];
}

- (SaveCardActionType)actionType {
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  if (model->save_card_delegate()->is_for_upload()) {
    return SaveCardActionType::kUpload;
  }
  if (model->save_card_delegate()->is_for_local_save()) {
    return SaveCardActionType::kLocal;
  }
  return SaveCardActionType::kSaveScanAndFill;
}

#pragma mark - SaveCardBottomSheetDataSource

- (AboveTitleImageLogoType)logoType {
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  return model->is_for_upload() ? kGoogleWalletLogo : kChromeLogo;
}

- (NSString*)logoAccessibilityLabel {
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  return base::SysUTF16ToNSString(
      model->is_for_upload()
          ? l10n_util::GetStringUTF16(
                IDS_AUTOFILL_GOOGLE_WALLET_LOGO_ACCESSIBLE_NAME)
          : l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CHROME_LOGO_ACCESSIBLE_NAME));
}

#pragma mark - SaveCardBottomSheetMutator

- (void)didAccept {
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  model->OnAccepted();
  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kAccepted,
      model->is_for_upload(),
      model->save_card_delegate()->GetSaveCreditCardOptions(),
      SaveCreditCardPromptOverlayType::kBottomSheet);

  if (model->is_for_upload()) {
    [_consumer showLoadingStateWithAccessibilityLabel:
                   base::SysUTF16ToNSString(
                       model->loading_accessibility_description())];
    autofill::autofill_metrics::LogCreditCardUploadLoadingViewShownMetric(
        /*is_shown=*/true);
  } else {
    // Since the card is saved locally on the device, immediately confirm
    // saving the card within the bottom sheet.
    [self showConfirmationForUploadSave:NO];
  }
}

- (void)didCancel {
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  model->OnCanceled();
  autofill::autofill_metrics::LogSaveCreditCardPromptResultIOS(
      autofill::autofill_metrics::SaveCreditCardPromptResultIOS::kDenied,
      model->is_for_upload(),
      model->save_card_delegate()->GetSaveCreditCardOptions(),
      SaveCreditCardPromptOverlayType::kBottomSheet);
  _dismissing = YES;
  [_autofillCommandsHandler dismissSaveCardBottomSheet];
}

- (void)didUpdateValue:(NSString*)value
              forField:(AutofillCreditCardUIType)type {
  BOOL isValid = [self isValidValue:value forField:type];

  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      _cardNumber = value;
      _cardNumberValid = isValid;
      break;
    case AutofillCreditCardUIType::kExpMonth:
      _expirationDate = value;
      _expirationDateValid = isValid;
      break;
    case AutofillCreditCardUIType::kSecurityCode:
      _cvc = value;
      _cardCVCValid = isValid;
      break;
    case AutofillCreditCardUIType::kNickname:
      _nickname = value;
      _nicknameValid = isValid;
      break;
    case AutofillCreditCardUIType::kFullName:
      _cardholderName = value;
      break;
    default:
      NOTREACHED();
  }

  NSString* errorMessage =
      isValid ? nil : [AutofillSettingsUtil errorMessageForUIType:type];
  if ([self.consumer
          respondsToSelector:@selector(setField:isValid:errorMessage:)]) {
    [self.consumer setField:type isValid:isValid errorMessage:errorMessage];
  }
  [self updateSaveButtonStatus];
}

- (void)updateSaveButtonStatus {
  BOOL enableSave = _cardNumberValid && _expirationDateValid && _cardCVCValid &&
                    _nicknameValid;
  if ([self.consumer respondsToSelector:@selector(setSaveButtonEnabled:)]) {
    [self.consumer setSaveButtonEnabled:enableSave];
  }
}

- (void)didTapSave {
  autofill::payments::PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails
      details;
  details.card_number = base::SysNSStringToUTF16(_cardNumber);
  details.cardholder_name = base::SysNSStringToUTF16(_cardholderName);
  details.cvc = base::SysNSStringToUTF16(_cvc);
  if (_cvc.length > 0) {
    details.security_code = base::SysNSStringToUTF16(_cvc);
  }
  if (_nickname.length > 0) {
    details.nickname = base::SysNSStringToUTF16(_nickname);
  }

  // Parse expiration date.
  std::pair<NSString*, NSString*> parsedDate =
      ParseExpirationDate(_expirationDate);
  NSString* parsedMonth = parsedDate.first;
  NSString* parsedYear = parsedDate.second;
  details.expiration_date_month = base::SysNSStringToUTF16(parsedMonth);
  details.expiration_date_year = base::SysNSStringToUTF16(parsedYear);

  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  model->OnUpdatedAndAcceptedForSaveAndFill(std::move(details));
}

- (BOOL)isValidValue:(NSString*)value forField:(AutofillCreditCardUIType)type {
  switch (type) {
    case AutofillCreditCardUIType::kNumber:
      return [AutofillCreditCardUtil
          isValidCreditCardNumber:value
                         appLocal:std::string(
                                      GetApplicationContext()
                                          ->GetApplicationLocaleStorage()
                                          ->GetTag()
                                          .tag_string())];
    case AutofillCreditCardUIType::kExpMonth: {
      std::pair<NSString*, NSString*> parsedDate = ParseExpirationDate(value);
      NSString* expMonth = parsedDate.first;
      NSString* expYear = parsedDate.second;

      if (expMonth.length > 0 && expYear.length > 0) {
        return
            [AutofillCreditCardUtil
                isValidCreditCardExpirationMonth:expMonth] &&
            [AutofillCreditCardUtil
                isValidCreditCardExpirationYear:expYear
                                       appLocal:
                                           std::string(
                                               GetApplicationContext()
                                                   ->GetApplicationLocaleStorage()
                                                   ->GetTag()
                                                   .tag_string())];
      }
      return NO;
    }
    case AutofillCreditCardUIType::kNickname:
      return [AutofillCreditCardUtil
          isValidCardNickname:[value
                                  stringByTrimmingCharactersInSet:
                                      [NSCharacterSet
                                          whitespaceAndNewlineCharacterSet]]];
    case AutofillCreditCardUIType::kSecurityCode:
      return [AutofillCreditCardUtil isValidCardCvc:value];
    default:
      return YES;
  }
}

#pragma mark - SaveCardBottomSheetModel Observer

- (void)onCreditCardUploadCompleted:(BOOL)cardSaved {
  autofill::autofill_metrics::LogCreditCardUploadLoadingViewResultMetric(
      autofill::autofill_metrics::LegacySaveCardPromptResult::kNotInteracted);

  if (cardSaved) {
    [self showConfirmationForUploadSave:YES];
  } else {
    _dismissing = YES;
    [_autofillCommandsHandler dismissSaveCardBottomSheet];
  }
}

#pragma mark - Private

- (void)showConfirmationForUploadSave:(BOOL)forUpload {
  [_consumer showConfirmationState];
  autofill::autofill_metrics::LogCreditCardUploadConfirmationViewShownMetric(
      /*is_shown=*/true, forUpload);

  // Auto dismiss bottomsheet after showing successful save card confirmation.
  __weak __typeof(self) weakSelf = self;
  _autoDismissConfirmationTimer.Start(
      FROM_HERE, GetConfirmationDismissDelay(),
      base::BindOnce(
          [](__weak __typeof(self) weakSelf) {
            if (weakSelf) {
              [weakSelf dimissConfirmationStateOnTimeout];
            }
          },
          weakSelf));
}

- (void)dimissConfirmationStateOnTimeout {
  _autoDismissConfirmationTimer.Stop();
  autofill::SaveCardBottomSheetModel* model = [self model];
  CHECK(model);
  autofill::autofill_metrics::LogCreditCardUploadConfirmationViewResultMetric(
      autofill::autofill_metrics::LegacySaveCardPromptResult::kNotInteracted,
      /*is_card_uploaded=*/model->is_for_upload());
  _dismissing = YES;
  [_autofillCommandsHandler dismissSaveCardBottomSheet];
}

@end
