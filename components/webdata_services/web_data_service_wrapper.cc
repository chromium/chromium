// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"
#include "components/autofill/core/browser/webdata/addresses/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/addresses/contact_info_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_credential_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_offer_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/autofill_wallet_usage_data_sync_bridge.h"
#include "components/autofill/core/browser/webdata/payments/payments_autofill_table.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/sync/base/features.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"

#if BUILDFLAG(USE_BLINK)
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_method_manifest_table.h"
#include "components/payments/content/web_app_manifest_section_table.h"
#endif

namespace {

void InitAutofillSyncBridgesOnDBSequence(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  autofill::AutocompleteSyncBridge::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend);
  autofill::AutofillProfileSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
  autofill::ContactInfoSyncBridge::CreateForWebDataServiceAndBackend(
      autofill_backend, autofill_web_data.get());
}

void InitWalletSyncBridgesOnDBSequence(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  autofill::AutofillWalletSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
  autofill::AutofillWalletMetadataSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
}

void InitWalletOfferSyncBridgeOnDBSequence(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());
  autofill::AutofillWalletOfferSyncBridge::CreateForWebDataServiceAndBackend(
      autofill_backend, autofill_web_data.get());
}

void InitWalletUsageDataSyncBridgeOnDBSequence(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());
  autofill::AutofillWalletUsageDataSyncBridge::
      CreateForWebDataServiceAndBackend(autofill_backend,
                                        autofill_web_data.get());
}

void InitWalletCredentialSyncBridgeOnDBSequence(
    scoped_refptr<base::SequencedTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    autofill::AutofillWebDataBackend* autofill_backend) {
  CHECK(db_task_runner->RunsTasksInCurrentSequence());
  autofill::AutofillWalletCredentialSyncBridge::
      CreateForWebDataServiceAndBackend(autofill_backend,
                                        autofill_web_data.get());
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() = default;

WebDataServiceWrapper::WebDataServiceWrapper(
    const base::FilePath& context_path,
    const std::string& application_locale,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const ShowErrorCallback& show_error_callback,
    os_crypt_async::OSCryptAsync* os_crypt,
    bool use_in_memory_autofill_account_database) {
  base::FilePath path = context_path.Append(kWebDataFilename);
  auto db_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  profile_database_ = base::MakeRefCounted<WebDatabaseService>(
      path, ui_task_runner, db_task_runner);

  // All tables objects that participate in managing the database must
  // be added here.
  profile_database_->AddTable(
      std::make_unique<autofill::AddressAutofillTable>());
  profile_database_->AddTable(std::make_unique<autofill::AutocompleteTable>());
  profile_database_->AddTable(
      std::make_unique<autofill::AutofillSyncMetadataTable>());
  profile_database_->AddTable(
      std::make_unique<autofill::PaymentsAutofillTable>());
  profile_database_->AddTable(std::make_unique<KeywordTable>());
  profile_database_->AddTable(std::make_unique<TokenServiceTable>());
#if BUILDFLAG(USE_BLINK)
  profile_database_->AddTable(
      std::make_unique<payments::PaymentMethodManifestTable>());
  profile_database_->AddTable(
      std::make_unique<payments::WebAppManifestSectionTable>());
#endif
  profile_database_->AddTable(
      std::make_unique<plus_addresses::PlusAddressTable>());
  profile_database_->LoadDatabase(os_crypt);

  profile_autofill_web_data_ =
      base::MakeRefCounted<autofill::AutofillWebDataService>(profile_database_,
                                                             ui_task_runner);
  profile_autofill_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_AUTOFILL));

  keyword_web_data_ = base::MakeRefCounted<KeywordWebDataService>(
      profile_database_, ui_task_runner);
  keyword_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_KEYWORD));

  plus_address_web_data_ =
      base::MakeRefCounted<plus_addresses::PlusAddressWebDataService>(
          profile_database_, ui_task_runner);
  plus_address_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_PLUS_ADDRESS));

  token_web_data_ =
      base::MakeRefCounted<TokenWebData>(profile_database_, ui_task_runner);
  token_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_TOKEN));

#if BUILDFLAG(USE_BLINK)
  payment_manifest_web_data_ =
      base::MakeRefCounted<payments::PaymentManifestWebDataService>(
          profile_database_, ui_task_runner);
  payment_manifest_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_PAYMENT_MANIFEST));
