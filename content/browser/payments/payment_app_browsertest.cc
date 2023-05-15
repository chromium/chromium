// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/installed_payment_apps_finder.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {
namespace {

using ::payments::mojom::CanMakePaymentEventData;
using ::payments::mojom::CanMakePaymentEventDataPtr;
using ::payments::mojom::CanMakePaymentResponsePtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentHandlerResponsePtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;

void OnPaymentAppInstall(base::OnceClosure done_callback,
                         bool* out_success,
                         bool success) {
  *out_success = success;
  std::move(done_callback).Run();
}

void GetAllPaymentAppsCallback(
    base::OnceClosure done_callback,
    InstalledPaymentAppsFinder::PaymentApps* out_apps,
    InstalledPaymentAppsFinder::PaymentApps apps) {
  *out_apps = std::move(apps);
  std::move(done_callback).Run();
}

void CaptureCanMakePaymentResult(base::OnceClosure done_callback,
                                 bool* out_payment_event_result,
                                 CanMakePaymentResponsePtr response) {
  *out_payment_event_result = response->can_make_payment;
  std::move(done_callback).Run();
}

void CaptureAbortResult(base::OnceClosure done_callback,
                        bool* out_payment_event_result,
                        bool payment_event_result) {
  *out_payment_event_result = payment_event_result;
  std::move(done_callback).Run();
}

void InvokePaymentAppCallback(base::OnceClosure done_callback,
                              PaymentHandlerResponsePtr* out_response,
                              PaymentHandlerResponsePtr response) {
  *out_response = std::move(response);
  std::move(done_callback).Run();
}

}  // namespace

class PaymentAppBrowserTest : public ContentBrowserTest {
 public:
  PaymentAppBrowserTest() {}

  PaymentAppBrowserTest(const PaymentAppBrowserTest&) = delete;
  PaymentAppBrowserTest& operator=(const PaymentAppBrowserTest&) = delete;

