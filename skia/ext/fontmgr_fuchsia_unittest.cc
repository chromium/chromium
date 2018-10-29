// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/fontmgr_fuchsia.h"

#include <fuchsia/fonts/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skia {

namespace {

constexpr zx_rights_t kFontDataRights =
    ZX_RIGHTS_BASIC | ZX_RIGHT_READ | ZX_RIGHT_MAP;

fuchsia::mem::Buffer LoadFont(const base::FilePath& file_path) {
  std::string file_content;
  CHECK(ReadFileToString(file_path, &file_content));
  fuchsia::mem::Buffer buffer;
  zx_status_t status = zx::vmo::create(file_content.size(), 0, &buffer.vmo);
  ZX_CHECK(status == ZX_OK, status);
  status = buffer.vmo.write(file_content.data(), 0, file_content.size());
  ZX_CHECK(status == ZX_OK, status);
  buffer.size = file_content.size();
  return buffer;
}

class MockFontProvider : public fuchsia::fonts::Provider {
 public:
  MockFontProvider() {
    base::FilePath assets_dir;
    EXPECT_TRUE(base::PathService::Get(base::DIR_ASSETS, &assets_dir));

    // Roboto is not in test_fonts. Just load some arbitrary fonts for the
    // tests.
    roboto_ = LoadFont(assets_dir.Append("test_fonts/Arimo-Regular.ttf"));
    roboto_slab_ = LoadFont(assets_dir.Append("test_fonts/Tinos-Regular.ttf"));
  }

  // fuchsia::fonts::Provider implementation.
  void GetFont(fuchsia::fonts::Request request,
               GetFontCallback callback) override {
    fuchsia::mem::Buffer* font_buffer = nullptr;
    if (*request.family == "Roboto") {
      font_buffer = &roboto_;
    } else if (*request.family == "RobotoSlab") {
      font_buffer = &roboto_slab_;
    }

    if (!font_buffer) {
      callback(nullptr);
      return;
    }

    auto response = fuchsia::fonts::Response::New();
    EXPECT_EQ(
        font_buffer->vmo.duplicate(kFontDataRights, &(response->buffer.vmo)),
        ZX_OK);
    response->buffer.size = font_buffer->size;
    callback(std::move(response));
  }
  void GetFamilyInfo(fidl::StringPtr family,
                     GetFamilyInfoCallback callback) override {}

 private:
  fuchsia::mem::Buffer roboto_;
  fuchsia::mem::Buffer roboto_slab_;
};

class MockFontProviderService {
 public:
  MockFontProviderService() : provider_thread_("MockFontProvider") {
    provider_thread_.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  }

  ~MockFontProviderService() {
    provider_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                               std::move(provider_binding_));
  }

  void Bind(fidl::InterfaceRequest<fuchsia::fonts::Provider> request) {
    provider_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&MockFontProviderService::DoBind,
                                  base::Unretained(this), std::move(request)));
  }

 private:
  void DoBind(fidl::InterfaceRequest<fuchsia::fonts::Provider> request) {
    provider_binding_ =
        std::make_unique<fidl::Binding<fuchsia::fonts::Provider>>(
            &provider_, std::move(request));
  }

  base::Thread provider_thread_;

  MockFontProvider provider_;
  std::unique_ptr<fidl::Binding<fuchsia::fonts::Provider>> provider_binding_;
};

}  // namespace

class FuchsiaFontManagerTest : public testing::Test {
 public:
  FuchsiaFontManagerTest() {
    fuchsia::fonts::ProviderSyncPtr font_provider;
    font_provider_service_.Bind(font_provider.NewRequest());
    font_manager_ = sk_make_sp<FuchsiaFontManager>(std::move(font_provider));
  }

 protected:
  MockFontProviderService font_provider_service_;
  sk_sp<SkFontMgr> font_manager_;
};

// Verify that SkTypeface objects are cached.
TEST_F(FuchsiaFontManagerTest, Caching) {
  sk_sp<SkTypeface> sans(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));
  sk_sp<SkTypeface> sans2(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));

  // Expect that the same SkTypeface is returned for both requests.
  EXPECT_EQ(sans.get(), sans2.get());

  // Request serif and verify that a different SkTypeface is returned.
  sk_sp<SkTypeface> serif(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_NE(sans.get(), serif.get());
}

// Verify that SkTypeface can outlive the manager.
TEST_F(FuchsiaFontManagerTest, TypefaceOutlivesManager) {
  sk_sp<SkTypeface> sans(
      font_manager_->matchFamilyStyle("sans", SkFontStyle()));
  font_manager_.reset();
}

// Verify that we can query a font after releasing a previous instance.
TEST_F(FuchsiaFontManagerTest, ReleaseThenCreateAgain) {
  sk_sp<SkTypeface> serif(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_TRUE(serif);
  serif.reset();

  sk_sp<SkTypeface> serif2(
      font_manager_->matchFamilyStyle("serif", SkFontStyle()));
  EXPECT_TRUE(serif2);
}

}  // namespace skia
