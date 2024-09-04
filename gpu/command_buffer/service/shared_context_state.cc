// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_context_state.h"

#include <atomic>

#include "base/debug/dump_without_crashing.h"
#include "base/immediate_crash.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gr_cache_controller.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/graphite_cache_controller.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/skia_trace_memory_dump_impl.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/mock/GrMockTypes.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/create_gr_gl_interface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan.h>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/external_semaphore_pool.h"
#endif

#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if BUILDFLAG(SKIA_USE_METAL)
#include "components/viz/common/gpu/metal_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace {

static constexpr size_t kInitialScratchDeserializationBufferSize = 1024;

size_t MaxNumSkSurface() {
  static constexpr size_t kNormalMaxNumSkSurface = 16;
#if BUILDFLAG(IS_ANDROID)
  static constexpr size_t kLowEndMaxNumSkSurface = 4;
  if (base::SysInfo::IsLowEndDevice()) {
    return kLowEndMaxNumSkSurface;
  } else {
    return kNormalMaxNumSkSurface;
  }
#else
  return kNormalMaxNumSkSurface;
#endif
}

// Creates a Graphite recorder, supplying it with a GraphiteImageProvider.
std::unique_ptr<skgpu::graphite::Recorder> MakeGraphiteRecorder(
    skgpu::graphite::Context* context,
    size_t max_resource_cache_bytes,
    size_t max_image_provider_cache_bytes) {
  skgpu::graphite::RecorderOptions options;
  options.fGpuBudgetInBytes = max_resource_cache_bytes;
  options.fImageProvider =
      sk_make_sp<gpu::GraphiteImageProvider>(max_image_provider_cache_bytes);
  return context->makeRecorder(options);
}

// Used to represent Skia backend type for UMA.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SkiaBackendType {
  kUnknown = 0,
  kNone = 1,
  kGaneshGL = 2,
  kGaneshVulkan = 3,
  kGraphiteDawnVulkan = 4,
  kGraphiteDawnMetal = 5,
  kGraphiteDawnD3D11 = 6,
  kGraphiteDawnD3D12 = 7,
  // It's not clear what granularity of kGraphiteDawnGL* backend dawn will
  // provided yet so those values are to be added later.
  kMaxValue = kGraphiteDawnD3D12
};

SkiaBackendType FindSkiaBackendType(SharedContextState* context) {
  switch (context->gr_context_type()) {
    case gpu::GrContextType::kNone:
      return SkiaBackendType::kNone;
    case gpu::GrContextType::kGL:
      return SkiaBackendType::kGaneshGL;
    case gpu::GrContextType::kVulkan:
      return SkiaBackendType::kGaneshVulkan;
    case gpu::GrContextType::kGraphiteMetal:
      // Graphite/Metal isn't expected to be used outside tests.
      return SkiaBackendType::kUnknown;
    case gpu::GrContextType::kGraphiteDawn: {
#if BUILDFLAG(SKIA_USE_DAWN)
      if (!context->dawn_context_provider()) {
        // TODO(kylechar): Bail out of GPU process earlier if
        // DawnContextProvider initialization fails.
        return SkiaBackendType::kUnknown;
      }
      switch (context->dawn_context_provider()->backend_type()) {
        case wgpu::BackendType::Vulkan:
          return SkiaBackendType::kGraphiteDawnVulkan;
        case wgpu::BackendType::D3D11:
          return SkiaBackendType::kGraphiteDawnD3D11;
        case wgpu::BackendType::D3D12:
          return SkiaBackendType::kGraphiteDawnD3D12;
        case wgpu::BackendType::Metal:
          return SkiaBackendType::kGraphiteDawnMetal;
        default:
          break;
      }
#else
      break;
#endif
    }
  }
  return SkiaBackendType::kUnknown;
}

}  // anonymous namespace

void SharedContextState::compileError(const char* shader,
                                      const char* errors,
                                      bool shaderWasCached) {
  if (!context_lost()) {
    LOG(ERROR) << "Skia shader compilation error (was cached = "
               << shaderWasCached << ")" << "\n"
               << "------------------------\n"
               << shader << "\nErrors:\n"
               << errors;

    static crash_reporter::CrashKeyString<8192> shader_key("skia-error-shader");
    shader_key.Set(shader);

    static crash_reporter::CrashKeyString<2048> error_key("skia-compile-error");
    error_key.Set(errors);

    if (shaderWasCached && use_shader_cache_shm_count_ != nullptr) {
      // https://crbug.com/1442633 Sometimes we would fail to compile a cached
      // GLSL shader because of GL driver change. Increase shader cache shm
      // count and crash the GPU process so that the browser process would clear
      // the cache.
      GpuProcessShmCount::ScopedIncrement increment(
          use_shader_cache_shm_count_.get());

      base::ImmediateCrash();
    } else {
      base::debug::DumpWithoutCrashing();
    }
  }
}

SharedContextState::MemoryTrackerObserver::MemoryTrackerObserver(
    base::WeakPtr<gpu::MemoryTracker::Observer> peak_memory_monitor)
    : peak_memory_monitor_(peak_memory_monitor) {}

SharedContextState::MemoryTrackerObserver::~MemoryTrackerObserver() {
  DCHECK(!size_);
}

void SharedContextState::MemoryTrackerObserver::OnMemoryAllocatedChange(
    CommandBufferId id,
    uint64_t old_size,
    uint64_t new_size,
    GpuPeakMemoryAllocationSource source) {
  size_ += new_size - old_size;
  if (source == GpuPeakMemoryAllocationSource::UNKNOWN)
    source = GpuPeakMemoryAllocationSource::SHARED_CONTEXT_STATE;
  if (peak_memory_monitor_) {
    peak_memory_monitor_->OnMemoryAllocatedChange(id, old_size, new_size,
                                                  source);
  }
}

base::AtomicSequenceNumber g_next_command_buffer_id;

