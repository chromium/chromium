// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <memory>
#include <utility>

#include "cc/paint/paint_canvas.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace chrome_pdf {

namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Pointwise;
using ::testing::Return;

// `kCanvasSize` needs to be big enough to hold plugin's snapshots during
// testing.
constexpr gfx::Size kCanvasSize(100, 100);

// Note: Make sure `kDefaultColor` is different from `kPaintColor` and the
// plugin's background color. This will help identify bitmap changes after
// painting.
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr SkColor kPaintColor = SK_ColorRED;

struct PaintParams {
  // The plugin container's device scale.
  float device_scale;

  // The window area.
  gfx::Rect window_rect;

  // The target painting area on the canvas.
  gfx::Rect paint_rect;
};

MATCHER(SearchStringResultEq, "") {
  PDFEngine::Client::SearchStringResult l = std::get<0>(arg);
  PDFEngine::Client::SearchStringResult r = std::get<1>(arg);
  return l.start_index == r.start_index && l.length == r.length;
}

// Generates the expected `SkBitmap` with `paint_color` filled in the expected
// clipped area and `kDefaultColor` as the background color.
SkBitmap GenerateExpectedBitmapForPaint(float device_scale,
                                        bool use_zoom_for_dsf,
                                        const gfx::Rect& plugin_rect,
                                        const gfx::Rect& paint_rect,
                                        SkColor paint_color) {
  float inverse_scale = use_zoom_for_dsf ? 1.0f : 1.0f / device_scale;
  // TODO(crbug.com/1238395): Improve the test by pre-define the
  // `expected_clipped_area` instead of calculating it inside the test.
  gfx::Rect expected_clipped_area = gfx::IntersectRects(
      gfx::ScaleToEnclosingRectSafe(plugin_rect, inverse_scale), paint_rect);
  SkBitmap expected_bitmap =
      CreateN32PremulSkBitmap(gfx::SizeToSkISize(kCanvasSize));
  expected_bitmap.eraseColor(kDefaultColor);
  expected_bitmap.erase(paint_color, gfx::RectToSkIRect(expected_clipped_area));
  return expected_bitmap;
}

class FakeContainerWrapper final : public PdfViewWebPlugin::ContainerWrapper {
 public:
  explicit FakeContainerWrapper(PdfViewWebPlugin* web_plugin)
      : web_plugin_(web_plugin) {
    ON_CALL(*this, UpdateTextInputState)
        .WillByDefault(Invoke(
            this, &FakeContainerWrapper::UpdateTextInputStateFromPlugin));

    UpdateTextInputStateFromPlugin();
  }

  FakeContainerWrapper(const FakeContainerWrapper&) = delete;
  FakeContainerWrapper& operator=(const FakeContainerWrapper&) = delete;
  ~FakeContainerWrapper() override = default;

  // PdfViewWebPlugin::ContainerWrapper:
  void Invalidate() override {}

  MOCK_METHOD(void, ReportFindInPageMatchCount, (int, int, bool), (override));

  MOCK_METHOD(void, ReportFindInPageSelection, (int, int), (override));

  float DeviceScaleFactor() override { return device_scale_; }

  MOCK_METHOD(void,
              SetReferrerForRequest,
              (blink::WebURLRequest&, const blink::WebURL&),
              (override));

  MOCK_METHOD(void, Alert, (const blink::WebString&), (override));

  MOCK_METHOD(bool, Confirm, (const blink::WebString&), (override));

  MOCK_METHOD(blink::WebString,
              Prompt,
              (const blink::WebString&, const blink::WebString&),
              (override));

  MOCK_METHOD(void,
              TextSelectionChanged,
              (const blink::WebString&, uint32_t, const gfx::Range&),
              (override));

  MOCK_METHOD(std::unique_ptr<blink::WebAssociatedURLLoader>,
              CreateAssociatedURLLoader,
              (const blink::WebAssociatedURLLoaderOptions&),
              (override));

  MOCK_METHOD(void, UpdateTextInputState, (), (override));

  MOCK_METHOD(void, UpdateSelectionBounds, (), (override));

  blink::WebLocalFrame* GetFrame() override { return nullptr; }

  blink::WebLocalFrameClient* GetWebLocalFrameClient() override {
    return nullptr;
  }

