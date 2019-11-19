// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/ios_payment_instrument_launcher.h"

#include <map>
#include <memory>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/web_payment_request.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#include "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payments {

namespace {

class FakePaymentInstrumentDelegate : public PaymentApp::Delegate {
 public:
  FakePaymentInstrumentDelegate() {}

  void OnInstrumentDetailsReady(const std::string& method_name,
                                const std::string& stringified_details,
                                const PayerData& payer_data) override {
    if (run_loop_)
      run_loop_->Quit();
    on_instrument_details_ready_called_ = true;
  }

  void OnInstrumentDetailsError(const std::string& error_message) override {
    if (run_loop_)
      run_loop_->Quit();
    on_instrument_details_error_called_ = true;
  }

  bool WasOnInstrumentDetailsReadyCalled() {
    return on_instrument_details_ready_called_;
  }

  bool WasOnInstrumentDetailsErrorCalled() {
    return on_instrument_details_error_called_;
  }

  void RunLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

 private:
  bool on_instrument_details_ready_called_ = false;
  bool on_instrument_details_error_called_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class PaymentRequestIOSPaymentInstrumentLauncherTest : public PlatformTest {
 protected:
  PaymentRequestIOSPaymentInstrumentLauncherTest()
      : chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {
    test_personal_data_manager_.SetAutofillProfileEnabled(true);
    test_personal_data_manager_.SetAutofillCreditCardEnabled(true);
    test_personal_data_manager_.SetAutofillWalletImportEnabled(true);
  }

  base::Value SerializeMethodDataWrapper(
      const std::map<std::string, std::set<std::string>>&
          stringified_method_data) {
    IOSPaymentInstrumentLauncher launcher;
    return launcher.SerializeMethodData(stringified_method_data);
  }

  void SetPaymentRequestID(IOSPaymentInstrumentLauncher& launcher) {
    launcher.payment_request_id_ = "some-payment-request-id";
  }

  base::test::TaskEnvironment task_environment_;

  autofill::TestPersonalDataManager test_personal_data_manager_;
  web::TestWebState web_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests that serializing empty stringified method data yields the expected
// result.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       EmptyStringifiedMethodDataDictionary) {
  WebPaymentRequest web_payment_request;
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  base::Value expected_dict(base::Value::Type::DICTIONARY);
  EXPECT_EQ(expected_dict, SerializeMethodDataWrapper(
                               payment_request.stringified_method_data()));
}

// Tests that serializing populated stringified method data yields the expected
// result.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       PopulatedStringifiedMethodDataDictionary) {
  WebPaymentRequest web_payment_request;
  PaymentMethodData method_datum1;
  method_datum1.supported_method = "https://jefpay.com";
  PaymentMethodData method_datum2;
  method_datum2.supported_method = "https://bobpay.com";
  std::unique_ptr<base::DictionaryValue> data =
      std::make_unique<base::DictionaryValue>();
  data->SetString("Some data", "Some stringified data");
  std::string stringified_data;
  base::JSONWriter::Write(*data, &stringified_data);
  method_datum1.data = stringified_data;
  method_datum2.data = stringified_data;
  web_payment_request.method_data.push_back(method_datum1);
  web_payment_request.method_data.push_back(method_datum2);
  PaymentMethodData method_datum3;
  method_datum3.supported_method = "https://alicepay.com";
  web_payment_request.method_data.push_back(method_datum3);
  PaymentMethodData method_datum4;
  method_datum4.supported_method = "https://jefpay.com";
  web_payment_request.method_data.push_back(method_datum4);
  PaymentMethodData method_datum5;
  method_datum5.supported_method = "https://alicepay.com";
  PaymentMethodData method_datum6;
  method_datum6.supported_method = "https://bobpay.com";
  web_payment_request.method_data.push_back(method_datum5);
  web_payment_request.method_data.push_back(method_datum6);

  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  // Jef data...
  base::Value jef_data(base::Value::Type::DICTIONARY);
  jef_data.SetKey("Some data", base::Value("Some stringified data"));
  std::string jef_stringified_data;
  base::JSONWriter::Write(jef_data, &jef_stringified_data);
  base::Value jef_data_list(base::Value::Type::LIST);
  jef_data_list.Append(jef_stringified_data);

  // Bob data...
  base::Value bob_data(base::Value::Type::DICTIONARY);
  bob_data.SetKey("Some data", base::Value("Some stringified data"));
  std::string bob_stringified_data;
  base::JSONWriter::Write(bob_data, &bob_stringified_data);
  base::Value bob_data_list(base::Value::Type::LIST);
  bob_data_list.Append(bob_stringified_data);

  // Alice data...
  base::Value alice_data_list(base::Value::Type::LIST);

