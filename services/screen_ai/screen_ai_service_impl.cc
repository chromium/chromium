// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/proto/main_content_extractor_proto_convertor.h"
#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "services/screen_ai/screen_ai_ax_tree_serializer.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(USE_FAKE_SCREEN_AI)
#include "services/screen_ai/screen_ai_library_wrapper_fake.h"
#else
#include "services/screen_ai/screen_ai_library_wrapper_impl.h"
#endif

namespace screen_ai {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OcrClientTypeForMetrics {
  kTest = 0,
  kPdfViewer = 1,
  kLocalSearch = 2,
  kCameraApp = 3,
  kPdfSearchify = 4,
  kMediaApp = 5,
  kMaxValue = kMediaApp
};

OcrClientTypeForMetrics GetClientType(mojom::OcrClientType client_type) {
  switch (client_type) {
    case mojom::OcrClientType::kTest:
      CHECK_IS_TEST();
      return OcrClientTypeForMetrics::kTest;
    case mojom::OcrClientType::kPdfViewer:
      return OcrClientTypeForMetrics::kPdfViewer;
    case mojom::OcrClientType::kLocalSearch:
      return OcrClientTypeForMetrics::kLocalSearch;
    case mojom::OcrClientType::kCameraApp:
      return OcrClientTypeForMetrics::kCameraApp;
    case mojom::OcrClientType::kPdfSearchify:
      return OcrClientTypeForMetrics::kPdfSearchify;
    case mojom::OcrClientType::kMediaApp:
      return OcrClientTypeForMetrics::kMediaApp;
  }
}

ui::AXTreeUpdate ConvertVisualAnnotationToTreeUpdate(
    const std::optional<chrome_screen_ai::VisualAnnotation>& annotation_proto,
    const gfx::Rect& image_rect) {
  if (!annotation_proto) {
    VLOG(0) << "Screen AI library could not process snapshot or no OCR data.";
    return ui::AXTreeUpdate();
  }

  return VisualAnnotationToAXTreeUpdate(*annotation_proto, image_rect);
}

ui::AXNodeID ComputeMainNode(
    const ui::AXTree* tree,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  ui::AXNode* front = tree->GetFromId(content_node_ids.front());
  ui::AXNode* back = tree->GetFromId(content_node_ids.back());
  ui::AXNode* main = front->GetLowestCommonAncestor(*back);
  return main->id();
}

}  // namespace

// The library accepts simple pointers to model data retrieval functions, hence
// callback functions with linked object are not safe to pass.
// Since library initialization functions are called in a single thread process,
// we choose the active model data instance before calling the library
// initializer and release it when initialization is completed.
PreloadedModelData* g_active_model_data_instance = nullptr;

// Keeps the content of model files, and replies to calls for copying them.
class PreloadedModelData {
 public:
  PreloadedModelData(const PreloadedModelData&) = delete;
  PreloadedModelData& operator=(const PreloadedModelData&) = delete;
  ~PreloadedModelData() { CHECK_NE(g_active_model_data_instance, this); }

  static std::unique_ptr<PreloadedModelData> Create(
      base::flat_map<base::FilePath, base::File> model_files) {
    return base::WrapUnique<PreloadedModelData>(
        new PreloadedModelData(std::move(model_files)));
  }

  // Returns 0 if file is not found.
  static uint32_t GetDataSize(const char* relative_file_path) {
    CHECK(g_active_model_data_instance);
    return base::Contains(g_active_model_data_instance->data_,
                          relative_file_path)
               ? g_active_model_data_instance->data_[relative_file_path].size()
               : 0;
  }

  // Assumes that `buffer` has enough size.
  static void CopyData(const char* relative_file_path,
                       uint32_t buffer_size,
                       char* buffer) {
    CHECK(g_active_model_data_instance);
    CHECK(base::Contains(g_active_model_data_instance->data_,
                         relative_file_path));
    const std::vector<char>& data =
        g_active_model_data_instance->data_[relative_file_path];
    CHECK_GE(buffer_size, data.size());
    memcpy(buffer, data.data(), data.size());
  }

  void SetAsActive(bool assign) {
    if (assign) {
      g_active_model_data_instance = this;
    } else {
      g_active_model_data_instance = nullptr;
    }
  }

