// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/emf_win.h"

// For quick access.
#include <stdint.h>
#include <wingdi.h>
#include <winspool.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_hdc.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/printing_context_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

using testing::ElementsAre;

namespace printing {

namespace {

// This test is automatically disabled if no printer named "UnitTest Printer" is
// available.
class EmfPrintingTest : public testing::Test, public PrintingContext::Delegate {
 public:
  typedef testing::Test Parent;
  static bool IsTestCaseDisabled() {
    // It is assumed this printer is a HP Color LaserJet 4550 PCL or 4700.
    base::win::ScopedCreateDC hdc(
        CreateDC(L"WINSPOOL", L"UnitTest Printer", nullptr, nullptr));
    return !hdc.is_valid();
  }

  // PrintingContext::Delegate methods.
  gfx::NativeView GetParentView() override { return gfx::NativeView(); }
  std::string GetAppLocale() override { return std::string(); }
};

const uint32_t EMF_HEADER_SIZE = 128;

std::vector<char> CreateTestEmfData() {
  std::vector<char> data;
  Emf emf;
  EXPECT_TRUE(emf.Init());
  EXPECT_TRUE(emf.context());
  ::Rectangle(emf.context(), 10, 10, 190, 190);

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = 1;
  bmi.bmiHeader.biHeight = 1;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  uint32_t pixel = 0x00FF0000;
  ::StretchDIBits(emf.context(), 0, 0, 10, 10, 0, 0, 1, 1, &pixel, &bmi,
                  DIB_RGB_COLORS, SRCCOPY);

  ::SetLayout(emf.context(), LAYOUT_RTL);
  ::Rectangle(emf.context(), 20, 20, 180, 180);
  EXPECT_TRUE(emf.FinishDocument());
  uint32_t size = emf.GetDataSize();
  EXPECT_GT(size, EMF_HEADER_SIZE);
  EXPECT_TRUE(emf.GetDataAsVector(&data));
  EXPECT_EQ(data.size(), size);
  return data;
}

// To simplify testing, only examine these types and ignore others like
// `EMR_SAVEDC` and `EMR_SELECTOBJECT`.
bool IsRecordTypeOfInterest(DWORD type) {
  return type == EMR_HEADER || type == EMR_RECTANGLE ||
         type == EMR_STRETCHDIBITS || type == EMR_EOF;
}

}  // namespace

TEST(EmfTest, DC) {
  // Simplest use case.
  std::vector<char> data;
  {
    Emf emf;
    EXPECT_TRUE(emf.Init());
    EXPECT_TRUE(emf.context());
    // An empty EMF is invalid, so we put at least a rectangle in it.
    ::Rectangle(emf.context(), 10, 10, 190, 190);
    EXPECT_TRUE(emf.FinishDocument());
    uint32_t size = emf.GetDataSize();
    EXPECT_GT(size, EMF_HEADER_SIZE);
    EXPECT_TRUE(emf.GetDataAsVector(&data));
    ASSERT_EQ(data.size(), size);
  }

  // Playback the data.
  Emf emf;
  // TODO(thestig): Make `data` uint8_t and avoid the base::as_byte_span() call.
  EXPECT_TRUE(emf.InitFromData(base::as_byte_span(data)));
  base::win::ScopedCreateDC hdc(CreateCompatibleDC(nullptr));
  ASSERT_TRUE(hdc.is_valid());
  RECT output_rect = {0, 0, 10, 10};
  EXPECT_TRUE(emf.Playback(hdc.get(), &output_rect));
}

// Disabled if no "UnitTest printer" exist. Useful to reproduce bug 1186598.
TEST_F(EmfPrintingTest, Enumerate) {
  if (IsTestCaseDisabled()) {
    GTEST_SKIP();
  }

  auto settings = std::make_unique<PrintSettings>();

  // My test case is a HP Color LaserJet 4550 PCL.
  settings->set_device_name(u"UnitTest Printer");

  // Initialize it.
  PrintingContextWin context(this,
                             PrintingContext::OutOfProcessBehavior::kDisabled);
  EXPECT_EQ(mojom::ResultCode::kSuccess,
            context.InitWithSettingsForTest(std::move(settings)));

  base::FilePath emf_file;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &emf_file));
  emf_file = emf_file.Append(FILE_PATH_LITERAL("printing"))
                 .Append(FILE_PATH_LITERAL("test"))
                 .Append(FILE_PATH_LITERAL("data"))
                 .Append(FILE_PATH_LITERAL("emf"))
                 .Append(FILE_PATH_LITERAL("test4.emf"));

  // Load any EMF with an image.
  std::optional<std::vector<uint8_t>> emf_data =
      base::ReadFileToBytes(emf_file);
  ASSERT_TRUE(emf_data.has_value());
  ASSERT_TRUE(emf_data.value().size());

  Emf emf;
  EXPECT_TRUE(emf.InitFromData(emf_data.value()));

  // This will print to file. The reason is that when running inside a
  // unit_test, PrintingContext automatically dumps its files to the
  // current directory.
  // TODO(maruel):  Clean the .PRN file generated in current directory.
  context.NewDocument(u"EmfTest.Enumerate");
  // Process one at a time.
  RECT page_bounds = emf.GetPageBounds(1).ToRECT();
  Emf::Enumerator emf_enum(emf, context.context(), &page_bounds);
  for (Emf::Enumerator::const_iterator itr = emf_enum.begin();
       itr != emf_enum.end(); ++itr) {
    itr->SafePlayback(&emf_enum.context_);
  }
  context.DocumentDone();
}

