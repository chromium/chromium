// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/preview_mode_client.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "pdf/buildflags.h"
#include "pdf/document_layout.h"
#include "pdf/loader/url_loader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace chrome_pdf {

PreviewModeClient::PreviewModeClient(Client* client) : client_(client) {}

PreviewModeClient::~PreviewModeClient() = default;

void PreviewModeClient::ProposeDocumentLayout(const DocumentLayout& layout) {
  // This will be invoked if the PreviewModeClient is used, which currently
  // occurs if and only if loading a non-PDF document with more than 1 page.
}

void PreviewModeClient::Invalidate(const gfx::Rect& rect) {
  NOTREACHED();
}

void PreviewModeClient::DidScroll(const gfx::Vector2d& point) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToX(int x_in_screen_coords) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToY(int y_in_screen_coords) {
  NOTREACHED();
}

void PreviewModeClient::ScrollBy(const gfx::Vector2d& scroll_delta) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToPage(int page) {
  NOTREACHED();
}

void PreviewModeClient::NavigateTo(const std::string& url,
                                   WindowOpenDisposition disposition) {
  NOTREACHED();
}

void PreviewModeClient::UpdateCursor(ui::mojom::CursorType cursor_type) {
  NOTREACHED();
}

void PreviewModeClient::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  NOTREACHED();
}

void PreviewModeClient::NotifyNumberOfFindResultsChanged(int total,
                                                         bool final_result) {
  NOTREACHED();
}

void PreviewModeClient::NotifySelectedFindResultChanged(int current_find_index,
                                                        bool final_result) {
  NOTREACHED();
}

void PreviewModeClient::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("");
}

void PreviewModeClient::Alert(const std::string& message) {
  NOTREACHED();
}

bool PreviewModeClient::Confirm(const std::string& message) {
  NOTREACHED();
}

std::string PreviewModeClient::Prompt(const std::string& question,
                                      const std::string& default_answer) {
  NOTREACHED();
}

std::string PreviewModeClient::GetURL() {
  NOTREACHED();
}

void PreviewModeClient::Email(const std::string& to,
                              const std::string& cc,
                              const std::string& bcc,
                              const std::string& subject,
                              const std::string& body) {
  NOTREACHED();
}

void PreviewModeClient::Print() {
  NOTREACHED();
}

void PreviewModeClient::SubmitForm(const std::string& url,
                                   const void* data,
                                   int length) {
  NOTREACHED();
}

std::unique_ptr<UrlLoader> PreviewModeClient::CreateUrlLoader() {
  NOTREACHED();
}

v8::Isolate* PreviewModeClient::GetIsolate() {
  NOTREACHED();
}

std::vector<PDFiumEngineClient::SearchStringResult>
PreviewModeClient::SearchString(const char16_t* string,
                                const char16_t* term,
                                bool case_sensitive) {
  NOTREACHED();
}

void PreviewModeClient::DocumentLoadComplete() {
  client_->PreviewDocumentLoadComplete();
}

void PreviewModeClient::DocumentLoadFailed() {
  client_->PreviewDocumentLoadFailed();
}

void PreviewModeClient::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  NOTREACHED();
}

void PreviewModeClient::FormFieldFocusChange(
    PDFiumEngineClient::FocusFieldType type) {
  NOTREACHED();
}

bool PreviewModeClient::IsPrintPreview() const {
  return true;
}

SkColor PreviewModeClient::GetBackgroundColor() const {
  NOTREACHED();
}

void PreviewModeClient::SetSelectedText(const std::string& selected_text) {
  NOTREACHED();
}

void PreviewModeClient::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  NOTREACHED();
}

bool PreviewModeClient::IsValidLink(const std::string& url) {
  NOTREACHED();
}

#if BUILDFLAG(ENABLE_PDF_INK2)
bool PreviewModeClient::IsInAnnotationMode() const {
  NOTREACHED();
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

}  // namespace chrome_pdf
