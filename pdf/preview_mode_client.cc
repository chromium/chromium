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
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::DidScroll(const gfx::Vector2d& point) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::ScrollToX(int x_in_screen_coords) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::ScrollToY(int y_in_screen_coords) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::ScrollBy(const gfx::Vector2d& scroll_delta) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::ScrollToPage(int page) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::NavigateTo(const std::string& url,
                                   WindowOpenDisposition disposition) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::UpdateCursor(ui::mojom::CursorType cursor_type) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::NotifyNumberOfFindResultsChanged(int total,
                                                         bool final_result) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::NotifySelectedFindResultChanged(int current_find_index,
                                                        bool final_result) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("");
}

void PreviewModeClient::Alert(const std::string& message) {
  NOTREACHED_IN_MIGRATION();
}

bool PreviewModeClient::Confirm(const std::string& message) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

std::string PreviewModeClient::Prompt(const std::string& question,
                                      const std::string& default_answer) {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string PreviewModeClient::GetURL() {
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

void PreviewModeClient::Email(const std::string& to,
                              const std::string& cc,
                              const std::string& bcc,
                              const std::string& subject,
                              const std::string& body) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::Print() {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::SubmitForm(const std::string& url,
                                   const void* data,
                                   int length) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<UrlLoader> PreviewModeClient::CreateUrlLoader() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

v8::Isolate* PreviewModeClient::GetIsolate() {
  NOTREACHED_NORETURN();
}

std::vector<PDFiumEngineClient::SearchStringResult>
PreviewModeClient::SearchString(const char16_t* string,
                                const char16_t* term,
                                bool case_sensitive) {
  NOTREACHED_IN_MIGRATION();
  return std::vector<SearchStringResult>();
}

void PreviewModeClient::DocumentLoadComplete() {
  client_->PreviewDocumentLoadComplete();
}

void PreviewModeClient::DocumentLoadFailed() {
  client_->PreviewDocumentLoadFailed();
}

void PreviewModeClient::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::FormFieldFocusChange(
    PDFiumEngineClient::FocusFieldType type) {
  NOTREACHED_IN_MIGRATION();
}

bool PreviewModeClient::IsPrintPreview() const {
  return true;
}

SkColor PreviewModeClient::GetBackgroundColor() const {
  NOTREACHED_IN_MIGRATION();
  return SK_ColorTRANSPARENT;
}

void PreviewModeClient::SetSelectedText(const std::string& selected_text) {
  NOTREACHED_IN_MIGRATION();
}

void PreviewModeClient::SetLinkUnderCursor(
    const std::string& link_under_cursor) {
  NOTREACHED_IN_MIGRATION();
}

bool PreviewModeClient::IsValidLink(const std::string& url) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace chrome_pdf