  // Expected output.
  base::Value expected_dict(base::Value::Type::DICTIONARY);
  expected_dict.SetKey("https://jefpay.com", std::move(jef_data_list));
  expected_dict.SetKey("https://bobpay.com", std::move(bob_data_list));
  expected_dict.SetKey("https://alicepay.com", std::move(alice_data_list));

  EXPECT_EQ(expected_dict, SerializeMethodDataWrapper(
                               payment_request.stringified_method_data()));
}

// Tests that attempting to open an invalid universal link calls the
// OnInstrumentDetailsError() function of the delegate.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       LaunchIOSPaymentInstrument_MalformedUniversalLink) {
  std::unique_ptr<web::TestNavigationManager> navigation_manager =
      std::make_unique<web::TestNavigationManager>();
  web_state_.SetNavigationManager(std::move(navigation_manager));

  WebPaymentRequest web_payment_request =
      payment_request_test_util::CreateTestWebPaymentRequest();
  TestPaymentRequest payment_request(web_payment_request,
                                     chrome_browser_state_.get(), &web_state_,
                                     &test_personal_data_manager_);

  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  GURL malformed_link = GURL("http://bad-link.com");
  launcher.LaunchIOSPaymentInstrument(&payment_request, &web_state_,
                                      malformed_link, &instrument_delegate);
  instrument_delegate.RunLoop();

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

// Tests that if the response from the payment app is not a valid JSON
// dictionary then the OnInstrumentDetailsError() function of the delegate
// is called.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       ReceiveResponseFromIOSPaymentInstrument_ResponseNotDictionary) {
  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  std::string stringified_response = "\"JSON\"";
  std::string base_64_params;
  base::Base64Encode(stringified_response, &base_64_params);

  launcher.set_delegate(&instrument_delegate);
  launcher.ReceiveResponseFromIOSPaymentInstrument(base_64_params);

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

// Tests that if a payment app claims to have not been successful in
// fulfilling its side of the transaction then OnInstrumentDetailsError()
// is called.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       ReceiveResponseFromIOSPaymentInstrument_PaymentAppFailed) {
  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  base::DictionaryValue response;
  response.SetInteger("success", 0);
  response.SetString("methodName", "visa");
  response.SetString("details", "Some details");
  std::string stringified_response;
  base::JSONWriter::Write(response, &stringified_response);
  std::string base_64_params;
  base::Base64Encode(stringified_response, &base_64_params);

  launcher.set_delegate(&instrument_delegate);
  launcher.ReceiveResponseFromIOSPaymentInstrument(base_64_params);

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

// Tests that if the response from the payment app does not contain a
// method name then OnInstrumentDetailsError() of the delegate is
// called.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       ReceiveResponseFromIOSPaymentInstrument_NoMethodName) {
  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  base::DictionaryValue response;
  response.SetInteger("success", 1);
  response.SetString("details", "Some details");
  std::string stringified_response;
  base::JSONWriter::Write(response, &stringified_response);
  std::string base_64_params;
  base::Base64Encode(stringified_response, &base_64_params);

  launcher.set_delegate(&instrument_delegate);
  launcher.ReceiveResponseFromIOSPaymentInstrument(base_64_params);

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

// Tests that if the response from the payment app does not contain any
// stringified details then OnInstrumentDetailsError() of the delegate is
// called.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       ReceiveResponseFromIOSPaymentInstrument_NoDetails) {
  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  base::DictionaryValue response;
  response.SetInteger("success", 1);
  response.SetString("methodName", "visa");
  std::string stringified_response;
  base::JSONWriter::Write(response, &stringified_response);
  std::string base_64_params;
  base::Base64Encode(stringified_response, &base_64_params);

  launcher.set_delegate(&instrument_delegate);
  launcher.ReceiveResponseFromIOSPaymentInstrument(base_64_params);

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

// Tests that if the response from the payment app has all necessary
// parameters with valid values then OnInstrumentDetailsReady() of the
// delegate is called.
TEST_F(PaymentRequestIOSPaymentInstrumentLauncherTest,
       ReceiveResponseFromIOSPaymentInstrument_WellFormedResponse) {
  FakePaymentInstrumentDelegate instrument_delegate;
  IOSPaymentInstrumentLauncher launcher;

  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());

  base::DictionaryValue response;
  response.SetInteger("success", 1);
  response.SetString("methodName", "visa");
  response.SetString("details", "Some details");
  std::string stringified_response;
  base::JSONWriter::Write(response, &stringified_response);
  std::string base_64_params;
  base::Base64Encode(stringified_response, &base_64_params);

  launcher.set_delegate(&instrument_delegate);
  launcher.ReceiveResponseFromIOSPaymentInstrument(base_64_params);

  EXPECT_TRUE(instrument_delegate.WasOnInstrumentDetailsReadyCalled());
  EXPECT_FALSE(instrument_delegate.WasOnInstrumentDetailsErrorCalled());
}

}  // namespace payments
