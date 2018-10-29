// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_test_base.h"

#include <memory>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_document_loader.h"

namespace chrome_pdf {

namespace {

const base::FilePath::CharType* g_test_pdf_name;

std::unique_ptr<DocumentLoader> CreateTestDocumentLoader(
    DocumentLoader::Client* client) {
  return std::make_unique<TestDocumentLoader>(client, g_test_pdf_name);
}

}  // namespace

PDFiumTestBase::PDFiumTestBase() = default;

PDFiumTestBase::~PDFiumTestBase() = default;

void PDFiumTestBase::SetUp() {
  InitializePDFium();
}

void PDFiumTestBase::TearDown() {
  PDFiumEngine::SetCreateDocumentLoaderFunctionForTesting(nullptr);
  g_test_pdf_name = nullptr;
  FPDF_DestroyLibrary();
}

std::unique_ptr<PDFiumEngine> PDFiumTestBase::InitializeEngine(
    TestClient* client,
    const base::FilePath::CharType* pdf_name) {
  SetDocumentForTest(pdf_name);
  pp::URLLoader dummy_loader;
  auto engine = std::make_unique<PDFiumEngine>(client, true);
  if (!engine->New("https://chromium.org/dummy.pdf", "") ||
      !engine->HandleDocumentLoad(dummy_loader)) {
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

}  // namespace chrome_pdf
