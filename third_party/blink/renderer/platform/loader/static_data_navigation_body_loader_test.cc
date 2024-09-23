// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class StaticDataNavigationBodyLoaderTest
    : public ::testing::Test,
      public WebNavigationBodyLoader::Client {
 protected:
  void SetUp() override {
    loader_ = std::make_unique<StaticDataNavigationBodyLoader>();
  }

  void Write(const String& buffer) {
    std::string string = buffer.Utf8();
    loader_->Write(string);
  }

  void BodyDataReceived(base::span<const char> data) override {
    ASSERT_TRUE(expecting_data_received_);
    expecting_data_received_ = false;
    data_received_ =
        data_received_ + String::FromUTF8(data.data(), data.size());
    TakeActions();
  }

  void BodyLoadingFinished(
      base::TimeTicks completion_time,
      int64_t total_encoded_data_length,
      int64_t total_encoded_body_length,
      int64_t total_decoded_body_length,
      const std::optional<blink::WebURLError>& error) override {
    ASSERT_TRUE(expecting_finished_);
    expecting_finished_ = false;
    ASSERT_TRUE(!did_finish_);
    did_finish_ = true;
    TakeActions();
  }

  void TakeActions() {
    if (freeze_mode_ != LoaderFreezeMode::kNone) {
      freeze_mode_ = LoaderFreezeMode::kNone;
      loader_->SetDefersLoading(LoaderFreezeMode::kStrict);
    }
    if (!buffer_to_write_.empty()) {
      String buffer = buffer_to_write_;
      buffer_to_write_ = String();
      expecting_data_received_ = true;
      Write(buffer);
    }
    if (destroy_loader_) {
      destroy_loader_ = false;
      loader_.reset();
    }
  }

  String TakeDataReceived() {
    String data = data_received_;
    data_received_ = g_empty_string;
    return data;
  }

  std::unique_ptr<StaticDataNavigationBodyLoader> loader_;
  bool expecting_data_received_ = false;
  bool expecting_finished_ = false;
  bool did_finish_ = false;
  String buffer_to_write_;
  LoaderFreezeMode freeze_mode_ = LoaderFreezeMode::kNone;
  bool destroy_loader_ = false;
  String data_received_;
};

TEST_F(StaticDataNavigationBodyLoaderTest, DataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, WriteFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  buffer_to_write_ = "world";
  Write("hello");
  EXPECT_EQ("helloworld", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingAndWriteFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kStrict;
  buffer_to_write_ = "world";
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("world", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingWithBfcacheAndWriteFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kBufferIncoming;
  buffer_to_write_ = "world";
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("world", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, DestroyFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  destroy_loader_ = false;
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, SetDefersLoadingFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kStrict;
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
  Write("world");
  EXPECT_EQ("", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingWithBfcacheFromDataReceived) {
  loader_->StartLoadingBody(this);
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kBufferIncoming;
  Write("hello");
  EXPECT_EQ("hello", TakeDataReceived());
  Write("world");
  EXPECT_EQ("", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, WriteThenStart) {
  Write("hello");
  expecting_data_received_ = true;
  loader_->StartLoadingBody(this);
  EXPECT_EQ("hello", TakeDataReceived());
  expecting_finished_ = true;
  loader_->Finish();
  EXPECT_EQ("", TakeDataReceived());
  EXPECT_TRUE(did_finish_);
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingFromFinishedDataReceived) {
  Write("hello");
  loader_->Finish();
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kStrict;
  loader_->StartLoadingBody(this);
  EXPECT_EQ("hello", TakeDataReceived());
  expecting_finished_ = true;
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("", TakeDataReceived());
  EXPECT_TRUE(did_finish_);
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingWithBfcacheFromFinishedDataReceived) {
  Write("hello");
  loader_->Finish();
  expecting_data_received_ = true;
  freeze_mode_ = LoaderFreezeMode::kBufferIncoming;
  loader_->StartLoadingBody(this);
  EXPECT_EQ("hello", TakeDataReceived());
  expecting_finished_ = true;
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("", TakeDataReceived());
  EXPECT_TRUE(did_finish_);
}

TEST_F(StaticDataNavigationBodyLoaderTest, StartDeferred) {
  loader_->SetDefersLoading(LoaderFreezeMode::kStrict);
  loader_->StartLoadingBody(this);
  Write("hello");
  expecting_data_received_ = true;
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, StartDeferredWithBackForwardCache) {
  loader_->SetDefersLoading(LoaderFreezeMode::kBufferIncoming);
  loader_->StartLoadingBody(this);
  Write("hello");
  expecting_data_received_ = true;
  loader_->SetDefersLoading(LoaderFreezeMode::kNone);
  EXPECT_EQ("hello", TakeDataReceived());
}

TEST_F(StaticDataNavigationBodyLoaderTest, DestroyFromFinished) {
  loader_->StartLoadingBody(this);
  expecting_finished_ = true;
  destroy_loader_ = true;
  loader_->Finish();
  EXPECT_TRUE(did_finish_);
}

TEST_F(StaticDataNavigationBodyLoaderTest, SetDefersLoadingFromFinished) {
  loader_->StartLoadingBody(this);
  expecting_finished_ = true;
  freeze_mode_ = LoaderFreezeMode::kStrict;
  loader_->Finish();
  EXPECT_TRUE(did_finish_);
}

TEST_F(StaticDataNavigationBodyLoaderTest,
       SetDefersLoadingWithBfcacheFromFinished) {
  loader_->StartLoadingBody(this);
  expecting_finished_ = true;
  freeze_mode_ = LoaderFreezeMode::kBufferIncoming;
  loader_->Finish();
  EXPECT_TRUE(did_finish_);
}
}  // namespace blink