SharedContextState::MemoryTracker::MemoryTracker(Observer* observer)
    : command_buffer_id_(gpu::CommandBufferId::FromUnsafeValue(
          g_next_command_buffer_id.GetNext() + 1)),
      client_tracing_id_(base::trace_event::MemoryDumpManager::GetInstance()
                             ->GetTracingProcessId()),
      observer_(observer) {}

SharedContextState::MemoryTracker::~MemoryTracker() {
  DCHECK(!size_);
}

void SharedContextState::MemoryTracker::TrackMemoryAllocatedChange(
    int64_t delta) {
  DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
  uint64_t old_size = size_;
  size_ += delta;
  DCHECK(observer_);
  observer_->OnMemoryAllocatedChange(command_buffer_id_, old_size, size_,
                                     gpu::GpuPeakMemoryAllocationSource::SKIA);
}

uint64_t SharedContextState::MemoryTracker::GetSize() const {
  return size_;
}

uint64_t SharedContextState::MemoryTracker::ClientTracingId() const {
  return client_tracing_id_;
}

int SharedContextState::MemoryTracker::ClientId() const {
  return gpu::ChannelIdFromCommandBufferId(command_buffer_id_);
}

uint64_t SharedContextState::MemoryTracker::ContextGroupTracingId() const {
  return command_buffer_id_.GetUnsafeValue();
}

SharedContextState::SharedContextState(
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<gl::GLSurface> surface,
    scoped_refptr<gl::GLContext> context,
    bool use_virtualized_gl_contexts,
    ContextLostCallback context_lost_callback,
    GrContextType gr_context_type,
    viz::VulkanContextProvider* vulkan_context_provider,
    viz::MetalContextProvider* metal_context_provider,
    DawnContextProvider* dawn_context_provider,
    base::WeakPtr<gpu::MemoryTracker::Observer> peak_memory_monitor,
    bool created_on_compositor_gpu_thread)
    : use_virtualized_gl_contexts_(use_virtualized_gl_contexts),
      context_lost_callback_(std::move(context_lost_callback)),
      gr_context_type_(gr_context_type),
      memory_tracker_observer_(peak_memory_monitor),
      memory_tracker_(&memory_tracker_observer_),
      memory_type_tracker_(&memory_tracker_),
      vk_context_provider_(vulkan_context_provider),
      metal_context_provider_(metal_context_provider),
      dawn_context_provider_(dawn_context_provider),
      created_on_compositor_gpu_thread_(created_on_compositor_gpu_thread),
      share_group_(std::move(share_group)),
      context_(context),
      real_context_(std::move(context)),
      sk_surface_cache_(MaxNumSkSurface()) {
  if (gr_context_type_ == GrContextType::kVulkan) {
    if (vk_context_provider_) {
#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN))
      external_semaphore_pool_ = std::make_unique<ExternalSemaphorePool>(this);
#endif
      use_virtualized_gl_contexts_ = false;
    }
  }

  DCHECK(context_ && surface && context_->default_surface());
  // |this| no longer stores the |surface| as that one must be stored by the
  // context. Do a sanity check that it is true.
  DCHECK(context_->default_surface() == surface);

  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "SharedContextState",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
  // Initialize the scratch buffer to some small initial size.
  scratch_deserialization_buffer_.resize(
      kInitialScratchDeserializationBufferSize);
}