  // TODO(https://crbug.com/1207575): Container() should not be used for testing
  // since it doesn't have a valid blink::WebPluginContainer. Make this method
  // fail once ContainerWrapper instead of blink::WebPluginContainer is used for
  // initializing `PostMessageSender`.
  blink::WebPluginContainer* Container() override { return nullptr; }

  blink::WebTextInputType widget_text_input_type() const {
    return widget_text_input_type_;
  }

  void set_device_scale(float device_scale) { device_scale_ = device_scale; }

 private:
  void UpdateTextInputStateFromPlugin() {
    widget_text_input_type_ = web_plugin_->GetPluginTextInputType();
  }

  float device_scale_ = 1.0f;

  // Represents the frame widget's text input type.
  blink::WebTextInputType widget_text_input_type_;

  PdfViewWebPlugin* web_plugin_;
};

class FakePdfViewWebPluginClient : public PdfViewWebPlugin::Client {
 public:
  FakePdfViewWebPluginClient() = default;
  FakePdfViewWebPluginClient(const FakePdfViewWebPluginClient&) = delete;
  FakePdfViewWebPluginClient& operator=(const FakePdfViewWebPluginClient&) =
      delete;
  ~FakePdfViewWebPluginClient() override = default;

  // PdfViewWebPlugin::Client:
  MOCK_METHOD(bool, IsUseZoomForDSFEnabled, (), (const, override));
};

}  // namespace

class PdfViewWebPluginTest : public testing::Test {
 public:
  // Custom deleter for `plugin_`. PdfViewWebPlugin must be destroyed by
  // PdfViewWebPlugin::Destroy() instead of its destructor.
  struct PluginDeleter {
    void operator()(PdfViewWebPlugin* ptr) { ptr->Destroy(); }
  };

  PdfViewWebPluginTest() = default;
  PdfViewWebPluginTest(const PdfViewWebPluginTest&) = delete;
  PdfViewWebPluginTest& operator=(const PdfViewWebPluginTest&) = delete;
  ~PdfViewWebPluginTest() override = default;

  void SetUp() override {
    // Set a dummy URL for initializing the plugin.
    blink::WebPluginParams params;
    params.attribute_names.push_back(blink::WebString("src"));
    params.attribute_values.push_back(blink::WebString("dummy.pdf"));

    auto client = std::make_unique<NiceMock<FakePdfViewWebPluginClient>>();
    client_ptr_ = client.get();

    mojo::AssociatedRemote<pdf::mojom::PdfService> unbound_remote;
    plugin_ =
        std::unique_ptr<PdfViewWebPlugin, PluginDeleter>(new PdfViewWebPlugin(
            std::move(client), std::move(unbound_remote), params));

    auto wrapper = std::make_unique<FakeContainerWrapper>(plugin_.get());
    wrapper_ptr_ = wrapper.get();
    plugin_->InitializeForTesting(std::move(wrapper));
  }

  void TearDown() override {
    plugin_.reset();
    wrapper_ptr_ = nullptr;
  }

  void UpdatePluginGeometry(float device_scale, const gfx::Rect& window_rect) {
    // The plugin container's device scale must be set before calling
    // UpdateGeometry().
    ASSERT_TRUE(wrapper_ptr_);
    wrapper_ptr_->set_device_scale(device_scale);
    plugin_->UpdateGeometry(window_rect, window_rect, window_rect,
                            /*is_visible=*/true);
  }

