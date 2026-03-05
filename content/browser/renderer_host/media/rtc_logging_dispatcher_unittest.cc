// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/rtc_logging_dispatcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/rtc_logging/rtc_logging.mojom.h"

namespace content {

namespace {

using ::testing::_;
using ::testing::Ref;

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MOCK_METHOD(void,
              StartRtcDiagnosticLogging,
              (RenderFrameHost&,
               bool,
               (base::flat_map<std::string, std::string>),
               base::OnceCallback<void(const std::string&)>),
              (override));
  MOCK_METHOD(void,
              FinishRtcDiagnosticLogging,
              (RenderFrameHost&, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              CancelRtcDiagnosticLogging,
              (RenderFrameHost&, base::OnceClosure),
              (override));
};

class RTCLoggingDispatcherImplTest : public RenderViewHostTestHarness {
 public:
  RTCLoggingDispatcherImplTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kRTCDiagnosticLogging);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    old_client_ = SetBrowserClientForTesting(&mock_client_);
    RTCLoggingDispatcherImpl::Create(main_rfh(),
                                     remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockContentBrowserClient mock_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  mojo::Remote<blink::mojom::RTCLoggingDispatcher> remote_;
};

TEST_F(RTCLoggingDispatcherImplTest, StartDiagnosticLoggingForwardsToClient) {
  base::flat_map<std::string, std::string> metadata = {{"key", "value"}};
  const std::string kUuid = "test-uuid";

  EXPECT_CALL(mock_client_,
              StartRtcDiagnosticLogging(Ref(*main_rfh()), true, metadata, _))
      .WillOnce([&](RenderFrameHost&, bool,
                    base::flat_map<std::string, std::string>,
                    base::OnceCallback<void(const std::string&)> cb) {
        std::move(cb).Run(kUuid);
      });

  base::test::TestFuture<const std::string&> future;
  remote_->StartDiagnosticLogging(true, metadata, future.GetCallback());
  EXPECT_EQ(future.Get(), kUuid);
}

TEST_F(RTCLoggingDispatcherImplTest, FinishDiagnosticLoggingForwardsToClient) {
  EXPECT_CALL(mock_client_, FinishRtcDiagnosticLogging(Ref(*main_rfh()), _))
      .WillOnce(
          [](RenderFrameHost&, base::OnceClosure cb) { std::move(cb).Run(); });

  base::test::TestFuture<void> future;
  remote_->FinishDiagnosticLogging(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(RTCLoggingDispatcherImplTest, CancelDiagnosticLoggingForwardsToClient) {
  EXPECT_CALL(mock_client_, CancelRtcDiagnosticLogging(Ref(*main_rfh()), _))
      .WillOnce(
          [](RenderFrameHost&, base::OnceClosure cb) { std::move(cb).Run(); });

  base::test::TestFuture<void> future;
  remote_->CancelDiagnosticLogging(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(RTCLoggingDispatcherImplTest, FeatureDisabledStart) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kRTCDiagnosticLogging);

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->StartDiagnosticLogging(true, {}, base::DoNothing());
  EXPECT_EQ("RTCDiagnosticLogging feature not enabled",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RTCLoggingDispatcherImplTest, FeatureDisabledFinish) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kRTCDiagnosticLogging);

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->FinishDiagnosticLogging(base::DoNothing());
  EXPECT_EQ("RTCDiagnosticLogging feature not enabled",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RTCLoggingDispatcherImplTest, FeatureDisabledCancel) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(blink::features::kRTCDiagnosticLogging);

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->CancelDiagnosticLogging(base::DoNothing());
  EXPECT_EQ("RTCDiagnosticLogging feature not enabled",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RTCLoggingDispatcherImplTest, TooManyMetadata) {
  base::flat_map<std::string, std::string> metadata;
  for (size_t i = 0; i < RTCLoggingDispatcherImpl::kMaxMetadataSize + 1; ++i) {
    metadata["key" + base::NumberToString(i)] = "value";
  }

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->StartDiagnosticLogging(/*upload=*/true, metadata, base::DoNothing());
  EXPECT_EQ("Too many metadata entries",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RTCLoggingDispatcherImplTest, TooLongMetadataValue) {
  base::flat_map<std::string, std::string> metadata;
  metadata["key"] =
      std::string(RTCLoggingDispatcherImpl::kMaxMetadataLength + 1, 'a');

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->StartDiagnosticLogging(/*upload=*/true, metadata, base::DoNothing());
  EXPECT_EQ("Metadata key or value too long",
            bad_message_observer.WaitForBadMessage());
}

TEST_F(RTCLoggingDispatcherImplTest, TooLongMetadataKey) {
  base::flat_map<std::string, std::string> metadata;
  metadata[std::string(RTCLoggingDispatcherImpl::kMaxMetadataLength + 1, 'a')] =
      "value";

  mojo::test::BadMessageObserver bad_message_observer;
  remote_->StartDiagnosticLogging(/*upload=*/true, metadata, base::DoNothing());
  EXPECT_EQ("Metadata key or value too long",
            bad_message_observer.WaitForBadMessage());
}

class RTCLoggingDispatcherImplDefaultContentClientTest
    : public RenderViewHostTestHarness {
 public:
  RTCLoggingDispatcherImplDefaultContentClientTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kRTCDiagnosticLogging);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    RTCLoggingDispatcherImpl::Create(main_rfh(),
                                     remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<blink::mojom::RTCLoggingDispatcher> remote_;
};

TEST_F(RTCLoggingDispatcherImplDefaultContentClientTest,
       StartDiagnosticLogging) {
  base::test::TestFuture<const std::string&> future;
  remote_->StartDiagnosticLogging(/*upload=*/true, {}, future.GetCallback());
  EXPECT_TRUE(base::Uuid::ParseLowercase(future.Get()).is_valid());
}

TEST_F(RTCLoggingDispatcherImplDefaultContentClientTest,
       FinishDiagnosticLogging) {
  base::test::TestFuture<void> future;
  remote_->FinishDiagnosticLogging(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

TEST_F(RTCLoggingDispatcherImplDefaultContentClientTest,
       CancelDiagnosticLogging) {
  base::test::TestFuture<void> future;
  remote_->CancelDiagnosticLogging(future.GetCallback());
  EXPECT_TRUE(future.Wait());
}

}  // namespace

}  // namespace content
