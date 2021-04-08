// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_plugin_base.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/ppapi_migration/callback.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "ppapi/cpp/instance.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

// This test approach relies on PdfViewPluginBase continuing to exist.
// PdfViewPluginBase and PdfViewWebPlugin are going to merge once
// OutOfProcessInstance is deprecated.
class FakePdfViewPluginBase : public PdfViewPluginBase {
 public:
  // Public for testing.
  using PdfViewPluginBase::ConsumeSaveToken;
  using PdfViewPluginBase::HandleMessage;

  MOCK_METHOD(bool, Confirm, (const std::string&), (override));

  MOCK_METHOD(std::string,
              Prompt,
              (const std::string&, const std::string&),
              (override));

  MOCK_METHOD(std::unique_ptr<UrlLoader>, CreateUrlLoader, (), (override));

  MOCK_METHOD(std::vector<PDFEngine::Client::SearchStringResult>,
              SearchString,
              (const char16_t*, const char16_t*, bool),
              (override));

  MOCK_METHOD(pp::Instance*, GetPluginInstance, (), (override));

  MOCK_METHOD(bool, IsPrintPreview, (), (override));

  MOCK_METHOD(void, SetSelectedText, (const std::string&), (override));

  MOCK_METHOD(void, SetLinkUnderCursor, (const std::string&), (override));

  MOCK_METHOD(bool, IsValidLink, (const std::string&), (override));

  MOCK_METHOD(std::unique_ptr<Graphics>,
              CreatePaintGraphics,
              (const gfx::Size&),
              (override));

  MOCK_METHOD(bool, BindPaintGraphics, (Graphics&), (override));

  MOCK_METHOD(void,
              ScheduleTaskOnMainThread,
              (const base::Location&, ResultCallback, int32_t, base::TimeDelta),
              (override));

  MOCK_METHOD(base::WeakPtr<PdfViewPluginBase>, GetWeakPtr, (), (override));

  MOCK_METHOD(std::unique_ptr<UrlLoader>,
              CreateUrlLoaderInternal,
              (),
              (override));

  MOCK_METHOD(void, DidOpen, (std::unique_ptr<UrlLoader>, int32_t), (override));

  MOCK_METHOD(void,
              DidOpenPreview,
              (std::unique_ptr<UrlLoader>, int32_t),
              (override));

  void SendMessage(base::Value message) override {
    sent_message_ = std::move(message);
  }

  MOCK_METHOD(void, InitImageData, (const gfx::Size&), (override));

  MOCK_METHOD(void, SetFormFieldInFocus, (bool in_focus), (override));

  MOCK_METHOD(void,
              SetAccessibilityDocInfo,
              (const AccessibilityDocInfo&),
              (override));

  MOCK_METHOD(void,
              SetAccessibilityPageInfo,
              (AccessibilityPageInfo,
               std::vector<AccessibilityTextRunInfo>,
               std::vector<AccessibilityCharInfo>,
               AccessibilityPageObjects),
              (override));

  MOCK_METHOD(void,
              SetAccessibilityViewportInfo,
              (const AccessibilityViewportInfo&),
              (override));

  MOCK_METHOD(void, SetContentRestrictions, (int), (override));

  MOCK_METHOD(void, DidStartLoading, (), (override));

  MOCK_METHOD(void, DidStopLoading, (), (override));

  MOCK_METHOD(void, OnPrintPreviewLoaded, (), (override));

  MOCK_METHOD(void, UserMetricsRecordAction, (const std::string&), (override));

  const base::Value& sent_message() const { return sent_message_; }

 private:
  base::Value sent_message_;
};

}  // namespace

class PdfViewPluginBaseTest : public testing::Test {
 protected:
  FakePdfViewPluginBase fake_plugin_;
};

TEST_F(PdfViewPluginBaseTest, ConsumeSaveToken) {
  const std::string kTokenString("12345678901234567890");
  fake_plugin_.ConsumeSaveToken(kTokenString);

  base::Value expected_message(base::Value::Type::DICTIONARY);
  expected_message.SetStringKey("type", "consumeSaveToken");
  expected_message.SetStringKey("token", kTokenString);

  EXPECT_EQ(expected_message, fake_plugin_.sent_message());
}

TEST_F(PdfViewPluginBaseTest, HandleSetBackgroundColorMessage) {
  const SkColor kNewBackgroundColor = SK_ColorGREEN;
  ASSERT_NE(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());

  base::Value message(base::Value::Type::DICTIONARY);
  message.SetStringKey("type", "setBackgroundColor");
  message.SetDoubleKey("color", kNewBackgroundColor);

  fake_plugin_.HandleMessage(message);
  EXPECT_EQ(kNewBackgroundColor, fake_plugin_.GetBackgroundColor());
}

}  // namespace chrome_pdf