SharedContextState::~SharedContextState() {
  DCHECK(sk_surface_cache_.empty());
  // Delete the transfer cache first: that way, destruction callbacks for image
  // entries can use *|this| to make the context current and do GPU clean up.
  // The context should be current so that texture deletes that result from
  // destroying the cache happen in the right context (unless the context is
  // lost in which case we don't delete the textures).
  DCHECK(IsCurrent(nullptr) || context_lost());
  transfer_cache_.reset();

#if BUILDFLAG(ENABLE_VULKAN) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN))
  external_semaphore_pool_.reset();
#endif

  // We should have the last ref on this GrContext to ensure we're not holding
  // onto any skia objects using this context. Note that some tests don't run
  // InitializeSkia(), so |owned_gr_context_| is not expected to be initialized,
  // and also when using Graphite.
  DCHECK(!owned_gr_context_ || owned_gr_context_->unique());

  // GPU memory allocations except skia_resource_cache_size_ tracked by this
  // memory_tracker_observer_ should have been released.
  DCHECK_EQ(skia_resource_cache_size_,
            memory_tracker_observer_.GetMemoryUsage());
  // gr_context_ and all resources owned by it will be released soon, so set it
  // to null.
  gr_context_ = nullptr;

  // Null out `graphite_context_` as well to ensure that the below call clears
  // memory usage.
  graphite_context_ = nullptr;

  // UpdateSkiaOwnedMemorySize() will update skia memory usage to 0, to ensure
  // that PeakGpuMemoryMonitor sees 0 allocated memory.
  UpdateSkiaOwnedMemorySize();

  // Delete the GrContext. This will either do cleanup if the context is
  // current, or the GrContext was already abandoned if the GLContext was lost.
  owned_gr_context_.reset();

  last_current_surface_ = nullptr;

  if (context_->IsCurrent(nullptr))
    context_->ReleaseCurrent(nullptr);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool SharedContextState::IsUsingGL() const {
  // If context type is none then SharedContextState exists for WebGL fallback
  // to hold a GL context.
  return gr_context_type_ == GrContextType::kGL ||
         gr_context_type_ == GrContextType::kNone;
}

bool SharedContextState::IsGraphiteDawn() const {
  return gr_context_type() == GrContextType::kGraphiteDawn &&
         dawn_context_provider();
}

bool SharedContextState::IsGraphiteMetal() const {
  return gr_context_type() == GrContextType::kGraphiteMetal &&
         metal_context_provider();
}

bool SharedContextState::IsGraphiteDawnMetal() const {
#if BUILDFLAG(SKIA_USE_DAWN)
  return IsGraphiteDawn() &&
         dawn_context_provider()->backend_type() == wgpu::BackendType::Metal;
#else
  return false;
#endif
}

bool SharedContextState::IsGraphiteDawnD3D() const {
#if BUILDFLAG(SKIA_USE_DAWN)
  return IsGraphiteDawn() &&
         (dawn_context_provider()->backend_type() == wgpu::BackendType::D3D11 ||
          dawn_context_provider()->backend_type() == wgpu::BackendType::D3D12);
#else
  return false;
#endif
}

bool SharedContextState::IsGraphiteDawnVulkan() const {
#if BUILDFLAG(SKIA_USE_DAWN)
  return IsGraphiteDawn() &&
         dawn_context_provider()->backend_type() == wgpu::BackendType::Vulkan;
#else
  return false;
#endif
}

bool SharedContextState::IsGraphiteDawnVulkanSwiftShader() const {
#if BUILDFLAG(SKIA_USE_DAWN)
  return IsGraphiteDawn() &&
         dawn_context_provider()->is_vulkan_swiftshader_adapter();
#else
  return false;
#endif
}

bool SharedContextState::InitializeSkia(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    gpu::raster::GrShaderCache* cache,
    GpuProcessShmCount* use_shader_cache_shm_count,
    gl::ProgressReporter* progress_reporter) {
  static crash_reporter::CrashKeyString<16> crash_key("gr-context-type");
  crash_key.Set(GrContextTypeToString(gr_context_type_));

  // Record the Skia backend type the first time Skia/SharedContextState is
  // initialized. This can happen more than once and on different threads but
  // the backend type should never change.
  static std::atomic<bool> once(true);
  if (once.exchange(false, std::memory_order_relaxed)) {
    SkiaBackendType context_enum = FindSkiaBackendType(this);
    base::UmaHistogramEnumeration("GPU.SkiaBackendType", context_enum);
  }

  if (gr_context_type_ == GrContextType::kNone) {
    // SharedContextState only exists to hold a GL context for WebGL fallback
    // if context type is set to none. We don't need to initialization Skia
    // for raster/compositing work.
    return true;
  }

  if (gr_context_type_ == GrContextType::kGraphiteDawn ||
      gr_context_type_ == GrContextType::kGraphiteMetal) {
    return InitializeGraphite(gpu_preferences, workarounds);
  }

  return InitializeGanesh(gpu_preferences, workarounds, cache,
                          use_shader_cache_shm_count, progress_reporter);
}

bool SharedContextState::InitializeGanesh(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    gpu::raster::GrShaderCache* cache,
    GpuProcessShmCount* use_shader_cache_shm_count,
    gl::ProgressReporter* progress_reporter) {
  progress_reporter_ = progress_reporter;
  gr_shader_cache_ = cache;
  use_shader_cache_shm_count_ = use_shader_cache_shm_count;

  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);

  // If you make any changes to the GrContext::Options here that could
  // affect text rendering, make sure to match the capabilities initialized
  // in GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options = GetDefaultGrContextOptions();

  options.fAllowMSAAOnNewIntel = !gles2::MSAAIsSlow(workarounds);
  options.fReduceOpsTaskSplitting = GrContextOptions::Enable::kNo;
  options.fPersistentCache = cache;
  options.fShaderErrorHandler = this;
  if (gpu_preferences.force_max_texture_size)
    options.fMaxTextureSizeOverride = gpu_preferences.force_max_texture_size;

  if (gr_context_type_ == GrContextType::kGL) {
    DCHECK(context_->IsCurrent(nullptr));
    sk_sp<GrGLInterface> gr_gl_interface(gl::init::CreateGrGLInterface(
        *context_->GetVersionInfo(), progress_reporter));
    if (!gr_gl_interface) {
      LOG(ERROR) << "OOP raster support disabled: GrGLInterface creation "
                    "failed.";
      return false;
    }

    if (use_shader_cache_shm_count && cache) {
      // |use_shader_cache_shm_count| is safe to capture here since it must
      // outlive the this context state.
      gr_gl_interface->fFunctions.fProgramBinary =
          [use_shader_cache_shm_count](GrGLuint program, GrGLenum binaryFormat,
                                       void* binary, GrGLsizei length) {
            GpuProcessShmCount::ScopedIncrement increment(
                use_shader_cache_shm_count);
            glProgramBinary(program, binaryFormat, binary, length);
          };
    }
    options.fDriverBugWorkarounds =
        GrDriverBugWorkarounds(workarounds.ToIntSet());
    options.fAvoidStencilBuffers = workarounds.avoid_stencil_buffers;
    if (workarounds.disable_program_disk_cache) {
      options.fShaderCacheStrategy =
          GrContextOptions::ShaderCacheStrategy::kBackendSource;
    }
    options.fPreferExternalImagesOverES3 = true;

    if (gl::GetGLImplementation() == gl::kGLImplementationStubGL) {
      // gl::kGLImplementationStubGL doesn't implement enough functions for
      // successful GrContext::MakeGL initialization. Fallback to mock context
      // instead.
      GrMockOptions mock_options;
      owned_gr_context_ = GrDirectContext::MakeMock(&mock_options, options);
      DCHECK(owned_gr_context_);
    } else {
      owned_gr_context_ =
          GrDirectContexts::MakeGL(std::move(gr_gl_interface), options);
    }

    gr_context_ = owned_gr_context_.get();
  } else {
    CHECK_EQ(gr_context_type_, GrContextType::kVulkan);
#if BUILDFLAG(ENABLE_VULKAN)
    if (vk_context_provider_) {
      if (!vk_context_provider_->InitializeGrContext(options)) {
        LOG(ERROR) << "Failed to initialize GrContext for Vulkan.";
        return false;
      }
      gr_context_ = vk_context_provider_->GetGrContext();
      DCHECK(gr_context_);
    }
#endif
  }

  if (!gr_context_) {
    LOG(ERROR) << "OOP raster support disabled: GrContext creation failed.";
    return false;
  }

  gr_context_->setResourceCacheLimit(max_resource_cache_bytes);
  transfer_cache_ = std::make_unique<ServiceTransferCache>(
      gpu_preferences,
      base::BindRepeating(&SharedContextState::ScheduleSkiaCleanup,
                          base::Unretained(this)));
  gr_cache_controller_ = std::make_unique<raster::GrCacheController>(this);
  is_drdc_enabled_ = features::IsDrDcEnabled() && !workarounds.disable_drdc;
  return true;
}

