// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ink_module.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

namespace {

class FakeClient : public InkModule::Client {
 public:
  FakeClient() = default;
  FakeClient(const FakeClient&) = delete;
  FakeClient& operator=(const FakeClient&) = delete;
  ~FakeClient() override = default;

  // InkModule::Client:
  int VisiblePageIndexFromPoint(const gfx::PointF& point) override {
    // TODO(crbug.com/335524380): Implement.
    return -1;
  }
};

class InkModuleTest : public testing::Test {
 protected:
  InkModule& ink_module() { return ink_module_; }

 private:
  base::test::ScopedFeatureList feature_list_{features::kPdfInk2};

  FakeClient client_;
  InkModule ink_module_{client_};
};

TEST_F(InkModuleTest, UnknownMessage) {
  base::Value::Dict message;
  message.Set("type", "nonInkMessage");
  EXPECT_FALSE(ink_module().OnMessage(message));
}

TEST_F(InkModuleTest, HandleSetAnnotationModeMessage) {
  EXPECT_FALSE(ink_module().enabled());

  base::Value::Dict message;
  message.Set("type", "setAnnotationMode");
  message.Set("enable", false);

  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());

  message.Set("enable", true);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_TRUE(ink_module().enabled());

  message.Set("enable", false);
  EXPECT_TRUE(ink_module().OnMessage(message));
  EXPECT_FALSE(ink_module().enabled());
}

}  // namespace

}  // namespace chrome_pdf
