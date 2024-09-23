// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/user_consent_handler.h"

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using url::Origin;

namespace content {

namespace {

const char kTestUrl[] = "https://testing.test";

class PromptBasedUserConsentHandlerTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    WebContentsImpl* web_contents_impl =
        static_cast<WebContentsImpl*>(web_contents());
    web_contents_impl->SetDelegate(&delegate_);
  }

  using OriginList = std::vector<url::Origin>;
  void ExpectCreateSmsPrompt(RenderFrameHost* rfh,
                             const OriginList& origin_list,
                             const std::string& one_time_code) {
    EXPECT_CALL(delegate_,
                CreateSmsPrompt(rfh, origin_list, one_time_code, _, _))
        .WillOnce(
            Invoke([=, this](RenderFrameHost*, const OriginList& origin_list,
                             const std::string&, base::OnceClosure on_confirm,
                             base::OnceClosure on_cancel) {
              confirm_callback_ = std::move(on_confirm);
              dismiss_callback_ = std::move(on_cancel);
            }));
  }

  void ExpectNoSmsPrompt() {
    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _)).Times(0);
  }

  void ConfirmPrompt() {
    if (!confirm_callback_.is_null()) {
      std::move(confirm_callback_).Run();
      dismiss_callback_.Reset();
      return;
    }
    FAIL() << "SmsInfobar not available";
  }

  void DismissPrompt() {
    if (dismiss_callback_.is_null()) {
      FAIL() << "SmsInfobar not available";
    }
    std::move(dismiss_callback_).Run();
    confirm_callback_.Reset();
  }

 private:
  NiceMock<MockSmsWebContentsDelegate> delegate_;
  base::OnceClosure confirm_callback_;
  base::OnceClosure dismiss_callback_;
};

TEST_F(PromptBasedUserConsentHandlerTest, PromptsUser) {
  NavigateAndCommit(GURL(kTestUrl));

  const url::Origin& origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  base::RunLoop loop;

  ExpectCreateSmsPrompt(main_rfh(), OriginList{origin}, "12345");
  CompletionCallback callback;
  PromptBasedUserConsentHandler consent_handler{*main_rfh(),
                                                OriginList{origin}};
  consent_handler.RequestUserConsent("12345", std::move(callback));
}

TEST_F(PromptBasedUserConsentHandlerTest, ConfirmInvokedCallback) {
  NavigateAndCommit(GURL(kTestUrl));

  const url::Origin& origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  ExpectCreateSmsPrompt(main_rfh(), OriginList{origin}, "12345");
  PromptBasedUserConsentHandler consent_handler{*main_rfh(),
                                                OriginList{origin}};
  EXPECT_FALSE(consent_handler.is_active());
  bool succeed;
  auto callback = base::BindLambdaForTesting([&](UserConsentResult result) {
    succeed = (result == UserConsentResult::kApproved);
  });
  consent_handler.RequestUserConsent("12345", std::move(callback));
  EXPECT_TRUE(consent_handler.is_active());
  ConfirmPrompt();
  EXPECT_FALSE(consent_handler.is_active());
  EXPECT_TRUE(succeed);
}

TEST_F(PromptBasedUserConsentHandlerTest, CancelingInvokedCallback) {
  NavigateAndCommit(GURL(kTestUrl));

  const url::Origin& origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  ExpectCreateSmsPrompt(main_rfh(), OriginList{origin}, "12345");
  PromptBasedUserConsentHandler consent_handler{*main_rfh(),
                                                OriginList{origin}};
  EXPECT_FALSE(consent_handler.is_active());
  bool cancelled;
  auto callback = base::BindLambdaForTesting([&](UserConsentResult result) {
    cancelled = (result == UserConsentResult::kDenied);
  });
  consent_handler.RequestUserConsent("12345", std::move(callback));
  EXPECT_TRUE(consent_handler.is_active());
  DismissPrompt();
  EXPECT_FALSE(consent_handler.is_active());
  EXPECT_TRUE(cancelled);
}

TEST_F(PromptBasedUserConsentHandlerTest, CancelsWhenNoDelegate) {
  NavigateAndCommit(GURL(kTestUrl));

  const url::Origin& origin =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(nullptr);

  ExpectNoSmsPrompt();

  PromptBasedUserConsentHandler consent_handler{*main_rfh(),
                                                OriginList{origin}};
  bool cancelled;
  auto callback = base::BindLambdaForTesting([&](UserConsentResult result) {
    cancelled = (result == UserConsentResult::kNoDelegate);
  });
  consent_handler.RequestUserConsent("12345", std::move(callback));
  EXPECT_TRUE(cancelled);
}

class PromptBasedUserConsentHandlerAlwaysAllowedTest
    : public PromptBasedUserConsentHandlerTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        GetBasicBackForwardCacheFeatureForTesting(),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
    PromptBasedUserConsentHandlerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PromptBasedUserConsentHandlerAlwaysAllowedTest, CancelsWhenInactiveRFH) {
  NavigateAndCommit(GURL(kTestUrl));
  RenderFrameHost& old_main_frame_host = *main_rfh();
  const url::Origin& origin = old_main_frame_host.GetLastCommittedOrigin();

  ExpectNoSmsPrompt();

  NavigateAndCommit(GURL("https://testing.test2"));

  PromptBasedUserConsentHandler consent_handler{old_main_frame_host,
                                                OriginList{origin}};
  bool cancelled;
  auto callback = base::BindLambdaForTesting([&](UserConsentResult result) {
    cancelled = (result == UserConsentResult::kInactiveRenderFrameHost);
  });
  consent_handler.RequestUserConsent("12345", std::move(callback));
  EXPECT_TRUE(cancelled);
}

}  // namespace
}  // namespace content
