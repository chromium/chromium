// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/font_warmup_win.h"

#include <windows.h>

#include <dwrite.h>
#include <stddef.h>
#include <stdint.h>
#include <wrl.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/public/common/content_paths.h"
#include "content/test/dwrite_font_fake_sender_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

class GDIFontEmulationTest : public testing::Test {
 public:
  GDIFontEmulationTest() {
    fake_collection_ = std::make_unique<FakeFontCollection>();
    SetupFonts(fake_collection_.get());
    DWriteFontCollectionProxy::Create(&collection_, factory.Get(),
                                      fake_collection_->CreateRemote());
    EXPECT_TRUE(collection_.Get());

    content::SetPreSandboxWarmupFontMgrForTesting(
        SkFontMgr_New_DirectWrite(factory.Get(), collection_.Get()));
  }

  ~GDIFontEmulationTest() override {
    content::SetPreSandboxWarmupFontMgrForTesting(nullptr);

    if (collection_)
      collection_->Unregister();
  }

  static void SetupFonts(FakeFontCollection* fonts) {
    base::FilePath data_path;
    EXPECT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &data_path));

    base::FilePath gdi_path = data_path.AppendASCII("font/gdi_test.ttf");
    fonts->AddFont(u"GDITest")
        .AddFamilyName(u"en-us", u"GDITest")
        .AddFamilyName(u"de-de", u"GDIUntersuchung")
        .AddFilePath(gdi_path);
  }

  static void SetUpTestCase() {
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                        &factory);
  }

 protected:
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<FakeFontCollection> fake_collection_;
  mswr::ComPtr<DWriteFontCollectionProxy> collection_;

  static mswr::ComPtr<IDWriteFactory> factory;
};
mswr::ComPtr<IDWriteFactory> GDIFontEmulationTest::factory;

// The test fixture will provide a font named "GDITest".
const wchar_t* kTestFontFamilyW = L"GDITest";

// The "GDITest" font will have an 'hhea' table (all ttf fonts do).
const DWORD kTestFontTableTag = DWRITE_MAKE_OPENTYPE_TAG('h', 'h', 'e', 'a');

// The 'hhea' table will be of length 36 (all 'hhea' tables do).
const size_t kTestFontTableDataLength = 36;

// The 'hhea' table will contain this content (specific to this font).
const uint8_t kTestFontTableData[kTestFontTableDataLength] = {
    0x00, 0x01, 0x00, 0x00, 0x03, 0x34, 0xFF, 0x33, 0x00, 0x5e, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03};

// The test fixture will not provide a font names "InvalidFont".
const wchar_t* kTestFontFamilyInvalid = L"InvalidFont";

void InitLogFont(LOGFONTW* logfont, const wchar_t* fontname) {
  size_t length = std::min(sizeof(logfont->lfFaceName),
                           (wcslen(fontname) + 1) * sizeof(wchar_t));
  memcpy(logfont->lfFaceName, fontname, length);
}

content::GdiFontPatchData* SetupTest() {
  HMODULE module_handle;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                         reinterpret_cast<LPCWSTR>(SetupTest),
                         &module_handle)) {
    WCHAR module_path[MAX_PATH];

    if (GetModuleFileNameW(module_handle, module_path, MAX_PATH) > 0) {
      base::FilePath path(module_path);
      content::ResetEmulatedGdiHandlesForTesting();
      return content::PatchGdiFontEnumeration(path);
    }
  }
  return nullptr;
}

int CALLBACK EnumFontCallbackTest(const LOGFONT* log_font,
                                  const TEXTMETRIC* text_metric,
                                  DWORD font_type,
                                  LPARAM param) {
  const NEWTEXTMETRICEX* new_text_metric =
      reinterpret_cast<const NEWTEXTMETRICEX*>(text_metric);

  return !(font_type & TRUETYPE_FONTTYPE) &&
         !(new_text_metric->ntmTm.ntmFlags & NTM_PS_OPENTYPE);
}

}  // namespace

TEST_F(GDIFontEmulationTest, CreateDeleteDCSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_FALSE(!patch_data);

  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  EXPECT_EQ(1u, GetEmulatedGdiHandleCountForTesting());
  EXPECT_TRUE(DeleteDC(hdc));
  EXPECT_EQ(0u, GetEmulatedGdiHandleCountForTesting());
}

