// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_PLUGIN_BASE_H_
#define PDF_PDF_VIEW_PLUGIN_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "pdf/paint_manager.h"
#include "pdf/pdf_engine.h"
#include "pdf/pdfium/pdfium_form_filler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace base {
class Value;
}  // namespace base

namespace chrome_pdf {

class PDFiumEngine;
class UrlLoader;

// Common base to share code between the two plugin implementations,
// `OutOfProcessInstance` (Pepper) and `PdfViewWebPlugin` (Blink).
class PdfViewPluginBase : public PDFEngine::Client,
                          public PaintManager::Client {
 public:
  PdfViewPluginBase(const PdfViewPluginBase& other) = delete;
  PdfViewPluginBase& operator=(const PdfViewPluginBase& other) = delete;

  // PDFEngine::Client:
  void Invalidate(const gfx::Rect& rect) override;
  uint32_t GetBackgroundColor() override;

  // PaintManager::Client
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>* ready,
               std::vector<gfx::Rect>* pending) override;

 protected:
  struct BackgroundPart {
    gfx::Rect location;
    uint32_t color;
  };

  // The minimum zoom level allowed.
  static constexpr double kMinZoom = 0.01;

  PdfViewPluginBase();
  ~PdfViewPluginBase() override;

  // Initializes the main `PDFiumEngine`. Any existing engine will be replaced.
  void InitializeEngine(PDFiumFormFiller::ScriptOption script_option);

  // Destroys the main `PDFiumEngine`. Subclasses should call this method in
  // their destructor to ensure the engine is destroyed first.
  void DestroyEngine();

  PDFiumEngine* engine() { return engine_.get(); }

  PaintManager& paint_manager() { return paint_manager_; }

  // Starts loading `url`. If `is_print_preview` is `true`, load for print
  // preview instead of normal PDF viewing.
  void LoadUrl(const std::string& url, bool is_print_preview);

  // Gets a weak pointer with a lifetime matching the derived class.
  virtual base::WeakPtr<PdfViewPluginBase> GetWeakPtr() = 0;

  // Creates a URL loader and allows it to access all urls, i.e. not just the
  // frame's origin.
  virtual std::unique_ptr<UrlLoader> CreateUrlLoaderInternal() = 0;

  // Handles `LoadUrl()` result.
  virtual void DidOpen(std::unique_ptr<UrlLoader> loader, int32_t result) = 0;

  // Handles `LoadUrl()` result for print preview.
  virtual void DidOpenPreview(std::unique_ptr<UrlLoader> loader,
                              int32_t result) = 0;

  // Handles `postMessage()` calls from the embedder.
  void HandleMessage(const base::Value& message);

  // Paints the given invalid area of the plugin to the given graphics device.
  // PaintManager::Client::OnPaint() should be its only caller.
  virtual void DoPaint(const std::vector<gfx::Rect>& paint_rects,
                       std::vector<PaintReadyRect>* ready,
                       std::vector<gfx::Rect>* pending) = 0;

  // Callback to do invalidation after painting finishes.
  void InvalidateAfterPaintDone(int32_t /*unused_but_required*/);

  // Updates the available area and the background parts, notifies the PDF
  // engine, and updates the accessibility information.
  virtual void OnGeometryChanged(double old_zoom, float old_device_scale) = 0;

  // A helper of OnGeometryChanged() which updates the available area and
  // the background parts, and notifies the PDF engine of geometry changes.
  void RecalculateAreas(double old_zoom, float old_device_scale);

  // Figures out the location of any background rectangles (i.e. those that
  // aren't painted by the PDF engine).
  void CalculateBackgroundParts();

  // Computes document width/height in device pixels, based on current zoom and
  // device scale
  int GetDocumentPixelWidth() const;
  int GetDocumentPixelHeight() const;

  const std::vector<BackgroundPart>& background_parts() const {
    return background_parts_;
  }

  const std::vector<gfx::Rect>& deferred_invalidates() const {
    return deferred_invalidates_;
  }

  const SkBitmap& image_data() const { return image_data_; }
  SkBitmap& mutable_image_data() { return image_data_; }

  const gfx::Rect& available_area() const { return available_area_; }

  const gfx::Size& document_size() const { return document_size_; }
  void set_document_size(const gfx::Size& size) { document_size_ = size; }

  const gfx::Size& plugin_size() const { return plugin_size_; }
  void set_plugin_size(const gfx::Size& size) { plugin_size_ = size; }

  const gfx::Size& plugin_dip_size() const { return plugin_dip_size_; }
  void set_plugin_dip_size(const gfx::Size& size) { plugin_dip_size_ = size; }

  const gfx::Point& plugin_offset() const { return plugin_offset_; }
  void set_plugin_offset(const gfx::Point& offset) { plugin_offset_ = offset; }

  void SetBackgroundColor(uint32_t background_color) {
    background_color_ = background_color;
  }

  int top_toolbar_height_in_viewport_coords() const {
    return top_toolbar_height_in_viewport_coords_;
  }

  void set_top_toolbar_height_in_viewport_coords(int height) {
    top_toolbar_height_in_viewport_coords_ = height;
  }

  // Sets the new zoom scale.
  void SetZoom(double scale);

  double zoom() const { return zoom_; }

  float device_scale() const { return device_scale_; }
  void set_device_scale(float device_scale) { device_scale_ = device_scale; }

  bool first_paint() const { return first_paint_; }
  void set_first_paint(bool first_paint) { first_paint_ = first_paint; }

  bool needs_reraster() const { return needs_reraster_; }
  void set_needs_reraster(bool needs_reraster) {
    needs_reraster_ = needs_reraster;
  }

  bool received_viewport_message() const { return received_viewport_message_; }
  void set_received_viewport_message(bool received) {
    received_viewport_message_ = received;
  }

 private:
  // Message handlers.
  void HandleSetTwoUpViewMessage(const base::Value& message);

  std::unique_ptr<PDFiumEngine> engine_;
  PaintManager paint_manager_{this};

  // Image data buffer for painting.
  SkBitmap image_data_;

  std::vector<BackgroundPart> background_parts_;

  // Deferred invalidates while |in_paint_| is true.
  std::vector<gfx::Rect> deferred_invalidates_;

  // Remaining area, in pixels, to render the pdf in after accounting for
  // horizontal centering.
  gfx::Rect available_area_;

  // The size of the entire document in pixels (i.e. if each page is 800 pixels
  // high and there are 10 pages, the height will be 8000).
  gfx::Size document_size_;

  // Size, in pixels, of plugin rectangle.
  gfx::Size plugin_size_;

  // Size, in DIPs, of plugin rectangle.
  gfx::Size plugin_dip_size_;

  // Positional offset, in CSS pixels, of the plugin rectangle.
  gfx::Point plugin_offset_;

  // The background color of the PDF viewer.
  uint32_t background_color_ = 0;

  // The blank space above the first page of the document reserved for the
  // toolbar.
  int top_toolbar_height_in_viewport_coords_ = 0;

  // Current zoom factor.
  double zoom_ = 1.0;

  // Current device scale factor. Multiply by `device_scale_` to convert from
  // viewport to screen coordinates. Divide by `device_scale_` to convert from
  // screen to viewport coordinates.
  float device_scale_ = 1.0f;

  // True if we haven't painted the plugin viewport yet.
  bool first_paint_ = true;

  // Whether OnPaint() is in progress or not.
  bool in_paint_ = false;

  // True if we request a new bitmap rendering.
  bool needs_reraster_ = true;

  // Whether the plugin has received a viewport changed message. Nothing should
  // be painted until this is received.
  bool received_viewport_message_ = false;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
