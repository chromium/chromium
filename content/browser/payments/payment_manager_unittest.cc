// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/browser/payments/payment_app_content_unittest_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

using ::payments::mojom::PaymentHandlerStatus;
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

const char kServiceWorkerScope[] = "https://example.test/a/";
const char kServiceWorkerScript[] = "https://example.test/a/script.js";
const char kServiceWorkerScope2[] = "https://example.test/b/";
const char kServiceWorkerScript2[] = "https://example.test/b/script.js";

void DeletePaymentInstrumentCallback(PaymentHandlerStatus* out_status,
                                     PaymentHandlerStatus status) {
  *out_status = status;
}

void KeysOfPaymentInstrumentsCallback(std::vector<std::string>* out_keys,
                                      PaymentHandlerStatus* out_status,
                                      const std::vector<std::string>& keys,
                                      PaymentHandlerStatus status) {
  *out_keys = keys;
  *out_status = status;
}

void HasPaymentInstrumentCallback(PaymentHandlerStatus* out_status,
                                  PaymentHandlerStatus status) {
  *out_status = status;
}

void GetPaymentInstrumentCallback(PaymentInstrumentPtr* out_instrument,
                                  PaymentHandlerStatus* out_status,
                                  PaymentInstrumentPtr instrument,
                                  PaymentHandlerStatus status) {
  *out_instrument = std::move(instrument);
  *out_status = status;
}

void ClearPaymentInstrumentsCallback(PaymentHandlerStatus* out_status,
                                     PaymentHandlerStatus status) {
  *out_status = status;
}

}  // namespace

class PaymentManagerTest : public PaymentAppContentUnitTestBase {
 public:
  PaymentManagerTest() {
    manager_ = CreatePaymentManager(GURL(kServiceWorkerScope),
                                    GURL(kServiceWorkerScript));
    EXPECT_NE(nullptr, manager_);
  }

  PaymentManagerTest(const PaymentManagerTest&) = delete;
  PaymentManagerTest& operator=(const PaymentManagerTest&) = delete;

  PaymentManager* payment_manager() const { return manager_; }

  void DeletePaymentInstrument(const std::string& instrument_key,
                               PaymentHandlerStatus* out_status) {
    manager_->DeletePaymentInstrument(
        instrument_key,
        base::BindOnce(&DeletePaymentInstrumentCallback, out_status));
    base::RunLoop().RunUntilIdle();
  }

  void KeysOfPaymentInstruments(std::vector<std::string>* out_keys,
                                PaymentHandlerStatus* out_status) {
    manager_->KeysOfPaymentInstruments(base::BindOnce(
        &KeysOfPaymentInstrumentsCallback, out_keys, out_status));
    base::RunLoop().RunUntilIdle();
  }

  void HasPaymentInstrument(const std::string& instrument_key,
                            PaymentHandlerStatus* out_status) {
    manager_->HasPaymentInstrument(
        instrument_key,
        base::BindOnce(&HasPaymentInstrumentCallback, out_status));
    base::RunLoop().RunUntilIdle();
  }

  void GetPaymentInstrument(const std::string& instrument_key,
                            PaymentInstrumentPtr* out_instrument,
                            PaymentHandlerStatus* out_status) {
    manager_->GetPaymentInstrument(instrument_key,
                                   base::BindOnce(&GetPaymentInstrumentCallback,
                                                  out_instrument, out_status));
    base::RunLoop().RunUntilIdle();
  }

  void ClearPaymentInstruments(PaymentHandlerStatus* out_status) {
    manager_->ClearPaymentInstruments(
        base::BindOnce(&ClearPaymentInstrumentsCallback, out_status));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // Owned by payment_app_context_.
  raw_ptr<PaymentManager> manager_;
};

TEST_F(PaymentManagerTest, GetUnstoredPaymentInstrument) {
  PaymentHandlerStatus read_status = PaymentHandlerStatus::SUCCESS;
  PaymentInstrumentPtr read_details;
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, read_status);
}

TEST_F(PaymentManagerTest, UninitializedPaymentManager) {
  manager_ = CreateUninitializedPaymentManager(GURL(kServiceWorkerScope2),
                                               GURL(kServiceWorkerScript2));

  // Test that calling the payment manager does not crash, and instead
  // disconnects due to the invalid state (Init not called).
  PaymentHandlerStatus status = PaymentHandlerStatus::NOT_FOUND;
  DeletePaymentInstrument("test_key", &status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, status);

  PaymentInstrumentPtr instrument;
  GetPaymentInstrument("test_key", &instrument, &status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, status);

  std::vector<std::string> keys;
  KeysOfPaymentInstruments(&keys, &status);

  HasPaymentInstrument("test_key", &status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, status);

  ClearPaymentInstruments(&status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, status);
}

}  // namespace content
