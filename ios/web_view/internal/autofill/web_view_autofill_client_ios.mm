// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"

#import <utility>
#import <vector>

#import "base/check.h"
#import "base/check_deref.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "components/autofill/core/browser/form_data_importer.h"
#import "components/autofill/core/browser/logging/log_router.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/security_state/ios/security_state_utils.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/ios_web_view_payments_autofill_client.h"
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
      // TODO(crbug.com/40612524): Replace the closure with a callback to the
      // renderer that indicates if log messages should be sent from the
      // renderer.
      LogManager::Create(
          autofill::WebViewAutofillLogRouterFactory::GetForBrowserState(
              browser_state),
          base::RepeatingClosure()));
}

WebViewAutofillClientIOS::WebViewAutofillClientIOS(
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
      strike_database_(strike_database),
      sync_service_(sync_service),
      log_manager_(std::move(log_manager)) {}

WebViewAutofillClientIOS::~WebViewAutofillClientIOS() {
  HideAutofillSuggestions(SuggestionHidingReason::kTabGone);
}

bool WebViewAutofillClientIOS::IsOffTheRecord() const {
  return web_state_->GetBrowserState()->IsOffTheRecord();
}

scoped_refptr<network::SharedURLLoaderFactory>
WebViewAutofillClientIOS::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      web_state_->GetBrowserState()->GetURLLoaderFactory());
}

AutofillDriverFactory& WebViewAutofillClientIOS::GetAutofillDriverFactory() {
  return CHECK_DEREF(AutofillDriverIOSFactory::FromWebState(web_state_));
}

AutofillCrowdsourcingManager*
WebViewAutofillClientIOS::GetCrowdsourcingManager() {
  if (!crowdsourcing_manager_) {
    // Lazy initialization to avoid virtual function calls in the constructor.
    crowdsourcing_manager_ = std::make_unique<AutofillCrowdsourcingManager>(
        this, GetChannel(), GetLogManager());
  }
  return crowdsourcing_manager_.get();
}

PersonalDataManager* WebViewAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

AutocompleteHistoryManager*
WebViewAutofillClientIOS::GetAutocompleteHistoryManager() {
  return autocomplete_history_manager_;
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
  return const_cast<signin::IdentityManager*>(
      std::as_const(*this).GetIdentityManager());
}

const signin::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager()
    const {
  return identity_manager_;
}

FormDataImporter* WebViewAutofillClientIOS::GetFormDataImporter() {
  if (!form_data_importer_) {
    form_data_importer_ = std::make_unique<FormDataImporter>(
        this,
        /*history_service=*/nullptr,
        ios_web_view::ApplicationContext::GetInstance()
            ->GetApplicationLocale());
  }
  return form_data_importer_.get();
}

payments::PaymentsAutofillClient*
WebViewAutofillClientIOS::GetPaymentsAutofillClient() {
  return &payments_autofill_client_;
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

void WebViewAutofillClientIOS::ShowAutofillSettings(
    SuggestionType suggestion_type) {
  NOTREACHED_IN_MIGRATION();
}

void WebViewAutofillClientIOS::ConfirmSaveAddressProfile(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    bool is_migration_to_account,
    AddressProfileSavePromptCallback callback) {
  [bridge_ confirmSaveAddressProfile:profile
                     originalProfile:original_profile
                            callback:std::move(callback)];
}

void WebViewAutofillClientIOS::ShowEditAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileSavePromptCallback on_user_decision_callback) {
  // Please note: This method is only implemented on desktop and is therefore
  // unreachable here.
  NOTREACHED_IN_MIGRATION();
}

void WebViewAutofillClientIOS::ShowDeleteAddressProfileDialog(
    const AutofillProfile& profile,
    AddressProfileDeleteDialogCallback delete_dialog_callback) {
  // Please note: This method is only implemented on desktop and is therefore
  // unreachable here.
  NOTREACHED_IN_MIGRATION();
}

AutofillClient::SuggestionUiSessionId
WebViewAutofillClientIOS::ShowAutofillSuggestions(
    const AutofillClient::PopupOpenArgs& open_args,
    base::WeakPtr<AutofillSuggestionDelegate> delegate) {
  [bridge_ showAutofillPopup:open_args.suggestions suggestionDelegate:delegate];
  return SuggestionUiSessionId();
}

void WebViewAutofillClientIOS::UpdateAutofillDataListValues(
    base::span<const autofill::SelectOption> datalist) {
  // No op. ios/web_view does not support display datalist.
}

void WebViewAutofillClientIOS::PinAutofillSuggestions() {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::HideAutofillSuggestions(
    SuggestionHidingReason reason) {
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

bool WebViewAutofillClientIOS::IsContextSecure() const {
  return IsContextSecureForWebState(web_state_);
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

LogManager* WebViewAutofillClientIOS::GetLogManager() const {
  return log_manager_.get();
}

void WebViewAutofillClientIOS::set_bridge(
    id<CWVAutofillClientIOSBridge> bridge) {
  bridge_ = bridge;
  payments_autofill_client_.set_bridge(bridge);
}

}  // namespace autofill
