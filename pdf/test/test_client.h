// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_CLIENT_H_
#define PDF_TEST_TEST_CLIENT_H_

#include <string>
#include <vector>

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
  pp::URLLoader CreateURLLoader() override;
  std::vector<SearchStringResult> SearchString(const base::char16* string,
                                               const base::char16* term,
                                               bool case_sensitive) override;
  pp::Instance* GetPluginInstance() override;
  bool IsPrintPreview() override;
  uint32_t GetBackgroundColor() override;
  float GetToolbarHeightInScreenCoords() override;

 private:
  // Not owned. Expected to dangle briefly, as the engine usually is destroyed
  // before the client.
  PDFEngine* engine_ = nullptr;
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_CLIENT_H_
