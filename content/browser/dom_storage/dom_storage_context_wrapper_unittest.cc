// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace content {

constexpr const int kTestProcessIdOrigin1 = 11;
constexpr const int kTestProcessIdOrigin2 = 12;

class DOMStorageContextWrapperTest : public testing::Test {
 public:
  DOMStorageContextWrapperTest() = default;

  void SetUp() override {
    context_ = new DOMStorageContextWrapper(
        /*partition=*/nullptr, /*special_storage_policy=*/nullptr);

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Add(kTestProcessIdOrigin1, &browser_context_);
    security_policy->Add(kTestProcessIdOrigin2, &browser_context_);
    security_policy->AddIsolatedOrigins(
        {test_origin1_, test_origin2_},
        ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
    IsolationContext isolation_context(BrowsingInstanceId(1),
                                       &browser_context_);
    security_policy->LockProcessForTesting(
        isolation_context, kTestProcessIdOrigin1, test_origin1_.GetURL());
    security_policy->LockProcessForTesting(
        isolation_context, kTestProcessIdOrigin2, test_origin2_.GetURL());
  }

  void TearDown() override {
    context_->Shutdown();
    context_.reset();
    base::RunLoop().RunUntilIdle();

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Remove(kTestProcessIdOrigin1);
    security_policy->Remove(kTestProcessIdOrigin2);
  }

 protected:
  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  mojo::ReportBadMessageCallback MakeBadMessageCallback() {
    return base::BindOnce(&DOMStorageContextWrapperTest::OnBadMessage,
                          base::Unretained(this));
  }

  ChildProcessSecurityPolicyImpl::Handle CreateSecurityPolicyHandle(
      int process_id) {
    return ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
        process_id);
  }

  const std::string test_namespace_id_{base::GenerateGUID()};
  const url::Origin test_origin1_{
      url::Origin::Create(GURL("https://host1.com/"))};
  const url::Origin test_origin2_{
      url::Origin::Create(GURL("https://host2.com/"))};

  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  scoped_refptr<DOMStorageContextWrapper> context_;
  bool bad_message_called_ = false;
};

TEST_F(DOMStorageContextWrapperTest, ProcessLockedToOtherOrigin) {
  // Tries to open an area with a process that is locked to a different origin
  // and verifies the bad message callback.

  mojo::Remote<blink::mojom::StorageArea> area;
  context_->BindStorageArea(CreateSecurityPolicyHandle(kTestProcessIdOrigin1),
                            test_origin2_, test_namespace_id_,
                            MakeBadMessageCallback(),
                            area.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(bad_message_called_);
}

}  // namespace content