 private:
  explicit PreloadedModelData(
      base::flat_map<base::FilePath, base::File> model_files) {
    for (auto& model_file : model_files) {
      std::vector<char> buffer;
      int64_t length = model_file.second.GetLength();
      if (length < 0) {
        VLOG(0) << "Could not query Screen AI model file's length: "
                << model_file.first;
        continue;
      }

      buffer.resize(length);
      if (model_file.second.Read(0, buffer.data(), length) != length) {
        VLOG(0) << "Could not read Screen AI model file's content: "
                << model_file.first;
        continue;
      }
      data_[model_file.first.MaybeAsASCII()] = std::move(buffer);
    }
  }

  std::map<std::string, std::vector<char>> data_;
};

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver)
    : factory_receiver_(this, std::move(receiver)),
      ocr_receiver_(this),
      main_content_extraction_receiver_(this) {
  screen_ai_annotators_.set_disconnect_handler(base::BindRepeating(
      &ScreenAIService::ReceiverDisconnected, weak_ptr_factory_.GetWeakPtr()));
}

ScreenAIService::~ScreenAIService() = default;

void ScreenAIService::LoadLibrary(const base::FilePath& library_path) {
  // The ScopedBlockingCall in LoadLibrary guarantees that this is not run on
  // the UI thread.
#if BUILDFLAG(USE_FAKE_SCREEN_AI)
  library_ = std::make_unique<ScreenAILibraryWrapperFake>();
#else
  library_ = std::make_unique<ScreenAILibraryWrapperImpl>();
#endif

  bool load_sucessful = library_->Load(library_path);
  base::UmaHistogramBoolean("Accessibility.ScreenAI.Library.Initialized",
                            load_sucessful);

  if (!load_sucessful) {
    library_.reset();
    return;
  }

  uint32_t version_major;
  uint32_t version_minor;
  library_->GetLibraryVersion(version_major, version_minor);
  VLOG(2) << "Screen AI library version: " << version_major << "."
          << version_minor;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  library_->SetLogger();
#endif

  if (features::IsScreenAIDebugModeEnabled()) {
    library_->EnableDebugMode();
  }

  library_->SetFileContentFunctions(&PreloadedModelData::GetDataSize,
                                    &PreloadedModelData::CopyData);
}

void ScreenAIService::InitializeMainContentExtraction(
    const base::FilePath& library_path,
    base::flat_map<base::FilePath, base::File> model_files,
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback) {
  if (!library_) {
    LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PreloadedModelData::Create, std::move(model_files)),
      base::BindOnce(&ScreenAIService::InitializeMainContentExtractionInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(main_content_extractor_service_receiver),
                     std::move(callback)));
}

void ScreenAIService::InitializeMainContentExtractionInternal(
    mojo::PendingReceiver<mojom::MainContentExtractionService>
        main_content_extractor_service_receiver,
    InitializeMainContentExtractionCallback callback,
    std::unique_ptr<PreloadedModelData> model_data) {
  // `model_data` contains the content of the model files and its accessors are
  // passed to the library. It should be kept in memory until after library
  // initialization.
  model_data->SetAsActive(true);
  bool init_successful = library_->InitMainContentExtraction();
  model_data->SetAsActive(false);
  base::UmaHistogramBoolean(
      "Accessibility.ScreenAI.MainContentExtraction.Initialized",
      init_successful);
  if (!init_successful) {
    std::move(callback).Run(false);
    return;
  }

  // This interface should be created only once.
  CHECK(!main_content_extraction_receiver_.is_bound());

  main_content_extraction_receiver_.Bind(
      std::move(main_content_extractor_service_receiver));

  std::move(callback).Run(true);
}

void ScreenAIService::InitializeOCR(
    const base::FilePath& library_path,
    base::flat_map<base::FilePath, base::File> model_files,
    mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
    InitializeOCRCallback callback) {
  if (!library_) {
    LoadLibrary(library_path);
  }

  if (!library_) {
    std::move(callback).Run(false);
    base::Process::TerminateCurrentProcessImmediately(-1);
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PreloadedModelData::Create, std::move(model_files)),
      base::BindOnce(&ScreenAIService::InitializeOCRInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(ocr_service_receiver), std::move(callback)));
}

