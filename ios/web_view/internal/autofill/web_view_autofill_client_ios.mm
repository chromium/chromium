// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"

#import <utility>
#import <vector>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/form_data_importer.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#import "components/autofill/core/browser/payments/payments_client.h"
#import "components/autofill/core/browser/ui/popup_item_ids.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/web_view_autocomplete_history_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_autofill_log_router_factory.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/autofill/web_view_strike_database_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace autofill {

// static
std::unique_ptr<WebViewAutofillClientIOS> WebViewAutofillClientIOS::Create(
    web::WebState* web_state,
    ios_web_view::WebViewBrowserState* browser_state) {
  return std::make_unique<autofill::WebViewAutofillClientIOS>(
      ios_web_view::ApplicationContext::GetInstance()->GetApplicationLocale(),
      browser_state->GetPrefs(),
      ios_web_view::WebViewPersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewAutocompleteHistoryManagerFactory::
          GetForBrowserState(browser_state),
      web_state,
      ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewStrikeDatabaseFactory::GetForBrowserState(
          browser_state->GetRecordingBrowserState()),
      ios_web_view::WebViewSyncServiceFactory::GetForBrowserState(
          browser_state),
      // TODO(crbug.com/928595): Replace the closure with a callback to the
      // renderer that indicates if log messages should be sent from the
      // renderer.
      LogManager::Create(
          autofill::WebViewAutofillLogRouterFactory::GetForBrowserState(
              browser_state),
          base::RepeatingClosure()));
}

WebViewAutofillClientIOS::WebViewAutofillClientIOS(
    const std::string& locale,
    PrefService* pref_service,
    PersonalDataManager* personal_data_manager,
    AutocompleteHistoryManager* autocomplete_history_manager,
    web::WebState* web_state,
    signin::IdentityManager* identity_manager,
    StrikeDatabase* strike_database,
    syncer::SyncService* sync_service,
    std::unique_ptr<autofill::LogManager> log_manager)
    : pref_service_(pref_service),
      personal_data_manager_(personal_data_manager),
      autocomplete_history_manager_(autocomplete_history_manager),
      web_state_(web_state),
      identity_manager_(identity_manager),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              web_state_->GetBrowserState()->GetURLLoaderFactory()),
          identity_manager_,
          personal_data_manager_,
          web_state_->GetBrowserState()->IsOffTheRecord())),
      form_data_importer_(
          std::make_unique<FormDataImporter>(this,
                                             payments_client_.get(),
                                             personal_data_manager_,
                                             locale)),
      strike_database_(strike_database),
      sync_service_(sync_service),
      log_manager_(std::move(log_manager)) {}

WebViewAutofillClientIOS::~WebViewAutofillClientIOS() {
  HideAutofillPopup(PopupHidingReason::kTabGone);
}

bool WebViewAutofillClientIOS::IsOffTheRecord() {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewAutofillClientIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state_->GetBrowserState()->GetURLLoaderFactory());
}

AutofillDownloadManager* WebViewAutofillClientIOS::GetDownloadManager() {
  if (!download_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    download_manager_ = std::make_unique<AutofillDownloadManager>(
        this, GetChannel(), GetLogManager());
  }
  return download_manager_.get();
}

PersonalDataManager* WebViewAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
WebViewAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
}

CreditCardCvcAuthenticator* WebViewAutofillClientIOS::GetCvcAuthenticator() {
  if (!cvc_authenticator_) {
    cvc_authenticator_ = std::make_unique<CreditCardCvcAuthenticator>(this);
  }
  return cvc_authenticator_.get();
}

PrefService* WebViewAutofillClientIOS::GetPrefs() {
  return const_cast<PrefService*>(std::as_const(*this).GetPrefs());
}

const PrefService* WebViewAutofillClientIOS::GetPrefs() const {
  return pref_service_;
}

syncer::SyncService* WebViewAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

signin::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager() {
  return identity_manager_;
}

FormDataImporter* WebViewAutofillClientIOS::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* WebViewAutofillClientIOS::GetPaymentsClient() {
  return payments_client_.get();
}

StrikeDatabase* WebViewAutofillClientIOS::GetStrikeDatabase() {
  return strike_database_;
}

ukm::UkmRecorder* WebViewAutofillClientIOS::GetUkmRecorder() {
  // UKM recording is not supported for WebViews.
  return nullptr;
}

ukm::SourceId WebViewAutofillClientIOS::GetUkmSourceId() {
  // UKM recording is not supported for WebViews.
  return 0;
}

AddressNormalizer* WebViewAutofillClientIOS::GetAddressNormalizer() {
  return nullptr;
}

const GURL& WebViewAutofillClientIOS::GetLastCommittedPrimaryMainFrameURL()
    const {
  return web_state_->GetLastCommittedURL();
}

url::Origin WebViewAutofillClientIOS::GetLastCommittedPrimaryMainFrameOrigin()
    const {
  return url::Origin::Create(GetLastCommittedPrimaryMainFrameURL());
}

