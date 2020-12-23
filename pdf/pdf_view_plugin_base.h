// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_VIEW_PLUGIN_BASE_H_
#define PDF_PDF_VIEW_PLUGIN_BASE_H_

#include "pdf/pdf_engine.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "pdf/paint_manager.h"
#include "pdf/pdfium/pdfium_form_filler.h"

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
  uint32_t GetBackgroundColor() override;

  // PaintManager::Client
  void OnPaint(const std::vector<gfx::Rect>& paint_rects,
               std::vector<PaintReadyRect>* ready,
               std::vector<gfx::Rect>* pending) override;

 protected:
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

  // Paints the given invalid area of the plugin to the given graphics device.
  // PaintManager::Client::OnPaint() should be its only caller.
  virtual void DoPaint(const std::vector<gfx::Rect>& paint_rects,
                       std::vector<PaintReadyRect>* ready,
                       std::vector<gfx::Rect>* pending) = 0;

  void SetBackgroundColor(uint32_t background_color) {
    background_color_ = background_color;
  }

  int top_toolbar_height_in_viewport_coords() const {
    return top_toolbar_height_in_viewport_coords_;
  }

  void set_top_toolbar_height_in_viewport_coords(int height) {
    top_toolbar_height_in_viewport_coords_ = height;
  }

  double zoom() const { return zoom_; }
  void set_zoom(double zoom) { zoom_ = zoom; }

  float device_scale() const { return device_scale_; }
  void set_device_scale(float device_scale) { device_scale_ = device_scale; }

  bool first_paint() const { return first_paint_; }
  void set_first_paint(bool first_paint) { first_paint_ = first_paint; }

  bool in_paint() const { return in_paint_; }

 private:
  std::unique_ptr<PDFiumEngine> engine_;
  PaintManager paint_manager_{this};

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
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_VIEW_PLUGIN_BASE_H_
