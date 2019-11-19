// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PREVIEW_MODE_CLIENT_H_
#define PDF_PREVIEW_MODE_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "pdf/pdf_engine.h"

namespace chrome_pdf {

// The interface that's provided to the print preview rendering engine.
class PreviewModeClient : public PDFEngine::Client {
 public:
  class Client {
   public:
    virtual void PreviewDocumentLoadFailed() = 0;
    virtual void PreviewDocumentLoadComplete() = 0;
  };

  explicit PreviewModeClient(Client* client);
  ~PreviewModeClient() override {}

  // PDFEngine::Client implementation.
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  void Invalidate(const pp::Rect& rect) override;
  void DidScroll(const pp::Point& point) override;
  void ScrollToX(int x_in_screen_coords) override;
  void ScrollToY(int y_in_screen_coords, bool compensate_for_toolbar) override;
  void ScrollBy(const pp::Point& point) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override;
  void UpdateCursor(PP_CursorType_Dev cursor) override;
  void UpdateTickMarks(const std::vector<pp::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void GetDocumentPassword(
      pp::CompletionCallbackWithOutput<pp::Var> callback) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  std::string GetURL() override;
  void Email(const std::string& to,
             const std::string& cc,
             const std::string& bcc,
             const std::string& subject,
             const std::string& body) override;
  void Print() override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  pp::URLLoader CreateURLLoader() override;
  std::vector<SearchStringResult> SearchString(const base::char16* string,
                                               const base::char16* term,
                                               bool case_sensitive) override;
  void DocumentLoadComplete(
      const PDFEngine::DocumentFeatures& document_features) override;
  void DocumentLoadFailed() override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void FormTextFieldFocusChange(bool in_focus) override;
  bool IsPrintPreview() override;
  float GetToolbarHeightInScreenCoords() override;
  uint32_t GetBackgroundColor() override;

 private:
  Client* const client_;
};

}  // namespace chrome_pdf

#endif  // PDF_PREVIEW_MODE_CLIENT_H_