security_state::SecurityLevel
WebViewAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  return security_state::GetSecurityLevelForWebState(web_state_);
}

const translate::LanguageState* WebViewAutofillClientIOS::GetLanguageState() {
  return nullptr;
}

translate::TranslateDriver* WebViewAutofillClientIOS::GetTranslateDriver() {
  return nullptr;
}

void WebViewAutofillClientIOS::ShowAutofillSettings(PopupType popup_type) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    const CardUnmaskPromptOptions& card_unmask_prompt_options,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  [bridge_ showUnmaskPromptForCard:card
           cardUnmaskPromptOptions:card_unmask_prompt_options
                          delegate:delegate];
}

void WebViewAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  [bridge_ didReceiveUnmaskVerificationResult:result];
}

void WebViewAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    const LegalMessageLines& legal_message_lines,
    SaveCreditCardOptions options,
    UploadSaveCardPromptCallback callback) {
  DCHECK(options.show_prompt);
  [bridge_ confirmSaveCreditCardToCloud:card
                      legalMessageLines:legal_message_lines
                  saveCreditCardOptions:options
                               callback:std::move(callback)];
}

void WebViewAutofillClientIOS::CreditCardUploadCompleted(bool card_saved) {
  [bridge_ handleCreditCardUploadCompleted:card_saved];
}

void WebViewAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {}

void WebViewAutofillClientIOS::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveAddressProfilePromptOptions options,
    AddressProfileSavePromptCallback callback) {
  // TODO(crbug.com/1167062): Respect SaveAddressProfilePromptOptions.
  [bridge_ confirmSaveAddressProfile:profile
                     originalProfile:original_profile
                            callback:std::move(callback)];
}

void WebViewAutofillClientIOS::ShowEditAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileSavePromptCallback on_user_decision_callback) {
  // Please note: This method is only implemented on desktop and is therefore
  // unreachable here.
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowDeleteAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileDeleteDialogCallback delete_dialog_callback) {
  // Please note: This method is only implemented on desktop and is therefore
  // unreachable here.
  NOTREACHED();
}

bool WebViewAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void WebViewAutofillClientIOS::ScanCreditCard(CreditCardScanCallback callback) {
  NOTREACHED();
}

bool WebViewAutofillClientIOS::IsTouchToFillCreditCardSupported() {
  return false;
}

bool WebViewAutofillClientIOS::ShowTouchToFillCreditCard(
    base::WeakPtr<TouchToFillDelegate> delegate,
    base::span<const autofill::CreditCard> cards_to_suggest) {
  NOTREACHED();
  return false;
}

void WebViewAutofillClientIOS::HideTouchToFillCreditCard() {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowAutofillPopup(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:open_args.suggestions popupDelegate:delegate];
}

void WebViewAutofillClientIOS::UpdateAutofillPopupDataListValues(
    base::span<const autofill::SelectOption> datalist) {
  // No op. ios/web_view does not support display datalist.
}

std::vector<Suggestion> WebViewAutofillClientIOS::GetPopupSuggestions() const {
  NOTIMPLEMENTED();
  return {};
}

void WebViewAutofillClientIOS::PinPopupView() {
  NOTIMPLEMENTED();
}

AutofillClient::PopupOpenArgs WebViewAutofillClientIOS::GetReopenPopupArgs(
    AutofillSuggestionTriggerSource trigger_source) const {
  NOTIMPLEMENTED();
  return {};
}

void WebViewAutofillClientIOS::UpdatePopup(
    const std::vector<Suggestion>& suggestions,
    PopupType popup_type,
    AutofillSuggestionTriggerSource trigger_source) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::HideAutofillPopup(PopupHidingReason reason) {
  [bridge_ hideAutofillPopup];
}

bool WebViewAutofillClientIOS::IsAutocompleteEnabled() const {
  return false;
}

bool WebViewAutofillClientIOS::IsPasswordManagerEnabled() {
  return GetPrefs()->GetBoolean(
      password_manager::prefs::kCredentialsEnableService);
}

void WebViewAutofillClientIOS::DidFillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    AutofillTriggerSource trigger_source,
    bool is_refill) {}

void WebViewAutofillClientIOS::DidFillOrPreviewField(
    const std::u16string& autofilled_value,
    const std::u16string& profile_full_name) {}

bool WebViewAutofillClientIOS::IsContextSecure() const {
  return IsContextSecureForWebState(web_state_);
}

void WebViewAutofillClientIOS::OpenPromoCodeOfferDetailsURL(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      /*is_renderer_initiated=*/false));
}

autofill::FormInteractionsFlowId
WebViewAutofillClientIOS::GetCurrentFormInteractionsFlowId() {
  // Currently not in use here. See `ChromeAutofillClient` for a proper
  // implementation.
  return {};
}

bool WebViewAutofillClientIOS::IsLastQueriedField(FieldGlobalId field_id) {
  return [bridge_ isLastQueriedField:field_id];
}

void WebViewAutofillClientIOS::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

LogManager* WebViewAutofillClientIOS::GetLogManager() const {
  return log_manager_.get();
}

}  // namespace autofill
