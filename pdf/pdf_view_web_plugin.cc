// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_canvas.h"
#include "pdf/pdf_engine.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "ppapi/cpp/url_loader.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "ui/base/cursor/cursor.h"

namespace chrome_pdf {

PdfViewWebPlugin::PdfViewWebPlugin(const blink::WebPluginParams& params) {}

PdfViewWebPlugin::~PdfViewWebPlugin() {
  // Explicitly destroy the PDFEngine during destruction as it may call back
  // into this object.
  DestroyEngine();
}

bool PdfViewWebPlugin::Initialize(blink::WebPluginContainer* container) {
  DCHECK_EQ(container->Plugin(), this);
  container_ = container;
  InitializeEngine(/*enable_javascript=*/false);
  return true;
}

void PdfViewWebPlugin::Destroy() {
  container_ = nullptr;
  delete this;
}

blink::WebPluginContainer* PdfViewWebPlugin::Container() const {
  return container_;
}

void PdfViewWebPlugin::UpdateAllLifecyclePhases(
    blink::DocumentUpdateReason reason) {}

void PdfViewWebPlugin::Paint(cc::PaintCanvas* canvas,
                             const blink::WebRect& rect) {}

void PdfViewWebPlugin::UpdateGeometry(const blink::WebRect& window_rect,
                                      const blink::WebRect& clip_rect,
                                      const blink::WebRect& unobscured_rect,
                                      bool is_visible) {}

void PdfViewWebPlugin::UpdateFocus(bool focused,
                                   blink::mojom::FocusType focus_type) {}

void PdfViewWebPlugin::UpdateVisibility(bool visibility) {}

blink::WebInputEventResult PdfViewWebPlugin::HandleInputEvent(
    const blink::WebCoalescedInputEvent& event,
    ui::Cursor* cursor) {
  return blink::WebInputEventResult::kNotHandled;
}

void PdfViewWebPlugin::DidReceiveResponse(
    const blink::WebURLResponse& response) {}

void PdfViewWebPlugin::DidReceiveData(const char* data, size_t data_length) {}

void PdfViewWebPlugin::DidFinishLoading() {}

void PdfViewWebPlugin::DidFailLoading(const blink::WebURLError& error) {}

void PdfViewWebPlugin::ProposeDocumentLayout(const DocumentLayout& layout) {}

void PdfViewWebPlugin::Invalidate(const pp::Rect& rect) {}

void PdfViewWebPlugin::DidScroll(const gfx::Vector2d& offset) {}

void PdfViewWebPlugin::ScrollToX(int x_in_screen_coords) {}

void PdfViewWebPlugin::ScrollToY(int y_in_screen_coords,
                                 bool compensate_for_toolbar) {}

void PdfViewWebPlugin::ScrollBy(const gfx::Vector2d& scroll_delta) {}

void PdfViewWebPlugin::ScrollToPage(int page) {}

void PdfViewWebPlugin::NavigateTo(const std::string& url,
                                  WindowOpenDisposition disposition) {}

void PdfViewWebPlugin::NavigateToDestination(int page,
                                             const float* x,
                                             const float* y,
                                             const float* zoom) {}

void PdfViewWebPlugin::UpdateCursor(PP_CursorType_Dev cursor) {}

void PdfViewWebPlugin::UpdateTickMarks(
    const std::vector<gfx::Rect>& tickmarks) {}

void PdfViewWebPlugin::NotifyNumberOfFindResultsChanged(int total,
                                                        bool final_result) {}

void PdfViewWebPlugin::NotifySelectedFindResultChanged(int current_find_index) {
}

void PdfViewWebPlugin::NotifyTouchSelectionOccurred() {}

void PdfViewWebPlugin::GetDocumentPassword(
    base::OnceCallback<void(const std::string&)> callback) {}

void PdfViewWebPlugin::Beep() {}

void PdfViewWebPlugin::Alert(const std::string& message) {}

bool PdfViewWebPlugin::Confirm(const std::string& message) {
  return false;
}

std::string PdfViewWebPlugin::Prompt(const std::string& question,
                                     const std::string& default_answer) {
  return "";
}

std::string PdfViewWebPlugin::GetURL() {
  return "";
}

void PdfViewWebPlugin::Email(const std::string& to,
                             const std::string& cc,
                             const std::string& bcc,
                             const std::string& subject,
                             const std::string& body) {}

void PdfViewWebPlugin::Print() {}

void PdfViewWebPlugin::SubmitForm(const std::string& url,
                                  const void* data,
                                  int length) {}

scoped_refptr<UrlLoader> PdfViewWebPlugin::CreateUrlLoader() {
  return base::MakeRefCounted<UrlLoader>();
}

std::vector<PDFEngine::Client::SearchStringResult>
PdfViewWebPlugin::SearchString(const base::char16* string,
                               const base::char16* term,
                               bool case_sensitive) {
  return {};
}

void PdfViewWebPlugin::DocumentLoadComplete(
    const PDFEngine::DocumentFeatures& document_features) {}

void PdfViewWebPlugin::DocumentLoadFailed() {}

pp::Instance* PdfViewWebPlugin::GetPluginInstance() {
  return nullptr;
}

void PdfViewWebPlugin::DocumentHasUnsupportedFeature(
    const std::string& feature) {}

void PdfViewWebPlugin::DocumentLoadProgress(uint32_t available,
                                            uint32_t doc_size) {}

void PdfViewWebPlugin::FormTextFieldFocusChange(bool in_focus) {}

bool PdfViewWebPlugin::IsPrintPreview() {
  return false;
}

uint32_t PdfViewWebPlugin::GetBackgroundColor() {
  return 0;
}

void PdfViewWebPlugin::IsSelectingChanged(bool is_selecting) {}

void PdfViewWebPlugin::SelectionChanged(const pp::Rect& left,
                                        const pp::Rect& right) {}

void PdfViewWebPlugin::EnteredEditMode() {}

float PdfViewWebPlugin::GetToolbarHeightInScreenCoords() {
  return 0;
}

void PdfViewWebPlugin::DocumentFocusChanged(bool document_has_focus) {}

}  // namespace chrome_pdf
