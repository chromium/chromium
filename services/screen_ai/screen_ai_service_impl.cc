// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_service_impl.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/crash/core/common/crash_key.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/proto/main_content_extractor_proto_convertor.h"
#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(USE_FAKE_SCREEN_AI)
#include "services/screen_ai/screen_ai_library_wrapper_fake.h"
#else
#include "services/screen_ai/screen_ai_library_wrapper_impl.h"
#endif

namespace screen_ai {

namespace {

// How often it would be checked that the service is idle and can be shutdown.
constexpr base::TimeDelta kIdleCheckingDelay = base::Minutes(5);

// How long after all clients are disconnected, it is checked if service is
// idle.
constexpr base::TimeDelta kCoolDownTime = base::Seconds(10);

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
    std::optional<chrome_screen_ai::VisualAnnotation>& annotation_proto,
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
// This global variable keeps the pointer the only instance of this class.
ModelDataHolder* g_model_data_holder_instance = nullptr;

// Keeps the handles of model files, and replies to calls for copying their
// content.
class ModelDataHolder {
 public:
  ModelDataHolder() {
    CHECK(!g_model_data_holder_instance);
    g_model_data_holder_instance = this;
  }

  ModelDataHolder(const ModelDataHolder&) = delete;
  ModelDataHolder& operator=(const ModelDataHolder&) = delete;

  ~ModelDataHolder() {
    CHECK_EQ(g_model_data_holder_instance, this);
    g_model_data_holder_instance = nullptr;
  }

  // Returns 0 if file is not found.
  static uint32_t GetDataSize(const char* relative_file_path) {
    CHECK(g_model_data_holder_instance);
    base::File* model_file =
        g_model_data_holder_instance->GetModelFile(relative_file_path);
    return model_file ? model_file->GetLength() : 0;
  }

  // Copies content of the file in `relative_file_path` to `buffer`. Expects
  // that `buffer_size` would be enough for the entire file content.
  static void CopyData(const char* relative_file_path,
                       uint32_t buffer_size,
                       char* buffer) {
    CHECK(g_model_data_holder_instance);
    base::File* model_file =
        g_model_data_holder_instance->GetModelFile(relative_file_path);
    CHECK(model_file);

    int64_t length = model_file->GetLength();
    CHECK_GE(buffer_size, length);
    CHECK_EQ(UNSAFE_TODO(model_file->Read(0, buffer, length)), length);

    // TODO(crbug.com/361733242): Remove after the crash is fixed.
    // `relative_file_path` is from `files_list_main_content_extraction.txt`
    // or `files_list_ocr.txt` and under 100 characters long.
    static crash_reporter::CrashKeyString<100> crash_info(
        "last_loaded_screen_ai_file");
    crash_info.Set(relative_file_path);
  }

  void AddModelFiles(base::flat_map<base::FilePath, base::File> model_files) {
    for (auto& model_file : model_files) {
      model_files_[model_file.first.MaybeAsASCII()] =
          std::move(model_file.second);
    }
  }

  // Returns the file handle for `relative_file_path` if it exists.
  base::File* GetModelFile(const char* relative_file_path) {
    if (!base::Contains(model_files_, relative_file_path)) {
      return nullptr;
    }
    return &model_files_[relative_file_path];
  }

