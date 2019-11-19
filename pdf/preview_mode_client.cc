// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/preview_mode_client.h"

#include <stdint.h>

#include "base/logging.h"
#include "pdf/document_layout.h"

namespace chrome_pdf {

PreviewModeClient::PreviewModeClient(Client* client) : client_(client) {}

void PreviewModeClient::ProposeDocumentLayout(const DocumentLayout& layout) {
  // This will be invoked if the PreviewModeClient is used, which currently
  // occurs if and only if loading a non-PDF document with more than 1 page.
}

void PreviewModeClient::Invalidate(const pp::Rect& rect) {
  NOTREACHED();
}

void PreviewModeClient::DidScroll(const pp::Point& point) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToX(int x_in_screen_coords) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToY(int y_in_screen_coords,
                                  bool compensate_for_toolbar) {
  NOTREACHED();
}

void PreviewModeClient::ScrollBy(const pp::Point& point) {
  NOTREACHED();
}

void PreviewModeClient::ScrollToPage(int page) {
  NOTREACHED();
}

void PreviewModeClient::NavigateTo(const std::string& url,
                                   WindowOpenDisposition disposition) {
  NOTREACHED();
}

void PreviewModeClient::UpdateCursor(PP_CursorType_Dev cursor) {
  NOTREACHED();
}

void PreviewModeClient::UpdateTickMarks(
    const std::vector<pp::Rect>& tickmarks) {
  NOTREACHED();
}

void PreviewModeClient::NotifyNumberOfFindResultsChanged(int total,
                                                         bool final_result) {
  NOTREACHED();
}

void PreviewModeClient::NotifySelectedFindResultChanged(
    int current_find_index) {
  NOTREACHED();
}

void PreviewModeClient::GetDocumentPassword(
    pp::CompletionCallbackWithOutput<pp::Var> callback) {
  callback.Run(PP_ERROR_FAILED);
}

void PreviewModeClient::Alert(const std::string& message) {
  NOTREACHED();
}

bool PreviewModeClient::Confirm(const std::string& message) {
  NOTREACHED();
  return false;
}

std::string PreviewModeClient::Prompt(const std::string& question,
                                      const std::string& default_answer) {
  NOTREACHED();
  return std::string();
}

std::string PreviewModeClient::GetURL() {
  NOTREACHED();
  return std::string();
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

pp::URLLoader PreviewModeClient::CreateURLLoader() {
  NOTREACHED();
  return pp::URLLoader();
}

std::vector<PDFEngine::Client::SearchStringResult>
PreviewModeClient::SearchString(const base::char16* string,
                                const base::char16* term,
                                bool case_sensitive) {
  NOTREACHED();
  return std::vector<SearchStringResult>();
}

void PreviewModeClient::DocumentLoadComplete(
    const PDFEngine::DocumentFeatures& document_features) {
  client_->PreviewDocumentLoadComplete();
}

void PreviewModeClient::DocumentLoadFailed() {
  client_->PreviewDocumentLoadFailed();
}

pp::Instance* PreviewModeClient::GetPluginInstance() {
  return nullptr;
}

void PreviewModeClient::DocumentHasUnsupportedFeature(
    const std::string& feature) {
  NOTREACHED();
}

void PreviewModeClient::FormTextFieldFocusChange(bool in_focus) {
  NOTREACHED();
}

bool PreviewModeClient::IsPrintPreview() {
  NOTREACHED();
  return false;
}

float PreviewModeClient::GetToolbarHeightInScreenCoords() {
  return 0.0f;
}

uint32_t PreviewModeClient::GetBackgroundColor() {
  NOTREACHED();
  return 0;
}

}  // namespace chrome_pdf
