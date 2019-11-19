// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_test_base.h"

#include <memory>

#include "build/build_config.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"

#if defined(OS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace chrome_pdf {

namespace {

const base::FilePath::CharType* g_test_pdf_name;

std::unique_ptr<DocumentLoader> CreateTestDocumentLoader(
    DocumentLoader::Client* client) {
  return std::make_unique<TestDocumentLoader>(client, g_test_pdf_name);
}

bool IsValidLinkForTesting(const std::string& url) {
  return !url.empty();
}

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
  PDFiumPage::SetIsValidLinkFunctionForTesting(&IsValidLinkForTesting);
}

void PDFiumTestBase::TearDown() {
  PDFiumEngine::SetCreateDocumentLoaderFunctionForTesting(nullptr);
  PDFiumPage::SetIsValidLinkFunctionForTesting(nullptr);
  g_test_pdf_name = nullptr;
  FPDF_DestroyLibrary();
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::InitializeEngine(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  SetDocumentForTest(pdf_name);
  pp::URLLoader dummy_loader;
  auto engine =
      std::make_unique<PDFiumEngine>(client, /*enable_javascript=*/false);
  client->set_engine(engine.get());
  if (!engine->New("https://chromium.org/dummy.pdf", "") ||
      !engine->HandleDocumentLoad(dummy_loader)) {
    client->set_engine(nullptr);
    return nullptr;
  }
  return engine;
}

void PDFiumTestBase::SetDocumentForTest(
    const base::FilePath::CharType* pdf_name) {
  DCHECK(!g_test_pdf_name);
  g_test_pdf_name = pdf_name;
  PDFiumEngine::SetCreateDocumentLoaderFunctionForTesting(
      &CreateTestDocumentLoader);
}

void PDFiumTestBase::InitializePDFium() {
  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = nullptr;
  config.m_v8EmbedderSlot = 0;
  FPDF_InitLibraryWithConfig(&config);
}

PDFiumPage* PDFiumTestBase::GetPDFiumPageForTest(PDFiumEngine* engine,
                                                 size_t page_index) {
  if (engine && page_index < engine->pages_.size())
    return engine->pages_[page_index].get();
  return nullptr;
}

}  // namespace chrome_pdf