bool SharedContextState::InitializeGraphite(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds) {
  const skgpu::graphite::ContextOptions context_options =
      GetDefaultGraphiteContextOptions(workarounds);

  if (gr_context_type_ == GrContextType::kGraphiteDawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    CHECK(dawn_context_provider_);
    if (dawn_context_provider_->InitializeGraphiteContext(context_options)) {
      graphite_context_ = dawn_context_provider_->GetGraphiteContext();
    } else {
      // There is currently no way for the GPU process to gracefully handle
      // failure to initialize Dawn, leaving the user in an unknown state if we
      // allow GPU process initialization to continue. Intentionally crash the
      // GPU process in this case to trigger browser-side fallback logic (either
      // to software or to Ganesh depending on the platform).
      // TODO(crbug.com/325000752): Handle this case within the GPU process.
      CHECK(0);
    }
#endif
  } else {
    CHECK_EQ(gr_context_type_, GrContextType::kGraphiteMetal);
#if BUILDFLAG(SKIA_USE_METAL)
    if (metal_context_provider_ &&
        metal_context_provider_->InitializeGraphiteContext(context_options)) {
      graphite_context_ = metal_context_provider_->GetGraphiteContext();
    } else {
      DLOG(ERROR) << "Failed to create Graphite Context for Metal";
      return false;
    }
#endif
  }
  if (!graphite_context_) {
    LOG(ERROR) << "Skia Graphite disabled: Graphite Context creation failed.";
    return false;
  }

  // We need image providers for both the OOP-R (gpu_main) recorder and the
  // SkiaRenderer (viz thread) recorder, as both need to process CPU-backed
  // images (for the SkiaRenderer recorder, this occurs in special cases such as
  // an SVG/CSS filter effect that references an image but that got the effect
  // promoted to composited).
  size_t max_gpu_main_image_provider_cache_bytes = 0;
  size_t max_viz_compositor_image_provider_cache_bytes = 0;
  DetermineGraphiteImageProviderCacheLimits(
      &max_gpu_main_image_provider_cache_bytes,
      &max_viz_compositor_image_provider_cache_bytes);

  gpu_main_graphite_recorder_ =
      MakeGraphiteRecorder(graphite_context_, context_options.fGpuBudgetInBytes,
                           max_gpu_main_image_provider_cache_bytes);
  gpu_main_graphite_cache_controller_ =
      base::MakeRefCounted<raster::GraphiteCacheController>(
          gpu_main_graphite_recorder_.get(), graphite_context_.get(),
          dawn_context_provider_);

  viz_compositor_graphite_recorder_ =
      MakeGraphiteRecorder(graphite_context_, context_options.fGpuBudgetInBytes,
                           max_viz_compositor_image_provider_cache_bytes);

  transfer_cache_ = std::make_unique<ServiceTransferCache>(
      gpu_preferences,
      base::BindRepeating(&SharedContextState::ScheduleSkiaCleanup,
                          base::Unretained(this)));
  return true;
}

