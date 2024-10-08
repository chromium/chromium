// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_H_
#define COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/syncable_service.h"
#include "sql/init_status.h"

class KeywordWebDataService;
class TokenWebData;
class WebDatabaseService;

#if BUILDFLAG(USE_BLINK)
namespace payments {
class PaymentManifestWebDataService;
}  // namespace payments
#endif

namespace autofill {
class AutofillWebDataService;
}  // namespace autofill

namespace plus_addresses {
class PlusAddressWebDataService;
}  // namespace plus_addresses

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace os_crypt_async {
class OSCryptAsync;
}

// WebDataServiceWrapper is a KeyedService that owns multiple WebDataServices
// so that they can be associated with a context.
class WebDataServiceWrapper : public KeyedService {
 public:
  // ErrorType indicates which service encountered an error loading its data.
  enum ErrorType {
    ERROR_LOADING_AUTOFILL,
    ERROR_LOADING_ACCOUNT_AUTOFILL,
    ERROR_LOADING_KEYWORD,
    ERROR_LOADING_TOKEN,
    ERROR_LOADING_PASSWORD,
    ERROR_LOADING_PAYMENT_MANIFEST,
    ERROR_LOADING_PLUS_ADDRESS,
  };

  // Shows an error message if a loading error occurs.
  // |error_type| shows which service encountered an error while loading.
  // |init_status| is the returned status of initializing the underlying
  // database.
  // |diagnostics| contains information about the underlying database
  // which can help in identifying the cause of the error.
  using ShowErrorCallback =
      base::RepeatingCallback<void(ErrorType error_type,
                                   sql::InitStatus init_status,
                                   const std::string& diagnostics)>;

  // Constructor for WebDataServiceWrapper that initializes the different
  // WebDataServices.
  WebDataServiceWrapper(
      const base::FilePath& context_path,
      const std::string& application_locale,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const ShowErrorCallback& show_error_callback,
      os_crypt_async::OSCryptAsync* os_crypt,
      bool use_in_memory_autofill_account_database);

  WebDataServiceWrapper(const WebDataServiceWrapper&) = delete;
  WebDataServiceWrapper& operator=(const WebDataServiceWrapper&) = delete;

  ~WebDataServiceWrapper() override;

  // KeyedService:
  void Shutdown() override;

  // Access the various types of service instances.
  scoped_refptr<autofill::AutofillWebDataService> GetProfileAutofillWebData();
  scoped_refptr<autofill::AutofillWebDataService> GetAccountAutofillWebData();
  scoped_refptr<KeywordWebDataService> GetKeywordWebData();
  scoped_refptr<plus_addresses::PlusAddressWebDataService>
  GetPlusAddressWebData();
  scoped_refptr<TokenWebData> GetTokenWebData();
#if BUILDFLAG(USE_BLINK)
  // Virtual for testing.
  virtual scoped_refptr<payments::PaymentManifestWebDataService>
  GetPaymentManifestWebData();
#endif

 protected:
  // For testing.
  WebDataServiceWrapper();

 private:
  scoped_refptr<WebDatabaseService> profile_database_;
  scoped_refptr<WebDatabaseService> account_database_;

  scoped_refptr<autofill::AutofillWebDataService> profile_autofill_web_data_;
  scoped_refptr<autofill::AutofillWebDataService> account_autofill_web_data_;
  scoped_refptr<KeywordWebDataService> keyword_web_data_;
  scoped_refptr<plus_addresses::PlusAddressWebDataService>
      plus_address_web_data_;
  scoped_refptr<TokenWebData> token_web_data_;

#if BUILDFLAG(USE_BLINK)
  scoped_refptr<payments::PaymentManifestWebDataService>
      payment_manifest_web_data_;
#endif
};

#endif  // COMPONENTS_WEBDATA_SERVICES_WEB_DATA_SERVICE_WRAPPER_H_
