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

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/win/scoped_hdc.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "printing/printing_context_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

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
    // To help debugging.
    ptrdiff_t index = itr - emf_enum.begin();
    // If you get this assert, you need to lookup iType in wingdi.h. It starts
    // with EMR_HEADER.
    EMR_HEADER;
    EXPECT_TRUE(itr->SafePlayback(&emf_enum.context_))
        << " index: " << index << " type: " << itr->record()->iType;
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

}  // namespace printing