bool SharedContextState::InitializeGL(
    const GpuPreferences& gpu_preferences,
    scoped_refptr<gles2::FeatureInfo> feature_info) {
  // We still need initialize GL when Vulkan is used, because RasterDecoder
  // depends on GL.
  // TODO(penghuang): don't initialize GL when RasterDecoder can work without
  // GL.
  if (IsGLInitialized()) {
    DCHECK(feature_info == feature_info_);
    DCHECK(context_state_);
    return true;
  }

  DCHECK(context_->IsCurrent(nullptr));

  bool use_passthrough_cmd_decoder =
      gpu_preferences.use_passthrough_cmd_decoder &&
      gles2::PassthroughCommandDecoderSupported();
  // Virtualized contexts don't work with passthrough command decoder.
  // See https://crbug.com/914976
  DCHECK(!use_passthrough_cmd_decoder || !use_virtualized_gl_contexts_);

  feature_info_ = std::move(feature_info);
  feature_info_->Initialize(feature_info_->context_type(),
                            use_passthrough_cmd_decoder,
                            gles2::DisallowedFeatures());

  auto* api = gl::g_current_gl_context;
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  GLint max_vertex_attribs = 0;
  api->glGetIntegervFn(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  if (max_vertex_attribs < kGLES2RequiredMinimumVertexAttribs) {
    LOG(ERROR)
        << "SharedContextState::InitializeGL failure max_vertex_attribs : "
        << max_vertex_attribs << " is less that minimum required : "
        << kGLES2RequiredMinimumVertexAttribs;
    feature_info_ = nullptr;
    return false;
  }

  const GLint kGLES2RequiredMinimumTextureUnits = 8u;
  GLint max_texture_units = 0;
  api->glGetIntegervFn(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_texture_units);
  if (max_texture_units < kGLES2RequiredMinimumTextureUnits) {
    LOG(ERROR)
        << "SharedContextState::InitializeGL failure max_texture_units : "
        << max_texture_units << " is less that minimum required : "
        << kGLES2RequiredMinimumTextureUnits;
    feature_info_ = nullptr;
    return false;
  }

  context_state_ = std::make_unique<gles2::ContextState>(
      feature_info_.get(), false /* track_texture_and_sampler_units */);

  context_state_->set_api(api);
  context_state_->InitGenericAttribs(max_vertex_attribs);

  // Set all the default state because some GL drivers get it wrong.
  // TODO(backer): Not all of this state needs to be initialized. Reduce the set
  // if perf becomes a problem.
  context_state_->InitCapabilities(nullptr);
  context_state_->InitState(nullptr);

  // Init |sampler_units|, ContextState uses the size of it to reset sampler to
  // ground state.
  // TODO(penghuang): remove it when GrContext is created with ES 3.0.
  context_state_->sampler_units.resize(max_texture_units);

  GLenum driver_status = real_context_->CheckStickyGraphicsResetStatus();
  if (driver_status != GL_NO_ERROR) {
    // If the context was lost at any point before or during initialization,
    // the values queried from the driver could be bogus, and potentially
    // inconsistent between various ContextStates on the same underlying real
    // GL context. Make sure to report the failure early, to not allow
    // virtualized context switches in that case.
    LOG(ERROR) << "SharedContextState::InitializeGL failure driver error : "
               << driver_status;
    feature_info_ = nullptr;
    context_state_ = nullptr;
    return false;
  }

  if (use_virtualized_gl_contexts_) {
    auto virtual_context = base::MakeRefCounted<GLContextVirtual>(
        share_group_.get(), real_context_.get(),
        weak_ptr_factory_.GetWeakPtr());
    if (!virtual_context->Initialize(surface(), gl::GLContextAttribs())) {
      LOG(ERROR) << "SharedContextState::InitializeGL failure Initialize "
                    "virtual context failed";
      feature_info_ = nullptr;
      context_state_ = nullptr;
      return false;
    }
    context_ = std::move(virtual_context);
    MakeCurrent(nullptr);
  }

  bool gl_supports_memory_object =
      gl::g_current_gl_driver->ext.b_GL_EXT_memory_object_fd ||
      gl::g_current_gl_driver->ext.b_GL_EXT_memory_object_win32 ||
      gl::g_current_gl_driver->ext.b_GL_ANGLE_memory_object_fuchsia;
  bool gl_supports_semaphore =
      gl::g_current_gl_driver->ext.b_GL_EXT_semaphore_fd ||
      gl::g_current_gl_driver->ext.b_GL_EXT_semaphore_win32 ||
      gl::g_current_gl_driver->ext.b_GL_ANGLE_semaphore_fuchsia;

  bool vk_supports_external_memory = false;
  bool vk_supports_external_semaphore = false;
  gpu::VulkanImplementationName vulkan_implementation =
      gpu::VulkanImplementationName::kNone;
#if BUILDFLAG(ENABLE_VULKAN)
  if (vk_context_provider_) {
    vulkan_implementation =
        vk_context_provider_->GetVulkanImplementation()->use_swiftshader()
            ? gpu::VulkanImplementationName::kSwiftshader
            : gpu::VulkanImplementationName::kNative;
    auto* device_queue = vk_context_provider_->GetDeviceQueue();
#if BUILDFLAG(IS_WIN)
    vk_supports_external_memory =
        gfx::HasExtension(device_queue->enabled_extensions(),
                          VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#elif BUILDFLAG(IS_FUCHSIA)
    vk_supports_external_memory =
        gfx::HasExtension(device_queue->enabled_extensions(),
                          VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME);
#else
    vk_supports_external_memory =
        gfx::HasExtension(device_queue->enabled_extensions(),
                          VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif
    vk_supports_external_semaphore =
        IsVkOpaqueExternalSemaphoreSupported(device_queue);
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)

  const bool is_native_vulkan_and_gl =
      vulkan_implementation == gpu::VulkanImplementationName::kNative &&
      !gl::g_current_gl_version->is_swiftshader &&
      !gl::g_current_gl_version->is_angle_swiftshader;

  // Swiftshader GL reports supporting external objects extensions, but doesn't.
  // However, Swiftshader Vulkan and ANGLE can interop using external objects.
  const bool is_swiftshader_vulkan_and_gl =
      vulkan_implementation == gpu::VulkanImplementationName::kSwiftshader &&
      gl::g_current_gl_version->is_angle_swiftshader;

  support_vulkan_external_object_ =
      (is_native_vulkan_and_gl || is_swiftshader_vulkan_and_gl) &&
      gl_supports_memory_object && gl_supports_semaphore &&
      vk_supports_external_memory && vk_supports_external_semaphore;

  support_gl_external_object_flags_ =
      gl::g_current_gl_driver->ext.b_GL_ANGLE_memory_object_flags;

  return true;
}

void SharedContextState::FlushGraphiteRecorder() {
  auto recording = gpu_main_graphite_recorder()->snap();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info = {};
    info.fRecording = recording.get();
    graphite_context()->insertRecording(info);
  }
}

void SharedContextState::FlushAndSubmit(bool sync_to_cpu) {
  if (graphite_context()) {
    FlushGraphiteRecorder();
    graphite_context()->submit(sync_to_cpu ? skgpu::graphite::SyncToCpu::kYes
                                           : skgpu::graphite::SyncToCpu::kNo);
  } else if (gr_context()) {
    gr_context()->flushAndSubmit(sync_to_cpu ? GrSyncCpu::kYes
                                             : GrSyncCpu::kNo);
  }
}

void SharedContextState::FlushWriteAccess(
    SkiaImageRepresentation::ScopedWriteAccess* access) {
  static int flush_count = 0;
  const base::TimeTicks start = base::TimeTicks::Now();
  if (graphite_context()) {
    // The only way to flush GPU work with Graphite is to snap and insert a
    // recording here. It's also necessary to submit before dropping the scoped
    // access since we want the Dawn texture to be alive on submit, but that's
    // handled in SubmitIfNecessary.
    FlushGraphiteRecorder();
  } else {
    if (access->HasBackendSurfaceEndState()) {
      access->ApplyBackendSurfaceEndState();
      // ApplyBackendSurfaceEndState flushes the surfaces so skip flushing here.
    } else if (access->has_surfaces()) {
      // Flush surfaces only if the write access was created with surfaces.
      int num_planes = access->representation()->format().NumberOfPlanes();
      for (int plane_index = 0; plane_index < num_planes; plane_index++) {
        auto* surface = access->surface(plane_index);
        DCHECK(surface);
        skgpu::ganesh::Flush(surface);
      }
    }
  }
  if (flush_count < 100) {
    ++flush_count;
    base::UmaHistogramCustomMicrosecondsTimes(
        "GPU.RasterDecoder.TimeToFlush", base::TimeTicks::Now() - start,
        base::Microseconds(1), base::Seconds(1), 100);
  }
}

void SharedContextState::SubmitIfNecessary(
    std::vector<GrBackendSemaphore> signal_semaphores,
    bool need_graphite_submit) {
  if (graphite_context() && need_graphite_submit) {
    // It's necessary to submit before dropping a scoped access since we want
    // the Dawn texture to be alive on submit.
    // NOTE: Graphite uses Dawn and the Graphite SharedImage representation does
    // not set semaphores.
    // TODO(crbug.com/328104159): Skip submit if supported by the shared image
    // and DrDC is not enabled.
    CHECK(signal_semaphores.empty());
    graphite_context()->submit(skgpu::graphite::SyncToCpu::kNo);
    return;
  }

  // Do nothing here if there is no context.
  if (!gr_context()) {
    return;
  }

  // Note that when DrDc is enabled, we need to call
  // AddVulkanCleanupTaskForSkiaFlush() on gpu main thread and do skia flush.
  // This will ensure that vulkan memory allocated on gpu main thread will be
  // cleaned up.
  if (!signal_semaphores.empty() || is_drdc_enabled_) {
    // NOTE: The Graphite SharedImage representation does not set semaphores,
    // and we are not enabling DrDC with Graphite.
    CHECK(gr_context());
    GrFlushInfo flush_info = {
        .fNumSemaphores = signal_semaphores.size(),
        .fSignalSemaphores = signal_semaphores.data(),
    };
    gpu::AddVulkanCleanupTaskForSkiaFlush(vk_context_provider(), &flush_info);

    auto result = gr_context()->flush(flush_info);
    DCHECK_EQ(result, GrSemaphoresSubmitted::kYes);
  }

  bool sync_cpu = gpu::ShouldVulkanSyncCpuForSkiaSubmit(vk_context_provider());

  // If DrDc is enabled, submit the gr_context() to ensure correct ordering
  // of vulkan commands between raster and display compositor.
  // TODO(vikassoni): This submit could be happening more often than
  // intended resulting in perf penalty. Explore ways to reduce it by
  // trying to issue submit only once per draw call for both gpu main and
  // drdc thread gr_context. Also add metric to see how often submits are
  // happening per frame.
  const bool need_submit =
      sync_cpu || !signal_semaphores.empty() || is_drdc_enabled_;

  if (need_submit) {
    CHECK(gr_context());
    gr_context()->submit(sync_cpu ? GrSyncCpu::kYes : GrSyncCpu::kNo);
  }
}

bool SharedContextState::MakeCurrent(gl::GLSurface* surface, bool needs_gl) {
  if (context_lost()) {
    LOG(ERROR) << "Failed to make current since context is marked as lost";
    return false;
  }

  const bool using_gl = IsUsingGL() || needs_gl;
  if (using_gl) {
    gl::GLSurface* dont_care_surface = last_current_surface_
                                           ? last_current_surface_.get()
                                           : context_->default_surface();
    surface = surface ? surface : dont_care_surface;

    if (!context_->MakeCurrent(surface)) {
      MarkContextLost(error::kMakeCurrentFailed);
      return false;
    }
    last_current_surface_ = surface;
  }

  return !CheckResetStatus(needs_gl);
}

void SharedContextState::ReleaseCurrent(gl::GLSurface* surface) {
  if (!surface)
    surface = last_current_surface_;

  if (surface != last_current_surface_)
    return;

  last_current_surface_ = nullptr;
  if (!context_lost())
    context_->ReleaseCurrent(surface);
}

void SharedContextState::MarkContextLost(error::ContextLostReason reason) {
  if (!context_lost()) {
    scoped_refptr<SharedContextState> prevent_last_ref_drop = this;
    context_lost_reason_ = reason;

    // Notify |context_lost_callback_| and |context_lost_observers_| first,
    // since maybe they still need the GrDirectContext for releasing some skia
    // resources.
    std::move(context_lost_callback_)
        .Run(!device_needs_reset_, context_lost_reason_.value());
    for (auto& observer : context_lost_observers_)
      observer.OnContextLost();

    // context_state_ could be nullptr for some unittests.
    if (context_state_)
      context_state_->MarkContextLost();

    // Only abandon the GrContext if it is owned by SharedContextState, because
    // the passed in GrContext will be reused.
    // TODO(crbug.com/40672147): always abandon GrContext to release all
    // resources when chrome goes into background with low end device.
    if (owned_gr_context_) {
      owned_gr_context_->abandonContext();
      owned_gr_context_.reset();
      gr_context_ = nullptr;
    }
    UpdateSkiaOwnedMemorySize();
  }
}

bool SharedContextState::IsCurrent(gl::GLSurface* surface, bool needs_gl) {
  if (!IsUsingGL() && !needs_gl) {
    return true;
  }
  if (context_lost())
    return false;
  return context_->IsCurrent(surface);
}

// TODO(https://crbug.com/1110357): Account for memory tracked by
// memory_tracker_ and memory_type_tracker_ (e.g. SharedImages allocated in
// SkiaOutputSurfaceImplOnGpu::CopyOutput).
bool SharedContextState::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  bool background = args.level_of_detail ==
                    base::trace_event::MemoryDumpLevelOfDetail::kBackground;
  if (gr_context()) {
    if (background) {
      raster::DumpBackgroundGrMemoryStatistics(gr_context(), pmd);
    } else {
      raster::DumpGrMemoryStatistics(gr_context(), pmd, std::nullopt);
    }
  } else if (graphite_context()) {
    // NOTE: We cannot dump the memory statistics of the Viz compositor
    // recorder here because it can be called only on the Viz thread. Instead,
    // we dump it in SkiaOutputSurfaceImpl.
    if (background) {
      DumpBackgroundGraphiteMemoryStatistics(graphite_context(),
                                             gpu_main_graphite_recorder(), pmd);
    } else {
      // Note: The image provider's allocations are already counted in Skia's
      // unbudgeted (client) resource allocations so we skip emitted them here.
      skia::SkiaTraceMemoryDumpImpl trace_memory_dump(args.level_of_detail,
                                                      pmd);
      graphite_context()->dumpMemoryStatistics(&trace_memory_dump);
      gpu_main_graphite_recorder()->dumpMemoryStatistics(&trace_memory_dump);
    }
  }

  return true;
}

void SharedContextState::AddContextLostObserver(ContextLostObserver* obs) {
  context_lost_observers_.AddObserver(obs);
}

void SharedContextState::RemoveContextLostObserver(ContextLostObserver* obs) {
  context_lost_observers_.RemoveObserver(obs);
}

void SharedContextState::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // Ensure the context is current before doing any GPU cleanup.
  if (!MakeCurrent(nullptr))
    return;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, clear any unlocked resources.
      sk_surface_cache_.Clear();
      if (gr_context_) {
        gr_context_->purgeUnlockedResources(
            GrPurgeResourceOptions::kScratchResourcesOnly);
      } else if (gpu_main_graphite_cache_controller_) {
        gpu_main_graphite_cache_controller_->CleanUpScratchResources();
      }
      UpdateSkiaOwnedMemorySize();
      scratch_deserialization_buffer_.resize(
          kInitialScratchDeserializationBufferSize);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      sk_surface_cache_.Clear();
      {
        std::optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
        // ScopedCacheUse is to avoid the empty/invalid client id DCHECKS caused
        // while accessing GrShaderCache. Note that since the actual client_id
        // here does not matter, we are using gpu::kDisplayCompositorClientId.
        UseShaderCache(cache_use, kDisplayCompositorClientId);
        if (gr_context_) {
          gr_context_->freeGpuResources();
        } else if (gpu_main_graphite_cache_controller_) {
          gpu_main_graphite_cache_controller_->CleanUpAllResources();
        }
      }
      UpdateSkiaOwnedMemorySize();
      scratch_deserialization_buffer_.resize(0u);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
  }

  if (transfer_cache_)
    transfer_cache_->PurgeMemory(memory_pressure_level);
}

