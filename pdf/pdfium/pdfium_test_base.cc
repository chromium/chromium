// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_test_base.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "pdf/loader/url_loader.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/environment.h"
#endif

namespace chrome_pdf {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
base::FilePath GetTestFontsDir() {
  // base::TestSuite::Initialize() should have already set this.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string fontconfig_sysroot;
  CHECK(env->GetVar("FONTCONFIG_SYSROOT", &fontconfig_sysroot));
  return base::FilePath(fontconfig_sysroot).AppendASCII("test_fonts");
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace

PDFiumTestBase::PDFiumTestBase() = default;

PDFiumTestBase::~PDFiumTestBase() = default;

// static
bool PDFiumTestBase::UsingTestFonts() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

void PDFiumTestBase::SetUp() {
  InitializePDFiumSDK();
}

void PDFiumTestBase::TearDown() {
  FPDF_DestroyLibrary();
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::InitializeEngine(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  InitializeEngineResult result =
      InitializeEngineWithoutLoading(client, pdf_name);
  SimulateLoading(result.engine.get(), result.document_loader);
  return std::move(result.engine);
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::InitializeEngineFromData(
    TestClient* client,
    std::vector<uint8_t> pdf_data) {
  auto engine = CreateEngine(client);
  auto document_loader =
      std::make_unique<TestDocumentLoader>(engine.get(), std::move(pdf_data));
  InitializeEngineResult result = InitializeEngineWithoutLoadingImpl(
      client, std::move(engine), std::move(document_loader));
  SimulateLoading(result.engine.get(), result.document_loader);
  return std::move(result.engine);
}

PDFiumTestBase::InitializeEngineResult
PDFiumTestBase::InitializeEngineWithoutLoading(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  auto engine = CreateEngine(client);
  auto document_loader =
      std::make_unique<TestDocumentLoader>(engine.get(), pdf_name);
  return InitializeEngineWithoutLoadingImpl(client, std::move(engine),
                                            std::move(document_loader));
}

PDFiumTestBase::InitializeEngineResult
PDFiumTestBase::InitializeEngineWithoutLoadingImpl(
    TestClient* client,
    std::unique_ptr<PDFiumEngine> engine,
    std::unique_ptr<TestDocumentLoader> document_loader) {
  InitializeEngineResult result;

  result.engine = std::move(engine);
  client->set_engine(result.engine.get());

  result.document_loader = document_loader.get();
  result.engine->SetDocumentLoaderForTesting(std::move(document_loader));

  if (!result.engine->HandleDocumentLoad(nullptr,
                                         "https://chromium.org/dummy.pdf")) {
    client->set_engine(nullptr);
    result.engine = nullptr;
    result.document_loader = nullptr;
  }
  return result;
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::CreateEngine(TestClient* client) {
  return std::make_unique<PDFiumEngine>(
      client, PDFiumFormFiller::ScriptOption::kNoJavaScript);
}

void PDFiumTestBase::SimulateLoading(PDFiumEngine* engine,
                                     TestDocumentLoader* document_loader) {
  if (!engine) {
    return;
  }

  // Simulate initializing plugin geometry.
  engine->PluginSizeUpdated({});

  CHECK(document_loader);

  // Incrementally read the PDF. To detect linearized PDFs, the first read
  // should be at least 1024 bytes.
  while (document_loader->SimulateLoadData(1024)) {
    continue;
  }
}

void PDFiumTestBase::InitializePDFiumSDK() {
  font_paths_.clear();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  test_fonts_path_ = GetTestFontsDir();
  font_paths_.push_back(test_fonts_path_.value().c_str());
  // When non-empty, `font_paths_` has to be terminated with a nullptr.
  font_paths_.push_back(nullptr);
#endif

  FPDF_LIBRARY_CONFIG config;
  config.version = 4;
  config.m_pUserFontPaths = font_paths_.data();
  config.m_pIsolate = nullptr;
  config.m_v8EmbedderSlot = 0;
  config.m_pPlatform = nullptr;
  config.m_RendererType =
      GetParam() ? FPDF_RENDERERTYPE_SKIA : FPDF_RENDERERTYPE_AGG;
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

void PDFiumTestBase::InitializeEngineResult::FinishLoading() {
  ASSERT_TRUE(document_loader);
  while (document_loader->SimulateLoadData(UINT32_MAX))
    continue;
}

}  // namespace chrome_pdf
