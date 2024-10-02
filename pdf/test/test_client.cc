// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_client.h"

#include <memory>

#include "base/time/time.h"
#include "pdf/buildflags.h"
#include "pdf/document_layout.h"
#include "pdf/loader/url_loader.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/test/test_helpers.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chrome_pdf {

TestClient::TestClient() = default;

TestClient::~TestClient() = default;

void TestClient::ProposeDocumentLayout(const DocumentLayout& layout) {
  // Most tests will want to accept the proposed layout immediately: Applying
  // layout asynchronously is more accurate, but in most cases, doing so adds
  // complexity without much gain. Instead, we can override this behavior just
  // where it matters (like PDFiumEngineTest.ProposeDocumentLayoutWithOverlap).
  engine()->ApplyDocumentLayout(layout.options());
}

bool TestClient::Confirm(const std::string& message) {
  return false;
}

std::string TestClient::Prompt(const std::string& question,
                               const std::string& default_answer) {
  return std::string();
}

std::string TestClient::GetURL() {
  return std::string();
}

std::unique_ptr<UrlLoader> TestClient::CreateUrlLoader() {
  return nullptr;
}

v8::Isolate* TestClient::GetIsolate() {
  return GetBlinkIsolate();
}

std::vector<PDFiumEngineClient::SearchStringResult> TestClient::SearchString(
    const char16_t* string,
    const char16_t* term,
    bool case_sensitive) {
  return std::vector<SearchStringResult>();
}

bool TestClient::IsPrintPreview() const {
  return false;
}

SkColor TestClient::GetBackgroundColor() const {
  return SK_ColorTRANSPARENT;
}

void TestClient::SetSelectedText(const std::string& selected_text) {}

void TestClient::SetLinkUnderCursor(const std::string& link_under_cursor) {}

bool TestClient::IsValidLink(const std::string& url) {
  return !url.empty();
}

#if BUILDFLAG(ENABLE_PDF_INK2)
bool TestClient::IsInAnnotationMode() const {
  return false;
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

}  // namespace chrome_pdf