uint64_t SharedContextState::GetMemoryUsage() {
  UpdateSkiaOwnedMemorySize();
  return memory_tracker_observer_.GetMemoryUsage();
}

void SharedContextState::UpdateSkiaOwnedMemorySize() {
  // NOTE: If `graphite_context_` is null, then either (a) it was not
  // successfully created or (b) this instance is being destroyed. In the former
  // case, the Graphite GPU main recorder will also not have been created, while
  // in the latter case, it will imminently be destroyed.
  if (!gr_context_ && !graphite_context_) {
    memory_tracker_observer_.OnMemoryAllocatedChange(
        CommandBufferId(), skia_resource_cache_size_, 0u);
    skia_resource_cache_size_ = 0u;
    return;
  }
  size_t new_size;
  if (gr_context_) {
    gr_context_->getResourceCacheUsage(nullptr /* resourceCount */, &new_size);
  } else {
    // NOTE: If `graphite_context_` is non-null, the GPU main recorder is
    // guaranteed to be non-null as well. Add the image provider's size too
    // since with Graphite that's owned by Chrome rather than Skia as in Ganesh.
    const auto* image_provider = static_cast<const gpu::GraphiteImageProvider*>(
        gpu_main_graphite_recorder_->clientImageProvider());
    new_size = graphite_context_->currentBudgetedBytes() +
               gpu_main_graphite_recorder_->currentBudgetedBytes() +
               image_provider->CurrentSizeInBytes();
  }
  // Skia does not have a CommandBufferId. PeakMemoryMonitor currently does not
  // use CommandBufferId to identify source, so use zero here to separate
  // prevent confusion.
  memory_tracker_observer_.OnMemoryAllocatedChange(
      CommandBufferId(), skia_resource_cache_size_,
      static_cast<uint64_t>(new_size));
  skia_resource_cache_size_ = static_cast<uint64_t>(new_size);
}