  void TestUpdateGeometrySetsPluginRect(float device_scale,
                                        const gfx::Rect& window_rect,
                                        const gfx::Rect& expected_plugin_rect) {
    UpdatePluginGeometry(device_scale, window_rect);
    EXPECT_EQ(expected_plugin_rect, plugin_->GetPluginRectForTesting())
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  void TestPaintEmptySnapshots(float device_scale,
                               bool use_zoom_for_dsf,
                               const gfx::Rect& window_rect,
                               const gfx::Rect& paint_rect) {
    EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
        .WillOnce(Return(use_zoom_for_dsf));
    UpdatePluginGeometry(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with plugin's background
    // color.
    SkBitmap expected_bitmap = GenerateExpectedBitmapForPaint(
        device_scale, use_zoom_for_dsf, plugin_->GetPluginRectForTesting(),
        paint_rect, plugin_->GetBackgroundColor());
    EXPECT_TRUE(
        cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                          cc::ExactPixelComparator(/*discard_alpha=*/false)))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  void TestPaintSnapshots(float device_scale,
                          bool use_zoom_for_dsf,
                          const gfx::Rect& window_rect,
                          const gfx::Rect& paint_rect) {
    EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
        .WillOnce(Return(use_zoom_for_dsf));
    UpdatePluginGeometry(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    // Fill the graphics device with `kPaintColor` and update the plugin's
    // snapshot.
    const gfx::Rect& plugin_rect = plugin_->GetPluginRectForTesting();
    std::unique_ptr<Graphics> graphics =
        plugin_->CreatePaintGraphics(plugin_rect.size());
    graphics->PaintImage(
        CreateSkiaImageForTesting(plugin_rect.size(), kPaintColor),
        gfx::Rect(plugin_rect.width(), plugin_rect.height()));
    graphics->Flush(base::DoNothing());

    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with `kPaintColor`.
    SkBitmap expected_bitmap = GenerateExpectedBitmapForPaint(
        device_scale, use_zoom_for_dsf, plugin_rect, paint_rect, kPaintColor);
    EXPECT_TRUE(
        cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                          cc::ExactPixelComparator(/*discard_alpha=*/false)))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  FakeContainerWrapper* wrapper_ptr_;
  FakePdfViewWebPluginClient* client_ptr_;
  std::unique_ptr<PdfViewWebPlugin, PluginDeleter> plugin_;

  // Provides the cc::PaintCanvas for painting.
  gfx::Canvas canvas_{kCanvasSize, /*image_scale=*/1.0f, /*is_opaque=*/true};
};

// TODO(crbug.com/1238395): Split this test into two: One with zoom-for-DSF
// enabled, one with zoom-for-DSF disabled.
TEST_F(PdfViewWebPluginTest,
       UpdateGeometrySetsPluginRectOnDifferentUseZoomForDSFSettings) {
  // Use the same device scale for this test.
  const float device_scale = 2.0f;

  // Test when using zoom for DSF is enabled.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled).WillOnce(Return(true));
  TestUpdateGeometrySetsPluginRect(device_scale, gfx::Rect(4, 4, 12, 12),
                                   gfx::Rect(4, 4, 12, 12));

  // Test when using zoom for DSF is disabled.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled).WillOnce(Return(false));
  TestUpdateGeometrySetsPluginRect(device_scale, gfx::Rect(4, 4, 12, 12),
                                   gfx::Rect(8, 8, 24, 24));
}

TEST_F(PdfViewWebPluginTest,
       UpdateGeometrySetsPluginRectOnVariousDeviceScales) {
  struct UpdateGeometryParams {
    // The plugin container's device scale.
    float device_scale;

    //  The window rect in device pixels.
    gfx::Rect window_rect;

    // The expected plugin rect in device pixels.
    gfx::Rect expected_plugin_rect;
  };

  // Keep the using zoom for DSF setting consistent within the test.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));

  static constexpr UpdateGeometryParams kUpdateGeometryParams[] = {
      {1.0f, gfx::Rect(3, 4, 5, 6), gfx::Rect(3, 4, 5, 6)},
      {2.0f, gfx::Rect(4, 4, 12, 12), gfx::Rect(4, 4, 12, 12)},
      {2.0f, gfx::Rect(5, 6, 7, 8), gfx::Rect(4, 6, 8, 8)},
  };

  for (const auto& params : kUpdateGeometryParams) {
    TestUpdateGeometrySetsPluginRect(params.device_scale, params.window_rect,
                                     params.expected_plugin_rect);
  }
}

TEST_F(PdfViewWebPluginTest, PaintEmptySnapshots) {
  static constexpr PaintParams kPaintEmptySnapshotsParams[]{
      // The window origin falls outside the `paint_rect` area.
      {1.0f, gfx::Rect(10, 10, 20, 20), gfx::Rect(5, 5, 15, 15)},
      {4.0f, gfx::Rect(10, 10, 20, 20), gfx::Rect(5, 5, 15, 15)},
      // The window origin falls within the `paint_rect` area.
      {1.0f, gfx::Rect(4, 4, 20, 20), gfx::Rect(8, 8, 15, 15)},
      {4.0f, gfx::Rect(4, 4, 20, 20), gfx::Rect(8, 8, 15, 15)},
  };

  for (const auto& params : kPaintEmptySnapshotsParams) {
    TestPaintEmptySnapshots(params.device_scale, /*use_zoom_for_dsf=*/true,
                            params.window_rect, params.paint_rect);
    TestPaintEmptySnapshots(params.device_scale, /*use_zoom_for_dsf=*/false,
                            params.window_rect, params.paint_rect);
  }
}

TEST_F(PdfViewWebPluginTest, PaintSnapshots) {
  static constexpr PaintParams kPaintWithScalesTestParams[] = {
      // The window origin falls outside the `paint_rect` area.
      {1.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30)},
      {2.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30)},
      // The window origin falls within the `paint_rect` area.
      {1.0f, gfx::Rect(10, 10, 30, 30), gfx::Rect(4, 4, 30, 30)},
      {2.0f, gfx::Rect(10, 10, 30, 30), gfx::Rect(4, 4, 30, 30)},
  };

  for (const auto& params : kPaintWithScalesTestParams) {
    TestPaintSnapshots(params.device_scale, /*use_zoom_for_dsf=*/true,
                       params.window_rect, params.paint_rect);
    TestPaintSnapshots(params.device_scale, /*use_zoom_for_dsf=*/false,
                       params.window_rect, params.paint_rect);
  }
}

TEST_F(PdfViewWebPluginTest, ChangeTextSelection) {
  ASSERT_FALSE(plugin_->HasSelection());
  ASSERT_TRUE(plugin_->SelectionAsText().IsEmpty());
  ASSERT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());

