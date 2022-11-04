// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_

#include <memory>
#include <vector>

#include "content/browser/payments/payment_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "url/gurl.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class BrowserContext;
class PaymentAppContextImpl;
class StoragePartitionImpl;
class BrowserTaskEnvironment;

class PaymentAppContentUnitTestBase : public testing::Test {
 public:
  PaymentAppContentUnitTestBase(const PaymentAppContentUnitTestBase&) = delete;
  PaymentAppContentUnitTestBase& operator=(
      const PaymentAppContentUnitTestBase&) = delete;

 protected:
  PaymentAppContentUnitTestBase();
  ~PaymentAppContentUnitTestBase() override;

  BrowserContext* browser_context();
  PaymentManager* CreatePaymentManager(const GURL& scope_url,
                                       const GURL& sw_script_url);
  void UnregisterServiceWorker(const GURL& scope_url,
                               const blink::StorageKey& key);

  void ResetPaymentAppInvoked() const;
  int64_t last_sw_registration_id() const;
  const GURL& last_sw_scope_url() const;

  void SetNoPaymentRequestResponseImmediately();
  void RespondPendingPaymentRequest();

 private:
  class PaymentAppForWorkerTestHelper;

  StoragePartitionImpl* storage_partition();
  PaymentAppContextImpl* payment_app_context();

  std::unique_ptr<BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<PaymentAppForWorkerTestHelper> worker_helper_;
  std::vector<mojo::Remote<payments::mojom::PaymentManager>> payment_managers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_CONTENT_UNITTEST_BASE_H_