void SharedContextState::PessimisticallyResetGrContext() const {
  // Calling GrContext::resetContext() is very cheap, so we do it
  // pessimistically. We could dirty less state if skia state setting
  // performance becomes an issue.
  if (gr_context_ && GrContextIsGL())
    gr_context_->resetContext();
}

void SharedContextState::StoreVkPipelineCacheIfNeeded() {
  // GrShaderCache::StoreVkPipelineCacheIfNeeded() must be called only for gpu
  // main thread. Hence using |created_on_compositor_gpu_thread_| to avoid
  // calling it for CompositorGpuThread when DrDc is enabled. See
  // GrShaderCache::StoreVkPipelineCacheIfNeeded for more details.
  if (gr_context_ && gr_shader_cache_ && GrContextIsVulkan() &&
      !created_on_compositor_gpu_thread_) {
    gpu::raster::GrShaderCache::ScopedCacheUse use(gr_shader_cache_,
                                                   kDisplayCompositorClientId);
    gr_shader_cache_->StoreVkPipelineCacheIfNeeded(gr_context_);
  }
}

void SharedContextState::UseShaderCache(
    std::optional<gpu::raster::GrShaderCache::ScopedCacheUse>& cache_use,
    int32_t client_id) const {
  if (gr_shader_cache_) {
    cache_use.emplace(gr_shader_cache_, client_id);
  }
}

gl::GLSurface* SharedContextState::surface() const {
  return context_->default_surface();
}

gl::GLDisplay* SharedContextState::display() {
  auto* gl_surface = surface();
  DCHECK(gl_surface);
  return gl_surface->GetGLDisplay();
}

bool SharedContextState::initialized() const {
  return true;
}

const gles2::ContextState* SharedContextState::GetContextState() {
  if (need_context_state_reset_) {
    // Returning nullptr to force full state restoration by the caller.  We do
    // this because GrContext changes to GL state are untracked in our
    // context_state_.
    return nullptr;
  }
  return context_state_.get();
}

void SharedContextState::RestoreState(const gles2::ContextState* prev_state) {
  PessimisticallyResetGrContext();
  context_state_->RestoreState(prev_state);
  need_context_state_reset_ = false;
}

void SharedContextState::RestoreGlobalState() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreGlobalState(nullptr);
}
void SharedContextState::ClearAllAttributes() const {}

