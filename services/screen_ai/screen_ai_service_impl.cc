// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_service_impl.h"

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
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "components/crash/core/common/crash_key.h"
#include "services/screen_ai/buildflags/buildflags.h"
#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "services/screen_ai/proto/main_content_extractor_proto_convertor.h"
#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"
#include "services/screen_ai/public/cpp/metrics.h"
#include "services/screen_ai/public/cpp/utilities.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"

#if BUILDFLAG(IS_LINUX)
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG( \
    ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)
#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_with_advanced_checks.h"
#endif
#endif

#if BUILDFLAG(USE_FAKE_SCREEN_AI)
#include "services/screen_ai/screen_ai_library_wrapper_fake.h"
#else
#include "services/screen_ai/screen_ai_library_wrapper_impl.h"
#endif

namespace screen_ai {

namespace {

// How often it would be checked that the service is idle and can be shutdown.
// LINT.IfChange(kIdleCheckingDelay)
constexpr base::TimeDelta kIdleCheckingDelay = base::Seconds(3);
// LINT.ThenChange(//chrome/browser/screen_ai/optical_character_recognizer_browsertest.cc:kServiceIdleCheckingDelay)

// How long to wait for a request to the library be responded, before assuming
// that the library is not responsive.
constexpr base::TimeDelta kMaxWaitForResponseTime = base::Seconds(10);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See `screen_ai_service.mojom` for more info.
// LINT.IfChange(OcrClientType)
enum class OcrClientTypeForMetrics {
  kTest = 0,
  kPdfViewer = 1,
  kLocalSearch = 2,
  kCameraApp = 3,
  kNotUsed = 4,  // Can be used for a new client.
  kMediaApp = 5,
  kScreenshotTextDetection,
  kMaxValue = kScreenshotTextDetection
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:OcrClientType)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// See `screen_ai_service.mojom` for more info.
// LINT.IfChange(MainContentExtractionClientType)
enum class MainContentExtractionClientTypeForMetrics {
  kTest = 0,
  kReadingMode = 1,
  kMainNode = 2,
  kMahi = 3,
  kMaxValue = kMahi
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/accessibility/enums.xml:MainContentExtractionClientType)

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
    case mojom::OcrClientType::kMediaApp:
      return OcrClientTypeForMetrics::kMediaApp;
    case mojom::OcrClientType::kScreenshotTextDetection:
      return OcrClientTypeForMetrics::kScreenshotTextDetection;
  }
}