  ~PaymentAppBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL("/payments/payment_app_invocation.html")));
    ContentBrowserTest::SetUpOnMainThread();
  }

  std::string RunScript(const std::string& script) {
    return EvalJs(shell()->web_contents(), script).ExtractString();
  }

  std::string PopConsoleString() {
    return RunScript("resultQueue.pop().then(result => String(result))");
  }

  void RegisterPaymentApp() {
    SkBitmap app_icon;
    constexpr int kBitmapDimension = 16;
    app_icon.allocN32Pixels(kBitmapDimension, kBitmapDimension);
    app_icon.eraseColor(SK_ColorRED);
    GURL service_worker_javascript_file_url =
        https_server_->GetURL("/payments/payment_app.js");
    base::RunLoop run_loop;
    bool success = false;
    PaymentAppProvider::GetOrCreateForWebContents(shell()->web_contents())
        ->InstallPaymentAppForTesting(
            app_icon, service_worker_javascript_file_url,
            /*service_worker_scope=*/
            service_worker_javascript_file_url.GetWithoutFilename(),
            /*payment_method_identifier=*/"https://bobpay.com",
            base::BindOnce(&OnPaymentAppInstall, run_loop.QuitClosure(),
                           &success));
    run_loop.Run();
    ASSERT_TRUE(success);
  }

  url::Origin GetTestServerOrigin() {
    return url::Origin::Create(https_server_->GetURL("/"));
  }

  std::vector<int64_t> GetAllPaymentAppRegistrationIDs() {
    base::RunLoop run_loop;
    InstalledPaymentAppsFinder::PaymentApps apps;
    InstalledPaymentAppsFinder::GetInstance(
        shell()->web_contents()->GetBrowserContext())
        ->GetAllPaymentApps(base::BindOnce(&GetAllPaymentAppsCallback,
                                           run_loop.QuitClosure(), &apps));
    run_loop.Run();

    std::vector<int64_t> registrationIds;
    for (const auto& app_info : apps) {
      registrationIds.push_back(app_info.second->registration_id);
    }

    return registrationIds;
  }

  bool AbortPayment(int64_t registration_id,
                    const url::Origin& sw_origin,
                    const std::string& payment_request_id) {
    base::RunLoop run_loop;
    bool payment_aborted = false;
    PaymentAppProvider::GetOrCreateForWebContents(shell()->web_contents())
        ->AbortPayment(
            registration_id, sw_origin, payment_request_id,
            base::BindOnce(&CaptureAbortResult, run_loop.QuitClosure(),
                           &payment_aborted));
    run_loop.Run();

    return payment_aborted;
  }

  bool CanMakePaymentWithTestData(int64_t registration_id,
                                  const url::Origin& sw_origin,
                                  const std::string& payment_request_id,
                                  const std::string& supported_method) {
    CanMakePaymentEventDataPtr event_data =
        CreateCanMakePaymentEventData(supported_method);

    base::RunLoop run_loop;
    bool can_make_payment = false;
    PaymentAppProvider::GetOrCreateForWebContents(shell()->web_contents())
        ->CanMakePayment(
            registration_id, sw_origin, payment_request_id,
            std::move(event_data),
            base::BindOnce(&CaptureCanMakePaymentResult, run_loop.QuitClosure(),
                           &can_make_payment));
    run_loop.Run();

    return can_make_payment;
  }

  PaymentHandlerResponsePtr InvokePaymentAppWithTestData(
      int64_t registration_id,
      const url::Origin& sw_origin,
      const std::string& supported_method,
      const std::string& instrument_key) {
    base::RunLoop run_loop;
    PaymentHandlerResponsePtr response;
    PaymentAppProvider::GetOrCreateForWebContents(shell()->web_contents())
        ->InvokePaymentApp(
            registration_id, sw_origin,
            CreatePaymentRequestEventData(supported_method, instrument_key),
            base::BindOnce(&InvokePaymentAppCallback, run_loop.QuitClosure(),
                           &response));
    run_loop.Run();

    return response;
  }

  void ClearStoragePartitionData() {
    // Clear data from the storage partition. Parameters are set to clear data
    // for service workers, for all origins, for an unbounded time range.
    base::RunLoop run_loop;

    static_cast<StoragePartitionImpl*>(shell()
                                           ->web_contents()
                                           ->GetBrowserContext()
                                           ->GetDefaultStoragePartition())
        ->ClearData(StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS,
                    StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                    blink::StorageKey(), base::Time(), base::Time::Max(),
                    run_loop.QuitClosure());

    run_loop.Run();
  }

 private:
  CanMakePaymentEventDataPtr CreateCanMakePaymentEventData(
      const std::string& supported_method) {
    CanMakePaymentEventDataPtr event_data = CanMakePaymentEventData::New();

    event_data->top_origin = GURL("https://example.test");

    event_data->payment_request_origin = GURL("https://example.test");

    event_data->method_data.push_back(PaymentMethodData::New());
    event_data->method_data[0]->supported_method = supported_method;

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->total->amount->value = "55";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = supported_method;
    event_data->modifiers.push_back(std::move(modifier));

    return event_data;
  }

  PaymentRequestEventDataPtr CreatePaymentRequestEventData(
      const std::string& supported_method,
      const std::string& instrument_key) {
    PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

    event_data->top_origin = GURL("https://example.test");

    event_data->payment_request_origin = GURL("https://example.test");

    event_data->payment_request_id = "payment-request-id";

    event_data->method_data.push_back(PaymentMethodData::New());
    event_data->method_data[0]->supported_method = supported_method;

    event_data->total = PaymentCurrencyAmount::New();
    event_data->total->currency = "USD";
    event_data->total->value = "55";

    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();
    modifier->total = PaymentItem::New();
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = "USD";
    modifier->total->amount->value = "55";
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = supported_method;
    event_data->modifiers.push_back(std::move(modifier));

    event_data->instrument_key = instrument_key;

    return event_data;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest,
                       AbortPaymentWithInvalidRegistrationId) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool payment_aborted =
      AbortPayment(blink::mojom::kInvalidServiceWorkerRegistrationId,
                   GetTestServerOrigin(), "id");
  ASSERT_FALSE(payment_aborted);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, AbortPayment) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool payment_aborted =
      AbortPayment(registrationIds[0], GetTestServerOrigin(), "id");
  ASSERT_TRUE(payment_aborted);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, CanMakePayment) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  bool can_make_payment = CanMakePaymentWithTestData(
      registrationIds[0], GetTestServerOrigin(), "id", "basic-card");
  ASSERT_TRUE(can_make_payment);

  EXPECT_EQ("undefined", PopConsoleString() /* topOrigin */);
  EXPECT_EQ("undefined", PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("undefined", PopConsoleString() /* methodData */);
  EXPECT_EQ("undefined", PopConsoleString() /* modifiers */);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocationAndFailed) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  // Remove all payment apps and service workers to cause error.
  ClearStoragePartitionData();

  PaymentHandlerResponsePtr response(
      InvokePaymentAppWithTestData(registrationIds[0], GetTestServerOrigin(),
                                   "basic-card", "basic-card-payment-app-id"));
  ASSERT_EQ("", response->method_name);

  ClearStoragePartitionData();
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppInvocation) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  PaymentHandlerResponsePtr response(
      InvokePaymentAppWithTestData(registrationIds[0], GetTestServerOrigin(),
                                   "basic-card", "basic-card-payment-app-id"));
  ASSERT_EQ("test", response->method_name);

  EXPECT_EQ("https://example.test/", PopConsoleString() /* topOrigin */);
  EXPECT_EQ("https://example.test/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":\"basic-card\"}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ(
      "{\"currency\":\"USD\","
      "\"value\":\"55\"}",
      PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":\"basic-card\","
      "\"total\":{\"amount\":{\"currency\":\"USD\","
      "\"value\":\"55\"},\"label\":\"\",\"pending\":false}}"
      "]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("basic-card-payment-app-id",
            PopConsoleString() /* instrumentKey */);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());
}

