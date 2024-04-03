// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/uuid.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/origin.h"

namespace content {

constexpr const int kTestProcessIdOrigin1 = 11;
constexpr const int kTestProcessIdOrigin2 = 12;

class DOMStorageContextWrapperTest : public testing::Test {
 public:
  DOMStorageContextWrapperTest() = default;

  void SetUp() override {
    context_ = base::MakeRefCounted<DOMStorageContextWrapper>(
        /*partition=*/nullptr);

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Add(kTestProcessIdOrigin1, &browser_context_);
    security_policy->Add(kTestProcessIdOrigin2, &browser_context_);
    security_policy->AddFutureIsolatedOrigins(
        {test_storage_key1_.origin(), test_storage_key2_.origin()},
        ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
    IsolationContext isolation_context(
        BrowsingInstanceId(1), &browser_context_,
        /*is_guest=*/false, /*is_fenced=*/false,
        OriginAgentClusterIsolationState::CreateForDefaultIsolation(
            &browser_context_));
    security_policy->LockProcessForTesting(
        isolation_context, kTestProcessIdOrigin1,
        test_storage_key1_.origin().GetURL());
    security_policy->LockProcessForTesting(
        isolation_context, kTestProcessIdOrigin2,
        test_storage_key2_.origin().GetURL());
  }

  void TearDown() override {
    context_->Shutdown();
    context_.reset();
    base::RunLoop().RunUntilIdle();

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Remove(kTestProcessIdOrigin1);
    security_policy->Remove(kTestProcessIdOrigin2);
    security_policy->ClearIsolatedOriginsForTesting();
  }

 protected:
  void OnBadMessage(std::string_view reason) {
    bad_message_called_ = true;
    bad_message_ = std::string(reason);
  }

  mojo::ReportBadMessageCallback MakeBadMessageCallback() {
    return base::BindOnce(&DOMStorageContextWrapperTest::OnBadMessage,
                          base::Unretained(this));
  }

  ChildProcessSecurityPolicyImpl::Handle CreateSecurityPolicyHandle(
      int process_id) {
    return ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
        process_id);
  }

  const std::string test_namespace_id_{
      base::Uuid::GenerateRandomV4().AsLowercaseString()};
  const blink::StorageKey test_storage_key1_{
      blink::StorageKey::CreateFromStringForTesting("https://host1.com/")};
  const blink::StorageKey test_storage_key2_{
      blink::StorageKey::CreateFromStringForTesting("https://host2.com/")};

  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
  scoped_refptr<DOMStorageContextWrapper> context_;
  bool bad_message_called_ = false;
  std::string bad_message_;
};

// Tries to open a local storage area with a process that is locked to a
// different StorageKey and verifies the bad message callback.
TEST_F(DOMStorageContextWrapperTest,
       OpenLocalStorageProcessLockedToOtherStorageKey) {
  mojo::Remote<blink::mojom::StorageArea> area;
  context_->OpenLocalStorage(test_storage_key2_, std::nullopt,
                             area.BindNewPipeAndPassReceiver(),
                             CreateSecurityPolicyHandle(kTestProcessIdOrigin1),
                             MakeBadMessageCallback());
  EXPECT_TRUE(bad_message_called_);
  EXPECT_EQ(bad_message_,
            "Access denied for localStorage request due to "
            "ChildProcessSecurityPolicy.");
}

// Tries to open a local storage area with a process that is locked to a
// different LocalFrameToken and verifies there isn't a bad message callback.
TEST_F(DOMStorageContextWrapperTest,
       OpenLocalStorageProcessLockedToOtherLocalFrameToken) {
  mojo::Remote<blink::mojom::StorageArea> area;
  context_->OpenLocalStorage(test_storage_key2_, blink::LocalFrameToken(),
                             area.BindNewPipeAndPassReceiver(),
                             CreateSecurityPolicyHandle(kTestProcessIdOrigin1),
                             MakeBadMessageCallback());
  EXPECT_FALSE(bad_message_called_);
}

// Tries to open a session storage area with a process that is locked to a
// different StorageKey and verifies the bad message callback.
TEST_F(DOMStorageContextWrapperTest,
       BindStorageAreaProcessLockedToOtherStorageKey) {
  mojo::Remote<blink::mojom::StorageArea> area;
  context_->BindStorageArea(test_storage_key2_, std::nullopt,
                            test_namespace_id_,
                            area.BindNewPipeAndPassReceiver(),
                            CreateSecurityPolicyHandle(kTestProcessIdOrigin1),
                            MakeBadMessageCallback());
  EXPECT_TRUE(bad_message_called_);
  EXPECT_EQ(bad_message_,
            "Access denied for sessionStorage request due to "
            "ChildProcessSecurityPolicy.");
}

// Tries to open a session storage area with a process that is locked to a
// different LocalFrameToken and verifies there isn't a bad message callback.
TEST_F(DOMStorageContextWrapperTest,
       BindStorageAreaProcessLockedToOtherLocalFrameToken) {
  mojo::Remote<blink::mojom::StorageArea> area;
  context_->BindStorageArea(test_storage_key2_, blink::LocalFrameToken(),
                            test_namespace_id_,
                            area.BindNewPipeAndPassReceiver(),
                            CreateSecurityPolicyHandle(kTestProcessIdOrigin1),
                            MakeBadMessageCallback());
  EXPECT_FALSE(bad_message_called_);
}

}  // namespace content
