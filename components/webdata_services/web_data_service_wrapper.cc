// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/signin/public/webdata/token_web_data.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"

#if !defined(OS_IOS)
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_method_manifest_table.h"
#include "components/payments/content/web_app_manifest_section_table.h"
#endif

namespace {

void InitAutofillSyncBridgesOnDBSequence(
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  autofill::AutocompleteSyncBridge::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend);
  autofill::AutofillProfileSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
}

void InitWalletSyncBridgesOnDBSequence(
    scoped_refptr<base::SingleThreadTaskRunner> db_task_runner,
    const scoped_refptr<autofill::AutofillWebDataService>& autofill_web_data,
    const base::FilePath& context_path,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(db_task_runner->RunsTasksInCurrentSequence());

  autofill::AutofillWalletSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
  autofill::AutofillWalletMetadataSyncBridge::CreateForWebDataServiceAndBackend(
      app_locale, autofill_backend, autofill_web_data.get());
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() {}

WebDataServiceWrapper::WebDataServiceWrapper(
    const base::FilePath& context_path,
    const std::string& application_locale,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const ShowErrorCallback& show_error_callback) {
  base::FilePath path = context_path.Append(kWebDataFilename);
  // TODO(pkasting): http://crbug.com/740773 This should likely be sequenced,
  // not single-threaded; it's also possible the various uses of this below
  // should each use their own sequences instead of sharing this one.
  auto db_task_runner = base::CreateSingleThreadTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  profile_database_ =
      new WebDatabaseService(path, ui_task_runner, db_task_runner);

  // All tables objects that participate in managing the database must
  // be added here.
  profile_database_->AddTable(std::make_unique<autofill::AutofillTable>());
  profile_database_->AddTable(std::make_unique<KeywordTable>());
  profile_database_->AddTable(std::make_unique<TokenServiceTable>());
#if !defined(OS_IOS)
  profile_database_->AddTable(
      std::make_unique<payments::PaymentMethodManifestTable>());
  profile_database_->AddTable(
      std::make_unique<payments::WebAppManifestSectionTable>());
#endif
  profile_database_->LoadDatabase();

  profile_autofill_web_data_ = new autofill::AutofillWebDataService(
      profile_database_, ui_task_runner, db_task_runner,
      base::Bind(show_error_callback, ERROR_LOADING_AUTOFILL));
  profile_autofill_web_data_->Init();

  keyword_web_data_ = new KeywordWebDataService(
      profile_database_, ui_task_runner,
      base::Bind(show_error_callback, ERROR_LOADING_KEYWORD));
  keyword_web_data_->Init();

  token_web_data_ =
      new TokenWebData(profile_database_, ui_task_runner, db_task_runner,
                       base::Bind(show_error_callback, ERROR_LOADING_TOKEN));
  token_web_data_->Init();

#if !defined(OS_IOS)
  payment_manifest_web_data_ = new payments::PaymentManifestWebDataService(
      profile_database_,
      base::Bind(show_error_callback, ERROR_LOADING_PAYMENT_MANIFEST),
      ui_task_runner);
#endif

  profile_autofill_web_data_->GetAutofillBackend(
      base::Bind(&InitAutofillSyncBridgesOnDBSequence, db_task_runner,
                 profile_autofill_web_data_, application_locale));
  profile_autofill_web_data_->GetAutofillBackend(
      base::Bind(&InitWalletSyncBridgesOnDBSequence, db_task_runner,
                 profile_autofill_web_data_, context_path, application_locale));

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)) {
    account_database_ =
        new WebDatabaseService(base::FilePath(WebDatabase::kInMemoryPath),
                               ui_task_runner, db_task_runner);
    account_database_->AddTable(std::make_unique<autofill::AutofillTable>());
    account_database_->LoadDatabase();

    account_autofill_web_data_ = new autofill::AutofillWebDataService(
        account_database_, ui_task_runner, db_task_runner,
        base::Bind(show_error_callback, ERROR_LOADING_ACCOUNT_AUTOFILL));
    account_autofill_web_data_->Init();
    account_autofill_web_data_->GetAutofillBackend(base::Bind(
        &InitWalletSyncBridgesOnDBSequence, db_task_runner,
        account_autofill_web_data_, context_path, application_locale));
  }
}

WebDataServiceWrapper::~WebDataServiceWrapper() {}

void WebDataServiceWrapper::Shutdown() {
  profile_autofill_web_data_->ShutdownOnUISequence();
  if (account_autofill_web_data_)
    account_autofill_web_data_->ShutdownOnUISequence();
  keyword_web_data_->ShutdownOnUISequence();
  token_web_data_->ShutdownOnUISequence();

#if !defined(OS_IOS)
  payment_manifest_web_data_->ShutdownOnUISequence();
#endif

  profile_database_->ShutdownDatabase();
  if (account_database_)
    account_database_->ShutdownDatabase();
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetProfileAutofillWebData() {
  return profile_autofill_web_data_.get();
}

scoped_refptr<autofill::AutofillWebDataService>
WebDataServiceWrapper::GetAccountAutofillWebData() {
  return account_autofill_web_data_.get();
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_.get();
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_.get();
}

#if !defined(OS_IOS)
scoped_refptr<payments::PaymentManifestWebDataService>
WebDataServiceWrapper::GetPaymentManifestWebData() {
  return payment_manifest_web_data_.get();
}
#endif
