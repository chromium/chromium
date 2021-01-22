// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_CLIENT_IOS_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_CLIENT_IOS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/strike_database.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#import "ios/web/public/web_state.h"
#import "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"
#include "ios/web_view/internal/web_view_browser_state.h"

namespace autofill {

// WebView implementation of AutofillClient.
class WebViewAutofillClientIOS : public AutofillClient {
 public:
  static std::unique_ptr<WebViewAutofillClientIOS> Create(
      web::WebState* web_state,
      ios_web_view::WebViewBrowserState* browser_state);

  WebViewAutofillClientIOS(
      const std::string& locale,
      PrefService* pref_service,
      PersonalDataManager* personal_data_manager,
      AutocompleteHistoryManager* autocomplete_history_manager,
      web::WebState* web_state,
      signin::IdentityManager* identity_manager,
      StrikeDatabase* strike_database,
      syncer::SyncService* sync_service,
      std::unique_ptr<autofill::LogManager> log_manager);
  ~WebViewAutofillClientIOS() override;

  // AutofillClient:
  PersonalDataManager* GetPersonalDataManager() override;
  AutocompleteHistoryManager* GetAutocompleteHistoryManager() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  signin::IdentityManager* GetIdentityManager() override;
  FormDataImporter* GetFormDataImporter() override;
  payments::PaymentsClient* GetPaymentsClient() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  const GURL& GetLastCommittedURL() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  const translate::LanguageState* GetLanguageState() override;
  translate::TranslateDriver* GetTranslateDriver() override;
  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ConfirmAccountNameFixFlow(
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmExpirationDateFixFlow(
      const CreditCard& card,
      base::OnceCallback<void(const base::string16&, const base::string16&)>
          callback) override;
  void ConfirmSaveCreditCardLocally(
      const CreditCard& card,
      SaveCreditCardOptions options,
      LocalSaveCardPromptCallback callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      const LegalMessageLines& legal_message_lines,
      SaveCreditCardOptions options,
      UploadSaveCardPromptCallback callback) override;
  void CreditCardUploadCompleted(bool card_saved) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   base::OnceClosure callback) override;
  void ConfirmSaveAddressProfile(
      const AutofillProfile& profile,
      AddressProfileSavePromptCallback callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(CreditCardScanCallback callback) override;
  void ShowAutofillPopup(
      const AutofillClient::PopupOpenArgs& open_args,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  base::span<const Suggestion> GetPopupSuggestions() const override;
  void PinPopupView() override;
  AutofillClient::PopupOpenArgs GetReopenPopupArgs() const override;
  void UpdatePopup(const std::vector<Suggestion>& suggestions,
                   PopupType popup_type) override;
  void HideAutofillPopup(PopupHidingReason reason) override;
  bool IsAutocompleteEnabled() override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

  // RiskDataLoader:
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;

  LogManager* GetLogManager() const override;

  void set_bridge(id<CWVAutofillClientIOSBridge> bridge) { bridge_ = bridge; }

  bool IsQueryIDRelevant(int query_id) override;

 private:
  PrefService* pref_service_;
  PersonalDataManager* personal_data_manager_;
  AutocompleteHistoryManager* autocomplete_history_manager_;
  web::WebState* web_state_;
  __weak id<CWVAutofillClientIOSBridge> bridge_;
  signin::IdentityManager* identity_manager_;
  std::unique_ptr<payments::PaymentsClient> payments_client_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  StrikeDatabase* strike_database_;
  syncer::SyncService* sync_service_ = nullptr;
  std::unique_ptr<LogManager> log_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebViewAutofillClientIOS);
};

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_CLIENT_IOS_H_
