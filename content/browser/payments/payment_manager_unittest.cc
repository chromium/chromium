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

const char kServiceWorkerScope[] = "https://example.test/a/";
const char kServiceWorkerScript[] = "https://example.test/a/script.js";
const char kServiceWorkerScope2[] = "https://example.test/b/";
const char kServiceWorkerScript2[] = "https://example.test/b/script.js";

void EnableDelegationsCallback(PaymentHandlerStatus* out_status,
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

  void EnableDelegations(PaymentHandlerStatus* out_status) {
    manager_->EnableDelegations(
        {}, base::BindOnce(&EnableDelegationsCallback, out_status));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // Owned by payment_app_context_.
  raw_ptr<PaymentManager> manager_;
};

TEST_F(PaymentManagerTest, UninitializedPaymentManager) {
  manager_ = CreateUninitializedPaymentManager(GURL(kServiceWorkerScope2),
                                               GURL(kServiceWorkerScript2));

  // Test that calling the payment manager does not crash, and instead
  // disconnects due to the invalid state (Init not called).
  PaymentHandlerStatus status = PaymentHandlerStatus::NOT_FOUND;
  EnableDelegations(&status);
  ASSERT_EQ(PaymentHandlerStatus::NOT_FOUND, status);
}

}  // namespace content
