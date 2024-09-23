// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_TEST_BASE_H_
#define PDF_PDFIUM_PDFIUM_TEST_BASE_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

class PDFiumEngine;
class PDFiumPage;
class TestClient;
class TestDocumentLoader;

class PDFiumTestBase : public testing::TestWithParam<bool> {
 public:
  PDFiumTestBase();
  PDFiumTestBase(const PDFiumTestBase&) = delete;
  PDFiumTestBase& operator=(const PDFiumTestBase&) = delete;
  ~PDFiumTestBase() override;

  // Returns true in test environments that use //third_party/test_fonts.
  static bool UsingTestFonts();

 protected:
  // Result of calling InitializeEngineWithoutLoading().
  struct InitializeEngineResult {
    InitializeEngineResult();
    InitializeEngineResult(InitializeEngineResult&& other) noexcept;
    InitializeEngineResult& operator=(InitializeEngineResult&& other) noexcept;
    ~InitializeEngineResult();

    // Completes loading the document.
    void FinishLoading();

    // Initialized engine.
    std::unique_ptr<PDFiumEngine> engine;

    // Corresponding test document loader.
    raw_ptr<TestDocumentLoader> document_loader;
  };

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Initializes a PDFiumEngine for use in testing with `client`. Loads a PDF
  // named `pdf_name`. See TestDocumentLoader for more info about `pdf_name`.
  std::unique_ptr<PDFiumEngine> InitializeEngine(
      TestClient* client,
      const base::FilePath::CharType* pdf_name);

  // Initializes a PDFiumEngine as with InitializeEngine(), but defers loading
  // until the test calls SimulateLoadData() on the returned TestDocumentLoader.
  InitializeEngineResult InitializeEngineWithoutLoading(
      TestClient* client,
      const base::FilePath::CharType* pdf_name);

  // Returns the `PDFiumPage` for the page index. The page index must be valid
  // (less than `engine.GetNumberOfPages()`).
  static const PDFiumPage& GetPDFiumPageForTest(const PDFiumEngine& engine,
                                                size_t page_index);
  static PDFiumPage& GetPDFiumPageForTest(PDFiumEngine& engine,
                                          size_t page_index);

 private:
  void InitializePDFiumSDK();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::FilePath test_fonts_path_;
#endif

  // Stores custom font paths, if any, in a format compatible with
  // FPDF_InitLibraryWithConfig(). This must outlive `test_fonts_path_`, as it
  // may point to it. This must remain valid while PDFium is active, from when
  // FPDF_InitLibraryWithConfig() first gets called to when
  // FPDF_DestroyLibrary() gets called.
  std::vector<const char*> font_paths_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_TEST_BASE_H_
