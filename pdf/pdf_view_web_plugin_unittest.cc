// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

namespace {

class FakeContainerWrapper final : public PdfViewWebPlugin::ContainerWrapper {
 public:
  FakeContainerWrapper() = default;
  FakeContainerWrapper(const FakeContainerWrapper&) = delete;
  FakeContainerWrapper& operator=(const FakeContainerWrapper&) = delete;
  ~FakeContainerWrapper() override = default;

  // PdfViewWebPlugin::ContainerWrapper:
  void Invalidate() override {}

  float DeviceScaleFactor() const override { return device_scale_; }

  blink::WebPluginContainer* Container() override { return nullptr; }

  void set_device_scale(float device_scale) { device_scale_ = device_scale; }

 private:
  float device_scale_ = 1.0f;
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
    plugin_ = std::unique_ptr<PdfViewWebPlugin, PluginDeleter>(
        new PdfViewWebPlugin(blink::WebPluginParams()));

    auto wrapper = std::make_unique<FakeContainerWrapper>();
    wrapper_ptr_ = wrapper.get();
    plugin_->InitializeForTesting(std::move(wrapper));
  }

  void TearDown() override {
    plugin_.reset();
    wrapper_ptr_ = nullptr;
  }

  void TestUpdateGeometrySetsPluginRect(float device_scale,
                                        const gfx::Rect& window_rect,
                                        const gfx::Rect& expected_plugin_rect) {
    // The plugin container's device scale must be set before calling
    // UpdateGeometry().
    ASSERT_TRUE(wrapper_ptr_);
    wrapper_ptr_->set_device_scale(device_scale);
    plugin_->UpdateGeometry(window_rect, window_rect, window_rect,
                            /*is_visible=*/true);

    EXPECT_EQ(expected_plugin_rect, plugin_->GetPluginRectForTesting())
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  FakeContainerWrapper* wrapper_ptr_;
  std::unique_ptr<PdfViewWebPlugin, PluginDeleter> plugin_;
};

TEST_F(PdfViewWebPluginTest, UpdateGeometrySetsPluginRect) {
  struct UpdateGeometryParams {
    // The plugin container's device scale.
    float device_scale;

    //  The window area.
    gfx::Rect window_rect;

    // The expected plugin rect.
    gfx::Rect expected_plugin_rect;
  };

  static constexpr UpdateGeometryParams kUpdateGeometryParams[] = {
      {1.0f, gfx::Rect(3, 4, 5, 6), gfx::Rect(3, 4, 5, 6)},
      {2.0f, gfx::Rect(5, 6, 7, 8), gfx::Rect(10, 12, 14, 16)},
  };

  for (const auto& params : kUpdateGeometryParams) {
    TestUpdateGeometrySetsPluginRect(params.device_scale, params.window_rect,
                                     params.expected_plugin_rect);
  }
}

}  // namespace chrome_pdf