TEST_F(GDIFontEmulationTest, CreateUniqueDCSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);

  HDC hdc1 = CreateCompatibleDC(0);
  EXPECT_NE(hdc1, nullptr);
  HDC hdc2 = CreateCompatibleDC(0);
  EXPECT_NE(hdc2, nullptr);
  EXPECT_NE(hdc1, hdc2);
  EXPECT_TRUE(DeleteDC(hdc2));
  EXPECT_EQ(1u, GetEmulatedGdiHandleCountForTesting());
  EXPECT_TRUE(DeleteDC(hdc1));
  EXPECT_EQ(0u, GetEmulatedGdiHandleCountForTesting());
}

TEST_F(GDIFontEmulationTest, CreateFontSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyW);
  HFONT font = CreateFontIndirectW(&logfont);
  EXPECT_NE(font, nullptr);
  EXPECT_TRUE(DeleteObject(font));
  EXPECT_EQ(0u, GetEmulatedGdiHandleCountForTesting());
}

TEST_F(GDIFontEmulationTest, CreateFontFailure) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyInvalid);
  HFONT font = CreateFontIndirectW(&logfont);
  EXPECT_EQ(font, nullptr);
}

TEST_F(GDIFontEmulationTest, EnumFontFamilySuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyW);
  int res = EnumFontFamiliesExW(hdc, &logfont, EnumFontCallbackTest, 0, 0);
  EXPECT_FALSE(res);
  EXPECT_TRUE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, EnumFontFamilyFailure) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyInvalid);
  int res = EnumFontFamiliesExW(hdc, &logfont, EnumFontCallbackTest, 0, 0);
  EXPECT_TRUE(res);
  EXPECT_TRUE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, DeleteDCFailure) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = reinterpret_cast<HDC>(0x55667788);
  EXPECT_FALSE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, DeleteObjectFailure) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HFONT font = reinterpret_cast<HFONT>(0x88aabbcc);
  EXPECT_FALSE(DeleteObject(font));
}

TEST_F(GDIFontEmulationTest, GetFontDataSizeSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyW);
  HFONT font = CreateFontIndirectW(&logfont);
  EXPECT_NE(font, nullptr);
  EXPECT_EQ(SelectObject(hdc, font), nullptr);
  DWORD size = GetFontData(hdc, kTestFontTableTag, 0, nullptr, 0);
  DWORD data_size = static_cast<DWORD>(kTestFontTableDataLength);
  EXPECT_EQ(size, data_size);
  EXPECT_TRUE(DeleteObject(font));
  EXPECT_TRUE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, GetFontDataInvalidTagSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyW);
  HFONT font = CreateFontIndirectW(&logfont);
  EXPECT_NE(font, nullptr);
  EXPECT_EQ(SelectObject(hdc, font), nullptr);
  DWORD size = GetFontData(hdc, kTestFontTableTag + 1, 0, nullptr, 0);
  EXPECT_EQ(size, GDI_ERROR);
  EXPECT_TRUE(DeleteObject(font));
  EXPECT_TRUE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, GetFontDataInvalidFontSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  DWORD size = GetFontData(hdc, kTestFontTableTag, 0, nullptr, 0);
  EXPECT_EQ(size, GDI_ERROR);
  EXPECT_TRUE(DeleteDC(hdc));
}

TEST_F(GDIFontEmulationTest, GetFontDataDataSuccess) {
  std::unique_ptr<GdiFontPatchData> patch_data(SetupTest());
  EXPECT_NE(patch_data, nullptr);
  HDC hdc = CreateCompatibleDC(0);
  EXPECT_NE(hdc, nullptr);
  LOGFONTW logfont = {0};
  InitLogFont(&logfont, kTestFontFamilyW);
  HFONT font = CreateFontIndirectW(&logfont);
  EXPECT_NE(font, nullptr);
  EXPECT_EQ(SelectObject(hdc, font), nullptr);
  DWORD data_size = static_cast<DWORD>(kTestFontTableDataLength);
  std::vector<char> data(data_size);
  DWORD size = GetFontData(hdc, kTestFontTableTag, 0, &data[0], data.size());
  EXPECT_EQ(size, data_size);
  EXPECT_EQ(memcmp(&data[0], kTestFontTableData, data.size()), 0);
  EXPECT_TRUE(DeleteObject(font));
  EXPECT_TRUE(DeleteDC(hdc));
}

}  // namespace content
