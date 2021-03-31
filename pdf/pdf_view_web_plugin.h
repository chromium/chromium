// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_WEB_PLUGIN_H_
#define PDF_PDF_VIEW_WEB_PLUGIN_H_

#include "base/memory/weak_ptr.h"
#include "cc/paint/paint_image.h"
#include "pdf/pdf_view_plugin_base.h"
#include "pdf/post_message_receiver.h"
#include "pdf/post_message_sender.h"
#include "pdf/ppapi_migration/graphics.h"
#include "pdf/ppapi_migration/url_loader.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "v8/include/v8.h"

namespace blink {
class WebPluginContainer;
}  // namespace blink

namespace chrome_pdf {

// Skeleton for a `blink::WebPlugin` to replace `OutOfProcessInstance`.
class PdfViewWebPlugin final : public PdfViewPluginBase,
                               public blink::WebPlugin,
                               public BlinkUrlLoader::Client,
                               public PostMessageReceiver::Client,
                               public SkiaGraphics::Client {
 public:
  explicit PdfViewWebPlugin(const blink::WebPluginParams& params);
  PdfViewWebPlugin(const PdfViewWebPlugin& other) = delete;
  PdfViewWebPlugin& operator=(const PdfViewWebPlugin& other) = delete;

  // blink::WebPlugin:
  bool Initialize(blink::WebPluginContainer* container) override;
  void Destroy() override;
  blink::WebPluginContainer* Container() const override;
  v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate) override;
  void UpdateAllLifecyclePhases(blink::DocumentUpdateReason reason) override;
  void Paint(cc::PaintCanvas* canvas, const gfx::Rect& rect) override;
  void UpdateGeometry(const gfx::Rect& window_rect,
                      const gfx::Rect& clip_rect,
                      const gfx::Rect& unobscured_rect,
                      bool is_visible) override;
  void UpdateFocus(bool focused, blink::mojom::FocusType focus_type) override;
  void UpdateVisibility(bool visibility) override;
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event,
      ui::Cursor* cursor) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidReceiveData(const char* data, size_t data_length) override;
  void DidFinishLoading() override;
  void DidFailLoading(const blink::WebURLError& error) override;

  // PdfViewPluginBase:
  void UpdateCursor(ui::mojom::CursorType cursor_type) override;
  void UpdateTickMarks(const std::vector<gfx::Rect>& tickmarks) override;
  void NotifyNumberOfFindResultsChanged(int total, bool final_result) override;
  void NotifySelectedFindResultChanged(int current_find_index) override;
  void Alert(const std::string& message) override;
  bool Confirm(const std::string& message) override;
  std::string Prompt(const std::string& question,
                     const std::string& default_answer) override;
  void Print() override;
  void SubmitForm(const std::string& url,
                  const void* data,
                  int length) override;
  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override;
  pp::Instance* GetPluginInstance() override;
  void DocumentHasUnsupportedFeature(const std::string& feature) override;
  bool IsPrintPreview() override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void EnteredEditMode() override;
  void SetSelectedText(const std::string& selected_text) override;
  void SetLinkUnderCursor(const std::string& link_under_cursor) override;
  bool IsValidLink(const std::string& url) override;
  std::unique_ptr<Graphics> CreatePaintGraphics(const gfx::Size& size) override;
  bool BindPaintGraphics(Graphics& graphics) override;
  void ScheduleTaskOnMainThread(const base::Location& from_here,
                                ResultCallback callback,
                                int32_t result,
                                base::TimeDelta delay) override;

  // BlinkUrlLoader::Client:
  bool IsValid() const override;
  blink::WebURL CompleteURL(const blink::WebString& partial_url) const override;
  net::SiteForCookies SiteForCookies() const override;
  void SetReferrerForRequest(blink::WebURLRequest& request,
                             const blink::WebURL& referrer_url) override;
  std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) override;

  // PostMessageReceiver::Client:
  void OnMessage(const base::Value& message) override;

  // SkiaGraphics::Client:
  void UpdateSnapshot(sk_sp<SkImage> snapshot) override;

 protected:
  // PdfViewPluginBase:
  base::WeakPtr<PdfViewPluginBase> GetWeakPtr() override;
  std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() override;
  void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result) override;
  void DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                      int32_t result) override;
  void SendMessage(base::Value message) override;
  void InitImageData(const gfx::Size& size) override;
  void SetFormFieldInFocus(bool in_focus) override;
  void SetAccessibilityDocInfo(const AccessibilityDocInfo& doc_info) override;
  void SetAccessibilityPageInfo(AccessibilityPageInfo page_info,
                                std::vector<AccessibilityTextRunInfo> text_runs,
                                std::vector<AccessibilityCharInfo> chars,
                                AccessibilityPageObjects page_objects) override;
  void SetAccessibilityViewportInfo(
      const AccessibilityViewportInfo& viewport_info) override;
  void SetContentRestrictions(int content_restrictions) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void OnPrintPreviewLoaded() override;
  void UserMetricsRecordAction(const std::string& action) override;

 private:
  // Call `Destroy()` instead.
  ~PdfViewWebPlugin() override;

  void OnViewportChanged(const gfx::Rect& view_rect, float new_device_scale);

  // Invalidates the entire web plugin container and schedules a paint of the
  // page in it.
  void InvalidatePluginContainer();

  // Schedules a paint of the page of a given region in the web plugin
  // container. The coordinates are relative to the top-left of the container.
  void InvalidateRectInPluginContainer(const gfx::Rect& rect);

  blink::WebPluginParams initial_params_;
  blink::WebPluginContainer* container_ = nullptr;

  v8::Persistent<v8::Object> scriptable_receiver_;
  PostMessageSender post_message_sender_;

  cc::PaintImage snapshot_;

  base::WeakPtrFactory<PdfViewWebPlugin> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_WEB_PLUGIN_H_
