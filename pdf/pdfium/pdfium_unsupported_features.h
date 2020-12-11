// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_UNSUPPORTED_FEATURES_H_
#define PDF_PDFIUM_PDFIUM_UNSUPPORTED_FEATURES_H_

namespace chrome_pdf {

class PDFiumEngine;

void InitializeUnsupportedFeaturesHandler();

// Create a local variable of this when calling PDFium functions which can call
// our global callback when an unsupported feature is reached.
class ScopedUnsupportedFeature {
 public:
  explicit ScopedUnsupportedFeature(PDFiumEngine* engine);
  ScopedUnsupportedFeature(const ScopedUnsupportedFeature&) = delete;
  ScopedUnsupportedFeature& operator=(const ScopedUnsupportedFeature&) = delete;
  ~ScopedUnsupportedFeature();

 private:
  PDFiumEngine* const old_engine_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_UNSUPPORTED_FEATURES_H_
