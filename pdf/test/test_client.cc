// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_client.h"

#include <memory>

#include "base/location.h"
#include "base/time/time.h"
#include "pdf/document_layout.h"
#include "pdf/ppapi_migration/url_loader.h"

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

std::vector<PDFEngine::Client::SearchStringResult> TestClient::SearchString(
    const base::char16* string,
    const base::char16* term,
    bool case_sensitive) {
  return std::vector<SearchStringResult>();
}

pp::Instance* TestClient::GetPluginInstance() {
  return nullptr;
}

bool TestClient::IsPrintPreview() {
  return false;
}

uint32_t TestClient::GetBackgroundColor() {
  return 0;
}

float TestClient::GetToolbarHeightInScreenCoords() {
  return 0;
}

void TestClient::SetSelectedText(const std::string& selected_text) {}

void TestClient::SetLinkUnderCursor(const std::string& link_under_cursor) {}

bool TestClient::IsValidLink(const std::string& url) {
  return !url.empty();
}

void TestClient::ScheduleTaskOnMainThread(base::TimeDelta delay,
                                          ResultCallback callback,
                                          int32_t result,
                                          const base::Location& from_here) {}

}  // namespace chrome_pdf
