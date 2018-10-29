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
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web_view/internal/autofill/cwv_autofill_client_ios_bridge.h"

namespace autofill {

// WebView implementation of AutofillClient.
class WebViewAutofillClientIOS : public AutofillClient {
 public:
  WebViewAutofillClientIOS(
      PrefService* pref_service,
      PersonalDataManager* personal_data_manager,
      web::WebState* web_state,
      id<CWVAutofillClientIOSBridge> bridge,
      identity::IdentityManager* identity_manager,
      StrikeDatabase* strike_database,
      scoped_refptr<AutofillWebDataService> autofill_web_data_service,
      syncer::SyncService* sync_service);
  ~WebViewAutofillClientIOS() override;

  // AutofillClient implementation.
  PersonalDataManager* GetPersonalDataManager() override;
  PrefService* GetPrefs() override;
  syncer::SyncService* GetSyncService() override;
  identity::IdentityManager* GetIdentityManager() override;
  StrikeDatabase* GetStrikeDatabase() override;
  ukm::UkmRecorder* GetUkmRecorder() override;
  ukm::SourceId GetUkmSourceId() override;
  AddressNormalizer* GetAddressNormalizer() override;
  security_state::SecurityLevel GetSecurityLevelForUmaHistograms() override;
  void ShowAutofillSettings(bool show_credit_card_settings) override;
  void ShowUnmaskPrompt(const CreditCard& card,
                        UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(PaymentsRpcResult result) override;
  void ShowLocalCardMigrationDialog(
      base::OnceClosure show_migration_dialog_closure) override;
  void ConfirmMigrateLocalCardToCloud(
      std::unique_ptr<base::DictionaryValue> legal_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      LocalCardMigrationCallback start_migrating_cards_callback) override;
  void ConfirmSaveAutofillProfile(const AutofillProfile& profile,
                                  base::OnceClosure callback) override;
  void ConfirmSaveCreditCardLocally(const CreditCard& card,
                                    bool show_prompt,
                                    base::OnceClosure callback) override;
  void ConfirmSaveCreditCardToCloud(
      const CreditCard& card,
      std::unique_ptr<base::DictionaryValue> legal_message,
      bool should_request_name_from_user,
      bool show_prompt,
      base::OnceCallback<void(const base::string16&)> callback) override;
  void ConfirmCreditCardFillAssist(const CreditCard& card,
                                   const base::Closure& callback) override;
  void LoadRiskData(
      base::OnceCallback<void(const std::string&)> callback) override;
  bool HasCreditCardScanFeature() override;
  void ScanCreditCard(const CreditCardScanCallback& callback) override;
  void ShowAutofillPopup(
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction,
      const std::vector<Suggestion>& suggestions,
      bool /*unused_autoselect_first_suggestion*/,
      base::WeakPtr<AutofillPopupDelegate> delegate) override;
  void HideAutofillPopup() override;
  bool IsAutocompleteEnabled() override;
  void UpdateAutofillPopupDataListValues(
      const std::vector<base::string16>& values,
      const std::vector<base::string16>& labels) override;
  void PropagateAutofillPredictions(
      content::RenderFrameHost* rfh,
      const std::vector<FormStructure*>& forms) override;
  void DidFillOrPreviewField(const base::string16& autofilled_value,
                             const base::string16& profile_full_name) override;
  scoped_refptr<AutofillWebDataService> GetDatabase() override;
  void DidInteractWithNonsecureCreditCardInput() override;
  bool IsContextSecure() override;
  bool ShouldShowSigninPromo() override;
  bool AreServerCardsSupported() override;
  void ExecuteCommand(int id) override;

 private:
  PrefService* pref_service_;
  PersonalDataManager* personal_data_manager_;
  web::WebState* web_state_;
  __weak id<CWVAutofillClientIOSBridge> bridge_;
  identity::IdentityManager* identity_manager_;
  StrikeDatabase* strike_database_;
  scoped_refptr<AutofillWebDataService> autofill_web_data_service_;
  syncer::SyncService* sync_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebViewAutofillClientIOS);
};

}  // namespace autofill

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_WEB_VIEW_AUTOFILL_CLIENT_IOS_H_