void ScreenAIService::InitializeOCRInternal(
    mojo::PendingReceiver<mojom::OCRService> ocr_service_receiver,
    InitializeOCRCallback callback,
    std::unique_ptr<PreloadedModelData> model_data) {
  // `model_data` contains the content of the model files and its accessors are
  // passed to the library. It should be kept in memory until after library
  // initialization.
  model_data->SetAsActive(true);
  bool init_successful = library_->InitOCR();
  model_data->SetAsActive(false);

  base::UmaHistogramBoolean("Accessibility.ScreenAI.OCR.Initialized",
                            init_successful);

  // TODO(crbug.com/40911117): Add a separate initialization interface for
  // layout extraction.
  if (features::IsLayoutExtractionEnabled()) {
    if (!library_->InitLayoutExtraction()) {
      VLOG(0) << "Could not initialize layout extraction.";
    }
  }

  if (!init_successful) {
    std::move(callback).Run(false);
    return;
  }

  // This interface should be created only once.
  CHECK(!ocr_receiver_.is_bound());

  ocr_receiver_.Bind(std::move(ocr_service_receiver));

  std::move(callback).Run(true);
}

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::BindAnnotatorClient(
    mojo::PendingRemote<mojom::ScreenAIAnnotatorClient> annotator_client) {
  DCHECK(!screen_ai_annotator_client_.is_bound());
  screen_ai_annotator_client_.Bind(std::move(annotator_client));
}

void ScreenAIService::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
        main_content_extractor) {
  screen_2x_main_content_extractors_.Add(this,
                                         std::move(main_content_extractor));
}

void ScreenAIService::ExtractSemanticLayout(
    const SkBitmap& image,
    const ui::AXTreeID& parent_tree_id,
    ExtractSemanticLayoutCallback callback) {
  DCHECK(screen_ai_annotator_client_.is_bound());

  std::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      library_->ExtractLayout(image);

  // The original caller is always replied to, and an AXTreeIDUnknown is sent to
  // tell it that the annotation function was not successful. However the client
  // is only contacted for successful runs and when we have an update.
  if (!annotation_proto) {
    VLOG(0) << "Layout Extraction failed. ";
    std::move(callback).Run(ui::AXTreeIDUnknown());
    return;
  }

  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));
  VLOG(1) << "Layout Extraction returned " << update.nodes.size() << " nodes.";

  // Convert `update` to a properly serialized `AXTreeUpdate`.
  ScreenAIAXTreeSerializer serializer(parent_tree_id, std::move(update.nodes));
  update = serializer.Serialize();

  // `ScreenAIAXTreeSerializer` should have assigned a new tree ID to `update`.
  // Thereby, it should never be an unknown tree ID, otherwise there has been an
  // unexpected serialization bug.
  DCHECK_NE(update.tree_data.tree_id, ui::AXTreeIDUnknown())
      << "Invalid serialization.\n"
      << update.ToString();
  std::move(callback).Run(update.tree_data.tree_id);
  screen_ai_annotator_client_->HandleAXTreeUpdate(update);
}

std::optional<chrome_screen_ai::VisualAnnotation>
ScreenAIService::PerformOcrAndRecordMetrics(const SkBitmap& image,
                                            bool a11y_tree_request) {
  auto entry = ocr_client_types_.find(screen_ai_annotators_.current_receiver());
  CHECK(entry != ocr_client_types_.end()) << "OCR client type is not set.";
  base::UmaHistogramEnumeration("Accessibility.ScreenAI.OCR.ClientType",
                                GetClientType(entry->second));

  base::TimeTicks start_time = base::TimeTicks::Now();
  auto result = library_->PerformOcr(image);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  int lines_count = result ? result->lines_size() : 0;
  unsigned image_size = image.width() * image.height();
  VLOG(1) << "OCR returned " << lines_count << " lines in " << elapsed_time;

  // TODO(crbug.com/342796806): Add browser test.
  base::UmaHistogramBoolean("Accessibility.ScreenAI.OCR.Successful",
                            result.has_value());
  base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount",
                              lines_count);
  base::UmaHistogramCounts10M("Accessibility.ScreenAI.OCR.ImageSize10M",
                              image_size);
  if (image_size < 500 * 500) {
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.Small",
                            elapsed_time);
  } else if (image_size < 1000 * 1000) {
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.Medium",
                            elapsed_time);
  } else if (image_size < 2000 * 2000) {
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.Large",
                            elapsed_time);
  } else {
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.XLarge",
                            elapsed_time);
  }

  // If needed to extend to more clients, an identifier can be passed from the
  // client to introduce itself and these metrics can be collected based on it.
  if (a11y_tree_request) {
    base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount.PDF",
                                lines_count);
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Time.PDF",
                            elapsed_time);
    base::UmaHistogramCounts10M("Accessibility.ScreenAI.OCR.ImageSize.PDF",
                                image.width() * image.height());
  }

  return result;
}

void ScreenAIService::SetClientType(mojom::OcrClientType client_type) {
  ocr_client_types_[screen_ai_annotators_.current_receiver()] = client_type;
}