  static constexpr char kSelectedText[] = "1234";
  EXPECT_CALL(*wrapper_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kSelectedText), 0,
                                   gfx::Range(0, 4)));

  plugin_->SetSelectedText(kSelectedText);
  EXPECT_TRUE(plugin_->HasSelection());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsText().Utf8());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsMarkup().Utf8());

  static constexpr char kEmptyText[] = "";
  EXPECT_CALL(*wrapper_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kEmptyText), 0,
                                   gfx::Range(0, 0)));
  plugin_->SetSelectedText(kEmptyText);
  EXPECT_FALSE(plugin_->HasSelection());
  EXPECT_TRUE(plugin_->SelectionAsText().IsEmpty());
  EXPECT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());
}

TEST_F(PdfViewWebPluginTest, FormTextFieldFocusChangeUpdatesTextInputType) {
  ASSERT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            wrapper_ptr_->widget_text_input_type());

  MockFunction<void()> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(checkpoint, Call);
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
  }

  plugin_->FormTextFieldFocusChange(true);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeText,
            wrapper_ptr_->widget_text_input_type());

  checkpoint.Call();

  plugin_->FormTextFieldFocusChange(false);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            wrapper_ptr_->widget_text_input_type());
}

TEST_F(PdfViewWebPluginTest, SearchString) {
  static constexpr char16_t kPattern[] = u"fox";
  static constexpr char16_t kTarget[] =
      u"The quick brown fox jumped over the lazy Fox";

  {
    static constexpr PDFEngine::Client::SearchStringResult kExpectation[] = {
        {16, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kTarget, kPattern, /*case_sensitive=*/true),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
  {
    static constexpr PDFEngine::Client::SearchStringResult kExpectation[] = {
        {16, 3}, {41, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kTarget, kPattern, /*case_sensitive=*/false),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
}

TEST_F(PdfViewWebPluginTest, UpdateFocus) {
  MockFunction<void(int checkpoint_num)> checkpoint;

  {
    InSequence sequence;

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(1));

    // Focus true -> true: No updates.
    EXPECT_CALL(checkpoint, Call(2));

    // Focus true -> false: Triggers updates. `UpdateTextInputState` is called
    // twice because it also gets called due to
    // `PDFiumEngine::UpdateFocus(false)`.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState).Times(2);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(3));

    // Focus false -> false: No updates.
    EXPECT_CALL(checkpoint, Call(4));

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
  }

  // The focus type does not matter in this test.
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(1);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(2);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(3);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(4);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
}

}  // namespace chrome_pdf