IN_PROC_BROWSER_TEST_F(PaymentAppBrowserTest, PaymentAppOpenWindowFailed) {
  RegisterPaymentApp();

  std::vector<int64_t> registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(1U, registrationIds.size());

  PaymentHandlerResponsePtr response(InvokePaymentAppWithTestData(
      registrationIds[0], GetTestServerOrigin(), "https://bobpay.test",
      "bobpay-payment-app-id"));
  // InvokePaymentAppCallback returns empty method_name in case of failure, like
  // in PaymentRequestRespondWithObserver::OnResponseRejected.
  ASSERT_EQ("", response->method_name);

  EXPECT_EQ("https://example.test/", PopConsoleString() /* topOrigin */);
  EXPECT_EQ("https://example.test/",
            PopConsoleString() /* paymentRequestOrigin */);
  EXPECT_EQ("payment-request-id", PopConsoleString() /* paymentRequestId */);
  EXPECT_EQ("[{\"supportedMethods\":\"https://bobpay.test\"}]",
            PopConsoleString() /* methodData */);
  EXPECT_EQ("{\"currency\":\"USD\",\"value\":\"55\"}",
            PopConsoleString() /* total */);
  EXPECT_EQ(
      "[{\"additionalDisplayItems\":[],\"supportedMethods\":\"https://"
      "bobpay.test\","
      "\"total\":{\"amount\":{\"currency\":\"USD\","
      "\"value\":\"55\"},\"label\":\"\",\"pending\":false}}"
      "]",
      PopConsoleString() /* modifiers */);
  EXPECT_EQ("bobpay-payment-app-id", PopConsoleString() /* instrumentKey */);

  ClearStoragePartitionData();

  registrationIds = GetAllPaymentAppRegistrationIDs();
  ASSERT_EQ(0U, registrationIds.size());
}
}  // namespace content
