// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_test_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace chrome_pdf {

namespace {

bool IsValidLinkForTesting(const std::string& url) {
  return !url.empty();
}

void SetSelectedTextForTesting(pp::Instance* instance,
                               const std::string& selected_text) {}

void SetLinkUnderCursorForTesting(pp::Instance* instance,
                                  const std::string& link_under_cursor) {}

}  // namespace

PDFiumTestBase::PDFiumTestBase() = default;

PDFiumTestBase::~PDFiumTestBase() = default;

// static
bool PDFiumTestBase::IsRunningOnChromeOS() {
#if defined(OS_CHROMEOS)
  return base::SysInfo::IsRunningOnChromeOS();
#else
  return false;
#endif
}

void PDFiumTestBase::SetUp() {
  InitializePDFium();
  PDFiumEngine::OverrideSetSelectedTextFunctionForTesting(
      &SetSelectedTextForTesting);
  PDFiumEngine::OverrideSetLinkUnderCursorFunctionForTesting(
      &SetLinkUnderCursorForTesting);
  PDFiumPage::SetIsValidLinkFunctionForTesting(&IsValidLinkForTesting);
}

void PDFiumTestBase::TearDown() {
  PDFiumPage::SetIsValidLinkFunctionForTesting(nullptr);
  PDFiumEngine::OverrideSetLinkUnderCursorFunctionForTesting(nullptr);
  PDFiumEngine::OverrideSetSelectedTextFunctionForTesting(nullptr);
  FPDF_DestroyLibrary();
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::InitializeEngine(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  InitializeEngineResult result =
      InitializeEngineWithoutLoading(client, pdf_name);
  if (result.engine) {
    // Incrementally read the PDF. To detect linearized PDFs, the first read
    // should be at least 1024 bytes.
    while (result.document_loader->SimulateLoadData(1024))
      continue;
  }
  return std::move(result.engine);
}

PDFiumTestBase::InitializeEngineResult
PDFiumTestBase::InitializeEngineWithoutLoading(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  InitializeEngineResult result;

  result.engine = std::make_unique<PDFiumEngine>(
      client, PDFiumFormFiller::ScriptOption::kNoJavaScript);
  client->set_engine(result.engine.get());

  auto test_loader =
      std::make_unique<TestDocumentLoader>(result.engine.get(), pdf_name);
  result.document_loader = test_loader.get();
  result.engine->SetDocumentLoaderForTesting(std::move(test_loader));

  if (!result.engine->New("https://chromium.org/dummy.pdf", "") ||
      !result.engine->HandleDocumentLoad(nullptr)) {
    client->set_engine(nullptr);
    result.engine = nullptr;
    result.document_loader = nullptr;
  }
  return result;
}

void PDFiumTestBase::InitializePDFium() {
  FPDF_LIBRARY_CONFIG config;
  config.version = 3;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = nullptr;
  config.m_v8EmbedderSlot = 0;
  config.m_pPlatform = nullptr;
  FPDF_InitLibraryWithConfig(&config);
}

const PDFiumPage& PDFiumTestBase::GetPDFiumPageForTest(
    const PDFiumEngine& engine,
    size_t page_index) {
  return GetPDFiumPageForTest(const_cast<PDFiumEngine&>(engine), page_index);
}

PDFiumPage& PDFiumTestBase::GetPDFiumPageForTest(PDFiumEngine& engine,
                                                 size_t page_index) {
  DCHECK_LT(page_index, engine.pages_.size());
  PDFiumPage* page = engine.pages_[page_index].get();
  DCHECK(page);
  return *page;
}

PDFiumTestBase::InitializeEngineResult::InitializeEngineResult() = default;

PDFiumTestBase::InitializeEngineResult::InitializeEngineResult(
    InitializeEngineResult&& other) noexcept = default;

PDFiumTestBase::InitializeEngineResult&
PDFiumTestBase::InitializeEngineResult::operator=(
    InitializeEngineResult&& other) noexcept = default;

PDFiumTestBase::InitializeEngineResult::~InitializeEngineResult() = default;

}  // namespace chrome_pdf
