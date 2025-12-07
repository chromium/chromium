// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_factory.mojom.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "services/screen_ai/screen_ai_library_wrapper.h"

namespace ui {
class AXTree;
}

namespace screen_ai {

class ModelDataHolder;

// Uses a local machine intelligence library to augment the accessibility
// tree. Functionalities include running OCR on images and extracting the main
// content of a page.
// See more in: google3/chrome/chromeos/accessibility/machine_intelligence/
// chrome_screen_ai/README.md
class ScreenAIService : public mojom::ScreenAIServiceFactory,
                        public mojom::OCRService,
                        public mojom::MainContentExtractionService,
                        public mojom::ScreenAIAnnotator,
                        public mojom::Screen2xMainContentExtractor {
 public:
  explicit ScreenAIService(
      mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver);
  ScreenAIService(const ScreenAIService&) = delete;
  ScreenAIService& operator=(const ScreenAIService&) = delete;
  ~ScreenAIService() override;

  static ui::AXNodeID ComputeMainNodeForTesting(
      const ui::AXTree* tree,
      const std::vector<ui::AXNodeID>& content_node_ids);

 private:
  std::unique_ptr<ScreenAILibraryWrapper> library_;

  void LoadLibrary(const base::FilePath& library_path);

  // mojom::ScreenAIAnnotator:
#if BUILDFLAG(IS_CHROMEOS)
  void PerformOcrAndReturnAXTreeUpdate(
      const SkBitmap& image,
      PerformOcrAndReturnAXTreeUpdateCallback callback) override;
#endif
  void PerformOcrAndReturnAnnotation(
      const SkBitmap& image,
      PerformOcrAndReturnAnnotationCallback callback) override;
  void SetClientType(mojom::OcrClientType client) override;
  void GetMaxImageDimension(GetMaxImageDimensionCallback callback) override;
  void SetOCRLightMode(bool enabled) override;
  void IsOCRBusy(IsOCRBusyCallback callback) override;

  // mojom::Screen2xMainContentExtractor:
  void ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                          ExtractMainContentCallback callback) override;
  void ExtractMainNode(const ui::AXTreeUpdate& snapshot,
                       ExtractMainNodeCallback callback) override;
  void IdentifyMainNode(const ui::AXTreeUpdate& snapshot,
                        IdentifyMainNodeCallback callback) override;
  void SetClientType(mojom::MceClientType client) override;

  // mojom::ScreenAIServiceFactory:
  void InitializeOCR(
      const base::FilePath& library_path,
      base::flat_map<base::FilePath, base::File> model_files,
      mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
      InitializeOCRCallback callback) override;
  void InitializeMainContentExtraction(
      const base::FilePath& library_path,
      base::flat_map<base::FilePath, base::File> model_files,
      mojo::PendingReceiver<mojom::MainContentExtractionService>
          main_content_extractor_service_receiver,
      InitializeMainContentExtractionCallback callback) override;
  void BindShutdownHandler(
      mojo::PendingRemote<mojom::ScreenAIServiceShutdownHandler>
          shutdown_handler) override;

  // mojom::OCRService:
  void BindAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) override;

  // mojom::MainContentExtractionService:
  void BindMainContentExtractor(
      mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
          main_content_extractor) override;

  // Takes as input as `AXTreeUpdate` and references to an empty AXTree and
  // vector of ints. Unserializes `snapshot` into `tree`. Runs the library
  // `ExtractMainContent` function whose return value sets `content_node_ids`.
  // If `content_node_ids` is empty; returns false; otherwise, returns true.
  bool ExtractMainContentInternalAndRecordMetrics(
      const ui::AXTreeUpdate& snapshot,
      ui::AXTree& tree,
      std::optional<std::vector<int32_t>>& content_node_ids);

  // Wrapper to call `PerformOcr` library function and record metrics.
  std::optional<chrome_screen_ai::VisualAnnotation> PerformOcrAndRecordMetrics(
      const SkBitmap& image);

  void OcrReceiverDisconnected();
  void MceReceiverDisconnected();

  // Starts a timer to frequently check if the service is idle. This is only
  // triggered after the library is initialized for either of the
  // functionalities.
  void StartShutDownOnIdleTimer();
  void ShutDownOnIdle();

  // Max image dimension for OCR that is processed without downsampling.
  // This value is received via `GetMaxImageDimension` after OCR is initialized,
  // and since it does not change after that, it is stored to be reused for
  // subsequent calls.
  // NOTE: Update here and all callers in case the above assumption changes.
  uint32_t max_ocr_dimension_ = 0;

  // Last time the feature is used. A null value means never, it is set when the
  // feature is initialized, and each time it is used.
  base::TimeTicks ocr_last_used_;
  base::TimeTicks mce_last_used_;

  std::unique_ptr<base::RepeatingTimer> idle_checking_timer_;

  mojo::Receiver<mojom::ScreenAIServiceFactory> factory_receiver_;
  mojo::Receiver<mojom::OCRService> ocr_receiver_;
  mojo::Receiver<mojom::MainContentExtractionService>
      main_content_extraction_receiver_;

  // Keeps handles for all model data files.
  std::unique_ptr<ModelDataHolder> model_data_holder_;

  // Client type for each OCR receiver.
  std::map<mojo::ReceiverId, mojom::OcrClientType> ocr_client_types_;

  // Light Mode OCR clients.
  std::set<mojo::ReceiverId> light_ocr_clients_;

  // OCR last mode across all client.
  bool last_ocr_light_ = false;

  // The number of times OCR mode was changed before shutting down the service.
  uint32_t ocr_mode_switch_count_ = 0;

  // Client type for each MCE receiver.
  std::map<mojo::ReceiverId, mojom::MceClientType> mce_client_types_;

  // Browser side shutdown handler.
  mojo::Remote<mojom::ScreenAIServiceShutdownHandler>
      screen_ai_shutdown_handler_;

  // The set of receivers used to receive messages from annotators.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;

  // The set of receivers used to receive messages from main content
  // extractors.
  mojo::ReceiverSet<mojom::Screen2xMainContentExtractor>
      screen2x_main_content_extractors_;

  // Task runner used to monitor unresponsiveness.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<ScreenAIService> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