 private:
  std::map<std::string, base::File> model_files_;
};

ScreenAIService::ScreenAIService(
    mojo::PendingReceiver<mojom::ScreenAIServiceFactory> receiver)
    : factory_receiver_(this, std::move(receiver)),
      ocr_receiver_(this),
      main_content_extraction_receiver_(this) {
  screen2x_main_content_extractors_.set_disconnect_handler(
      base::BindRepeating(&ScreenAIService::CheckIdleStateAfterDelay,
                          weak_ptr_factory_.GetWeakPtr()));
  screen_ai_annotators_.set_disconnect_handler(
      base::BindRepeating(&ScreenAIService::OcrReceiverDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
  model_data_holder_ = std::make_unique<ModelDataHolder>();
  idle_checking_timer_ = std::make_unique<base::RepeatingTimer>();
  idle_checking_timer_->Start(FROM_HERE, kIdleCheckingDelay, this,
                              &ScreenAIService::ShutDownIfNoClients);
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

  library_->SetFileContentFunctions(&ModelDataHolder::GetDataSize,
                                    &ModelDataHolder::CopyData);
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

  model_data_holder_->AddModelFiles(std::move(model_files));

  bool init_successful = library_->InitMainContentExtraction();
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
  main_content_extraction_last_used_ = base::TimeTicks::Now();
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

  model_data_holder_->AddModelFiles(std::move(model_files));

  bool init_successful = library_->InitOCR();
  base::UmaHistogramBoolean("Accessibility.ScreenAI.OCR.Initialized",
                            init_successful);

  if (!init_successful) {
    std::move(callback).Run(false);
    return;
  }

  // This interface should be created only once.
  CHECK(!ocr_receiver_.is_bound());

  ocr_receiver_.Bind(std::move(ocr_service_receiver));

  std::move(callback).Run(true);
  ocr_last_used_ = base::TimeTicks::Now();
}

void ScreenAIService::BindAnnotator(
    mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) {
  screen_ai_annotators_.Add(this, std::move(annotator));
}

void ScreenAIService::BindMainContentExtractor(
    mojo::PendingReceiver<mojom::Screen2xMainContentExtractor>
        main_content_extractor) {
  screen2x_main_content_extractors_.Add(this,
                                        std::move(main_content_extractor));
}

std::optional<chrome_screen_ai::VisualAnnotation>
ScreenAIService::PerformOcrAndRecordMetrics(const SkBitmap& image) {
  CHECK(base::Contains(ocr_client_types_,
                       screen_ai_annotators_.current_receiver()));
  mojom::OcrClientType client_type =
      ocr_client_types_.find(screen_ai_annotators_.current_receiver())->second;
  base::UmaHistogramEnumeration("Accessibility.ScreenAI.OCR.ClientType",
                                GetClientType(client_type));

  ocr_last_used_ = base::TimeTicks::Now();
  auto result = library_->PerformOcr(image);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - ocr_last_used_;
  int lines_count = result ? result->lines_size() : 0;
  unsigned image_size = image.width() * image.height();
  VLOG(1) << "OCR returned " << lines_count << " lines in " << elapsed_time;

  if (!result) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.OCR.Failed.ClientType",
        GetClientType(client_type));
  }
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

  // MediaApp provides OCR for ChromeOS PDF viewer.
  if (client_type == mojom::OcrClientType::kPdfViewer ||
      client_type == mojom::OcrClientType::kMediaApp) {
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
      PerformOcrAndRecordMetrics(image);

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
      PerformOcrAndRecordMetrics(image);
  ui::AXTreeUpdate update = ConvertVisualAnnotationToTreeUpdate(
      annotation_proto, gfx::Rect(image.width(), image.height()));

  // The original caller is always replied to, and an empty AXTreeUpdate tells
  // that the annotation function was not successful.
  std::move(callback).Run(update);
}

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ukm::SourceId ukm_source_id,
                                         ExtractMainContentCallback callback) {
  main_content_extraction_last_used_ = base::TimeTicks::Now();
  ui::AXTree tree;
  std::optional<std::vector<int32_t>> content_node_ids;
  bool success = ExtractMainContentInternal(snapshot, tree, content_node_ids);
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - main_content_extraction_last_used_;
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
  std::optional<ViewHierarchyAndTreeSize> converted_snapshot =
      SnapshotToViewHierarchy(tree);
  if (!converted_snapshot) {
    VLOG(0) << "Proto not generated.";
    return false;
  }

  // Report request specifications in case the call crashes.
  static crash_reporter::CrashKeyString<95> crash_info(
      "main_content_extraction_info");
  crash_info.Set(base::StringPrintf(
      "TD:%i, TR:%i, SNC:%10zu, SBS:%10zu, TS:%10i, TW:%6i, TH:%6i, SS:%10zu",
      snapshot.has_tree_data, snapshot.root_id != ui::kInvalidAXNodeID,
      snapshot.nodes.size(), snapshot.ByteSize(), tree.size(),
      static_cast<int>(converted_snapshot->tree_dimensions.width()),
      static_cast<int>(converted_snapshot->tree_dimensions.height()),
      converted_snapshot->serialized_proto.size()));

  content_node_ids =
      library_->ExtractMainContent(converted_snapshot->serialized_proto);
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

void ScreenAIService::OcrReceiverDisconnected() {
  auto entry = ocr_client_types_.find(screen_ai_annotators_.current_receiver());
  if (entry != ocr_client_types_.end()) {
    ocr_client_types_.erase(entry);
  }

  CheckIdleStateAfterDelay();
}

void ScreenAIService::CheckIdleStateAfterDelay() {
  // Check if service is idle, a little after the client disconnects.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScreenAIService::ShutDownIfNoClients,
                     weak_ptr_factory_.GetWeakPtr()),
      kCoolDownTime);
}

void ScreenAIService::ShutDownIfNoClients() {
  const base::TimeTicks kIdlenessThreshold =
      base::TimeTicks::Now() - kIdleCheckingDelay;
  bool ocr_not_needed =
      !screen_ai_annotators_.size() || ocr_last_used_ < kIdlenessThreshold;
  bool main_content_extractioncan_not_needed =
      !screen2x_main_content_extractors_.size() ||
      main_content_extraction_last_used_ < kIdlenessThreshold;

  if (ocr_not_needed && main_content_extractioncan_not_needed) {
    VLOG(2) << "Shutting down since no client or idle.";
    base::Process::TerminateCurrentProcessImmediately(0);
  }
}

}  // namespace screen_ai
