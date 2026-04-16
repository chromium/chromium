// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webid/federated_embedder_login_request.h"

#include "base/functional/callback_helpers.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::webid {

class FederatedEmbedderLoginRequestTest : public RenderViewHostTestHarness {
 public:
  FederatedEmbedderLoginRequestTest() = default;
  ~FederatedEmbedderLoginRequestTest() override = default;
};

TEST_F(FederatedEmbedderLoginRequestTest, OpenerCycle) {
  // Create another WebContents.
  std::unique_ptr<WebContents> opener = CreateTestWebContents();

  // Set up an opener cycle: web_contents() -> opener -> web_contents()
  WebContentsTester::For(web_contents())->SetOpener(opener.get());
  WebContentsTester::For(opener.get())->SetOpener(web_contents());

  // Get() should not hang and should return nullptr since no request is set.
  EXPECT_EQ(nullptr, FederatedEmbedderLoginRequest::Get(web_contents()));
}

TEST_F(FederatedEmbedderLoginRequestTest, OpenerSelfCycle) {
  // Set up an opener cycle: web_contents() -> web_contents()
  WebContentsTester::For(web_contents())->SetOpener(web_contents());

  // Get() should not hang and should return nullptr.
  EXPECT_EQ(nullptr, FederatedEmbedderLoginRequest::Get(web_contents()));
}

TEST_F(FederatedEmbedderLoginRequestTest, OpenerChain) {
  std::unique_ptr<WebContents> opener1 = CreateTestWebContents();
  std::unique_ptr<WebContents> opener2 = CreateTestWebContents();
  WebContentsTester::For(opener2.get())->SetOpener(opener1.get());
  WebContentsTester::For(web_contents())->SetOpener(opener2.get());

  url::Origin idp_origin = url::Origin::Create(GURL("https://idp.example"));
  std::string account_id = "account123";

  FederatedEmbedderLoginRequest::Set(opener1.get(), idp_origin, account_id,
                                     base::DoNothing());

  FederatedEmbedderLoginRequest* request =
      FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_NE(nullptr, request);
  EXPECT_EQ(idp_origin, request->idp_origin());
  EXPECT_EQ(account_id, request->account_id());
}

TEST_F(FederatedEmbedderLoginRequestTest, RequestInOpenerCycle) {
  // Create another WebContents.
  std::unique_ptr<WebContents> opener = CreateTestWebContents();

  // Set up an opener cycle: web_contents() -> opener -> web_contents()
  WebContentsTester::For(web_contents())->SetOpener(opener.get());
  WebContentsTester::For(opener.get())->SetOpener(web_contents());

  url::Origin idp_origin = url::Origin::Create(GURL("https://idp.example"));
  std::string account_id = "account123";

  // Set the request on the opener.
  FederatedEmbedderLoginRequest::Set(opener.get(), idp_origin, account_id,
                                     base::DoNothing());

  // Get() should find the request on the opener before detecting the cycle.
  FederatedEmbedderLoginRequest* request =
      FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_NE(nullptr, request);
  EXPECT_EQ(idp_origin, request->idp_origin());
  EXPECT_EQ(account_id, request->account_id());
}

}  // namespace content::webid