// Disabled if no "UnitTest printer" exists.
TEST_F(EmfPrintingTest, PageBreak) {
  base::win::ScopedCreateDC hdc(
      CreateDC(L"WINSPOOL", L"UnitTest Printer", nullptr, nullptr));
  if (!hdc.is_valid()) {
    GTEST_SKIP();
  }

  std::vector<char> data;
  {
    Emf emf;
    EXPECT_TRUE(emf.Init());
    EXPECT_TRUE(emf.context());
    int pages = 3;
    while (pages) {
      emf.StartPage(gfx::Size(), gfx::Rect(), 1,
                    mojom::PageOrientation::kUpright);
      ::Rectangle(emf.context(), 10, 10, 190, 190);
      EXPECT_TRUE(emf.FinishPage());
      --pages;
    }
    EXPECT_EQ(3U, emf.GetPageCount());
    EXPECT_TRUE(emf.FinishDocument());
    uint32_t size = emf.GetDataSize();
    EXPECT_TRUE(emf.GetDataAsVector(&data));
    ASSERT_EQ(data.size(), size);
  }

  // Playback the data.
  DOCINFO di = {0};
  di.cbSize = sizeof(DOCINFO);
  di.lpszDocName = L"Test Job";
  int job_id = ::StartDoc(hdc.Get(), &di);
  Emf emf;
  // TODO(thestig): Make `data` uint8_t and avoid the base::as_byte_span() call.
  EXPECT_TRUE(emf.InitFromData(base::as_byte_span(data)));
  EXPECT_TRUE(emf.SafePlayback(hdc.Get()));
  ::EndDoc(hdc.Get());
  // Since presumably the printer is not real, let us just delete the job from
  // the queue.
  HANDLE printer = nullptr;
  if (::OpenPrinter(const_cast<LPTSTR>(L"UnitTest Printer"), &printer,
                    nullptr)) {
    ::SetJob(printer, job_id, 0, nullptr, JOB_CONTROL_DELETE);
    ClosePrinter(printer);
  }
}

TEST(EmfTest, RemainingMetafileSize) {
  Emf emf;
  EXPECT_TRUE(emf.Init());
  EXPECT_TRUE(emf.context());
  ::Rectangle(emf.context(), 10, 10, 190, 190);
  EXPECT_TRUE(emf.FinishDocument());

  uint32_t total_size = emf.GetDataSize();
  EXPECT_GT(total_size, 0u);

  RECT page_bounds = emf.GetPageBounds(1).ToRECT();
  base::win::ScopedCreateDC hdc(CreateCompatibleDC(nullptr));
  ASSERT_TRUE(hdc.is_valid());
  Emf::Enumerator emf_enum(emf, hdc.Get(), &page_bounds);

  uint32_t remaining_size = total_size;
  for (const auto& record : emf_enum) {
    remaining_size -= record.record()->nSize;
  }

  EXPECT_EQ(emf_enum.context_.remaining_metafile_size, remaining_size);
  EXPECT_EQ(remaining_size, 0u);
}

