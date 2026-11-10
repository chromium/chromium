// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_SIGNATURE_CREATION_HANDLER_H_
#define PDF_PDF_INK_SIGNATURE_CREATION_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "pdf/buildflags.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace base {
class DictValue;
}  // namespace base

namespace chrome_pdf {

class PdfInkModuleClient;

// Handles signature creation dialog messages for `PdfInkModule`.
class PdfInkSignatureCreationHandler {
 public:
  explicit PdfInkSignatureCreationHandler(PdfInkModuleClient& client);
  PdfInkSignatureCreationHandler(const PdfInkSignatureCreationHandler&) =
      delete;
  PdfInkSignatureCreationHandler& operator=(
      const PdfInkSignatureCreationHandler&) = delete;
  ~PdfInkSignatureCreationHandler();

  // Returns whether `message` was handled or not.
  bool OnMessage(const base::DictValue& message);

 private:
  void HandleStartSignatureStrokeMessage(const base::DictValue& message);
  void HandleContinueSignatureStrokeMessage(const base::DictValue& message);
  void HandleFinishSignatureStrokeMessage(const base::DictValue& message);

  // Replies to `message` with the coat 0 mesh vertex outline loops from
  // `in_progress_stroke_`.
  void ReplyWithStrokeOutlines(const base::DictValue& message);

  const raw_ref<PdfInkModuleClient> client_;

  ink::InProgressStroke in_progress_stroke_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_SIGNATURE_CREATION_HANDLER_H_
