// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_font_win.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/memory/raw_ptr.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"

namespace chrome_pdf {

namespace {

// Tests the SkiaFontMapper without a sandbox - this is useful to validate the
// logic within SkiaFontMapper but does not test how skia's default font manager
// is wired up.
class PDFiumFontWinTest : public testing::Test {
 public:
  // Secretly this is a skia typeface id.
  using FontId = void*;

  PDFiumFontWinTest() = default;
  PDFiumFontWinTest(const PDFiumFontWinTest&) = delete;
  PDFiumFontWinTest& operator=(const PDFiumFontWinTest&) = delete;
  ~PDFiumFontWinTest() override = default;

 protected:
  void SetUp() override {
    InitializeSDK(/*enable_v8=*/false, /*use_skia=*/false,
                  FontMappingMode::kBlink);
    mapper_ = GetSkiaFontMapperForTesting();
    // Need these fields to do the tests.
    ASSERT_TRUE(mapper_);
    ASSERT_TRUE(mapper_->version);
    ASSERT_TRUE(mapper_->MapFont);
    ASSERT_TRUE(mapper_->GetFontData);
    ASSERT_TRUE(mapper_->DeleteFont);
  }

  void TearDown() override { ShutdownSDK(); }

  FontId MapFont(int weight,
                 bool italic,
                 int charset,
                 int pitch,
                 const char* face) {
    return mapper_->MapFont(nullptr, weight, italic, charset, pitch, face,
                            nullptr);
  }

  size_t GetFontData(FontId font,
                     unsigned int table,
                     unsigned char* buffer,
                     size_t buf_size) {
    return mapper_->GetFontData(nullptr, font, table, buffer, buf_size);
  }

  void DeleteFont(FontId font) { return mapper_->DeleteFont(nullptr, font); }

 private:
  raw_ptr<FPDF_SYSFONTINFO> mapper_;
};

}  // namespace

TEST_F(PDFiumFontWinTest, NonExistingFont) {
  EXPECT_EQ(nullptr, MapFont(FXFONT_FW_NORMAL, false, FXFONT_DEFAULT_CHARSET,
                             FXFONT_FF_ROMAN, "ThisIsHopefullyNeverAFont"));
}

TEST_F(PDFiumFontWinTest, Basic) {
  FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_DEFAULT_CHARSET,
                      FXFONT_FF_ROMAN, "Arial");
  EXPECT_TRUE(id);
  size_t needed = GetFontData(id, 0, nullptr, 0);
  EXPECT_GT(needed, 0u);
  auto data = base::HeapArray<unsigned char>::WithSize(needed);
  size_t got = GetFontData(id, 0, data.data(), data.size());
  EXPECT_EQ(got, needed);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, DefaultFonts) {
  for (const auto* face :
       {"Courier", "Helvetica", "Symbol", "Times-BoldItalic"}) {
    FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_DEFAULT_CHARSET,
                        FXFONT_FF_ROMAN, face);
    EXPECT_TRUE(id);
    DeleteFont(id);
  }
}

TEST_F(PDFiumFontWinTest, FallbackFontsGB) {
  // crbug.com/40109579 -> SimSun.
  FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_GB2312_CHARSET,
                      FXFONT_FF_ROMAN, "\xD0\xC2\xCB\xCE\xCC\xE5");
  EXPECT_TRUE(id);
  DeleteFont(id);

  // crbug.com/40109579 -> SimSun.
  id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_GB2312_CHARSET, FXFONT_FF_ROMAN,
               "\xB7\xBD\xD5\xFD\xCF\xB8\xB5\xC8\xCF\xDF\xBC\xF2\xCC\xE5");
  EXPECT_TRUE(id);
  DeleteFont(id);

  // KaiTi is supplemental -> KaiTi or SimSun
  id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_GB2312_CHARSET, FXFONT_FF_ROMAN,
               "KaiTi");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, FallbackFontsShiftJIS) {
  // crbug.com/40260882 -> MS Gothic.
  FontId id =
      MapFont(FXFONT_FW_NORMAL, false, FXFONT_SHIFTJIS_CHARSET, FXFONT_FF_ROMAN,
              "\x82\x6C\x82\x72\x83\x53\x83\x56\x83\x62\x83\x4E");
  EXPECT_TRUE(id);
  DeleteFont(id);

  // MS PMincho is supplemental -> MS PMincho or MS PGothic.
  id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_SHIFTJIS_CHARSET,
               FXFONT_FF_ROMAN, "MS PMincho");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, FallbackFontsHangeul) {
  // e.g. Skia corpus 02201.pdf -> Gulim or Malgun Gothic.
  FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_HANGEUL_CHARSET,
                      FXFONT_FF_ROMAN, "HYMyeongJo-Medium");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, FinalFixupsArialBlack) {
  // ArialBlack with a low weight needs to be forced to weight kBlack.
  FontId id = MapFont(390, false, FXFONT_DEFAULT_CHARSET, FXFONT_FF_ROMAN,
                      "ArialBlack");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, FinalFixupsComicSansMS) {
  // ComicSansMS -> Comic Sans MS.
  FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_DEFAULT_CHARSET,
                      FXFONT_FF_ROMAN, "ComicSansMS");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

TEST_F(PDFiumFontWinTest, FinalFixupsTrebuchetMS) {
  // TrebuchetMS -> Trebuchet MS.
  FontId id = MapFont(FXFONT_FW_NORMAL, false, FXFONT_DEFAULT_CHARSET,
                      FXFONT_FF_ROMAN, "TrebuchetMS");
  EXPECT_TRUE(id);
  DeleteFont(id);
}

}  // namespace chrome_pdf