void ScreenAIService::PerformOcrAndReturnAnnotation(
    const SkBitmap& image,
    PerformOcrAndReturnAnnotationCallback callback) {
  std::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      PerformOcrAndRecordMetrics(image, /*a11y_tree_request=*/false);

  if (annotation_proto) {
    std::move(callback).Run(ConvertProtoToVisualAnnotation(*annotation_proto));
    return;
  }

  std::move(callback).Run(mojom::VisualAnnotation::New());
}

void ScreenAIService::PerformOcrAndReturnAXTreeUpdate(
    const SkBitmap& image,
    PerformOcrAndReturnAXTreeUpdateCallback callback) {
  std::optional<chrome_screen_ai::VisualAnnotation> annotation_proto =
      PerformOcrAndRecordMetrics(image, /*a11y_tree_request=*/true);
  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));

  // The original caller is always replied to, and an empty AXTreeUpdate tells
  // that the annotation function was not successful.
  std::move(callback).Run(update);
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ukm::SourceId ukm_source_id,
                                         ExtractMainContentCallback callback) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  ui::AXTree tree;
  std::optional<std::vector<int32_t>> content_node_ids;
  bool success = ExtractMainContentInternal(snapshot, tree, content_node_ids);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  RecordMetrics(ukm_source_id, ukm::UkmRecorder::Get(), elapsed_time, success);

  if (success) {
    std::move(callback).Run(*content_node_ids);
  } else {
    std::move(callback).Run(std::vector<int32_t>());
  }
}

void ScreenAIService::ExtractMainNode(const ui::AXTreeUpdate& snapshot,
                                      ExtractMainNodeCallback callback) {
  ui::AXTree tree;
  std::optional<std::vector<int32_t>> content_node_ids;
  bool success = ExtractMainContentInternal(snapshot, tree, content_node_ids);

  if (success) {
    ui::AXNodeID main_node_id = ComputeMainNode(&tree, *content_node_ids);
    std::move(callback).Run(main_node_id);
  } else {
    std::move(callback).Run(ui::kInvalidAXNodeID);
  }
}

bool ScreenAIService::ExtractMainContentInternal(
    const ui::AXTreeUpdate& snapshot,
    ui::AXTree& tree,
    std::optional<std::vector<int32_t>>& content_node_ids) {
  // Early return if input is empty.
  if (snapshot.nodes.empty()) {
    return false;
  }

  // Deserialize the snapshot and reserialize it to a view hierarchy proto.
  CHECK(tree.Unserialize(snapshot));
  std::string serialized_snapshot = SnapshotToViewHierarchy(&tree);
  content_node_ids = library_->ExtractMainContent(serialized_snapshot);
  // TODO(crbug.com/342796806): Add browser test.
  base::UmaHistogramBoolean(
      "Accessibility.ScreenAI.MainContentExtraction.Successful",
      content_node_ids.has_value());
  if (content_node_ids.has_value() && content_node_ids->size() > 0) {
    VLOG(2) << "Screen2x returned " << content_node_ids->size() << " node ids.";
    return true;
  } else {
    VLOG(0) << "Screen2x returned no results.";
    return false;
  }
}

ui::AXNodeID ScreenAIService::ComputeMainNodeForTesting(
    const ui::AXTree* tree,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  return ComputeMainNode(tree, content_node_ids);
}

// static
void ScreenAIService::RecordMetrics(ukm::SourceId ukm_source_id,
                                    ukm::UkmRecorder* ukm_recorder,
                                    base::TimeDelta elapsed_time,
                                    bool success) {
  if (success) {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.Screen2xDistillationTime.Success",
        elapsed_time);
    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::Accessibility_ScreenAI(ukm_source_id)
          .SetScreen2xDistillationTime_Success(elapsed_time.InMilliseconds())
          .Record(ukm_recorder);
    }
  } else {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.Screen2xDistillationTime.Failure",
        elapsed_time);
    if (ukm_source_id != ukm::kInvalidSourceId) {
      ukm::builders::Accessibility_ScreenAI(ukm_source_id)
          .SetScreen2xDistillationTime_Failure(elapsed_time.InMilliseconds())
          .Record(ukm_recorder);
    }
  }
}

void ScreenAIService::ReceiverDisconnected() {
  auto entry = ocr_client_types_.find(screen_ai_annotators_.current_receiver());
  if (entry != ocr_client_types_.end()) {
    ocr_client_types_.erase(entry);
  }
}

}  // namespace screen_ai
