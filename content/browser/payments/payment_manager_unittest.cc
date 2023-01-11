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

void DeletePaymentInstrumentCallback(PaymentHandlerStatus* out_status,
                                     PaymentHandlerStatus status) {
  *out_status = status;
}

void SetPaymentInstrumentCallback(PaymentHandlerStatus* out_status,
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

  void SetPaymentInstrument(const std::string& instrument_key,
                            PaymentInstrumentPtr instrument,
                            PaymentHandlerStatus* out_status) {
    manager_->SetPaymentInstrument(
        instrument_key, std::move(instrument),
        base::BindOnce(&SetPaymentInstrumentCallback, out_status));
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

 private:
  // Owned by payment_app_context_.
  raw_ptr<PaymentManager> manager_;
};

TEST_F(PaymentManagerTest, GetUnstoredPaymentInstrument) {
  PaymentHandlerStatus read_status = PaymentHandlerStatus::SUCCESS;
  PaymentInstrumentPtr read_details;
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, read_status);
}

TEST_F(PaymentManagerTest, DeletePaymentInstrument) {
  PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr write_details = PaymentInstrument::New();
  write_details->name = "Visa ending ****4756";
  write_details->method = "visa";
  write_details->stringified_capabilities = "{}";
  SetPaymentInstrument("test_key", std::move(write_details), &write_status);
  // Write the first instrument of a web payment app will return
  // FETCH_PAYMENT_APP_INFO_FAILED since the web app's manifest is not
  // available, but the write of the instrument is succeed, otherwise will
  // return the other errors.
  ASSERT_EQ(PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED, write_status);

  PaymentHandlerStatus read_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr read_details;
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, read_status);

  PaymentHandlerStatus delete_status = PaymentHandlerStatus::NOT_FOUND;
  DeletePaymentInstrument("test_key", &delete_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, delete_status);

  read_status = PaymentHandlerStatus::NOT_FOUND;
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, read_status);
}

TEST_F(PaymentManagerTest, HasPaymentInstrument) {
  PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr write_details = PaymentInstrument::New();
  write_details->name = "Visa ending ****4756";
  write_details->method = "visa";
  write_details->stringified_capabilities = "{}";
  SetPaymentInstrument("test_key", std::move(write_details), &write_status);
  // Write the first instrument of a web payment app will return
  // FETCH_PAYMENT_APP_INFO_FAILED since the web app's manifest is not
  // available, but the write of the instrument is succeed, otherwise will
  // return the other errors.
  ASSERT_EQ(PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED, write_status);

  PaymentHandlerStatus has_status = PaymentHandlerStatus::NOT_FOUND;
  HasPaymentInstrument("test_key", &has_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, has_status);

  HasPaymentInstrument("unstored_test_key", &has_status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, has_status);
}

TEST_F(PaymentManagerTest, KeysOfPaymentInstruments) {
  PaymentHandlerStatus keys_status = PaymentHandlerStatus::NOT_FOUND;
  std::vector<std::string> keys;
  KeysOfPaymentInstruments(&keys, &keys_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, keys_status);
  ASSERT_EQ(0U, keys.size());

  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key1", PaymentInstrument::New(), &write_status);
    // Write the first instrument of a web payment app will return
    // FETCH_PAYMENT_APP_INFO_FAILED since the web app's manifest is not
    // available, but the write of the instrument is succeed, otherwise will
    // return the other errors.
    ASSERT_EQ(PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED,
              write_status);
  }
  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key3", PaymentInstrument::New(), &write_status);
    ASSERT_EQ(PaymentHandlerStatus::SUCCESS, write_status);
  }
  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key2", PaymentInstrument::New(), &write_status);
    ASSERT_EQ(PaymentHandlerStatus::SUCCESS, write_status);
  }

  keys_status = PaymentHandlerStatus::NOT_FOUND;
  KeysOfPaymentInstruments(&keys, &keys_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, keys_status);
  ASSERT_EQ(3U, keys.size());
  ASSERT_EQ("test_key1", keys[0]);
  ASSERT_EQ("test_key3", keys[1]);
  ASSERT_EQ("test_key2", keys[2]);
}

TEST_F(PaymentManagerTest, ClearPaymentInstruments) {
  PaymentHandlerStatus status = PaymentHandlerStatus::NOT_FOUND;
  std::vector<std::string> keys;
  KeysOfPaymentInstruments(&keys, &status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, status);
  ASSERT_EQ(0U, keys.size());

  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key1", PaymentInstrument::New(), &write_status);
    // Write the first instrument of a web payment app will return
    // FETCH_PAYMENT_APP_INFO_FAILED since the web app's manifest is not
    // available, but the write of the instrument is succeed, otherwise will
    // return the other errors.
    ASSERT_EQ(PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED,
              write_status);
  }
  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key3", PaymentInstrument::New(), &write_status);
    ASSERT_EQ(PaymentHandlerStatus::SUCCESS, write_status);
  }
  {
    PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
    SetPaymentInstrument("test_key2", PaymentInstrument::New(), &write_status);
    ASSERT_EQ(PaymentHandlerStatus::SUCCESS, write_status);
  }

  status = PaymentHandlerStatus::NOT_FOUND;
  KeysOfPaymentInstruments(&keys, &status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, status);
  ASSERT_EQ(3U, keys.size());

  status = PaymentHandlerStatus::NOT_FOUND;
  ClearPaymentInstruments(&status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, status);

  status = PaymentHandlerStatus::NOT_FOUND;
  KeysOfPaymentInstruments(&keys, &status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, status);
  ASSERT_EQ(0U, keys.size());
}

TEST_F(PaymentManagerTest, SetAndGetPaymentInstrument) {
  PaymentHandlerStatus write_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr write_details = PaymentInstrument::New();
  write_details->name = "ChromePay: chrome@chromepay.test";
  write_details->method = "https://www.chromium.org";
  write_details->stringified_capabilities = "{}";
  SetPaymentInstrument("test_key", std::move(write_details), &write_status);
  // Write the first instrument of a web payment app will return
  // FETCH_PAYMENT_APP_INFO_FAILED since the web app's manifest is not
  // available, but the write of the instrument is succeed, otherwise will
  // return the other errors.
  ASSERT_EQ(PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED, write_status);

  PaymentHandlerStatus read_status = PaymentHandlerStatus::NOT_FOUND;
  PaymentInstrumentPtr read_details;
  GetPaymentInstrument("test_key", &read_details, &read_status);
  ASSERT_EQ(PaymentHandlerStatus::SUCCESS, read_status);
  EXPECT_EQ("ChromePay: chrome@chromepay.test", read_details->name);
  EXPECT_EQ("https://www.chromium.org", read_details->method);
  EXPECT_EQ("", read_details->stringified_capabilities);
}

}  // namespace content
