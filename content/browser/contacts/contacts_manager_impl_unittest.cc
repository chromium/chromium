// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/contacts/contacts_manager_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/contacts/contacts_manager.mojom.h"

namespace content {

namespace {

class MockContactsProvider : public ContactsProvider {
 public:
  MockContactsProvider() = default;
  ~MockContactsProvider() override = default;

  MOCK_METHOD(void,
              Select,
              (bool multiple,
               bool include_names,
               bool include_emails,
               bool include_tel,
               bool include_addresses,
               bool include_icons,
               ContactsProvider::ContactsSelectedCallback callback),
              (override));
};

}  // namespace

class ContactsManagerImplTest : public RenderViewHostImplTestHarness {
 public:
  ContactsManagerImplTest() = default;
  ~ContactsManagerImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    RenderFrameHostTester::For(main_rfh())->InitializeRenderFrameIfNeeded();
    NavigateAndCommit(GURL("https://example.com"));
  }

  void TearDown() override {
    contacts_manager_impl_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  void InitService(std::unique_ptr<ContactsProvider> provider) {
    contacts_manager_impl_ = ContactsManagerImpl::CreateForTesting(
        main_rfh(), contacts_manager_remote_.BindNewPipeAndPassReceiver(),
        std::move(provider));
  }

  mojo::Remote<blink::mojom::ContactsManager>& remote() {
    return contacts_manager_remote_;
  }

 private:
  mojo::Remote<blink::mojom::ContactsManager> contacts_manager_remote_;
  raw_ptr<ContactsManagerImpl> contacts_manager_impl_ = nullptr;
};

TEST_F(ContactsManagerImplTest, SelectActivePrimaryMainFrame) {
  auto mock_provider = std::make_unique<MockContactsProvider>();
  MockContactsProvider* mock_provider_ptr = mock_provider.get();

  // Set up expectation on mock provider.
  std::vector<blink::mojom::ContactInfoPtr> expected_contacts;
  auto contact = blink::mojom::ContactInfo::New();
  contact->name = std::vector<std::string>{"John Doe"};
  expected_contacts.push_back(std::move(contact));

  EXPECT_CALL(*mock_provider_ptr, Select)
      .WillOnce(base::test::RunOnceCallback<6>(std::move(expected_contacts),
                                               /*percentage_shared=*/100,
                                               ContactsPickerProperties()));

  InitService(std::move(mock_provider));

  base::test::TestFuture<
      std::optional<std::vector<blink::mojom::ContactInfoPtr>>>
      future;
  remote()->Select(/*multiple=*/false, /*include_names=*/true,
                   /*include_emails=*/false, /*include_tel=*/false,
                   /*include_addresses=*/false, /*include_icons=*/false,
                   future.GetCallback());

  ASSERT_TRUE(future.Get().has_value());
  EXPECT_EQ(future.Get()->size(), 1u);
  EXPECT_EQ(future.Get()->at(0)->name->at(0), "John Doe");
}

TEST_F(ContactsManagerImplTest, SelectBFCachedFrame) {
  auto mock_provider = std::make_unique<MockContactsProvider>();
  MockContactsProvider* mock_provider_ptr = mock_provider.get();

  EXPECT_CALL(*mock_provider_ptr, Select).Times(0);

  InitService(std::move(mock_provider));

  // Put the frame into BFCache.
  static_cast<TestRenderFrameHost*>(main_rfh())->DidEnterBackForwardCache();
  EXPECT_TRUE(main_rfh()->IsInLifecycleState(
      RenderFrameHost::LifecycleState::kInBackForwardCache));

  base::test::TestFuture<
      std::optional<std::vector<blink::mojom::ContactInfoPtr>>>
      future;
  // Call Select. It should return nullopt safely.
  remote()->Select(/*multiple=*/false, /*include_names=*/true,
                   /*include_emails=*/false, /*include_tel=*/false,
                   /*include_addresses=*/false, /*include_icons=*/false,
                   future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

TEST_F(ContactsManagerImplTest, SelectSubframe) {
  // Create a subframe.
  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  ASSERT_TRUE(subframe);
  RenderFrameHostTester::For(subframe)->InitializeRenderFrameIfNeeded();

  // Navigate the subframe.
  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com/subframe"), subframe);
  navigation->Commit();
  subframe = navigation->GetFinalRenderFrameHost();

  mojo::Remote<blink::mojom::ContactsManager> subframe_remote;
  auto subframe_mock_provider = std::make_unique<MockContactsProvider>();
  MockContactsProvider* subframe_mock_provider_ptr =
      subframe_mock_provider.get();

  EXPECT_CALL(*subframe_mock_provider_ptr, Select).Times(0);

  ContactsManagerImpl::CreateForTesting(
      subframe, subframe_remote.BindNewPipeAndPassReceiver(),
      std::move(subframe_mock_provider));

  base::test::TestFuture<
      std::optional<std::vector<blink::mojom::ContactInfoPtr>>>
      future;
  // Call Select on subframe. It should return nullopt safely because it is not
  // a main frame.
  subframe_remote->Select(/*multiple=*/false, /*include_names=*/true,
                          /*include_emails=*/false, /*include_tel=*/false,
                          /*include_addresses=*/false, /*include_icons=*/false,
                          future.GetCallback());

  EXPECT_EQ(future.Get(), std::nullopt);
}

}  // namespace content
