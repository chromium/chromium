// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/identity_registry.h"

#include "content/browser/webid/test/mock_modal_dialog_view_delegate.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::NiceMock;

namespace content {

namespace {

constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kIdpUrl[] = "https://idp.example/";

}  // namespace

class TestFederatedIdentityModalDialogViewDelegate
    : public NiceMock<MockModalDialogViewDelegate> {
 public:
  bool closed_{false};

  void OnClose() override { closed_ = true; }

  base::WeakPtr<TestFederatedIdentityModalDialogViewDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestFederatedIdentityModalDialogViewDelegate>
      weak_ptr_factory_{this};
};

class IdentityRegistryTest : public RenderViewHostImplTestHarness {
 protected:
  IdentityRegistryTest() = default;
  ~IdentityRegistryTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    test_delegate_ =
        std::make_unique<TestFederatedIdentityModalDialogViewDelegate>();

    IdentityRegistry::CreateForWebContents(
        web_contents(), test_delegate_->GetWeakPtr(), GURL(kIdpUrl));
    identity_registry_ = IdentityRegistry::FromWebContents(web_contents());

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);
  }
  void TearDown() override {
    identity_registry_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestFederatedIdentityModalDialogViewDelegate> test_delegate_;
  raw_ptr<IdentityRegistry> identity_registry_;
};

// If notifier origin and registry origin are same-origin, modal dialog should
// be closed.
TEST_F(IdentityRegistryTest, NotifierAndRegistrySameOrigin) {
  EXPECT_FALSE(test_delegate_->closed_);
  identity_registry_->NotifyClose(url::Origin::Create(GURL(kIdpUrl)));
  EXPECT_TRUE(test_delegate_->closed_);
}

// If notifier origin and registry origin are cross-origin, modal dialog should
// remain open.
TEST_F(IdentityRegistryTest, NotifierAndRegistryCrossOrigin) {
  EXPECT_FALSE(test_delegate_->closed_);
  identity_registry_->NotifyClose(
      url::Origin::Create(GURL("https://cross-origin.example")));
  EXPECT_FALSE(test_delegate_->closed_);
}

}  // namespace content