MainContentExtractionClientTypeForMetrics GetClientType(
    mojom::MceClientType client_type) {
  switch (client_type) {
    case mojom::MceClientType::kTest:
      CHECK_IS_TEST();
      return MainContentExtractionClientTypeForMetrics::kTest;
    case mojom::MceClientType::kReadingMode:
      return MainContentExtractionClientTypeForMetrics::kReadingMode;
    case mojom::MceClientType::kMainNode:
      return MainContentExtractionClientTypeForMetrics::kMainNode;
    case mojom::MceClientType::kMahi:
      return MainContentExtractionClientTypeForMetrics::kMahi;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
ui::AXTreeUpdate ConvertVisualAnnotationToTreeUpdate(
    std::optional<chrome_screen_ai::VisualAnnotation>& annotation_proto,
    const gfx::Rect& image_rect) {
  if (!annotation_proto) {
    VLOG(0) << "Screen AI library could not process snapshot or no OCR data.";
    return ui::AXTreeUpdate();
  }

  return VisualAnnotationToAXTreeUpdate(*annotation_proto, image_rect);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

ui::AXNodeID ComputeMainNode(
    const ui::AXTree* tree,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  ui::AXNode* front = tree->GetFromId(content_node_ids.front());
  ui::AXNode* back = tree->GetFromId(content_node_ids.back());
  ui::AXNode* main = front->GetLowestCommonAncestor(*back);
  return main->id();
}

class HangTimer : public base::OneShotTimer {
 public:
  explicit HangTimer(bool is_ocr) : is_ocr_(is_ocr) {}

  void StartTimer() {
    Start(FROM_HERE, kMaxWaitForResponseTime,
          base::BindOnce(
              [](bool request_is_ocr) {
                base::UmaHistogramBoolean(
                    "Accessibility.ScreenAI.Service.NotResponsive.IsOCR",
                    request_is_ocr);
                base::Process::TerminateCurrentProcessImmediately(0);
              },
              is_ocr_));
  }

 private:
  bool is_ocr_;
};

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
    base::span<uint8_t> buffer_span =
        UNSAFE_TODO(base::as_writable_bytes(base::span(buffer, buffer_size)));
    CHECK(g_model_data_holder_instance);
    base::File* model_file =
        g_model_data_holder_instance->GetModelFile(relative_file_path);
    CHECK(model_file);

    int64_t length = model_file->GetLength();
    CHECK_GE(buffer_size, length);
    CHECK_EQ(model_file->Read(0, buffer_span).value_or(-1),
             base::checked_cast<size_t>(length));
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
#if BUILDFLAG(IS_LINUX) && \
    PA_BUILDFLAG(          \
        ENABLE_ALLOCATOR_SHIM_PARTITION_ALLOC_DISPATCH_WITH_ADVANCED_CHECKS_SUPPORT)
  // TODO(crbug.com/418199684): Remove when the bug is fixed.
  if (base::FeatureList::IsEnabled(
          ::features::kScreenAIPartitionAllocAdvancedChecksEnabled)) {
    allocator_shim::InstallCustomDispatchForPartitionAllocWithAdvancedChecks();
  }
#endif

  screen2x_main_content_extractors_.set_disconnect_handler(
      base::BindRepeating(&ScreenAIService::MceReceiverDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
  screen_ai_annotators_.set_disconnect_handler(
      base::BindRepeating(&ScreenAIService::OcrReceiverDisconnected,
                          weak_ptr_factory_.GetWeakPtr()));
  model_data_holder_ = std::make_unique<ModelDataHolder>();

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
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

#if BUILDFLAG(IS_CHROMEOS)
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
  mce_last_used_ = base::TimeTicks::Now();
  StartShutDownOnIdleTimer();
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

  max_ocr_dimension_ = library_->GetMaxImageDimension();
  CHECK(max_ocr_dimension_);

  // This interface should be created only once.
  CHECK(!ocr_receiver_.is_bound());

  ocr_receiver_.Bind(std::move(ocr_service_receiver));

  std::move(callback).Run(true);
  ocr_last_used_ = base::TimeTicks::Now();
  StartShutDownOnIdleTimer();
}

void ScreenAIService::BindShutdownHandler(
    mojo::PendingRemote<mojom::ScreenAIServiceShutdownHandler>
        shutdown_handler) {
  DCHECK(!screen_ai_shutdown_handler_.is_bound());
  screen_ai_shutdown_handler_.Bind(std::move(shutdown_handler));
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
  OcrClientTypeForMetrics client_type = GetClientType(
      ocr_client_types_.find(screen_ai_annotators_.current_receiver())->second);
  base::UmaHistogramEnumeration("Accessibility.ScreenAI.OCR.ClientType",
                                client_type);

  bool light_client = base::Contains(light_ocr_clients_,
                                     screen_ai_annotators_.current_receiver());
  if (light_client != last_ocr_light_) {
    library_->SetOCRLightMode(light_client);
    last_ocr_light_ = light_client;
    ocr_mode_switch_count_++;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::SequenceBound<HangTimer> hang_timer(background_task_runner_,
                                            /*is_ocr=*/true);
  hang_timer.AsyncCall(&HangTimer::StartTimer);
  auto result = library_->PerformOcr(image);
  hang_timer.AsyncCall(&base::OneShotTimer::Stop);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  int lines_count = result ? result->lines_size() : 0;
  VLOG(1) << "OCR returned " << lines_count << " lines in " << elapsed_time;

  if (!result) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.OCR.Failed.ClientType", client_type);
  }

  int max_dimension = base::checked_cast<int>(max_ocr_dimension_);
  if (image.width() > max_dimension || image.height() > max_dimension) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.OCR.Downsampled.ClientType", client_type);
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.Downsampled",
                            elapsed_time);
  } else {
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Latency.NotDownsampled",
                            elapsed_time);
  }

  base::UmaHistogramBoolean("Accessibility.ScreenAI.OCR.Successful",
                            result.has_value());
  base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount",
                              lines_count);

  // MediaApp provides OCR for ChromeOS PDF viewer.
  if (client_type == OcrClientTypeForMetrics::kPdfViewer ||
      client_type == OcrClientTypeForMetrics::kMediaApp) {
    base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.LinesCount.PDF",
                                lines_count);
    base::UmaHistogramTimes("Accessibility.ScreenAI.OCR.Time.PDF",
                            elapsed_time);
    base::UmaHistogramCounts10M(
        lines_count ? "Accessibility.ScreenAI.OCR.ImageSize.PDF.WithText"
                    : "Accessibility.ScreenAI.OCR.ImageSize.PDF.NoText",
        image.width() * image.height());

    if (result.has_value()) {
      std::optional<uint64_t> most_detected_language =
          GetMostDetectedLanguageInOcrData(*result);
      if (most_detected_language.has_value()) {
        base::UmaHistogramSparse(
            "Accessibility.ScreenAI.OCR.MostDetectedLanguage.PDF",
            most_detected_language.value());
      }
    }
  }

  ocr_last_used_ = base::TimeTicks::Now();
  return result;
}

