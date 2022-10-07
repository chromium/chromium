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
class PdfViewPluginBase : public PDFEngine::Client {
 public:
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
  void SelectionChanged(const gfx::Rect& left, const gfx::Rect& right) override;

 protected:
  PdfViewPluginBase();
  ~PdfViewPluginBase() override;

  virtual const PDFiumEngine* engine() const = 0;
  virtual PDFiumEngine* engine() = 0;

  // Gets a weak pointer with a lifetime matching the derived class.
  virtual base::WeakPtr<PdfViewPluginBase> GetWeakPtr() = 0;

  // Runs when document load completes in Print Preview, before
  // `OnDocumentLoadComplete()`.
  virtual void OnPrintPreviewLoaded() = 0;

  // Runs when document load completes.
  virtual void OnDocumentLoadComplete() = 0;

  // Sends the loading progress, where `percentage` represents the progress, or
  // -1 for loading error.
  virtual void SendLoadingProgress(double percentage) = 0;

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

  virtual PaintManager& paint_manager() = 0;
  virtual const gfx::Rect& available_area() const = 0;
  virtual double zoom() const = 0;
  virtual bool full_frame() const = 0;

  // TODO(crbug.com/1288847): Don't provide direct access to the origin of
  // `plugin_rect_`, as this exposes the unintuitive "paint offset."
  virtual const gfx::Rect& plugin_rect() const = 0;

  virtual float device_scale() const = 0;
  virtual DocumentLoadState document_load_state() const = 0;
  virtual void set_document_load_state(DocumentLoadState state) = 0;
  virtual AccessibilityState accessibility_state() const = 0;
  virtual void set_accessibility_state(AccessibilityState state) = 0;
  virtual int32_t next_accessibility_page_index() const = 0;
  virtual void increment_next_accessibility_page_index() = 0;
  virtual void reset_next_accessibility_page_index() = 0;

  // Starts loading accessibility information.
  void LoadAccessibility();

  // Gets the content restrictions based on the permissions which `engine_` has.
  int GetContentRestrictions() const;

  // Gets the accessibility doc info based on the information from `engine_`.
  AccessibilityDocInfo GetAccessibilityDocInfo() const;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