#endif

  profile_autofill_web_data_->GetAutofillBackend(
      base::BindOnce(&InitAutofillSyncBridgesOnDBSequence, db_task_runner,
                     profile_autofill_web_data_, application_locale));
  profile_autofill_web_data_->GetAutofillBackend(
      base::BindOnce(&InitWalletSyncBridgesOnDBSequence, db_task_runner,
                     profile_autofill_web_data_, application_locale));
  profile_autofill_web_data_->GetAutofillBackend(
      base::BindOnce(&InitWalletOfferSyncBridgeOnDBSequence, db_task_runner,
                     profile_autofill_web_data_));
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletUsageData)) {
    profile_autofill_web_data_->GetAutofillBackend(
        base::BindOnce(&InitWalletUsageDataSyncBridgeOnDBSequence,
                       db_task_runner, profile_autofill_web_data_));
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletCredentialData)) {
    profile_autofill_web_data_->GetAutofillBackend(
        base::BindOnce(&InitWalletCredentialSyncBridgeOnDBSequence,
                       db_task_runner, profile_autofill_web_data_));
  }

  const base::FilePath account_storage_path =
      use_in_memory_autofill_account_database
          ? base::FilePath(WebDatabase::kInMemoryPath)
          : context_path.Append(kAccountWebDataFilename);

  // Account database must run backend on same sequence as profile database. See
  // comment in ChromeSyncClient::CreateDataTypeControllers.
  account_database_ = base::MakeRefCounted<WebDatabaseService>(
      account_storage_path, ui_task_runner, db_task_runner);
  account_database_->AddTable(
      std::make_unique<autofill::AutofillSyncMetadataTable>());
  account_database_->AddTable(
      std::make_unique<autofill::PaymentsAutofillTable>());
  account_database_->LoadDatabase(os_crypt);

  account_autofill_web_data_ =
      base::MakeRefCounted<autofill::AutofillWebDataService>(account_database_,
                                                             ui_task_runner);
  account_autofill_web_data_->Init(
      base::BindOnce(show_error_callback, ERROR_LOADING_ACCOUNT_AUTOFILL));
  account_autofill_web_data_->GetAutofillBackend(
      base::BindOnce(&InitWalletSyncBridgesOnDBSequence, db_task_runner,
                     account_autofill_web_data_, application_locale));
  account_autofill_web_data_->GetAutofillBackend(
      base::BindOnce(&InitWalletOfferSyncBridgeOnDBSequence, db_task_runner,
                     account_autofill_web_data_));
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletUsageData)) {
    account_autofill_web_data_->GetAutofillBackend(
        base::BindOnce(&InitWalletUsageDataSyncBridgeOnDBSequence,
                       db_task_runner, account_autofill_web_data_));
  }
  if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletCredentialData)) {
    account_autofill_web_data_->GetAutofillBackend(
        base::BindOnce(&InitWalletCredentialSyncBridgeOnDBSequence,
                       db_task_runner, account_autofill_web_data_));
  }
}

WebDataServiceWrapper::~WebDataServiceWrapper() = default;

void WebDataServiceWrapper::Shutdown() {
  profile_autofill_web_data_->ShutdownOnUISequence();
  account_autofill_web_data_->ShutdownOnUISequence();
  keyword_web_data_->ShutdownOnUISequence();
  token_web_data_->ShutdownOnUISequence();

#if BUILDFLAG(USE_BLINK)
  payment_manifest_web_data_->ShutdownOnUISequence();
#endif

  profile_database_->ShutdownDatabase();
  account_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetProfileAutofillWebData() {
  return profile_autofill_web_data_;
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetAccountAutofillWebData() {
  return account_autofill_web_data_;
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_;
}

scoped_refptr<plus_addresses::PlusAddressWebDataService>
WebDataServiceWrapper::GetPlusAddressWebData() {
  return plus_address_web_data_;
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_;
}

#if BUILDFLAG(USE_BLINK)
scoped_refptr<payments::PaymentManifestWebDataService>
WebDataServiceWrapper::GetPaymentManifestWebData() {
  return payment_manifest_web_data_;
}
#endif