void ScreenAIService::SetClientType(mojom::OcrClientType client_type) {
  ocr_client_types_[screen_ai_annotators_.current_receiver()] = client_type;
}

void ScreenAIService::SetClientType(mojom::MceClientType client_type) {
  mce_client_types_[screen2x_main_content_extractors_.current_receiver()] =
      client_type;
}

void ScreenAIService::OcrReceiverDisconnected() {
  auto entry = ocr_client_types_.find(screen_ai_annotators_.current_receiver());
  if (entry != ocr_client_types_.end()) {
    ocr_client_types_.erase(entry);
  }
  // Modify last used time to ensure the service does not shutdown while a
  // client is disconnecting.
  ocr_last_used_ = base::TimeTicks::Now();
}

void ScreenAIService::MceReceiverDisconnected() {
  auto entry = mce_client_types_.find(
      screen2x_main_content_extractors_.current_receiver());
  if (entry != mce_client_types_.end()) {
    mce_client_types_.erase(entry);
  }
  // Modify last used time to ensure the service does not shutdown while a
  // client is disconnecting.
  mce_last_used_ = base::TimeTicks::Now();
}

void ScreenAIService::GetMaxImageDimension(
    GetMaxImageDimensionCallback callback) {
  CHECK(max_ocr_dimension_);
  std::move(callback).Run(max_ocr_dimension_);
}

void ScreenAIService::SetOCRLightMode(bool enabled) {
  const auto client = screen_ai_annotators_.current_receiver();
  if (enabled) {
    light_ocr_clients_.insert(client);
  } else {
    light_ocr_clients_.erase(client);
  }
}