TEST(EmfTest, SafePlayback) {
  std::vector<char> data = CreateTestEmfData();
  ASSERT_FALSE(HasFailure());

  Emf src_emf;
  ASSERT_TRUE(src_emf.InitFromData(base::as_byte_span(data)));

  Emf dest_emf;
  ASSERT_TRUE(dest_emf.Init());
  ASSERT_TRUE(dest_emf.context());
  EXPECT_TRUE(src_emf.SafePlayback(dest_emf.context()));
  ASSERT_TRUE(dest_emf.FinishDocument());

  Emf::Enumerator emf_enum(dest_emf, nullptr, nullptr);
  std::vector<DWORD> record_types;
  for (const auto& record : emf_enum) {
    DWORD type = record.record()->iType;
    if (IsRecordTypeOfInterest(type)) {
      record_types.push_back(type);
    }
  }
  EXPECT_THAT(record_types,
              ElementsAre(EMR_HEADER, EMR_RECTANGLE, EMR_STRETCHDIBITS,
                          EMR_RECTANGLE, EMR_EOF));
}

TEST(EmfTest, SafePlaybackSkipsInvalidRecords) {
  std::vector<char> data = CreateTestEmfData();
  ASSERT_FALSE(HasFailure());

  // Corrupt `cbBmiSrc` in the `EMR_STRETCHDIBITS` record so
  // `Emf::Record::SafePlayback()` rejects it while `record->nSize` remains
  // valid for enumeration.
  bool found_stretch_dibits = false;
  base::span<uint8_t> span = base::as_writable_byte_span(data);
  while (span.size() >= sizeof(ENHMETARECORD)) {
    auto* record = reinterpret_cast<ENHMETARECORD*>(span.data());
    if (record->iType == EMR_STRETCHDIBITS) {
      ASSERT_GE(span.size(), sizeof(EMRSTRETCHDIBITS));
      auto* sdib_record = reinterpret_cast<EMRSTRETCHDIBITS*>(span.data());
      sdib_record->cbBmiSrc = 0;
      found_stretch_dibits = true;
      break;
    }
    ASSERT_LE(record->nSize, span.size());
    span = span.subspan(record->nSize);
  }
  ASSERT_TRUE(found_stretch_dibits);

  Emf src_emf;
  ASSERT_TRUE(src_emf.InitFromData(base::as_byte_span(data)));

  Emf dest_emf;
  ASSERT_TRUE(dest_emf.Init());
  ASSERT_TRUE(dest_emf.context());
  EXPECT_TRUE(src_emf.SafePlayback(dest_emf.context()));
  ASSERT_TRUE(dest_emf.FinishDocument());

  Emf::Enumerator emf_enum(dest_emf, nullptr, nullptr);
  std::vector<DWORD> record_types;
  for (const auto& record : emf_enum) {
    DWORD type = record.record()->iType;
    if (IsRecordTypeOfInterest(type)) {
      record_types.push_back(type);
    }
  }
  EXPECT_THAT(record_types,
              ElementsAre(EMR_HEADER, EMR_RECTANGLE, EMR_RECTANGLE, EMR_EOF));
}

TEST(PostScriptMetaFileTest, SafePlaybackUndersizedComment) {
  std::vector<char> data;
  {
    PostScriptMetaFile emf;
    EXPECT_TRUE(emf.Init());
    EXPECT_TRUE(emf.context());
    // Add `EMR_GDICOMMENT` records where the comment size field is deliberately
    // wrong/undersized (< sizeof(uint16_t)). A valid PostScript comment
    // requires at least 2 bytes for the payload size header, so a size of 0 or
    // 1 cannot hold a valid payload size.
    EXPECT_TRUE(::GdiComment(emf.context(), 0, nullptr));
    uint8_t byte = 0;
    EXPECT_TRUE(::GdiComment(emf.context(), 1, &byte));
    ::Rectangle(emf.context(), 10, 10, 190, 190);
    EXPECT_TRUE(emf.FinishDocument());
    uint32_t size = emf.GetDataSize();
    EXPECT_GT(size, 0u);
    EXPECT_TRUE(emf.GetDataAsVector(&data));
  }

  PostScriptMetaFile emf;
  EXPECT_TRUE(emf.InitFromData(base::as_byte_span(data)));
  base::win::ScopedCreateDC hdc(CreateCompatibleDC(nullptr));
  ASSERT_TRUE(hdc.is_valid());
  // `SafePlayback()` should safely skip the undersized comment records and must
  // not crash from an out-of-bounds read.
  EXPECT_TRUE(emf.SafePlayback(hdc.get()));
}

}  // namespace printing
