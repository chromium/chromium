// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_PLUGIN_BASE_H_
#define PDF_PDF_VIEW_PLUGIN_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "pdf/accessibility_structs.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace chrome_pdf {

class PDFiumEngine;
struct AccessibilityCharInfo;
struct AccessibilityDocInfo;
struct AccessibilityPageInfo;
struct AccessibilityPageObjects;
struct AccessibilityTextRunInfo;
struct AccessibilityViewportInfo;

// TODO(crbug.com/1302059): Merge with PdfViewWebPlugin.
class PdfViewPluginBase : public PDFEngine::Client,
                          public PaintManager::Client {
 public:
  // Do not save files with over 100 MB. This cap should be kept in sync with
  // and is also enforced in chrome/browser/resources/pdf/pdf_viewer.js.
  static constexpr size_t kMaximumSavedFileSize = 100 * 1000 * 1000;

  enum class AccessibilityState {
    kOff = 0,  // Off.
    kPending,  // Enabled but waiting for doc to load.
    kLoaded,   // Fully loaded.
  };

  enum class DocumentLoadState {
    kLoading = 0,
    kComplete,
    kFailed,
  };

  PdfViewPluginBase(const PdfViewPluginBase& other) = delete;
  PdfViewPluginBase& operator=(const PdfViewPluginBase& other) = delete;

  // PDFEngine::Client:
  void DocumentLoadComplete() override;
  void DocumentLoadFailed() override;
  void DocumentLoadProgress(uint32_t available, uint32_t doc_size) override;
  void FormFieldFocusChange(PDFEngine::FocusFieldType type) override;
  void SetIsSelecting(bool is_selecting) override;
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;
  void DocumentFocusChanged(bool document_has_focus) override;

  // Gets the content restrictions based on the permissions which `engine_` has.
  int GetContentRestrictions() const;

  // Gets the accessibility doc info based on the information from `engine_`.
  AccessibilityDocInfo GetAccessibilityDocInfo() const;

  DocumentLoadState document_load_state_for_testing() const {
    return document_load_state_;
  }

 protected:
  PdfViewPluginBase();
  ~PdfViewPluginBase() override;

  // Creates a new `PDFiumEngine`.
  virtual std::unique_ptr<PDFiumEngine> CreateEngine(
      PDFEngine::Client* client,
      PDFiumFormFiller::ScriptOption script_option) = 0;

  virtual const PDFiumEngine* engine() const = 0;
  virtual PDFiumEngine* engine() = 0;

  // Gets a weak pointer with a lifetime matching the derived class.
  virtual base::WeakPtr<PdfViewPluginBase> GetWeakPtr() = 0;

  // Runs when document load completes in Print Preview, before
  // `OnDocumentLoadComplete()`.
  virtual void OnPrintPreviewLoaded() = 0;

  // Runs when document load completes.
  virtual void OnDocumentLoadComplete() = 0;

  // Enqueues a "message" event carrying `message` to the embedder. Messages are
  // guaranteed to be received in the order that they are sent. This method is
  // non-blocking.
  virtual void SendMessage(base::Value::Dict message) = 0;

  // Sends the loading progress, where `percentage` represents the progress, or
  // -1 for loading error.
  void SendLoadingProgress(double percentage);

  // Sets the text input type for this plugin based on `in_focus`.
  virtual void SetFormTextFieldInFocus(bool in_focus) = 0;

  // Sets the accessibility information about the PDF document in the renderer.
  virtual void SetAccessibilityDocInfo(AccessibilityDocInfo doc_info) = 0;

  // Sets the accessibility information about the given `page_index` in the
  // renderer.
  void PrepareAndSetAccessibilityPageInfo(int32_t page_index);

  // Sets the accessibility information about the page in the renderer.
  virtual void SetAccessibilityPageInfo(
      AccessibilityPageInfo page_info,
      std::vector<AccessibilityTextRunInfo> text_runs,
      std::vector<AccessibilityCharInfo> chars,
      AccessibilityPageObjects page_objects) = 0;

  // Prepares the accessibility information about the current viewport. Calls
  // SetAccessibilityViewportInfo() internally to set this information in the
  // renderer. This is done once when accessibility is first loaded and again
  // when the geometry changes.
  void PrepareAndSetAccessibilityViewportInfo();

  // Sets the accessibility information about the current viewport in the
  // renderer.
  virtual void SetAccessibilityViewportInfo(
      AccessibilityViewportInfo viewport_info) = 0;

  // Prints the pages specified by `page_numbers` using the parameters passed to
  // `PrintBegin()` Returns a vector of bytes containing the printed output. An
  // empty returned value indicates failure.
  std::vector<uint8_t> PrintPages(const std::vector<int>& page_numbers);

  // Disables browser commands because of restrictions on how the data is to be
  // used (i.e. can't copy/print). `content_restrictions` should have its bits
  // set by `chrome_pdf::ContentRestriction` enum values.
  virtual void SetContentRestrictions(int content_restrictions) = 0;

  // Sends start/stop loading notifications to the plugin's render frame.
  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Notifies the embedder of the top-left and bottom-right coordinates of the
  // current selection.
  virtual void NotifySelectionChanged(const gfx::PointF& left,
                                      int left_height,
                                      const gfx::PointF& right,
                                      int right_height) = 0;

  // Records user actions.
  virtual void UserMetricsRecordAction(const std::string& action) = 0;

  PaintManager& paint_manager() { return paint_manager_; }

  virtual bool full_frame() const = 0;

  const gfx::Rect& available_area() const { return available_area_; }
  gfx::Rect& mutable_available_area() { return available_area_; }

  // TODO(crbug.com/1288847): Don't provide direct access to the origin of
  // `plugin_rect_`, as this exposes the unintuitive "paint offset."
  virtual const gfx::Rect& plugin_rect() const = 0;

  double zoom() const { return zoom_; }
  void set_zoom(double zoom) { zoom_ = zoom; }

  virtual float device_scale() const = 0;

  double last_progress_sent() const { return last_progress_sent_; }
  void set_last_progress_sent(double progress) {
    last_progress_sent_ = progress;
  }

  DocumentLoadState document_load_state() const { return document_load_state_; }
  void set_document_load_state(DocumentLoadState state) {
    document_load_state_ = state;
  }

  AccessibilityState accessibility_state() const {
    return accessibility_state_;
  }

  void set_accessibility_state(AccessibilityState state) {
    accessibility_state_ = state;
  }

  static constexpr bool IsSaveDataSizeValid(size_t size) {
    return size > 0 && size <= kMaximumSavedFileSize;
  }

  // Starts loading accessibility information.
  void LoadAccessibility();

 private:
  // TODO(crbug.com/1302059): `PdfViewPluginBase` is being merged into
  // `PdfViewWebPlugin`, so all methods should be protected or public.

  PaintManager paint_manager_{this};

  // Remaining area, in pixels, to render the pdf in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;

  // Current zoom factor.
  double zoom_ = 1.0;

  // The last document load progress value sent to the web page.
  double last_progress_sent_ = 0.0;

  // The current state of document load.
  DocumentLoadState document_load_state_ = DocumentLoadState::kLoading;

  // The current state of accessibility.
  AccessibilityState accessibility_state_ = AccessibilityState::kOff;

  // The next accessibility page index, used to track interprocess calls when
  // reconstructing the tree for new document layouts.
  int32_t next_accessibility_page_index_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