void ScreenAIService::IsOCRBusy(IsOCRBusyCallback callback) {
  std::move(callback).Run(screen_ai_annotators_.size() > 1);
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

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

void ScreenAIService::ExtractMainContent(const ui::AXTreeUpdate& snapshot,
                                         ExtractMainContentCallback callback) {
  ui::AXTree tree;
  std::optional<std::vector<int32_t>> content_node_ids;
  bool success = ExtractMainContentInternalAndRecordMetrics(snapshot, tree,
                                                            content_node_ids);

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
  bool success = ExtractMainContentInternalAndRecordMetrics(snapshot, tree,
                                                            content_node_ids);

  if (success) {
    ui::AXNodeID main_node_id = ComputeMainNode(&tree, *content_node_ids);
    std::move(callback).Run(main_node_id);
  } else {
    std::move(callback).Run(ui::kInvalidAXNodeID);
  }
}

void ScreenAIService::IdentifyMainNode(const ui::AXTreeUpdate& snapshot,
                                       IdentifyMainNodeCallback callback) {
  ui::AXTree tree;
  std::optional<std::vector<int32_t>> content_node_ids;
  bool success = ExtractMainContentInternalAndRecordMetrics(snapshot, tree,
                                                            content_node_ids);

  if (success) {
    ui::AXNodeID main_node_id = ComputeMainNode(&tree, *content_node_ids);
    std::move(callback).Run(tree.GetAXTreeID(), main_node_id);
  } else {
    std::move(callback).Run(ui::AXTreeIDUnknown(), ui::kInvalidAXNodeID);
  }
}

bool ScreenAIService::ExtractMainContentInternalAndRecordMetrics(
    const ui::AXTreeUpdate& snapshot,
    ui::AXTree& tree,
    std::optional<std::vector<int32_t>>& content_node_ids) {
  CHECK(base::Contains(mce_client_types_,
                       screen2x_main_content_extractors_.current_receiver()));
  mce_last_used_ = base::TimeTicks::Now();
  MainContentExtractionClientTypeForMetrics client_type = GetClientType(
      mce_client_types_[screen2x_main_content_extractors_.current_receiver()]);

  static crash_reporter::CrashKeyString<2> mce_client(
      "main_content_extraction_client");
  mce_client.Set(base::StringPrintf("%i", static_cast<int>(client_type)));

  // Early return if input is empty.
  if (snapshot.nodes.empty()) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.MainContentExtraction.Error.SnapshotEmpty",
        client_type);
    return false;
  }

  // Deserialize the snapshot and reserialize it to a view hierarchy proto.
  if (!tree.Unserialize(snapshot)) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.MainContentExtraction.Error."
        "SnapshotUnserialize",
        client_type);
    return false;
  }

  std::optional<ViewHierarchyAndTreeSize> converted_snapshot =
      SnapshotToViewHierarchy(tree);
  if (!converted_snapshot) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.MainContentExtraction.Error.SnapshotProto",
        client_type);
    return false;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  base::SequenceBound<HangTimer> hang_timer(background_task_runner_,
                                            /*is_ocr=*/false);
  hang_timer.AsyncCall(&HangTimer::StartTimer);
  content_node_ids =
      library_->ExtractMainContent(converted_snapshot->serialized_proto);
  hang_timer.AsyncCall(&HangTimer::Stop);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  bool successful =
      content_node_ids.has_value() && content_node_ids->size() > 0;
  base::UmaHistogramBoolean(
      "Accessibility.ScreenAI.MainContentExtraction.Successful2", successful);

  if (!content_node_ids.has_value()) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.MainContentExtraction.Error.ResultNull",
        client_type);
  } else if (content_node_ids->empty()) {
    base::UmaHistogramEnumeration(
        "Accessibility.ScreenAI.MainContentExtraction.Error.ResultEmpty",
        client_type);
  }

  mce_last_used_ = base::TimeTicks::Now();
  if (successful) {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.MainContentExtraction.Latency.Success",
        elapsed_time);
    VLOG(2) << "Screen2x returned " << content_node_ids->size() << " node ids.";
    return true;
  } else {
    base::UmaHistogramTimes(
        "Accessibility.ScreenAI.MainContentExtraction.Latency.Failure",
        elapsed_time);
    VLOG(0) << "Screen2x returned no results.";
    return false;
  }
}

ui::AXNodeID ScreenAIService::ComputeMainNodeForTesting(
    const ui::AXTree* tree,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  return ComputeMainNode(tree, content_node_ids);
}

void ScreenAIService::StartShutDownOnIdleTimer() {
  if (!idle_checking_timer_) {
    idle_checking_timer_ = std::make_unique<base::RepeatingTimer>();
    idle_checking_timer_->Start(FROM_HERE, kIdleCheckingDelay, this,
                                &ScreenAIService::ShutDownOnIdle);
  }
}

void ScreenAIService::ShutDownOnIdle() {
  const base::TimeTicks kIdlenessThreshold =
      base::TimeTicks::Now() - kIdleCheckingDelay;
  if (ocr_last_used_ < kIdlenessThreshold &&
      mce_last_used_ < kIdlenessThreshold) {
    screen_ai_shutdown_handler_->ShuttingDownOnIdle();

    // If OCR was used, record the number of times it's mode was switched.
    if (ocr_last_used_ != base::TimeTicks()) {
      base::UmaHistogramCounts100("Accessibility.ScreenAI.OCR.ModeSwitch",
                                  ocr_mode_switch_count_);
    }

    base::Process::TerminateCurrentProcessImmediately(0);
  }
}

}  // namespace screen_ai
