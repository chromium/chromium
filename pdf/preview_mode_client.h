// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PREVIEW_MODE_CLIENT_H_
#define PDF_PREVIEW_MODE_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "pdf/buildflags.h"
#include "pdf/pdfium/pdfium_engine_client.h"

namespace chrome_pdf {

// The interface that's provided to the print preview rendering engine.
class PreviewModeClient : public PDFiumEngineClient {
 public:
  class Client {
   public:
    virtual void PreviewDocumentLoadFailed() = 0;
    virtual void PreviewDocumentLoadComplete() = 0;
  };

  explicit PreviewModeClient(Client* client);
  ~PreviewModeClient() override;

  // PDFiumEngineClient:
  void ProposeDocumentLayout(const DocumentLayout& layout) override;
  void Invalidate(const gfx::Rect& rect) override;
  void DidScroll(const gfx::Vector2d& offset) override;
  void ScrollToX(int x_in_screen_coords) override;
  void ScrollToY(int y_in_screen_coords) override;
  void ScrollBy(const gfx::Vector2d& scroll_delta) override;
  void ScrollToPage(int page) override;
  void NavigateTo(const std::string& url,
                  WindowOpenDisposition disposition) override;
  void UpdateCursor(ui::mojom::CursorType cursor_type) override;
  void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index,
                                       bool final_result) override;
  void GetDocumentPassword(
      base::OnceCallback<void(const std::string&)> callback) override;
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
  std::unique_ptr<UrlLoader> CreateUrlLoader() override;
  v8::Isolate* GetIsolate() override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  void DocumentLoadComplete() override;
  void DocumentLoadFailed() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  void FormFieldFocusChange(FocusFieldType type) override;
  bool IsPrintPreview() const override;
  SkColor GetBackgroundColor() const override;
  void SetSelectedText(const std::string& selected_text) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;
  bool IsValidLink(const std::string& url) override;
#if BUILDFLAG(ENABLE_PDF_INK2)
  bool IsInAnnotationMode() const override;
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

 private:
  const raw_ptr<Client> client_;
};

}  // namespace chrome_pdf

#endif  // PDF_PREVIEW_MODE_CLIENT_H_