void SharedContextState::RestoreActiveTexture() const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreAllTextureUnitAndSamplerBindings(
    const gles2::ContextState* prev_state) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreBufferBinding(unsigned int target) {
  PessimisticallyResetGrContext();
  if (target == GL_PIXEL_PACK_BUFFER) {
    context_state_->UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    context_state_->UpdateUnpackParameters();
  }
  context_state_->api()->glBindBufferFn(target, 0);
}

void SharedContextState::RestoreBufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreBufferBindings();
}

void SharedContextState::RestoreFramebufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->fbo_binding_for_scissor_workaround_dirty = true;
  context_state_->stencil_state_changed_since_validation = true;
}

void SharedContextState::RestoreRenderbufferBindings() {
  PessimisticallyResetGrContext();
  context_state_->RestoreRenderbufferBindings();
}

void SharedContextState::RestoreProgramBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreProgramSettings(nullptr, false);
}

void SharedContextState::RestoreTextureUnitBindings(unsigned unit) const {
  PessimisticallyResetGrContext();
}

void SharedContextState::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SharedContextState::RestoreAllExternalTextureBindingsIfNeeded() {
  PessimisticallyResetGrContext();
}

QueryManager* SharedContextState::GetQueryManager() {
  return nullptr;
}

std::optional<error::ContextLostReason> SharedContextState::GetResetStatus(
    bool needs_gl) {
  DCHECK(!context_lost());

  if (gr_context_) {
    // Maybe Skia detected VK_ERROR_DEVICE_LOST.
    if (gr_context_->abandoned()) {
      LOG(ERROR) << "SharedContextState context lost via Skia.";
      return error::kUnknown;
    }

    if (gr_context_->oomed()) {
      LOG(ERROR) << "SharedContextState context lost via Skia OOM.";
      return error::kOutOfMemory;
    }
  }

#if BUILDFLAG(SKIA_USE_DAWN)
  if (gr_context_type_ == GrContextType::kGraphiteDawn &&
      dawn_context_provider_) {
    return dawn_context_provider_->GetResetStatus();
  }
#endif

  // Not using GL.
  if (!IsUsingGL() && !needs_gl) {
    return std::nullopt;
  }

  // GL is not initialized.
  if (!context_state_)
    return std::nullopt;

  GLenum error;
  while ((error = context_state_->api()->glGetErrorFn()) != GL_NO_ERROR) {
    if (error == GL_OUT_OF_MEMORY) {
      LOG(ERROR) << "SharedContextState lost due to GL_OUT_OF_MEMORY";
      return error::kOutOfMemory;
    }
    if (error == GL_CONTEXT_LOST_KHR)
      break;
  }
  // Checking the reset status is expensive on some OS/drivers
  // (https://crbug.com/1090232). Rate limit it.
  constexpr base::TimeDelta kMinCheckDelay = base::Milliseconds(5);
  base::Time now = base::Time::Now();
  if (!disable_check_reset_status_throttling_for_test_ &&
      now < last_gl_check_graphics_reset_status_ + kMinCheckDelay) {
    return std::nullopt;
  }
  last_gl_check_graphics_reset_status_ = now;

  GLenum driver_status = context()->CheckStickyGraphicsResetStatus();
  if (driver_status == GL_NO_ERROR)
    return std::nullopt;
  LOG(ERROR) << "SharedContextState context lost via ARB/EXT_robustness. Reset "
                "status = "
             << gles2::GLES2Util::GetStringEnum(driver_status);

  switch (driver_status) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::nullopt;
}

bool SharedContextState::CheckResetStatus(bool need_gl) {
  DCHECK(!context_lost());
  DCHECK(!device_needs_reset_);

  auto status = GetResetStatus(need_gl);
  if (status.has_value()) {
    device_needs_reset_ = true;
    MarkContextLost(status.value());
    return true;
  }
  return false;
}

void SharedContextState::ScheduleSkiaCleanup() {
  if (!MakeCurrent(nullptr)) {
    return;
  }
  if (gr_cache_controller_) {
    gr_cache_controller_->ScheduleGrContextCleanup();
  }
  if (gpu_main_graphite_cache_controller_) {
    gpu_main_graphite_cache_controller_->ScheduleCleanup();
  }
}

int32_t SharedContextState::GetMaxTextureSize() {
  if (max_texture_size_.has_value()) {
    return max_texture_size_.value();
  }
  int32_t max_texture_size = 0;
  if (IsUsingGL()) {
    gl::GLApi* const api = gl::g_current_gl_context;
    api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  } else if (GrContextIsVulkan()) {
#if BUILDFLAG(ENABLE_VULKAN)
    max_texture_size = vk_context_provider()
                           ->GetDeviceQueue()
                           ->vk_physical_device_properties()
                           .limits.maxImageDimension2D;
#else
    NOTREACHED();
#endif
  } else {
#if BUILDFLAG(SKIA_USE_DAWN)
    if (dawn_context_provider()) {
      wgpu::SupportedLimits limits = {};
      auto succeded = dawn_context_provider()->GetDevice().GetLimits(&limits);
      CHECK(succeded);
      max_texture_size = limits.limits.maxTextureDimension2D;
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
#if BUILDFLAG(SKIA_USE_METAL)
    if (metal_context_provider()) {
      // This is a development only code path, so just assume 16K since that
      // should be supported on non-ancient HW and ARM Macs in particular.
      max_texture_size = 16384;
    }
#endif  // BUILDFLAG(SKIA_USE_METAL)
  }
  DCHECK_GT(max_texture_size, 0);
  max_texture_size_ = max_texture_size;
  return max_texture_size;
}

#if BUILDFLAG(IS_WIN)
Microsoft::WRL::ComPtr<ID3D11Device> SharedContextState::GetD3D11Device()
    const {
  switch (gr_context_type_) {
    case GrContextType::kNone:
      return nullptr;
    case GrContextType::kGL:
    case GrContextType::kVulkan:
      return gl::QueryD3D11DeviceObjectFromANGLE();
#if BUILDFLAG(SKIA_USE_DAWN)
    case GrContextType::kGraphiteDawn:
      return dawn_context_provider_->GetD3D11Device();
#endif
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}
#endif

}  // namespace gpu
