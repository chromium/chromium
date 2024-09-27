// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ON_DEMAND_SEARCHIFIER_H_
#define PDF_PDFIUM_PDFIUM_ON_DEMAND_SEARCHIFIER_H_

#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom-forward.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/skia/include/core/SkBitmap.h"
namespace chrome_pdf {

class PDFiumOnDemandSearchifier {
 public:
  explicit PDFiumOnDemandSearchifier(PDFiumEngine* engine);
  ~PDFiumOnDemandSearchifier();

  // Starts performing searchify on the scheduled pages. The function should be
  // called only once. If pages are added for searchifying later, they are
  // automatically picked up from the queue.
  void Start(PerformOcrCallbackAsync callback);

  // Called when OCR service is disconnected and is not available anymore.
  void OnOcrDisconnected();

  // Checks if the page is queued to be searchified or the searchifying process
  // has started for it but not finished yet.
  bool IsPageScheduled(int page_index) const;

  // Puts a page in the queue to be searchified. This function can be called
  // before `Start` and if so, the page stays in the queue until searchifier
  // starts.
  void SchedulePage(int page_index);

  // Removes the page form the searchifying queue if it's there.
  void RemovePageFromQueue(int page_index);

  bool HasFailed() const { return state_ == State::kFailed; }
  bool IsIdleForTesting() const { return state_ == State::kIdle; }

 private:
  enum class State { kIdle, kWaitingForResults, kFailed };

  void SearchifyNextPage();
  void SearchifyNextImage();

  struct BitmapResult {
    SkBitmap bitmap;
    int image_index;
  };

  std::optional<BitmapResult> GetNextBitmap();
  void OnGotOcrResult(int image_index,
                      const gfx::Size& image_size,
                      screen_ai::mojom::VisualAnnotationPtr annotation);

  // Owns this class.
  const raw_ref<PDFiumEngine> engine_;

  ScopedFPDFFont font_;

  // Callback function to perform OCR.
  PerformOcrCallbackAsync perform_ocr_callback_;

  // The page that is currently OCRed.
  raw_ptr<PDFiumPage> current_page_ = nullptr;
  std::vector<int> current_page_image_object_indices_;

  // Scheduled pages to be searchified.
  base::circular_deque<int> pages_queue_;

  State state_ = State::kIdle;

  base::WeakPtrFactory<PDFiumOnDemandSearchifier> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ON_DEMAND_SEARCHIFIER_H_
