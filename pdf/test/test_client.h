// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_CLIENT_H_
#define PDF_TEST_TEST_CLIENT_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "pdf/pdf_engine.h"

namespace chrome_pdf {

class TestClient : public PDFEngine::Client {
 public:
  TestClient();

  TestClient(const TestClient& other) = delete;
  TestClient& operator=(const TestClient& other) = delete;

  ~TestClient() override;

  PDFEngine* engine() const { return engine_; }
  void set_engine(PDFEngine* engine) { engine_ = engine; }

  // PDFEngine::Client:
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::string GetURL() override;
  std::unique_ptr<UrlLoader> CreateUrlLoader() override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  bool IsPrintPreview() const override;
  SkColor GetBackgroundColor() const override;
  void SetSelectedText(const std::string& selected_text) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;
  bool IsValidLink(const std::string& url) override;

 private:
  // Not owned. Expected to dangle briefly, as the engine usually is destroyed
  // before the client.
  raw_ptr<PDFEngine, DisableDanglingPtrDetection> engine_ = nullptr;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_CLIENT_H_
