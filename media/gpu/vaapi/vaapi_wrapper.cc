// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vaapi_wrapper.h"

#include <dlfcn.h>
#include <drm_fourcc.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_str.h>
#include <va/va_version.h>
#include <xf86drm.h>

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/cpu.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/platform_features.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/macros.h"
// Auto-generated for dlopen libva libraries
#include "media/gpu/vaapi/va_stubs.h"
#include "media/media_buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/libva_protected_content/va_protected_content.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <va/va_prot.h>
using media_gpu_vaapi::kModuleVa_prot;
#endif

#if BUILDFLAG(IS_LINUX)
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#endif

using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::IsVa_drmInitialized;
using media_gpu_vaapi::IsVaInitialized;
using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
using media_gpu_vaapi::StubPathMap;

namespace media {

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "VaapiFunctions" in src/tools/metrics/histograms/enums.xml.
enum class VaapiFunctions {
  kVABeginPicture = 0,
  kVACreateBuffer = 1,
  kVACreateConfig = 2,
  kVACreateContext = 3,
  kVACreateImage = 4,
  kVACreateSurfaces_Allocating = 5,
  kVACreateSurfaces_Importing = 6,
  kVADestroyBuffer = 7,
  kVADestroyConfig = 8,
  kVADestroyContext = 9,
  kVADestroySurfaces = 10,
  kVAEndPicture = 11,
  kVAExportSurfaceHandle = 12,
  kVAGetConfigAttributes = 13,
  kVAPutImage = 14,
  // kVAPutSurface = 15,  // UNUSED.
  kVAQueryConfigAttributes = 16,
  kVAQueryImageFormats = 17,
  kVAQuerySurfaceAttributes = 18,
  // kVAQueryVideoProcPipelineCaps = 19,  // UNUSED.
  kVARenderPicture_VABuffers = 20,
  kVARenderPicture_Vpp = 21,
  kVASyncSurface = 22,
  kVATerminate = 23,
  kVAUnmapBuffer = 24,
  // Protected mode functions below.
  kVACreateProtectedSession = 25,
  kVADestroyProtectedSession = 26,
  kVAAttachProtectedSession = 27,
  kVADetachProtectedSession = 28,
  kVAProtectedSessionHwUpdate_Deprecated = 29,
  kVAProtectedSessionExecute = 30,
  // Anything else is captured in this last entry.
  kOtherVAFunction = 31,
  kMaxValue = kOtherVAFunction,
};

void ReportVaapiErrorToUMA(const std::string& histogram_name,
                           VaapiFunctions value) {
  UMA_HISTOGRAM_ENUMERATION(histogram_name, value);
}

constexpr std::array<const char*,
                     static_cast<size_t>(VaapiFunctions::kMaxValue) + 1>
    kVaapiFunctionNames = {
        "vaBeginPicture",
        "vaCreateBuffer",
        "vaCreateConfig",
        "vaCreateContext",
        "vaCreateImage",
        "vaCreateSurfaces (allocate mode)",
        "vaCreateSurfaces (import mode)",
        "vaDestroyBuffer",
        "vaDestroyConfig",
        "vaDestroyContext",
        "vaDestroySurfaces",
        "vaEndPicture",
        "vaExportSurfaceHandle",
        "vaGetConfigAttributes",
        "vaPutImage",
        "",  // UNUSED (used to be vaPutSurface).
        "vaQueryConfigAttributes",
        "vaQueryImageFormats",
        "vaQuerySurfaceAttributes",
        "",  // UNUSED (used to be vaQueryVideoProcPipelineCaps).
        "vaRenderPicture (|pending_va_buffers_|)",
        "vaRenderPicture using Vpp",
        "vaSyncSurface",
        "vaTerminate",
        "vaUnmapBuffer",
        "vaCreateProtectedSession",
        "vaDestroyProtectedSession",
        "vaAttachProtectedSession",
        "vaDetachProtectedSession",
        "vaProtectedSessionHwUpdate (Deprecated)",
        "vaProtectedSessionExecute",
        "Other VA function"};

// Translates |function| into a human readable string for logging.
const char* VaapiFunctionName(VaapiFunctions function) {
  DCHECK(function <= VaapiFunctions::kMaxValue);
  return kVaapiFunctionNames[static_cast<size_t>(function)];
}

// This class is a wrapper around its |va_display_| (and its associated
// |va_lock_|) to guarantee mutual exclusion and singleton behaviour.
//
// Users of this class should hold onto a non-null VADisplayStateHandle for as
// long as they need to access any of the VADisplayStateSingleton methods. This
// guarantees that the VADisplayStateSingleton is properly initialized.
//
// Details:
//
// A VADisplayStateSingleton is immutable from the point of view of its users.
// That is, as long as a non-null VADisplayStateHandle exists, the va_display(),
// implementation_type(), and vendor_string() methods always return the same
// values.
//
// It's not strictly necessary to acquire the lock returned by va_lock() before
// calling va_display(), implementation_type(), or vendor_string(). However, on
// older drivers, it maybe necessary to acquire that lock before using the
// VADisplay returned by va_display() on any libva calls. That's because older
// drivers may not guarantee that it's safe to use the same VADisplay
// concurrently.
class VADisplayStateSingleton {
 public:
  VADisplayStateSingleton(const VADisplayStateSingleton&) = delete;
  VADisplayStateSingleton& operator=(const VADisplayStateSingleton&) = delete;

  // This method must be called exactly once before trying to acquire a
  // VADisplayStateHandle.
  static void PreSandboxInitialization();

  // If an initialized VADisplayStateSingleton exists, this method returns a
  // VADisplayStateHandle to it. Otherwise, it attempts to initialize a
  // VADisplayStateSingleton: if successful, it returns a VADisplayStateHandle
  // to it; otherwise, it returns a null VADisplayStateHandle.
  //
  // This method is thread- and sequence- safe.
  static VADisplayStateHandle GetHandle();

  base::Lock* va_lock() const { return &va_lock_; }
  VADisplay va_display() const { return va_display_; }
  VAImplementation implementation_type() const { return implementation_type_; }
  const std::string& vendor_string() const { return va_vendor_string_; }

 private:
  friend class base::NoDestructor<VADisplayStateSingleton>;
  friend class VADisplayStateHandle;

  static VADisplayStateSingleton& GetInstance();

  VADisplayStateSingleton() = default;
  ~VADisplayStateSingleton() = default;

  // If this method returns false, the VADisplayStateSingleton is unchanged.
  bool Initialize() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void OnRefDestroyed();

  // This lock makes reference counting and initialization/de-initialization
  // thread- and sequence-safe. It's independent of |va_lock_| which is only
  // used to guard the VADisplay in case the libva backend is not known to be
  // thread-safe.
  //
  // Note: the reason we don't use the same lock for everything is that it's
  // perfectly valid for a user to try to acquire a VADisplayStateHandle while
  // being in a block where libva calls are being made, so |va_lock_| could
  // already be acquired in that situation and trying to acquire it again for
  // reference counting would cause a deadlock.
  base::Lock lock_;

  // DRM FD used to obtain access to the driver interface by VA.
  base::ScopedFD drm_fd_ GUARDED_BY(lock_);

  int refcount_ GUARDED_BY(lock_) = 0;

  // Libva may or may not be thread safe depending on the backend. If not thread
  // safe, we have to do locking for it ourselves. Therefore, this lock may need
  // to be acquired for the duration of all VA-API calls and for the entire job
  // submission sequence in ExecuteAndDestroyPendingBuffers().
  //
  // Note: this field is made mutable to be able to mark va_lock() const: that
  // way, we convey that that method does not really change the
  // VADisplayStateSingleton. It's only mutable so that users of the
  // VADisplayStateSingleton can acquire the lock.
  //
  // TODO(andrescj): maybe it's better to provide an AcquireVALock() method so
  // that we control exactly how the lock can be used.
  mutable base::Lock va_lock_;

  // Note: the following members are deliberately not annotated with either
  // GUARDED_BY(lock_) or GUARDED_BY(va_lock_) because this annotation cannot
  // capture the required thread model: these members can't change as long as a
  // non-null VADisplayStateHandle exists, and users of VADisplayStateSingleton
  // should ensure that a non-null VADisplayStateHandle exists as long as they
  // need access to the VADisplayStateSingleton. Therefore, the accessor methods
  // don't need to acquire any lock.

  VADisplay va_display_ = nullptr;

  // Enumerated version of vaQueryVendorString().
  VAImplementation implementation_type_ = VAImplementation::kInvalid;

  // String representing a driver acquired by vaQueryVendorString().
  std::string va_vendor_string_;
};

}  // namespace media

#define LOG_VA_ERROR_AND_REPORT(va_error, function)              \
  do {                                                           \
    LOG(ERROR) << VaapiFunctionName(function)                    \
               << " failed, VA error: " << vaErrorStr(va_error); \
    report_error_to_uma_cb_.Run(function);                       \
  } while (0)

#define VA_LOG_ON_ERROR(va_res, function)                        \
  do {                                                           \
    const VAStatus va_res_va_log_on_error = (va_res);            \
    if (va_res_va_log_on_error != VA_STATUS_SUCCESS)             \
      LOG_VA_ERROR_AND_REPORT(va_res_va_log_on_error, function); \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_res, function, ret)                  \
  do {                                                               \
    const VAStatus va_res_va_sucess_or_return = (va_res);            \
    if (va_res_va_sucess_or_return != VA_STATUS_SUCCESS) {           \
      LOG_VA_ERROR_AND_REPORT(va_res_va_sucess_or_return, function); \
      return (ret);                                                  \
    }                                                                \
    DVLOG(3) << VaapiFunctionName(function);                         \
  } while (0)

namespace {

uint32_t BufferFormatToVAFourCC(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::BGRX_8888:
      return VA_FOURCC_BGRX;
    case gfx::BufferFormat::BGRA_8888:
      return VA_FOURCC_BGRA;
    case gfx::BufferFormat::RGBX_8888:
      return VA_FOURCC_RGBX;
    case gfx::BufferFormat::RGBA_8888:
      return VA_FOURCC_RGBA;
    case gfx::BufferFormat::YVU_420:
      return VA_FOURCC_YV12;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VA_FOURCC_NV12;
    case gfx::BufferFormat::P010:
      return VA_FOURCC_P010;
    default:
      NOTREACHED_IN_MIGRATION() << gfx::BufferFormatToString(fmt);
      return 0;
  }
}

media::VAImplementation VendorStringToImplementationType(
    const std::string& va_vendor_string) {
  if (base::StartsWith(va_vendor_string, "Mesa Gallium driver",
                       base::CompareCase::SENSITIVE)) {
    return media::VAImplementation::kMesaGallium;
  } else if (base::StartsWith(va_vendor_string, "Intel i965 driver",
                              base::CompareCase::SENSITIVE)) {
    return media::VAImplementation::kIntelI965;
  } else if (base::StartsWith(va_vendor_string, "Intel iHD driver",
                              base::CompareCase::SENSITIVE)) {
    return media::VAImplementation::kIntelIHD;
  } else if (base::StartsWith(va_vendor_string, "Chromium fake libva driver",
                              base::CompareCase::SENSITIVE)) {
    return media::VAImplementation::kChromiumFakeDriver;
  }
  return media::VAImplementation::kOther;
}

bool IsThreadSafeDriver(media::VAImplementation implementation_type) {
  // Only iHD and Mesa Gallium are known to be thread safe at the moment.
  // * Mesa Gallium: b/144877595
  // * iHD: crbug.com/1123429.
  constexpr auto kNoVaapiLockImplementations =
      base::MakeFixedFlatSet<media::VAImplementation>(
          {media::VAImplementation::kMesaGallium,
           media::VAImplementation::kIntelIHD});
  return kNoVaapiLockImplementations.contains(implementation_type);
}

bool UseGlobalVaapiLock(media::VAImplementation implementation_type) {
  if (!media::VaapiWrapper::allow_disabling_global_lock_) {
    return true;
  }

  return !IsThreadSafeDriver(implementation_type) ||
         base::FeatureList::IsEnabled(media::kGlobalVaapiLock);
}

bool FillVADRMPRIMESurfaceDescriptor(const gfx::NativePixmap& pixmap,
                                     VADRMPRIMESurfaceDescriptor& descriptor) {
  memset(&descriptor, 0, sizeof(VADRMPRIMESurfaceDescriptor));

  const gfx::BufferFormat buffer_format = pixmap.GetBufferFormat();
  const uint32_t va_fourcc = BufferFormatToVAFourCC(buffer_format);
  DCHECK(va_fourcc);

  const gfx::Size size = pixmap.GetBufferSize();
  const size_t num_planes = pixmap.GetNumberOfPlanes();
  const int drm_fourcc = ui::GetFourCCFormatFromBufferFormat(buffer_format);
  if (drm_fourcc == DRM_FORMAT_INVALID) {
    LOG(ERROR) << "Failed to get the DRM format from the buffer format";
    return false;
  }
  if (num_planes > std::size(descriptor.objects)) {
    LOG(ERROR) << "Too many planes in the NativePixmap; got " << num_planes
               << " but the maximum number is "
               << std::size(descriptor.objects);
    return false;
  }
  static_assert(std::size(VADRMPRIMESurfaceDescriptor{}.layers) ==
                std::size(VADRMPRIMESurfaceDescriptor{}.objects));
  static_assert(
      std::size(VADRMPRIMESurfaceDescriptor{}.layers[0].object_index) ==
      std::size(VADRMPRIMESurfaceDescriptor{}.objects));
  static_assert(std::size(VADRMPRIMESurfaceDescriptor{}.layers[0].offset) ==
                std::size(VADRMPRIMESurfaceDescriptor{}.objects));
  static_assert(std::size(VADRMPRIMESurfaceDescriptor{}.layers[0].pitch) ==
                std::size(VADRMPRIMESurfaceDescriptor{}.objects));

  descriptor.fourcc = va_fourcc;
  descriptor.width = base::checked_cast<uint32_t>(size.width());
  descriptor.height = base::checked_cast<uint32_t>(size.height());

  // We can pass the planes as separate layers or all in one layer. The choice
  // of doing the latter was arbitrary.
  descriptor.num_layers = 1u;
  descriptor.layers[0].drm_format = base::checked_cast<uint32_t>(drm_fourcc);
  descriptor.layers[0].num_planes = base::checked_cast<uint32_t>(num_planes);

  descriptor.num_objects = base::checked_cast<uint32_t>(num_planes);
  for (size_t i = 0u; i < num_planes; i++) {
    const int dma_buf_fd = pixmap.GetDmaBufFd(i);
    if (dma_buf_fd < 0) {
      LOG(ERROR) << "Failed to get dmabuf from a NativePixmap";
      return false;
    }
    const off_t data_size = lseek(dma_buf_fd, /*offset=*/0, SEEK_END);
    if (data_size == static_cast<off_t>(-1)) {
      PLOG(ERROR) << "Failed to get the size of the dma-buf";
      return false;
    }
    if (lseek(dma_buf_fd, /*offset=*/0, SEEK_SET) == static_cast<off_t>(-1)) {
      PLOG(ERROR) << "Failed to reset the file offset of the dma-buf";
      return false;
    }

    descriptor.objects[i].fd = dma_buf_fd;
    descriptor.objects[i].size = base::checked_cast<uint32_t>(data_size);
    descriptor.objects[i].drm_format_modifier =
        pixmap.GetBufferFormatModifier();

    descriptor.layers[0].object_index[i] = base::checked_cast<uint32_t>(i);
    if (!base::IsValueInRangeForNumericType<uint32_t>(
            pixmap.GetDmaBufOffset(i))) {
      LOG(ERROR) << "The offset for plane " << i << " is out-of-range";
      return false;
    }
    descriptor.layers[0].offset[i] =
        base::checked_cast<uint32_t>(pixmap.GetDmaBufOffset(i));
    descriptor.layers[0].pitch[i] = pixmap.GetDmaBufPitch(i);
  }

  return true;
}

struct VASurfaceAttribExternalBuffersAndFD {
  VASurfaceAttribExternalBuffers va_attrib_extbuf;
  uintptr_t fd;
};

bool FillVASurfaceAttribExternalBuffers(
    const gfx::NativePixmap& pixmap,
    VASurfaceAttribExternalBuffersAndFD& va_attrib_extbuf_and_fd) {
  VASurfaceAttribExternalBuffers& va_attrib_extbuf =
      va_attrib_extbuf_and_fd.va_attrib_extbuf;
  memset(&va_attrib_extbuf_and_fd, 0,
         sizeof(VASurfaceAttribExternalBuffersAndFD));

  const uint32_t va_fourcc = BufferFormatToVAFourCC(pixmap.GetBufferFormat());
  DCHECK(va_fourcc);

  const gfx::Size size = pixmap.GetBufferSize();
  const size_t num_planes = pixmap.GetNumberOfPlanes();

  va_attrib_extbuf.pixel_format = va_fourcc;
  va_attrib_extbuf.width = base::checked_cast<uint32_t>(size.width());
  va_attrib_extbuf.height = base::checked_cast<uint32_t>(size.height());

  static_assert(std::size(VASurfaceAttribExternalBuffers{}.pitches) ==
                std::size(VASurfaceAttribExternalBuffers{}.offsets));
  if (num_planes > std::size(va_attrib_extbuf.pitches)) {
    LOG(ERROR) << "Too many planes in the NativePixmap; got " << num_planes
               << " but the maximum number is "
               << std::size(va_attrib_extbuf.pitches);
    return false;
  }
  for (size_t i = 0; i < num_planes; ++i) {
    va_attrib_extbuf.pitches[i] = pixmap.GetDmaBufPitch(i);
    va_attrib_extbuf.offsets[i] =
        base::checked_cast<uint32_t>(pixmap.GetDmaBufOffset(i));
    DVLOG(4) << "plane " << i << ": pitch: " << va_attrib_extbuf.pitches[i]
             << " offset: " << va_attrib_extbuf.offsets[i];
  }
  va_attrib_extbuf.num_planes = base::checked_cast<uint32_t>(num_planes);

  const int dma_buf_fd = pixmap.GetDmaBufFd(0);
  if (dma_buf_fd < 0) {
    LOG(ERROR) << "Failed to get dmabuf from a NativePixmap";
    return false;
  }
  const off_t data_size = lseek(dma_buf_fd, /*offset=*/0, SEEK_END);
  if (data_size == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to get the size of the dma-buf";
    return false;
  }
  if (lseek(dma_buf_fd, /*offset=*/0, SEEK_SET) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to reset the file offset of the dma-buf";
    return false;
  }
  // If the data size doesn't fit in a uint32_t, we probably have bigger
  // problems.
  va_attrib_extbuf.data_size = base::checked_cast<uint32_t>(data_size);

  // We only have to pass the first file descriptor to a driver. A VA-API driver
  // shall create a VASurface from the single fd correctly.
  va_attrib_extbuf_and_fd.fd = base::checked_cast<uintptr_t>(dma_buf_fd);
  va_attrib_extbuf.buffers = &va_attrib_extbuf_and_fd.fd;
  va_attrib_extbuf.num_buffers = 1u;

  DCHECK_EQ(va_attrib_extbuf.flags, 0u);
  DCHECK_EQ(va_attrib_extbuf.private_data, nullptr);
  return true;
}

}  // namespace

namespace media {

namespace {
// VAEntrypoint is an enumeration starting from 1, but has no "invalid" value.
constexpr VAEntrypoint kVAEntrypointInvalid = static_cast<VAEntrypoint>(0);

// Returns true if the SoC has a Gen8 GPU. CPU model ID's are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsGen8Gpu() {
  static const bool is_gen8_gpu = []() {
    constexpr int kPentiumAndLaterFamily = 0x06;
    constexpr int kBroadwellCoreModelId = 0x3D;
    constexpr int kBroadwellGT3EModelId = 0x47;
    constexpr int kBroadwellXModelId = 0x4F;
    constexpr int kBroadwellXeonDModelId = 0x56;
    constexpr int kBraswellModelId = 0x4C;
    const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();

    return cpuid.family() == kPentiumAndLaterFamily &&
           (cpuid.model() == kBroadwellCoreModelId ||
            cpuid.model() == kBroadwellGT3EModelId ||
            cpuid.model() == kBroadwellXModelId ||
            cpuid.model() == kBroadwellXeonDModelId ||
            cpuid.model() == kBraswellModelId);
  }();

  return is_gen8_gpu;
}

// Returns true if the SoC has a Gen9 GPU. CPU model ID's are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsGen9Gpu() {
  static const bool is_gen9_gpu = []() {
    constexpr int kPentiumAndLaterFamily = 0x06;
    constexpr int kSkyLakeModelId = 0x5E;
    constexpr int kSkyLake_LModelId = 0x4E;
    constexpr int kApolloLakeModelId = 0x5c;
    const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();

    return cpuid.family() == kPentiumAndLaterFamily &&
           (cpuid.model() == kSkyLakeModelId ||
            cpuid.model() == kSkyLake_LModelId ||
            cpuid.model() == kApolloLakeModelId);
  }();

  return is_gen9_gpu;
}

// Returns true if the SoC has a Gen9.5 GPU. CPU model IDs are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsGen95Gpu() {
  static const bool is_gen95_gpu = []() {
    constexpr int kPentiumAndLaterFamily = 0x06;
    constexpr int kKabyLakeModelId = 0x9E;
    // Amber Lake, Whiskey Lake and some Comet Lake CPU IDs are the same as KBL
    // L.
    constexpr int kKabyLake_LModelId = 0x8E;
    constexpr int kGeminiLakeModelId = 0x7A;
    constexpr int kCometLakeModelId = 0xA5;
    constexpr int kCometLake_LModelId = 0xA6;
    const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();

    return cpuid.family() == kPentiumAndLaterFamily &&
           (cpuid.model() == kKabyLakeModelId ||
            cpuid.model() == kKabyLake_LModelId ||
            cpuid.model() == kGeminiLakeModelId ||
            cpuid.model() == kCometLakeModelId ||
            cpuid.model() == kCometLake_LModelId);
  }();

  return is_gen95_gpu;
}

// Returns true if the SoC has a Gen11 GPU. CPU model IDs are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsGen11Gpu() {
  static const bool is_gen11_gpu = []() {
    constexpr int kPentiumAndLaterFamily = 0x06;
    constexpr int kJasperLakeModelId = 0x9C;
    const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();

    return cpuid.family() == kPentiumAndLaterFamily &&
           (cpuid.model() == kJasperLakeModelId);
  }();

  return is_gen11_gpu;
}

// Returns true if the intel hybrid driver is used for decoding |va_profile|.
// https://github.com/intel/intel-hybrid-driver
// Note that since the hybrid driver runs as a part of the i965 driver,
// vaQueryVendorString() returns "Intel i965 driver".
bool IsUsingHybridDriverForDecoding(VAProfile va_profile) {
  // Note that Skylake (not gen8) also needs the hybrid decoder for VP9
  // decoding. However, it is disabled today on ChromeOS
  // (see crrev.com/c/390511).
  return va_profile == VAProfileVP9Profile0 && IsGen8Gpu();
}

// Returns true if the SoC is considered a low power one, i.e. it's an Intel
// Pentium, Celeron, or a Core Y-series. See go/intel-socs-101 or
// https://www.intel.com/content/www/us/en/processors/processor-numbers.html.
bool IsLowPowerIntelProcessor() {
  static const bool is_low_power_intel = []() {
    constexpr int kPentiumAndLaterFamily = 0x06;
    const base::CPU& cpuid = base::CPU::GetInstanceNoAllocation();
    const bool is_core_y_processor =
        base::MatchPattern(cpuid.cpu_brand(), "Intel(R) Core(TM) *Y CPU*");

    return cpuid.family() == kPentiumAndLaterFamily &&
           (base::Contains(cpuid.cpu_brand(), "Pentium") ||
            base::Contains(cpuid.cpu_brand(), "Celeron") ||
            is_core_y_processor);
  }();

  return is_low_power_intel;
}

bool IsModeDecoding(VaapiWrapper::CodecMode mode) {
  return mode == VaapiWrapper::CodecMode::kDecode
#if BUILDFLAG(IS_CHROMEOS_ASH)
         || VaapiWrapper::CodecMode::kDecodeProtected
#endif
      ;
}

bool IsModeEncoding(VaapiWrapper::CodecMode mode) {
  return mode == VaapiWrapper::CodecMode::kEncodeConstantBitrate ||
         mode ==
             VaapiWrapper::CodecMode::kEncodeConstantQuantizationParameter ||
         mode == VaapiWrapper::CodecMode::kEncodeVariableBitrate;
}

// Fill VAImage's non-visible area with 0s.
void FillNV12Padding(const VAImage& image,
                     const gfx::Size& visible_size,
                     uint8_t* data) {
  CHECK_NE(data, nullptr);
  CHECK_EQ(2u, image.num_planes);
  CHECK_EQ(image.format.fourcc, base::checked_cast<uint32_t>(VA_FOURCC_NV12));
  CHECK_LE(base::strict_cast<int>(image.width), media::limits::kMaxDimension);
  CHECK_GE(base::strict_cast<int>(image.width), visible_size.width());
  CHECK_LE(base::strict_cast<int>(image.height), media::limits::kMaxDimension);
  CHECK_GE(base::strict_cast<int>(image.height), visible_size.height());

  for (uint32_t plane = 0; plane < image.num_planes; plane++) {
    uint8_t* plane_data = data + image.offsets[plane];
    const int stride = base::checked_cast<int>(image.pitches[plane]);
    const int visible_width_in_bytes =
        plane == 0 ? visible_size.width()
                   : 2 * ((visible_size.width() + 1) / 2);
    const size_t visible_height =
        VideoFrame::Rows(plane, PIXEL_FORMAT_NV12, visible_size.height());
    const size_t image_height =
        VideoFrame::Rows(plane, PIXEL_FORMAT_NV12, image.height);

    // Fill 0 to the right non-visible area.
    CHECK_GE(stride, visible_width_in_bytes);
    libyuv::SetPlane(/*dst_y=*/plane_data + visible_width_in_bytes,
                     /*dst_stride_y=*/stride,
                     /*width=*/stride - visible_width_in_bytes,
                     /*height=*/base::checked_cast<int>(visible_height),
                     /*value=*/0u);

    // Fill 0 to the bottom non-visible area.
    CHECK_GE(image_height, visible_height);
    base::CheckedNumeric<size_t> num_bytes_above(visible_height);
    num_bytes_above *= base::checked_cast<size_t>(stride);
    libyuv::SetPlane(
        /*dst_y=*/plane_data + num_bytes_above.ValueOrDie(),
        /*dst_stride_y=*/stride,
        /*width=*/stride,
        /*height=*/base::checked_cast<int>(image_height - visible_height),
        /*value=*/0u);
  }
}

// Creates an AutoLock iff |va_lock_| is not null and the libva backend is
// thread-safe.
std::optional<base::AutoLock> AutoLockOnlyIfNeeded(base::Lock* lock) {
  if (lock && !IsThreadSafeDriver(VaapiWrapper::GetImplementationType())) {
    return std::optional<base::AutoLock>(*lock);
  }
  return std::nullopt;
}

// Can't statically initialize the profile map:
// https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables
using ProfileCodecMap = std::map<VideoCodecProfile, VAProfile>;
const ProfileCodecMap& GetProfileCodecMap() {
  static const base::NoDestructor<ProfileCodecMap> kMediaToVAProfileMap({
      // VAProfileH264Baseline is deprecated in <va/va.h> since libva 2.0.0.
      {H264PROFILE_BASELINE, VAProfileH264ConstrainedBaseline},
          {H264PROFILE_MAIN, VAProfileH264Main},
          // TODO(posciak): See if we can/want to support other variants of
          // H264PROFILE_HIGH*.
          {H264PROFILE_HIGH, VAProfileH264High},
          {VP8PROFILE_ANY, VAProfileVP8Version0_3},
          {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
          // VaapiWrapper does not support VP9 Profile 1, see b/153680337.
          // {VP9PROFILE_PROFILE1, VAProfileVP9Profile1},
          {VP9PROFILE_PROFILE2, VAProfileVP9Profile2},
          // VaapiWrapper does not support Profile 3.
          //{VP9PROFILE_PROFILE3, VAProfileVP9Profile3},
          {AV1PROFILE_PROFILE_MAIN, VAProfileAV1Profile0},
        // VaapiWrapper does not support AV1 Profile 1.
        // {AV1PROFILE_PROFILE_HIGH, VAProfileAV1Profile1},
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
          {HEVCPROFILE_MAIN, VAProfileHEVCMain},
          {HEVCPROFILE_MAIN_STILL_PICTURE, VAProfileHEVCMain},
          {HEVCPROFILE_MAIN10, VAProfileHEVCMain10},
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  });
  return *kMediaToVAProfileMap;
}

// Maps a VideoCodecProfile |profile| to a VAProfile, or VAProfileNone.
VAProfile ProfileToVAProfile(VideoCodecProfile profile) {
  const auto& profiles = GetProfileCodecMap();
  const auto& maybe_profile = profiles.find(profile);
  if (maybe_profile == profiles.end())
    return VAProfileNone;
  return maybe_profile->second;
}

bool IsVAProfileSupported(VAProfile va_profile, bool is_encoding) {
  // VAProfileJPEGBaseline and VAProfileProtected are always recognized but are
  // not video codecs per se.
  if (va_profile == VAProfileJPEGBaseline) {
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (va_profile == VAProfileProtected) {
    return true;
  }
#endif
  if (is_encoding) {
    constexpr VAProfile kSupportableEncoderProfiles[] = {
        VAProfileH264ConstrainedBaseline,
        VAProfileH264Main,
        VAProfileH264High,
        VAProfileVP8Version0_3,
        VAProfileVP9Profile0,
        VAProfileAV1Profile0,
    };
    return base::Contains(kSupportableEncoderProfiles, va_profile);
  }
  return base::Contains(GetProfileCodecMap(), va_profile,
                        &ProfileCodecMap::value_type::second);
}

bool IsBlockedDriver(VaapiWrapper::CodecMode mode,
                     VAProfile va_profile,
                     const std::string& va_vendor_string) {
  if (!IsModeEncoding(mode)) {
    return va_profile == VAProfileAV1Profile0 &&
           !base::FeatureList::IsEnabled(kChromeOSHWAV1Decoder);
  }

  if (va_profile == VAProfileVP8Version0_3 &&
      !base::FeatureList::IsEnabled(kVaapiVP8Encoder)) {
    return true;
  }

  if (va_profile == VAProfileVP9Profile0 &&
      !base::FeatureList::IsEnabled(kVaapiVP9Encoder)) {
    return true;
  }

  if (va_profile == VAProfileAV1Profile0 &&
      !base::FeatureList::IsEnabled(kVaapiAV1Encoder)) {
    return true;
  }

  if (mode == VaapiWrapper::CodecMode::kEncodeVariableBitrate) {
    // The rate controller on grunt is not good enough to support VBR encoding,
    // b/253988139.
    const bool is_amd_stoney_ridge_driver =
        va_vendor_string.find("stoney") != std::string::npos;
    if (!base::FeatureList::IsEnabled(kChromeOSHWVBREncoding) ||
        is_amd_stoney_ridge_driver) {
      return true;
    }
  }

  return false;
}

// Returns all the VAProfiles that the driver lists as supported, regardless of
// what Chrome supports or not.
std::vector<VAProfile> GetSupportedVAProfiles(const base::Lock* va_lock,
                                              VADisplay va_display) {
  MAYBE_ASSERT_ACQUIRED(va_lock);

  // Query the driver for supported profiles.
  const int max_va_profiles = vaMaxNumProfiles(va_display);
  std::vector<VAProfile> va_profiles(
      base::checked_cast<size_t>(max_va_profiles));

  int num_va_profiles;
  const VAStatus va_res =
      vaQueryConfigProfiles(va_display, &va_profiles[0], &num_va_profiles);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaQueryConfigProfiles failed: " << vaErrorStr(va_res);
    return {};
  }
  if (num_va_profiles < 0 || num_va_profiles > max_va_profiles) {
    LOG(ERROR) << "vaQueryConfigProfiles returned: " << num_va_profiles
               << " profiles";
    return {};
  }

  va_profiles.resize(base::checked_cast<size_t>(num_va_profiles));
  return va_profiles;
}

// Queries the driver for the supported entrypoints for |va_profile|, then
// returns those allowed for |mode|.
std::vector<VAEntrypoint> GetEntryPointsForProfile(const base::Lock* va_lock,
                                                   VADisplay va_display,
                                                   VaapiWrapper::CodecMode mode,
                                                   VAProfile va_profile) {
  MAYBE_ASSERT_ACQUIRED(va_lock);

  const int max_entrypoints = vaMaxNumEntrypoints(va_display);
  std::vector<VAEntrypoint> va_entrypoints(
      base::checked_cast<size_t>(max_entrypoints));

  int num_va_entrypoints;
  const VAStatus va_res = vaQueryConfigEntrypoints(
      va_display, va_profile, &va_entrypoints[0], &num_va_entrypoints);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaQueryConfigEntrypoints failed, VA error: "
               << vaErrorStr(va_res);
    return {};
  }
  if (num_va_entrypoints < 0 || num_va_entrypoints > max_entrypoints) {
    LOG(ERROR) << "vaQueryConfigEntrypoints returned: " << num_va_entrypoints
               << " entry points, when the max is: " << max_entrypoints;
    return {};
  }
  va_entrypoints.resize(num_va_entrypoints);

  const std::vector<VAEntrypoint> kAllowedEntryPoints[] = {
    {VAEntrypointVLD},  // kDecode.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {VAEntrypointVLD, VAEntrypointProtectedContent},  // kDecodeProtected.
#endif
    {VAEntrypointEncSlice, VAEntrypointEncPicture,
     VAEntrypointEncSliceLP},  // kEncodeConstantBitrate.
    {VAEntrypointEncSlice,
     VAEntrypointEncSliceLP},  // kEncodeConstantQuantizationParameter.
    {VAEntrypointEncSlice, VAEntrypointEncSliceLP},  // kEncodeVariableBitrate.
    {VAEntrypointVideoProc}                          // kVideoProcess.
  };
  static_assert(std::size(kAllowedEntryPoints) == VaapiWrapper::kCodecModeMax,
                "");

  std::vector<VAEntrypoint> entrypoints;
  base::ranges::copy_if(va_entrypoints, std::back_inserter(entrypoints),
                        [&kAllowedEntryPoints, mode](VAEntrypoint entry_point) {
                          return base::Contains(kAllowedEntryPoints[mode],
                                                entry_point);
                        });
  return entrypoints;
}

bool GetRequiredAttribs(const base::Lock* va_lock,
                        VADisplay va_display,
                        VaapiWrapper::CodecMode mode,
                        VAProfile profile,
                        VAEntrypoint entrypoint,
                        std::vector<VAConfigAttrib>* required_attribs) {
  MAYBE_ASSERT_ACQUIRED(va_lock);

  // Choose a suitable VAConfigAttribRTFormat for every |mode|. For video
  // processing, the supported surface attribs may vary according to which RT
  // format is set.
  if (profile == VAProfileVP9Profile2 || profile == VAProfileVP9Profile3) {
    required_attribs->push_back(
        {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420_10BPP});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  } else if (profile == VAProfileProtected) {
    DCHECK_EQ(mode, VaapiWrapper::kDecodeProtected);
    constexpr int kWidevineUsage = 0x1;
    required_attribs->push_back(
        {VAConfigAttribProtectedContentUsage, kWidevineUsage});
    required_attribs->push_back(
        {VAConfigAttribProtectedContentCipherAlgorithm, VA_PC_CIPHER_AES});
    required_attribs->push_back(
        {VAConfigAttribProtectedContentCipherBlockSize, VA_PC_BLOCK_SIZE_128});
    required_attribs->push_back(
        {VAConfigAttribProtectedContentCipherMode, VA_PC_CIPHER_MODE_CTR});
#endif
  } else {
    required_attribs->push_back({VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420});
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (mode == VaapiWrapper::kDecodeProtected && profile != VAProfileProtected) {
    required_attribs->push_back(
        {VAConfigAttribEncryption, VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR});
  }
#endif

  if (!IsModeEncoding(mode))
    return true;

  if (profile == VAProfileJPEGBaseline)
    return true;

  if (mode == VaapiWrapper::kEncodeConstantBitrate)
    required_attribs->push_back({VAConfigAttribRateControl, VA_RC_CBR});
  if (mode == VaapiWrapper::kEncodeConstantQuantizationParameter)
    required_attribs->push_back({VAConfigAttribRateControl, VA_RC_CQP});
  if (mode == VaapiWrapper::kEncodeVariableBitrate)
    required_attribs->push_back({VAConfigAttribRateControl, VA_RC_VBR});

  constexpr VAProfile kSupportedH264VaProfilesForEncoding[] = {
      VAProfileH264ConstrainedBaseline, VAProfileH264Main, VAProfileH264High};
  // VAConfigAttribEncPackedHeaders is H.264 specific.
  if (base::Contains(kSupportedH264VaProfilesForEncoding, profile)) {
    // Encode with Packed header if the driver supports.
    VAConfigAttrib attrib{};
    attrib.type = VAConfigAttribEncPackedHeaders;
    const VAStatus va_res =
        vaGetConfigAttributes(va_display, profile, entrypoint, &attrib, 1);
    if (va_res != VA_STATUS_SUCCESS) {
      LOG(ERROR) << "vaGetConfigAttributes failed: " << vaProfileStr(profile);
      return false;
    }

    const uint32_t packed_header_attributes =
        (VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE |
         VA_ENC_PACKED_HEADER_SLICE);
    if ((packed_header_attributes & attrib.value) == packed_header_attributes) {
      required_attribs->push_back(
          {VAConfigAttribEncPackedHeaders, packed_header_attributes});
    } else {
      required_attribs->push_back(
          {VAConfigAttribEncPackedHeaders, VA_ENC_PACKED_HEADER_NONE});
    }
  }
  return true;
}

// Returns true if |va_profile| for |entrypoint| with |required_attribs| is
// supported.
bool AreAttribsSupported(const base::Lock* va_lock,
                         VADisplay va_display,
                         VAProfile va_profile,
                         VAEntrypoint entrypoint,
                         const std::vector<VAConfigAttrib>& required_attribs) {
  MAYBE_ASSERT_ACQUIRED(va_lock);
  // Query the driver for required attributes.
  std::vector<VAConfigAttrib> attribs = required_attribs;
  for (size_t i = 0; i < required_attribs.size(); ++i)
    attribs[i].value = 0;

  VAStatus va_res = vaGetConfigAttributes(va_display, va_profile, entrypoint,
                                          &attribs[0], attribs.size());
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaGetConfigAttributes failed error: " << vaErrorStr(va_res);
    return false;
  }
  for (size_t i = 0; i < required_attribs.size(); ++i) {
    if (attribs[i].type != required_attribs[i].type ||
        (attribs[i].value & required_attribs[i].value) !=
            required_attribs[i].value) {
      DVLOG(1) << "Unsupported value " << required_attribs[i].value << " for "
               << vaConfigAttribTypeStr(required_attribs[i].type);
      return false;
    }
  }
  return true;
}

// This class encapsulates reading and giving access to the list of supported
// ProfileInfo entries, in a singleton way.
class VASupportedProfiles {
 public:
  struct ProfileInfo {
    VAProfile va_profile;
    VAEntrypoint va_entrypoint;
    gfx::Size min_resolution;
    gfx::Size max_resolution;
    std::vector<uint32_t> pixel_formats;
    VaapiWrapper::InternalFormats supported_internal_formats;
  };
  static const VASupportedProfiles& Get();

  VASupportedProfiles(const VASupportedProfiles&) = delete;
  VASupportedProfiles& operator=(const VASupportedProfiles&) = delete;

  // Determines if |mode| supports |va_profile| (and |va_entrypoint| if defined
  // and valid). If so, returns a const pointer to its ProfileInfo, otherwise
  // returns nullptr.
  // TODO(hiroh): If VAEntrypoint is kVAEntrypointInvalid, the default entry
  // point acquired by GetDefaultVaEntryPoint() is used. If the default entry
  // point is not supported, the earlier supported entrypoint in
  // |kAllowedEntryPopints| is used.
  const ProfileInfo* IsProfileSupported(
      VaapiWrapper::CodecMode mode,
      VAProfile va_profile,
      VAEntrypoint va_entrypoint = kVAEntrypointInvalid) const;

 private:
  friend class base::NoDestructor<VASupportedProfiles>;

  friend std::map<VAProfile, std::vector<VAEntrypoint>>
  VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(
      CodecMode mode);

  VASupportedProfiles();
  ~VASupportedProfiles() = default;

  // Fills in |supported_profiles_|.
  void FillSupportedProfileInfos(base::Lock* va_lock,
                                 VADisplay va_display,
                                 const std::string& va_vendor_string);

  // Fills |profile_info| for |va_profile| and |entrypoint| with
  // |required_attribs|. If the return value is true, the operation was
  // successful. Otherwise, the information in *|profile_info| shouldn't be
  // relied upon.
  bool FillProfileInfo_Locked(const base::Lock* va_lock,
                              VADisplay va_display,
                              VAProfile va_profile,
                              VAEntrypoint entrypoint,
                              std::vector<VAConfigAttrib>& required_attribs,
                              ProfileInfo* profile_info) const;

  std::vector<ProfileInfo> supported_profiles_[VaapiWrapper::kCodecModeMax];
  static_assert(std::extent<decltype(supported_profiles_)>() ==
                    VaapiWrapper::kCodecModeMax,
                "|supported_profiles_| size is incorrect.");

  const ReportErrorToUMACB report_error_to_uma_cb_;
};

// static
const VASupportedProfiles& VASupportedProfiles::Get() {
  static const base::NoDestructor<VASupportedProfiles> profile_infos;
  return *profile_infos;
}

const VASupportedProfiles::ProfileInfo* VASupportedProfiles::IsProfileSupported(
    VaapiWrapper::CodecMode mode,
    VAProfile va_profile,
    VAEntrypoint va_entrypoint) const {
  auto iter = base::ranges::find_if(
      supported_profiles_[mode],
      [va_profile, va_entrypoint](const ProfileInfo& profile) {
        return profile.va_profile == va_profile &&
               (va_entrypoint == kVAEntrypointInvalid ||
                profile.va_entrypoint == va_entrypoint);
      });
  if (iter != supported_profiles_[mode].end())
    return &*iter;
  return nullptr;
}

VASupportedProfiles::VASupportedProfiles()
    : report_error_to_uma_cb_(base::DoNothing()) {
  VADisplayStateHandle display_state = VADisplayStateSingleton::GetHandle();
  if (!display_state) {
    return;
  }

  VADisplay va_display = display_state->va_display();
  DCHECK(va_display)
      << "VADisplayStateSingleton hasn't been properly initialized";

  base::Lock* va_lock = display_state->va_lock();
  if (!UseGlobalVaapiLock(display_state->implementation_type())) {
    va_lock = nullptr;
  }

  FillSupportedProfileInfos(va_lock, va_display,
                            display_state->vendor_string());
}

void VASupportedProfiles::FillSupportedProfileInfos(
    base::Lock* va_lock,
    VADisplay va_display,
    const std::string& va_vendor_string) {
  base::AutoLockMaybe auto_lock(va_lock);

  const std::vector<VAProfile> va_profiles =
      GetSupportedVAProfiles(va_lock, va_display);

  constexpr VaapiWrapper::CodecMode kWrapperModes[] = {
    VaapiWrapper::kDecode,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    VaapiWrapper::kDecodeProtected,
#endif
    VaapiWrapper::kEncodeConstantBitrate,
    VaapiWrapper::kEncodeConstantQuantizationParameter,
    VaapiWrapper::kEncodeVariableBitrate,
    VaapiWrapper::kVideoProcess
  };
  static_assert(std::size(kWrapperModes) == VaapiWrapper::kCodecModeMax, "");

  for (VaapiWrapper::CodecMode mode : kWrapperModes) {
    std::vector<ProfileInfo> supported_profile_infos;

    for (const auto& va_profile : va_profiles) {
      if (IsBlockedDriver(mode, va_profile, va_vendor_string))
        continue;

      if ((mode != VaapiWrapper::kVideoProcess) &&
          !IsVAProfileSupported(va_profile, IsModeEncoding(mode))) {
        continue;
      }

      const std::vector<VAEntrypoint> supported_entrypoints =
          GetEntryPointsForProfile(va_lock, va_display, mode, va_profile);

      for (const auto& entrypoint : supported_entrypoints) {
        std::vector<VAConfigAttrib> required_attribs;
        if (!GetRequiredAttribs(va_lock, va_display, mode, va_profile,
                                entrypoint, &required_attribs)) {
          continue;
        }
        if (!AreAttribsSupported(va_lock, va_display, va_profile, entrypoint,
                                 required_attribs)) {
          continue;
        }
        ProfileInfo profile_info{};
        if (!FillProfileInfo_Locked(va_lock, va_display, va_profile, entrypoint,
                                    required_attribs, &profile_info)) {
          LOG(ERROR) << "FillProfileInfo_Locked failed for va_profile "
                     << vaProfileStr(va_profile) << " and entrypoint "
                     << vaEntrypointStr(entrypoint);
          continue;
        }

        supported_profile_infos.push_back(profile_info);
      }
    }
    supported_profiles_[static_cast<int>(mode)] = supported_profile_infos;
  }
}

bool VASupportedProfiles::FillProfileInfo_Locked(
    const base::Lock* va_lock,
    VADisplay va_display,
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib>& required_attribs,
    ProfileInfo* profile_info) const {
  MAYBE_ASSERT_ACQUIRED(va_lock);
  VAConfigID va_config_id;
  VAStatus va_res =
      vaCreateConfig(va_display, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  absl::Cleanup vaconfig_destroyer = [va_display, va_config_id] {
    if (va_config_id != VA_INVALID_ID) {
      VAStatus va_res = vaDestroyConfig(va_display, va_config_id);
      if (va_res != VA_STATUS_SUCCESS) {
        LOG(ERROR) << "vaDestroyConfig failed. VA error: "
                   << vaErrorStr(va_res);
      }
    }
  };

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Nothing further to query for protected profile.
  if (va_profile == VAProfileProtected) {
    profile_info->va_profile = va_profile;
    profile_info->va_entrypoint = entrypoint;
    return true;
  }
#endif

  // Calls vaQuerySurfaceAttributes twice. The first time is to get the number
  // of attributes to prepare the space and the second time is to get all
  // attributes.
  unsigned int num_attribs;
  va_res =
      vaQuerySurfaceAttributes(va_display, va_config_id, nullptr, &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQuerySurfaceAttributes,
                       false);
  if (!num_attribs)
    return false;

  std::vector<VASurfaceAttrib> attrib_list(
      base::checked_cast<size_t>(num_attribs));

  va_res = vaQuerySurfaceAttributes(va_display, va_config_id, &attrib_list[0],
                                    &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQuerySurfaceAttributes,
                       false);

  profile_info->va_profile = va_profile;
  profile_info->va_entrypoint  = entrypoint;
  profile_info->min_resolution = gfx::Size();
  profile_info->max_resolution = gfx::Size();
  for (const auto& attrib : attrib_list) {
    if (attrib.type == VASurfaceAttribMaxWidth) {
      profile_info->max_resolution.set_width(
          base::strict_cast<int>(attrib.value.value.i));
    } else if (attrib.type == VASurfaceAttribMaxHeight) {
      profile_info->max_resolution.set_height(
          base::strict_cast<int>(attrib.value.value.i));
    } else if (attrib.type == VASurfaceAttribMinWidth) {
      profile_info->min_resolution.set_width(
          base::strict_cast<int>(attrib.value.value.i));
    } else if (attrib.type == VASurfaceAttribMinHeight) {
      profile_info->min_resolution.set_height(
          base::strict_cast<int>(attrib.value.value.i));
    } else if (attrib.type == VASurfaceAttribPixelFormat) {
      // According to va.h, VASurfaceAttribPixelFormat is meaningful as input to
      // vaQuerySurfaceAttributes(). However, per the implementation of
      // i965_QuerySurfaceAttributes(), our usage here should enumerate all the
      // formats.
      profile_info->pixel_formats.push_back(attrib.value.value.i);
    }
  }
  if (profile_info->max_resolution.IsEmpty()) {
    LOG(ERROR) << "Empty codec maximum resolution";
    return false;
  }

  if (va_profile != VAProfileJPEGBaseline) {
    // Set a reasonable minimum value for both encoding and decoding.
    profile_info->min_resolution.SetToMax(gfx::Size(16, 16));

    const bool is_encoding = entrypoint == VAEntrypointEncSliceLP ||
                             entrypoint == VAEntrypointEncSlice;
    const bool is_hybrid_decoding = entrypoint == VAEntrypointVLD &&
                                    IsUsingHybridDriverForDecoding(va_profile);

    // Using HW encoding for small resolutions is less efficient than using a SW
    // encoder. Similarly, using the intel-hybrid-driver for decoding is less
    // efficient than using a SW decoder. In both cases, increase
    // |min_resolution| to QVGA + 1 which is an experimental lower threshold.
    // This can be turned off with kVaapiVideoMinResolutionForPerformance for
    // testing.
    if ((is_encoding || is_hybrid_decoding) &&
        base::FeatureList::IsEnabled(kVaapiVideoMinResolutionForPerformance)) {
      constexpr gfx::Size kMinVideoResolution(320 + 1, 240 + 1);
      profile_info->min_resolution.SetToMax(kMinVideoResolution);
      DVLOG(2) << "Setting the minimum supported resolution for "
               << vaProfileStr(va_profile)
               << (is_encoding ? " encoding" : " decoding") << " to "
               << profile_info->min_resolution.ToString();
    }
  }

  // Create a new configuration to find the supported RT formats. We don't pass
  // required attributes here because we want the driver to tell us all the
  // supported RT formats.
  va_res = vaCreateConfig(va_display, va_profile, entrypoint, nullptr, 0,
                          &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  absl::Cleanup vaconfig_no_attribs_destroyer = [va_display, va_config_id] {
    if (va_config_id != VA_INVALID_ID) {
      VAStatus va_res = vaDestroyConfig(va_display, va_config_id);
      if (va_res != VA_STATUS_SUCCESS) {
        LOG(ERROR) << "vaDestroyConfig failed. VA error: "
                   << vaErrorStr(va_res);
      }
    }
  };
  profile_info->supported_internal_formats = {};
  size_t max_num_config_attributes;
  if (!base::CheckedNumeric<int>(vaMaxNumConfigAttributes(va_display))
           .AssignIfValid(&max_num_config_attributes)) {
    LOG(ERROR) << "Can't get the maximum number of config attributes";
    return false;
  }
  const bool is_mesa_gallium = VaapiWrapper::GetImplementationType() ==
                               media::VAImplementation::kMesaGallium;

  std::vector<VAConfigAttrib> config_attributes(max_num_config_attributes);
  int num_config_attributes;
  va_res = vaQueryConfigAttributes(va_display, va_config_id, &va_profile,
                                   &entrypoint, config_attributes.data(),
                                   &num_config_attributes);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQueryConfigAttributes, false);
  for (int i = 0; i < num_config_attributes; i++) {
    const VAConfigAttrib& attrib = config_attributes[i];
    if (attrib.type != VAConfigAttribRTFormat)
      continue;
    if (attrib.value & VA_RT_FORMAT_YUV420)
      profile_info->supported_internal_formats.yuv420 = true;
    if (attrib.value & VA_RT_FORMAT_YUV420_10)
      profile_info->supported_internal_formats.yuv420_10 = true;
    if (attrib.value & VA_RT_FORMAT_YUV422)
      profile_info->supported_internal_formats.yuv422 = true;
    // The Mesa Gallium backend doesn't support converting YUV 4:4:4 surfaces to
    // any VAImage fourcc that we care about, so don't VA_RT_FORMAT_YUV444 on
    // that driver.
    if ((attrib.value & VA_RT_FORMAT_YUV444) && !is_mesa_gallium)
      profile_info->supported_internal_formats.yuv444 = true;
    break;
  }

  const bool is_any_profile_supported =
      profile_info->supported_internal_formats.yuv420 ||
      profile_info->supported_internal_formats.yuv420_10 ||
      profile_info->supported_internal_formats.yuv422 ||
      profile_info->supported_internal_formats.yuv444;
  DLOG_IF(ERROR, !is_any_profile_supported)
      << "No cool internal formats supported";
  return is_any_profile_supported;
}

void DestroyVAImage(VADisplay va_display, const VAImage& image) {
  if (image.image_id != VA_INVALID_ID)
    vaDestroyImage(va_display, image.image_id);
}

// This class encapsulates fetching the list of supported output image formats
// from the VAAPI driver, in a singleton way.
class VASupportedImageFormats {
 public:
  static const VASupportedImageFormats& Get();

  VASupportedImageFormats(const VASupportedImageFormats&) = delete;
  VASupportedImageFormats& operator=(const VASupportedImageFormats&) = delete;

  bool IsImageFormatSupported(const VAImageFormat& va_format) const;

  const std::vector<VAImageFormat>& GetSupportedImageFormats() const;

 private:
  friend class base::NoDestructor<VASupportedImageFormats>;

  VASupportedImageFormats();
  ~VASupportedImageFormats() = default;

  // Initialize the list of supported image formats.
  bool InitSupportedImageFormats_Locked(const base::Lock* va_lock,
                                        VADisplay va_display);

  std::vector<VAImageFormat> supported_formats_;
  const ReportErrorToUMACB report_error_to_uma_cb_;
};

// static
const VASupportedImageFormats& VASupportedImageFormats::Get() {
  static const base::NoDestructor<VASupportedImageFormats> image_formats;
  return *image_formats;
}

bool VASupportedImageFormats::IsImageFormatSupported(
    const VAImageFormat& va_image_format) const {
  return base::Contains(supported_formats_, va_image_format.fourcc,
                        &VAImageFormat::fourcc);
}

const std::vector<VAImageFormat>&
VASupportedImageFormats::GetSupportedImageFormats() const {
#if DCHECK_IS_ON()
  std::string formats_str;
  for (size_t i = 0; i < supported_formats_.size(); i++) {
    if (i > 0)
      formats_str += ", ";
    formats_str += FourccToString(supported_formats_[i].fourcc);
  }
  DVLOG(1) << "Supported image formats: " << formats_str;
#endif
  return supported_formats_;
}

VASupportedImageFormats::VASupportedImageFormats()
    : report_error_to_uma_cb_(base::DoNothing()) {
  auto display_state = VADisplayStateSingleton::GetHandle();
  if (!display_state) {
    return;
  }

  // Pointer to VADisplayStateSingleton's |va_lock_| member if using a global VA
  // lock or if the implementation is not thread-safe.
  base::Lock* va_lock = display_state->va_lock();
  if (!UseGlobalVaapiLock(display_state->implementation_type())) {
    va_lock = nullptr;
  }

  {
    base::AutoLockMaybe auto_lock(va_lock);
    VADisplay va_display = display_state->va_display();
    DCHECK(va_display)
        << "VADisplayStateSingleton hasn't been properly initialized";

    if (!InitSupportedImageFormats_Locked(va_lock, va_display))
      LOG(ERROR) << "Failed to get supported image formats";
  }
}

bool VASupportedImageFormats::InitSupportedImageFormats_Locked(
    const base::Lock* va_lock,
    VADisplay va_display) {
  MAYBE_ASSERT_ACQUIRED(va_lock);

  // Query the driver for the max number of image formats and allocate space.
  const int max_image_formats = vaMaxNumImageFormats(va_display);
  if (max_image_formats < 0) {
    LOG(ERROR) << "vaMaxNumImageFormats returned: " << max_image_formats;
    return false;
  }
  supported_formats_.resize(static_cast<size_t>(max_image_formats));

  // Query the driver for the list of supported image formats.
  int num_image_formats;
  const VAStatus va_res = vaQueryImageFormats(
      va_display, supported_formats_.data(), &num_image_formats);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQueryImageFormats, false);
  if (num_image_formats < 0 || num_image_formats > max_image_formats) {
    LOG(ERROR) << "vaQueryImageFormats returned: " << num_image_formats;
    supported_formats_.clear();
    return false;
  }

  // Resize the list to the actual number of formats returned by the driver.
  supported_formats_.resize(static_cast<size_t>(num_image_formats));
  return true;
}

bool IsLowPowerEncSupported(VAProfile va_profile) {
  constexpr VAProfile kSupportedLowPowerEncodeProfiles[] = {
      VAProfileH264ConstrainedBaseline,
      VAProfileH264Main,
      VAProfileH264High,
      VAProfileVP9Profile0,
      VAProfileAV1Profile0,
  };
  if (!base::Contains(kSupportedLowPowerEncodeProfiles, va_profile))
    return false;

  if ((IsGen95Gpu() || IsGen9Gpu()) &&
      !base::FeatureList::IsEnabled(kVaapiLowPowerEncoderGen9x)) {
    return false;
  }

  if (VASupportedProfiles::Get().IsProfileSupported(
          VaapiWrapper::kEncodeConstantBitrate, va_profile,
          VAEntrypointEncSliceLP)) {
    return true;
  }
  return false;
}

bool IsVBREncodingSupported(VAProfile va_profile) {
  auto mode = VaapiWrapper::CodecMode::kCodecModeMax;
  switch (va_profile) {
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
      mode = VaapiWrapper::CodecMode::kEncodeVariableBitrate;
      break;
    default:
      return false;
  }

  return VASupportedProfiles::Get().IsProfileSupported(mode, va_profile);
}

bool IsLibVACompatible(const base::Version& runtime,
                       const base::Version& build_time) {
  // Since the libva is now ABI-compatible, relax the version check which helps
  // in upgrading the libva, without breaking any existing functionality. Make
  // sure the system version (|runtime|) is not older than the version with
  // which the chromium is built (|build_time|) since libva is only guaranteed
  // to be backward (and not forward) compatible.
  return runtime >= build_time;
}

}  // namespace

// static
VADisplayStateSingleton& VADisplayStateSingleton::GetInstance() {
  static base::NoDestructor<VADisplayStateSingleton> va_display_state;
  return *va_display_state;
}

// static
void VADisplayStateSingleton::PreSandboxInitialization() {
  VADisplayStateSingleton& va_display_state = GetInstance();
  base::AutoLock lock(va_display_state.lock_);

  constexpr char kRenderNodeFilePattern[] = "/dev/dri/renderD%d";
  // This loop ends on either the first card that does not exist or the first
  // render node that is not vgem.
  for (int i = 128;; i++) {
    base::FilePath dev_path(FILE_PATH_LITERAL(
        base::StringPrintf(kRenderNodeFilePattern, i).c_str()));
    base::File drm_file =
        base::File(dev_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
    if (!drm_file.IsValid()) {
      return;
    }
    drmVersionPtr version = drmGetVersion(drm_file.GetPlatformFile());
    if (!version) {
      continue;
    }
    std::string version_name(
        version->name,
        base::checked_cast<std::string::size_type>(version->name_len));
    drmFreeVersion(version);
    // Skip the virtual graphics memory manager device.
    if (base::EqualsCaseInsensitiveASCII(version_name, "vgem")) {
      continue;
    }
    // Skip NVIDIA device because their VA-API drivers do not support
    // Chromium and can sometimes cause crashes (see crbug.com/1492880).
    if (base::EqualsCaseInsensitiveASCII(version_name, "nvidia-drm") &&
        !base::FeatureList::IsEnabled(kVaapiOnNvidiaGPUs)) {
      LOG(WARNING) << "Skipping nVidia device named: " << version_name;
      continue;
    }
    va_display_state.drm_fd_ = base::ScopedFD(drm_file.TakePlatformFile());
    return;
  }
}

// static
VADisplayStateHandle VADisplayStateSingleton::GetHandle() {
  VADisplayStateSingleton& va_display_state = GetInstance();
  base::AutoLock lock(va_display_state.lock_);
  if (va_display_state.refcount_ > 0) {
    // There's already an initialized VADisplayStateSingleton. Return a handle
    // to it.
    CHECK_LT(va_display_state.refcount_,
             std::numeric_limits<decltype(va_display_state.refcount_)>::max());
    va_display_state.refcount_++;
    return VADisplayStateHandle(&va_display_state);
  }

  if (!va_display_state.drm_fd_.is_valid()) {
    VLOGF(1)
        << "Either VADisplayStateSingleton::PreSandboxInitialization() hasn't "
           "been called or that method failed to find a suitable render node";
    return {};
  }

  const bool libraries_initialized = IsVaInitialized() && IsVa_drmInitialized();
  if (!libraries_initialized) {
    return {};
  }

  static_assert(
      VA_MAJOR_VERSION >= 2 || (VA_MAJOR_VERSION == 1 && VA_MINOR_VERSION >= 1),
      "Requires VA-API >= 1.1.0");

  const bool success = va_display_state.Initialize();
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiWrapper.VADisplayStateInitializeSuccess",
                        success);
  return success ? VADisplayStateHandle(&va_display_state) : VADisplayStateHandle();
}

bool VADisplayStateSingleton::Initialize() {
  // Set VA logging level, unless already set.
  constexpr char libva_log_level_env[] = "LIBVA_MESSAGING_LEVEL";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(libva_log_level_env)) {
    env->SetVar(libva_log_level_env, "1");
  }

  const VADisplay va_display = vaGetDisplayDRM(drm_fd_.get());
  absl::Cleanup va_display_cleaner_cb = [va_display] {
    if (vaDisplayIsValid(va_display)) {
      vaTerminate(va_display);
    }
  };

  if (!vaDisplayIsValid(va_display)) {
    LOG(ERROR) << "Could not get a valid VA display";
    return false;
  }

  // The VAAPI version is determined from what is loaded on the system by
  // calling vaInitialize().
  int major_version, minor_version;
  VAStatus va_res = vaInitialize(va_display, &major_version, &minor_version);
  if (va_res != VA_STATUS_SUCCESS) {
    VLOGF(1) << "vaInitialize failed: " << vaErrorStr(va_res);
    return false;
  }

  const std::string va_vendor_string = vaQueryVendorString(va_display);
  if (va_vendor_string.empty()) {
    VLOGF(1) << "vaQueryVendorString returned an empty string";
    return false;
  }
  DVLOG(1) << "VAAPI version: " << major_version << "." << minor_version << " "
           << va_vendor_string;

  const VAImplementation implementation_type =
      VendorStringToImplementationType(va_vendor_string);

  const base::Version runtime_version(
      {base::checked_cast<uint32_t>(major_version),
       base::checked_cast<uint32_t>(minor_version)});
  CHECK(runtime_version.IsValid());
  const base::Version build_time_version({VA_MAJOR_VERSION, VA_MINOR_VERSION});
  CHECK(build_time_version.IsValid());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (IsGen11Gpu()) {
    // Jasperlake devices run with pinned libva driver (VA-API version 1.15)
    // due to b/303841978.
    // Relax the VA-API version check so Lacros does not fall back to
    // software encoding on these devices by hardcoding the minor version number
    // to be 15 instead of the actual (higher) one.
    // TODO(b/303841978): go back to using the actual minor version number
    // when libva is upreved in Jasperlake devices.
    const base::Version jsl_build_version({VA_MAJOR_VERSION, 15});
    CHECK(jsl_build_version.IsValid());
    if (!IsLibVACompatible(runtime_version, jsl_build_version)) {
      return false;
    }
  } else
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!IsLibVACompatible(runtime_version, build_time_version)) {
    return false;
  }

  std::move(va_display_cleaner_cb).Cancel();
  refcount_ = 1;
  va_display_ = va_display;
  implementation_type_ = implementation_type;
  va_vendor_string_ = va_vendor_string;
  return true;
}

void VADisplayStateSingleton::OnRefDestroyed() {
  base::AutoLock lock(lock_);
  if (--refcount_ > 0) {
    return;
  }

  // No more handles to the VADisplayStateSingleton remain. We can clean up.
  vaTerminate(va_display_);
  va_display_ = nullptr;
  implementation_type_ = VAImplementation::kInvalid;
  va_vendor_string_ = "";
}

VADisplayStateHandle::VADisplayStateHandle() : va_display_state_(nullptr) {}

VADisplayStateHandle::VADisplayStateHandle(
    VADisplayStateSingleton* va_display_state)
    : va_display_state_(va_display_state) {}

VADisplayStateHandle::~VADisplayStateHandle() {
  if (va_display_state_) {
    va_display_state_->OnRefDestroyed();
  }
}

NativePixmapAndSizeInfo::NativePixmapAndSizeInfo() = default;

NativePixmapAndSizeInfo::~NativePixmapAndSizeInfo() = default;

// static
VAImplementation VaapiWrapper::GetImplementationType() {
  auto va_display_state_handle = VADisplayStateSingleton::GetHandle();
  return va_display_state_handle
             ? va_display_state_handle->implementation_type()
             : VAImplementation::kInvalid;
}

// static
int VaapiWrapper::GetMaxNumDecoderInstances() {
  if (!base::FeatureList::IsEnabled(media::kLimitConcurrentDecoderInstances)) {
    return std::numeric_limits<int>::max();
  }

  // Chromebook "grunt", with an AMD Radeon R5 (Stoney Ridge) GPU, b/266003084.
  constexpr int kAMDStoneyRidgeMaxNumOfInstances = 10;
  auto va_display_state_handle = VADisplayStateSingleton::GetHandle();
  if (va_display_state_handle &&
      base::Contains(va_display_state_handle->vendor_string(), "stoney")) {
    return kAMDStoneyRidgeMaxNumOfInstances;
  }
  // TODO(andrescj): we can relax this once we extract video decoding into its
  // own process.
  constexpr int kDefaultMaxNumOfInstances = 16;
  return kDefaultMaxNumOfInstances;
}

// static
base::expected<scoped_refptr<VaapiWrapper>, DecoderStatus> VaapiWrapper::Create(
    CodecMode mode,
    VAProfile va_profile,
    EncryptionScheme encryption_scheme,
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  if (!VASupportedProfiles::Get().IsProfileSupported(mode, va_profile)) {
    DVLOG(1) << "Unsupported va_profile: " << vaProfileStr(va_profile);
    return base::unexpected(DecoderStatus::Codes::kUnsupportedProfile);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In protected decode |mode| we need to ensure that |va_profile| is supported
  // (which we verified above) and that VAProfileProtected is supported, which
  // we check here.
  if (mode == kDecodeProtected &&
      !VASupportedProfiles::Get().IsProfileSupported(mode,
                                                     VAProfileProtected)) {
    LOG(ERROR) << "Protected content profile not supported";
    return base::unexpected(DecoderStatus::Codes::kUnsupportedEncryptionMode);
  }
#endif

  auto va_display_state_handle = VADisplayStateSingleton::GetHandle();
  if (!va_display_state_handle) {
    return base::unexpected(DecoderStatus::Codes::kFailed);
  }

  if (mode == kDecode) {
    static const auto decoder_instances_limit =
        VaapiWrapper::GetMaxNumDecoderInstances();
    const bool can_create_decoder =
        num_decoder_instances_.Increment() < decoder_instances_limit ||
        !base::FeatureList::IsEnabled(media::kLimitConcurrentDecoderInstances);
    if (!can_create_decoder) {
      num_decoder_instances_.Decrement();
      return base::unexpected(DecoderStatus::Codes::kTooManyDecoders);
    }
  }

  scoped_refptr<VaapiWrapper> vaapi_wrapper(
      new VaapiWrapper(std::move(va_display_state_handle), mode));
  vaapi_wrapper->VaInitialize(report_error_to_uma_cb);
  if (vaapi_wrapper->Initialize(va_profile, encryption_scheme))
    return vaapi_wrapper;

  LOG(ERROR) << "Failed to create VaapiWrapper for va_profile: "
             << vaProfileStr(va_profile);
  return base::unexpected(DecoderStatus::Codes::kFailed);
}

// static
base::AtomicRefCount VaapiWrapper::num_decoder_instances_(0);

// static
base::expected<scoped_refptr<VaapiWrapper>, DecoderStatus>
VaapiWrapper::CreateForVideoCodec(
    CodecMode mode,
    VideoCodecProfile profile,
    EncryptionScheme encryption_scheme,
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  const VAProfile va_profile = ProfileToVAProfile(profile);
  return Create(mode, va_profile, encryption_scheme, report_error_to_uma_cb);
}

// static
std::vector<SVCScalabilityMode> VaapiWrapper::GetSupportedScalabilityModes(
    VideoCodecProfile media_profile,
    VAProfile va_profile) {
  std::vector<SVCScalabilityMode> scalability_modes;
  scalability_modes.push_back(SVCScalabilityMode::kL1T1);
#if BUILDFLAG(IS_CHROMEOS)
  if (media_profile == VP9PROFILE_PROFILE0) {
    scalability_modes.push_back(SVCScalabilityMode::kL1T2);
    scalability_modes.push_back(SVCScalabilityMode::kL1T3);
    const VAEntrypoint va_entry_point = GetDefaultVaEntryPoint(
        VaapiWrapper::kEncodeConstantQuantizationParameter, va_profile);
    if (va_entry_point == VAEntrypointEncSliceLP ||
        va_entry_point == VAEntrypointEncSlice) {
      scalability_modes.push_back(SVCScalabilityMode::kL2T2Key);
      scalability_modes.push_back(SVCScalabilityMode::kL2T3Key);
      scalability_modes.push_back(SVCScalabilityMode::kL3T2Key);
      scalability_modes.push_back(SVCScalabilityMode::kL3T3Key);
      if (base::FeatureList::IsEnabled(kVaapiVp9SModeHWEncoding)) {
        scalability_modes.push_back(SVCScalabilityMode::kS2T1);
        scalability_modes.push_back(SVCScalabilityMode::kS2T2);
        scalability_modes.push_back(SVCScalabilityMode::kS2T3);
        scalability_modes.push_back(SVCScalabilityMode::kS3T1);
        scalability_modes.push_back(SVCScalabilityMode::kS3T2);
        scalability_modes.push_back(SVCScalabilityMode::kS3T3);
      }
    }
  }

  if (media_profile >= VP8PROFILE_MIN && media_profile <= VP8PROFILE_MAX) {
    if (base::FeatureList::IsEnabled(kVaapiVp8TemporalLayerHWEncoding)) {
      scalability_modes.push_back(SVCScalabilityMode::kL1T2);
      scalability_modes.push_back(SVCScalabilityMode::kL1T3);
    }
  }

  if (media_profile >= H264PROFILE_MIN && media_profile <= H264PROFILE_MAX) {
    if (base::FeatureList::IsEnabled(kVaapiH264TemporalLayerHWEncoding)) {
      scalability_modes.push_back(SVCScalabilityMode::kL1T2);
      scalability_modes.push_back(SVCScalabilityMode::kL1T3);
    }
  }
#endif
  return scalability_modes;
}

// static
VideoEncodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;

  for (const auto& [media_profile, va_profile] : GetProfileCodecMap()) {
    DCHECK(va_profile != VAProfileNone);

    const VASupportedProfiles::ProfileInfo* profile_info =
        VASupportedProfiles::Get().IsProfileSupported(kEncodeConstantBitrate,
                                                      va_profile);
    if (!profile_info)
      continue;

    VideoEncodeAccelerator::SupportedProfile profile;
    profile.profile = media_profile;
    profile.min_resolution = profile_info->min_resolution;
    profile.max_resolution = profile_info->max_resolution;
    // Maximum framerate of encoded profile. This value is an arbitrary
    // limit and not taken from HW documentation.
    constexpr int kMaxEncoderFramerate = 30;
    profile.max_framerate_numerator = kMaxEncoderFramerate;
    profile.max_framerate_denominator = 1;
    profile.rate_control_modes = media::VideoEncodeAccelerator::kConstantMode;
    // This code assumes that the resolutions are the same between CBR and VBR.
    // This is checked in a test in vaapi_unittest.cc: VbrAndCbrResolutionsMatch
    if (IsVBREncodingSupported(va_profile)) {
      profile.rate_control_modes |=
          media::VideoEncodeAccelerator::kVariableMode;
    }
    profile.scalability_modes =
        GetSupportedScalabilityModes(media_profile, va_profile);
    profiles.push_back(profile);
  }
  return profiles;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedDecodeProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles;

  for (const auto& [media_profile, va_profile] : GetProfileCodecMap()) {
    DCHECK(va_profile != VAProfileNone);

    const VASupportedProfiles::ProfileInfo* profile_info =
        VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
    if (!profile_info)
      continue;

    VideoDecodeAccelerator::SupportedProfile profile;
    profile.profile = media_profile;
    profile.max_resolution = profile_info->max_resolution;
    profile.min_resolution = profile_info->min_resolution;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
bool VaapiWrapper::IsDecodeSupported(VAProfile va_profile) {
  return VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
}

// static
VaapiWrapper::InternalFormats VaapiWrapper::GetDecodeSupportedInternalFormats(
    VAProfile va_profile) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
  if (!profile_info)
    return InternalFormats{};
  return profile_info->supported_internal_formats;
}

// static
bool VaapiWrapper::IsDecodingSupportedForInternalFormat(
    VAProfile va_profile,
    unsigned int rt_format) {
  static const VaapiWrapper::InternalFormats supported_internal_formats(
      VaapiWrapper::GetDecodeSupportedInternalFormats(va_profile));
  switch (rt_format) {
    case VA_RT_FORMAT_YUV420:
      return supported_internal_formats.yuv420;
    case VA_RT_FORMAT_YUV420_10:
      return supported_internal_formats.yuv420_10;
    case VA_RT_FORMAT_YUV422:
      return supported_internal_formats.yuv422;
    case VA_RT_FORMAT_YUV444:
      return supported_internal_formats.yuv444;
  }
  return false;
}

// static
bool VaapiWrapper::GetSupportedResolutions(VAProfile va_profile,
                                           CodecMode codec_mode,
                                           gfx::Size& min_size,
                                           gfx::Size& max_size) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(codec_mode, va_profile);
  if (!profile_info || profile_info->max_resolution.IsEmpty())
    return false;

  min_size = gfx::Size(std::max(1, profile_info->min_resolution.width()),
                       std::max(1, profile_info->min_resolution.height()));
  max_size = profile_info->max_resolution;
  return true;
}

// static
bool VaapiWrapper::GetJpegDecodeSuitableImageFourCC(unsigned int rt_format,
                                                    uint32_t preferred_fourcc,
                                                    uint32_t* suitable_fourcc) {
  if (!IsDecodingSupportedForInternalFormat(VAProfileJPEGBaseline, rt_format))
    return false;

  // Work around some driver-specific conversion issues. If you add a workaround
  // here, please update the VaapiJpegDecoderTest.MinimalImageFormatSupport
  // test.
  DCHECK_NE(VAImplementation::kInvalid, GetImplementationType());
  if (GetImplementationType() == VAImplementation::kMesaGallium) {
    // The VAAPI mesa state tracker only supports conversion from NV12 to YV12
    // and IYUV (synonym of I420).
    if (rt_format == VA_RT_FORMAT_YUV420) {
      if (preferred_fourcc != VA_FOURCC_I420 &&
          preferred_fourcc != VA_FOURCC_YV12) {
        preferred_fourcc = VA_FOURCC_NV12;
      }
    } else if (rt_format == VA_RT_FORMAT_YUV422) {
      preferred_fourcc = VA_FOURCC('Y', 'U', 'Y', 'V');
    } else {
      // Out of the three internal formats we care about (4:2:0, 4:2:2, and
      // 4:4:4), this driver should only support the first two. Since we check
      // for supported internal formats at the beginning of this function, we
      // shouldn't get here.
      NOTREACHED();
    }
  } else if (GetImplementationType() == VAImplementation::kIntelI965) {
    // Workaround deduced from observations in samus and nocturne: we found that
    //
    // - For a 4:2:2 image, the internal format is 422H.
    // - For a 4:2:0 image, the internal format is IMC3.
    // - For a 4:4:4 image, the internal format is 444P.
    //
    // For these internal formats and an image format of either 422H or P010, an
    // intermediate NV12 surface is allocated. Then, a conversion is made from
    // {422H, IMC3, 444P} -> NV12 -> {422H, P010}. Unfortunately, the
    // NV12 -> {422H, P010} conversion is unimplemented in
    // i965_image_pl2_processing(). So, when |preferred_fourcc| is either 422H
    // or P010, we can just fallback to I420.
    //
    if (preferred_fourcc == VA_FOURCC_422H ||
        preferred_fourcc == VA_FOURCC_P010) {
      preferred_fourcc = VA_FOURCC_I420;
    }
  } else if (GetImplementationType() == VAImplementation::kIntelIHD) {
    // (b/159896972): iHD v20.1.1 cannot create Y216 and Y416 images from a
    // decoded JPEG on gen 12. It is also failing to support Y800 format.
    //
    // (b/344835625): the VPP in MTL cannot be configured to output 422H.
    if (preferred_fourcc == VA_FOURCC_Y216 ||
        preferred_fourcc == VA_FOURCC_Y416 ||
        preferred_fourcc == VA_FOURCC_Y800 ||
        preferred_fourcc == VA_FOURCC_422H) {
      preferred_fourcc = VA_FOURCC_I420;
    }
  }

  if (!VASupportedImageFormats::Get().IsImageFormatSupported(
          VAImageFormat{.fourcc = preferred_fourcc})) {
    preferred_fourcc = VA_FOURCC_I420;
  }

  // After workarounds, assume the conversion is supported.
  *suitable_fourcc = preferred_fourcc;
  return true;
}

// static
bool VaapiWrapper::IsVppResolutionAllowed(const gfx::Size& size) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kVideoProcess,
                                                    VAProfileNone);
  if (!profile_info)
    return false;

  return size.width() >= profile_info->min_resolution.width() &&
         size.width() <= profile_info->max_resolution.width() &&
         size.height() >= profile_info->min_resolution.height() &&
         size.height() <= profile_info->max_resolution.height();
}

// static
bool VaapiWrapper::IsVppFormatSupported(uint32_t va_fourcc) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kVideoProcess,
                                                    VAProfileNone);
  if (!profile_info)
    return false;

  return base::Contains(profile_info->pixel_formats, va_fourcc);
}

// static
std::vector<Fourcc> VaapiWrapper::GetVppSupportedFormats() {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kVideoProcess,
                                                    VAProfileNone);
  if (!profile_info)
    return {};

  std::vector<Fourcc> supported_fourccs;
  for (uint32_t pixel_format : profile_info->pixel_formats) {
    auto fourcc = Fourcc::FromVAFourCC(pixel_format);
    if (!fourcc)
      continue;
    supported_fourccs.push_back(*fourcc);
  }
  return supported_fourccs;
}

// static
bool VaapiWrapper::IsVppSupportedForJpegDecodedSurfaceToFourCC(
    unsigned int rt_format,
    uint32_t fourcc) {
  if (!IsDecodingSupportedForInternalFormat(VAProfileJPEGBaseline, rt_format))
    return false;


  return IsVppFormatSupported(fourcc);
}

// static
bool VaapiWrapper::IsJpegEncodeSupported() {
  return VASupportedProfiles::Get().IsProfileSupported(kEncodeConstantBitrate,
                                                       VAProfileJPEGBaseline);
}

// static
bool VaapiWrapper::IsImageFormatSupported(const VAImageFormat& format) {
  return VASupportedImageFormats::Get().IsImageFormatSupported(format);
}

// static
const std::vector<VAImageFormat>&
VaapiWrapper::GetSupportedImageFormatsForTesting() {
  return VASupportedImageFormats::Get().GetSupportedImageFormats();
}

// static
std::map<VAProfile, std::vector<VAEntrypoint>>
VaapiWrapper::GetSupportedConfigurationsForCodecModeForTesting(CodecMode mode) {
  std::map<VAProfile, std::vector<VAEntrypoint>> configurations;
  for (const auto& supported_profile :
       VASupportedProfiles::Get().supported_profiles_[mode]) {
    configurations[supported_profile.va_profile].push_back(
        supported_profile.va_entrypoint);
  }
  return configurations;
}

// static
VAEntrypoint VaapiWrapper::GetDefaultVaEntryPoint(CodecMode mode,
                                                  VAProfile profile) {
  switch (mode) {
    case VaapiWrapper::kDecode:
      return VAEntrypointVLD;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case VaapiWrapper::kDecodeProtected:
      if (profile == VAProfileProtected)
        return VAEntrypointProtectedContent;
      return VAEntrypointVLD;
#endif
    case VaapiWrapper::kEncodeConstantBitrate:
    case VaapiWrapper::kEncodeConstantQuantizationParameter:
    case VaapiWrapper::kEncodeVariableBitrate:
      if (profile == VAProfileJPEGBaseline)
        return VAEntrypointEncPicture;
      DCHECK(IsModeEncoding(mode));
      if (IsLowPowerEncSupported(profile))
        return VAEntrypointEncSliceLP;
      return VAEntrypointEncSlice;
    case VaapiWrapper::kVideoProcess:
      return VAEntrypointVideoProc;
    case VaapiWrapper::kCodecModeMax:
      NOTREACHED();
  }
}

// static
uint32_t VaapiWrapper::BufferFormatToVARTFormat(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBA_8888:
      return VA_RT_FORMAT_RGB32;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VA_RT_FORMAT_YUV420;
    case gfx::BufferFormat::P010:
      return VA_RT_FORMAT_YUV420_10BPP;
    default:
      NOTREACHED_IN_MIGRATION() << gfx::BufferFormatToString(fmt);
      return 0;
  }
}

bool VaapiWrapper::CreateContextAndSurfaces(
    unsigned int va_format,
    const gfx::Size& size,
    const std::vector<SurfaceUsageHint>& surface_usage_hints,
    size_t num_surfaces,
    std::vector<VASurfaceID>* va_surfaces) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";
  DCHECK(va_surfaces->empty());

  if (va_context_id_ != VA_INVALID_ID) {
    LOG(ERROR)
        << "The current context should be destroyed before creating a new one";
    return false;
  }

  if (!CreateSurfaces(va_format, size, surface_usage_hints, num_surfaces,
                      va_surfaces)) {
    return false;
  }

  const bool success = CreateContext(size);
  if (!success)
    DestroyContextAndSurfaces(*va_surfaces);
  return success;
}

std::vector<std::unique_ptr<ScopedVASurface>>
VaapiWrapper::CreateContextAndScopedVASurfaces(
    unsigned int va_format,
    const gfx::Size& size,
    const std::vector<SurfaceUsageHint>& usage_hints,
    size_t num_surfaces,
    const std::optional<gfx::Size>& visible_size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (va_context_id_ != VA_INVALID_ID) {
    LOG(ERROR) << "The current context should be destroyed before creating a "
                  "new one";
    return {};
  }

  std::vector<std::unique_ptr<ScopedVASurface>> scoped_va_surfaces =
      CreateScopedVASurfaces(va_format, size, usage_hints, num_surfaces,
                             visible_size, /*va_fourcc=*/std::nullopt);
  if (scoped_va_surfaces.empty())
    return {};

  if (CreateContext(size))
    return scoped_va_surfaces;

  DestroyContext();
  return {};
}

bool VaapiWrapper::CreateProtectedSession(
    EncryptionScheme encryption,
    const std::vector<uint8_t>& hw_config,
    std::vector<uint8_t>* hw_identifier_out) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK_EQ(va_protected_config_id_, VA_INVALID_ID);
  DCHECK_EQ(va_protected_session_id_, VA_INVALID_ID);
  DCHECK(hw_identifier_out);
  if (mode_ != kDecodeProtected) {
    LOG(ERROR) << "Cannot attached protected context if not in protected mode";
    return false;
  }
  if (encryption == EncryptionScheme::kUnencrypted) {
    LOG(ERROR) << "Must specify encryption scheme for protected mode";
    return false;
  }
  const VAProfile va_profile = VAProfileProtected;
  const VAEntrypoint entrypoint = GetDefaultVaEntryPoint(mode_, va_profile);
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    std::vector<VAConfigAttrib> required_attribs;
    if (!GetRequiredAttribs(va_lock_, va_display_, mode_, va_profile,
                            entrypoint, &required_attribs)) {
      LOG(ERROR) << "Failed getting required attributes for protected mode";
      return false;
    }
    DCHECK(!required_attribs.empty());

    // We need to adjust the attribute for encryption scheme.
    for (auto& attrib : required_attribs) {
      if (attrib.type == VAConfigAttribProtectedContentCipherMode) {
        attrib.value = (encryption == EncryptionScheme::kCbcs)
                           ? VA_PC_CIPHER_MODE_CBC
                           : VA_PC_CIPHER_MODE_CTR;
      }
    }

    VAStatus va_res = vaCreateConfig(
        va_display_, va_profile, entrypoint, &required_attribs[0],
        required_attribs.size(), &va_protected_config_id_);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);

    va_res = vaCreateProtectedSession(va_display_, va_protected_config_id_,
                                      &va_protected_session_id_);
    DCHECK(va_res == VA_STATUS_SUCCESS ||
           va_protected_session_id_ == VA_INVALID_ID);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateProtectedSession,
                         false);
  }
  // We have to hold the VABuffer outside of the lock because its destructor
  // will acquire the lock when it goes out of scope. We also must do this after
  // we create the protected session.
  VAProtectedSessionExecuteBuffer hw_update_buf;
  std::unique_ptr<ScopedVABuffer> hw_update = CreateVABuffer(
      VAProtectedSessionExecuteBufferType, sizeof(hw_update_buf));
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    constexpr size_t kHwIdentifierMaxSize = 64;
    memset(&hw_update_buf, 0, sizeof(hw_update_buf));
    hw_update_buf.function_id = VA_TEE_EXEC_TEE_FUNCID_HW_UPDATE;
    hw_update_buf.input.data_size = hw_config.size();
    hw_update_buf.input.data =
        static_cast<void*>(const_cast<uint8_t*>(hw_config.data()));
    hw_update_buf.output.max_data_size = kHwIdentifierMaxSize;
    hw_identifier_out->resize(kHwIdentifierMaxSize);
    hw_update_buf.output.data = hw_identifier_out->data();
    if (!MapAndCopy_Locked(
            hw_update->id(),
            {hw_update->type(), hw_update->size(), &hw_update_buf})) {
      LOG(ERROR) << "Failed mapping Execute buf";
      return false;
    }

    VAStatus va_res = vaProtectedSessionExecute(
        va_display_, va_protected_session_id_, hw_update->id());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAProtectedSessionExecute,
                         false);

    auto mapping =
        ScopedVABufferMapping::Create(va_lock_, va_display_, hw_update->id());
    if (!mapping) {
      LOG(ERROR) << "Failed mapping returned Execute buf";
      return false;
    }
    auto* hw_update_buf_out =
        reinterpret_cast<VAProtectedSessionExecuteBuffer*>(mapping->data());
    if (!hw_update_buf_out->output.data_size) {
      LOG(ERROR) << "Received empty HW identifier";
      return false;
    }
    hw_identifier_out->resize(hw_update_buf_out->output.data_size);
    memcpy(hw_identifier_out->data(), hw_update_buf_out->output.data,
           hw_update_buf_out->output.data_size);

    // If the decoding context is created, attach the protected session.
    // Otherwise this is done in CreateContext when the decoding context is
    // created.
    return MaybeAttachProtectedSession_Locked();
  }
#else
  NOTIMPLEMENTED() << "Protected content mode not supported";
  return false;
#endif
}

bool VaapiWrapper::IsProtectedSessionDead() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return IsProtectedSessionDead(va_protected_session_id_);
#else
  return false;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool VaapiWrapper::IsProtectedSessionDead(
    VAProtectedSessionID va_protected_session_id) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (va_protected_session_id == VA_INVALID_ID)
    return false;

  uint8_t alive;
  VAProtectedSessionExecuteBuffer tee_exec_buf = {};
  tee_exec_buf.function_id = VA_TEE_EXEC_TEE_FUNCID_IS_SESSION_ALIVE;
  tee_exec_buf.input.data_size = 0;
  tee_exec_buf.input.data = nullptr;
  tee_exec_buf.output.data_size = sizeof(alive);
  tee_exec_buf.output.data = &alive;

  base::AutoLockMaybe auto_lock(va_lock_.get());
  VABufferID buf_id;
  VAStatus va_res = vaCreateBuffer(
      va_display_, va_protected_session_id, VAProtectedSessionExecuteBufferType,
      sizeof(tee_exec_buf), 1, &tee_exec_buf, &buf_id);
  // Failure here is valid if the protected session has been closed.
  if (va_res != VA_STATUS_SUCCESS)
    return true;

  va_res =
      vaProtectedSessionExecute(va_display_, va_protected_session_id, buf_id);
  vaDestroyBuffer(va_display_, buf_id);
  if (va_res != VA_STATUS_SUCCESS)
    return true;

  return !alive;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
VAProtectedSessionID VaapiWrapper::GetProtectedSessionID() const {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return va_protected_session_id_;
}
#endif

void VaapiWrapper::DestroyProtectedSession() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (va_protected_session_id_ == VA_INVALID_ID)
    return;
  base::AutoLockMaybe auto_lock(va_lock_.get());
  VAStatus va_res =
      vaDestroyProtectedSession(va_display_, va_protected_session_id_);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyProtectedSession);
  va_res = vaDestroyConfig(va_display_, va_protected_config_id_);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyConfig);
  va_protected_session_id_ = VA_INVALID_ID;
  va_protected_config_id_ = VA_INVALID_ID;
#endif
}

void VaapiWrapper::DestroyContextAndSurfaces(
    std::vector<VASurfaceID> va_surfaces) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyContext();
  DestroySurfaces(va_surfaces);
}

bool VaapiWrapper::CreateContext(const gfx::Size& size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Creating context";
  base::AutoLockMaybe auto_lock(va_lock_.get());

  // vaCreateContext() doesn't really need an array of VASurfaceIDs (see
  // https://lists.01.org/pipermail/intel-vaapi-media/2017-July/000052.html and
  // https://github.com/intel/libva/issues/251); pass a dummy list of valid
  // (non-null) IDs until the signature gets updated.
  constexpr VASurfaceID* empty_va_surfaces_ids_pointer = nullptr;
  constexpr size_t empty_va_surfaces_ids_size = 0u;

  // No flag must be set and passing picture size is irrelevant in the case of
  // vpp, just passing 0x0.
  const int flag = mode_ != kVideoProcess ? VA_PROGRESSIVE : 0x0;
  const gfx::Size picture_size = mode_ != kVideoProcess ? size : gfx::Size();
  if (base::FeatureList::IsEnabled(kVaapiEnforceVideoMinMaxResolution) &&
      mode_ != kVideoProcess) {
    const VASupportedProfiles::ProfileInfo* profile_info =
        VASupportedProfiles::Get().IsProfileSupported(mode_, va_profile_,
                                                      va_entrypoint_);
    DCHECK(profile_info);
    const bool is_picture_within_bounds =
        gfx::Rect(picture_size)
            .Contains(gfx::Rect(profile_info->min_resolution)) &&
        gfx::Rect(profile_info->max_resolution)
            .Contains(gfx::Rect(picture_size));
    if (!is_picture_within_bounds) {
      VLOG(2) << "Requested resolution=" << picture_size.ToString()
              << " is not within bounds ["
              << profile_info->min_resolution.ToString() << ", "
              << profile_info->max_resolution.ToString() << "]";
      return false;
    }
  }

  VAStatus va_res = vaCreateContext(
      va_display_, va_config_id_, picture_size.width(), picture_size.height(),
      flag, empty_va_surfaces_ids_pointer, empty_va_surfaces_ids_size,
      &va_context_id_);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVACreateContext);
  if (va_res != VA_STATUS_SUCCESS)
    return false;

  // TODO(b/200779101): Remove low resolution i965 condition. This was
  // added to avoid a duplicated frame specific to quality 7 at ~400kbps.
  if (IsModeEncoding(mode_) && IsLowPowerIntelProcessor() &&
      !(GetImplementationType() == VAImplementation::kIntelI965 &&
        picture_size.GetArea() <= gfx::Size(320, 240).GetArea())) {
    MaybeSetLowQualityEncoding_Locked();
  }

  // If we have a protected session already, attach it to this new context.
  return MaybeAttachProtectedSession_Locked();
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateVASurfaceForFrameResource(
    const FrameResource& frame,
    bool protected_content) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto pixmap = frame.GetNativePixmapDmaBuf();
  if (!pixmap) {
    LOG(ERROR) << "Failed to create NativePixmap from FrameResource";
    return nullptr;
  }
  return CreateVASurfaceForPixmap(std::move(pixmap), protected_content);
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateVASurfaceForPixmap(
    scoped_refptr<const gfx::NativePixmap> pixmap,
    bool protected_content) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const gfx::BufferFormat buffer_format = pixmap->GetBufferFormat();
  if (!BufferFormatToVAFourCC(buffer_format)) {
    LOG(ERROR) << "Failed to get the VA fourcc from the buffer format";
    return nullptr;
  }

  // TODO(b/233894465): use the DRM_PRIME_2 API with the Mesa Gallium driver
  // when AMD supports it.
  // TODO(b/233924862): use the DRM_PRIME_2 API with protected content.
  // TODO(b/233929647): use the DRM_PRIME_2 API with the i965 driver.
  // TODO(b/236746283): remove the kNoModifier check once the modifier is
  // plumbed for JPEG decoding and encoding.
  const bool use_drm_prime_2 =
      (GetImplementationType() == VAImplementation::kIntelIHD ||
       GetImplementationType() == VAImplementation::kChromiumFakeDriver ||
       GetImplementationType() == VAImplementation::kMesaGallium) &&
      !protected_content &&
      pixmap->GetBufferFormatModifier() != gfx::NativePixmapHandle::kNoModifier;

  union {
    VADRMPRIMESurfaceDescriptor descriptor;
    VASurfaceAttribExternalBuffersAndFD va_attrib_extbuf_and_fd;
  };

  if (use_drm_prime_2) {
    if (!FillVADRMPRIMESurfaceDescriptor(*pixmap, descriptor))
      return nullptr;
  } else {
    if (!FillVASurfaceAttribExternalBuffers(*pixmap, va_attrib_extbuf_and_fd))
      return nullptr;
  }

  unsigned int va_format =
      base::strict_cast<unsigned int>(BufferFormatToVARTFormat(buffer_format));
  if (!va_format) {
    LOG(ERROR) << "Failed to get the VA RT format from the buffer format";
    return nullptr;
  }

  if (protected_content) {
    if (GetImplementationType() == VAImplementation::kMesaGallium) {
      va_format |= VA_RT_FORMAT_PROTECTED;
    } else {
      va_attrib_extbuf_and_fd.va_attrib_extbuf.flags =
          VA_SURFACE_EXTBUF_DESC_PROTECTED |
          VA_SURFACE_EXTBUF_DESC_ENABLE_TILING;
    }
  }

  std::vector<VASurfaceAttrib> va_attribs(2);

  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = use_drm_prime_2
                                    ? VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2
                                    : VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = use_drm_prime_2
                                    ? static_cast<void*>(&descriptor)
                                    : &va_attrib_extbuf_and_fd.va_attrib_extbuf;

  const gfx::Size size = pixmap->GetBufferSize();
  VASurfaceID va_surface_id = VA_INVALID_ID;
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    VAStatus va_res = vaCreateSurfaces(
        va_display_, va_format, base::checked_cast<unsigned int>(size.width()),
        base::checked_cast<unsigned int>(size.height()), &va_surface_id, 1,
        &va_attribs[0], va_attribs.size());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateSurfaces_Importing,
                         nullptr);
  }
  DVLOG(3) << __func__ << " " << va_surface_id;
  // VASurface shares an ownership of the buffer referred by the passed file
  // descriptor. |pixmap| can be released here.
  return std::make_unique<ScopedVASurface>(this, va_surface_id, size,
                                           va_format);
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateVASurfaceForUserPtr(
    const gfx::Size& size,
    uintptr_t* buffers,
    size_t buffer_size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VASurfaceAttribExternalBuffers va_attrib_extbuf{};
  va_attrib_extbuf.num_planes = 3;
  va_attrib_extbuf.buffers = buffers;
  va_attrib_extbuf.data_size = base::checked_cast<uint32_t>(buffer_size);
  va_attrib_extbuf.num_buffers = 1u;
  va_attrib_extbuf.width = base::checked_cast<uint32_t>(size.width());
  va_attrib_extbuf.height = base::checked_cast<uint32_t>(size.height());
  va_attrib_extbuf.offsets[0] = 0;
  va_attrib_extbuf.offsets[1] = size.GetCheckedArea().ValueOrDie<uint32_t>();
  va_attrib_extbuf.offsets[2] =
      (size.GetCheckedArea() * 2).ValueOrDie<uint32_t>();
  std::fill(va_attrib_extbuf.pitches, va_attrib_extbuf.pitches + 3,
            base::checked_cast<uint32_t>(size.width()));
  va_attrib_extbuf.pixel_format = VA_FOURCC_RGBP;

  std::vector<VASurfaceAttrib> va_attribs(2);
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = &va_attrib_extbuf;

  VASurfaceID va_surface_id = VA_INVALID_ID;
  const unsigned int va_format = VA_RT_FORMAT_RGBP;
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    VAStatus va_res = vaCreateSurfaces(
        va_display_, va_format, base::checked_cast<unsigned int>(size.width()),
        base::checked_cast<unsigned int>(size.height()), &va_surface_id, 1,
        &va_attribs[0], va_attribs.size());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateSurfaces_Importing,
                         nullptr);
  }
  DVLOG(2) << __func__ << " " << va_surface_id;
  return std::make_unique<ScopedVASurface>(this, va_surface_id, size,
                                           va_format);
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateVASurfaceWithUsageHints(
    unsigned int va_rt_format,
    const gfx::Size& size,
    const std::vector<SurfaceUsageHint>& usage_hints) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<VASurfaceID> surfaces;
  if (!CreateSurfaces(va_rt_format, size, usage_hints, 1, &surfaces))
    return nullptr;
  return std::make_unique<ScopedVASurface>(this, surfaces[0], size,
                                           va_rt_format);
}

std::unique_ptr<NativePixmapAndSizeInfo>
VaapiWrapper::ExportVASurfaceAsNativePixmapDmaBufUnwrapped(
    VASurfaceID va_surface_id,
    const gfx::Size& va_surface_size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(va_surface_id, VA_INVALID_SURFACE);
  DCHECK(!va_surface_size.IsEmpty());
  VADRMPRIMESurfaceDescriptor descriptor;
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, nullptr);
    va_res = vaExportSurfaceHandle(
        va_display_, va_surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
        &descriptor);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAExportSurfaceHandle,
                         nullptr);
  }

  // We only support one bo containing all the planes. The fd should be owned by
  // us: per va/va.h, "the exported handles are owned by the caller."
  //
  // TODO(crbug.com/40632250): support multiple buffer objects so that this can
  // work in AMD.
  CHECK_EQ(descriptor.num_objects, 1u)
      << "Only surface descriptors with one bo are supported";
  base::ScopedFD bo_fd(descriptor.objects[0].fd);
  const uint64_t bo_modifier = descriptor.objects[0].drm_format_modifier;

  // Translate the pixel format to a gfx::BufferFormat.
  gfx::BufferFormat buffer_format;
  switch (descriptor.fourcc) {
    case VA_FOURCC_IMC3:
      // IMC3 is like I420 but all the planes have the same stride. This is used
      // for decoding 4:2:0 JPEGs in the Intel i965 driver. We don't currently
      // have a gfx::BufferFormat for YUV420. Instead, we reuse YVU_420 and
      // later swap the U and V planes.
      //
      // TODO(andrescj): revisit this once crrev.com/c/1573718 lands.
      buffer_format = gfx::BufferFormat::YVU_420;
      break;
    case VA_FOURCC_NV12:
      buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
      break;
    case VA_FOURCC_P010:
      buffer_format = gfx::BufferFormat::P010;
      break;
    case VA_FOURCC_ARGB:
      buffer_format = gfx::BufferFormat::BGRA_8888;
      break;
    default:
      LOG(ERROR) << "Cannot export a surface with FOURCC "
                 << FourccToString(descriptor.fourcc);
      return nullptr;
  }

  gfx::NativePixmapHandle handle{};
  handle.modifier = bo_modifier;
  for (uint32_t layer = 0; layer < descriptor.num_layers; layer++) {
    // According to va/va_drmcommon.h, if VA_EXPORT_SURFACE_SEPARATE_LAYERS is
    // specified, each layer should contain one plane.
    DCHECK_EQ(1u, descriptor.layers[layer].num_planes);

    auto plane_fd = base::ScopedFD(
        layer == 0 ? bo_fd.release()
                   : HANDLE_EINTR(dup(handle.planes[0].fd.get())));
    PCHECK(plane_fd.is_valid());
    constexpr uint64_t kZeroSizeToPreventMapping = 0u;
    handle.planes.emplace_back(
        base::checked_cast<int>(descriptor.layers[layer].pitch[0]),
        base::checked_cast<int>(descriptor.layers[layer].offset[0]),
        kZeroSizeToPreventMapping, std::move(plane_fd));
  }

  if (descriptor.fourcc == VA_FOURCC_IMC3) {
    // Recall that for VA_FOURCC_IMC3, we will return a format of
    // gfx::BufferFormat::YVU_420, so we need to swap the U and V planes to keep
    // the semantics.
    DCHECK_EQ(3u, handle.planes.size());
    std::swap(handle.planes[1], handle.planes[2]);
  }

  auto exported_pixmap = std::make_unique<NativePixmapAndSizeInfo>();
  exported_pixmap->va_surface_resolution =
      gfx::Size(base::checked_cast<int>(descriptor.width),
                base::checked_cast<int>(descriptor.height));
  exported_pixmap->byte_size =
      base::strict_cast<size_t>(descriptor.objects[0].size);
  if (!gfx::Rect(exported_pixmap->va_surface_resolution)
           .Contains(gfx::Rect(va_surface_size))) {
    LOG(ERROR) << "A " << va_surface_size.ToString()
               << " surface cannot be contained by a "
               << exported_pixmap->va_surface_resolution.ToString()
               << " buffer";
    return nullptr;
  }
  exported_pixmap->pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      va_surface_size, buffer_format, std::move(handle));
  return exported_pixmap;
}

std::unique_ptr<NativePixmapAndSizeInfo>
VaapiWrapper::ExportVASurfaceAsNativePixmapDmaBuf(
    const ScopedVASurface& scoped_va_surface) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!scoped_va_surface.IsValid()) {
    LOG(ERROR) << "Cannot export an invalid surface";
    return nullptr;
  }
  return ExportVASurfaceAsNativePixmapDmaBufUnwrapped(scoped_va_surface.id(),
                                                      scoped_va_surface.size());
}

bool VaapiWrapper::SyncSurface(VASurfaceID va_surface_id) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(va_surface_id, VA_INVALID_ID);

  base::AutoLockMaybe auto_lock(va_lock_.get());

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, false);
  return true;
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                const void* data) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffer");
  base::AutoLockMaybe auto_lock(va_lock_.get());
  return SubmitBuffer_Locked({va_buffer_type, size, data});
}

bool VaapiWrapper::SubmitBuffers(
    const std::vector<VABufferDescriptor>& va_buffers) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffers");
  base::AutoLockMaybe auto_lock(va_lock_.get());
  for (const VABufferDescriptor& va_buffer : va_buffers) {
    if (!SubmitBuffer_Locked(va_buffer))
      return false;
  }
  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DestroyPendingBuffers");
  base::AutoLockMaybe auto_lock(va_lock_.get());
  DestroyPendingBuffers_Locked();
}

void VaapiWrapper::DestroyPendingBuffers_Locked() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DestroyPendingBuffers_Locked");
  MAYBE_ASSERT_ACQUIRED(va_lock_);
  for (const auto& pending_va_buf : pending_va_buffers_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_va_buf);
    VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyBuffer);
  }
  pending_va_buffers_.clear();
}

bool VaapiWrapper::ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLockMaybe auto_lock(va_lock_.get());
  bool result = Execute_Locked(va_surface_id, pending_va_buffers_);
  DestroyPendingBuffers_Locked();
  return result;
}

bool VaapiWrapper::MapAndCopyAndExecute(
    VASurfaceID va_surface_id,
    const std::vector<std::pair<VABufferID, VABufferDescriptor>>& va_buffers) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(va_surface_id, VA_INVALID_SURFACE);

  TRACE_EVENT0("media,gpu", "VaapiWrapper::MapAndCopyAndExecute");
  base::AutoLockMaybe auto_lock(va_lock_.get());
  std::vector<VABufferID> va_buffer_ids;

  for (const auto& va_buffer : va_buffers) {
    const VABufferID va_buffer_id = va_buffer.first;
    const VABufferDescriptor& descriptor = va_buffer.second;
    DCHECK_NE(va_buffer_id, VA_INVALID_ID);

    if (!MapAndCopy_Locked(va_buffer_id, descriptor))
      return false;

    va_buffer_ids.push_back(va_buffer_id);
  }

  return Execute_Locked(va_surface_id, va_buffer_ids);
}

std::unique_ptr<ScopedVAImage> VaapiWrapper::CreateVaImage(
    VASurfaceID va_surface_id,
    const VAImageFormat& format,
    const gfx::Size& size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());

    VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, nullptr);

    scoped_image = ScopedVAImage::Create(va_lock_, va_display_, va_surface_id,
                                         format, size);
  }
  return scoped_image;
}

bool VaapiWrapper::UploadVideoFrameToSurface(const VideoFrame& frame,
                                             VASurfaceID va_surface_id,
                                             const gfx::Size& va_surface_size)
    NO_THREAD_SAFETY_ANALYSIS {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::UploadVideoFrameToSurface");
  std::optional<base::AutoLock> auto_lock =
      AutoLockOnlyIfNeeded(va_lock_.get());
  TRACE_EVENT0("media,gpu", "VaapiWrapper::UploadVideoFrameToSurfaceLocked");

  if (frame.visible_rect().origin() != gfx::Point(0, 0)) {
    LOG(ERROR) << "The origin of the frame's visible rectangle is not (0, 0), "
               << "frame.visible_rect().origin()="
               << frame.visible_rect().origin().ToString();
    return false;
  }

  const gfx::Size visible_size = frame.visible_rect().size();
  bool needs_va_put_image = false;
  VAImage image;
  VAStatus va_res = vaDeriveImage(va_display_, va_surface_id, &image);
  if (va_res == VA_STATUS_ERROR_OPERATION_FAILED) {
    DVLOG(4) << "vaDeriveImage failed and fallback to Create_PutImage";
    constexpr VAImageFormat kImageFormatNV12{.fourcc = VA_FOURCC_NV12,
                                             .byte_order = VA_LSB_FIRST,
                                             .bits_per_pixel = 12};
    VAImageFormat image_format = kImageFormatNV12;

    va_res = vaCreateImage(va_display_, &image_format, va_surface_size.width(),
                           va_surface_size.height(), &image);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateImage, false);
    needs_va_put_image = true;
  }
  absl::Cleanup vaimage_deleter =
      [this, &image]() EXCLUSIVE_LOCKS_REQUIRED(va_lock_.get()) {
        VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        DestroyVAImage(va_display_, image);
      };

  if (image.format.fourcc != VA_FOURCC_NV12) {
    LOG(ERROR) << "Unsupported image format: " << image.format.fourcc;
    return false;
  }

  if (image.width % 2 != 0 || image.height % 2 != 0) {
    LOG(ERROR) << "Buffer's width and height are not even, "
               << "width=" << image.width << ", height=" << image.height;
    return false;
  }

  if (!gfx::Rect(image.width, image.height).Contains(gfx::Rect(visible_size))) {
    LOG(ERROR) << "Buffer too small to fit the frame.";
    return false;
  }

  auto mapping = ScopedVABufferMapping::Create(auto_lock ? va_lock_ : nullptr,
                                               va_display_, image.buf);
  if (!mapping) {
    return false;
  }

  uint8_t* image_ptr = static_cast<uint8_t*>(mapping->data());

  int ret = 0;
  {
    TRACE_EVENT0("media,gpu", "VaapiWrapper::UploadVideoFrameToSurface_copy");

    std::optional<base::AutoUnlock> auto_unlock;
    if (auto_lock) {
      auto_unlock.emplace(*va_lock_);
    }

    if (frame.format() == PIXEL_FORMAT_I420) {
      ret = libyuv::I420ToNV12(frame.data(VideoFrame::Plane::kY),
                               frame.stride(VideoFrame::Plane::kY),
                               frame.data(VideoFrame::Plane::kU),
                               frame.stride(VideoFrame::Plane::kU),
                               frame.data(VideoFrame::Plane::kV),
                               frame.stride(VideoFrame::Plane::kV),
                               image_ptr + image.offsets[0], image.pitches[0],
                               image_ptr + image.offsets[1], image.pitches[1],
                               visible_size.width(), visible_size.height());
    } else {
      LOG(ERROR) << "Unsupported pixel format: "
                 << VideoPixelFormatToString(frame.format());
      return false;
    }

    FillNV12Padding(image, visible_size, image_ptr);
  }
  if (needs_va_put_image) {
    va_res = vaPutImage(va_display_, va_surface_id, image.image_id, 0, 0,
                        visible_size.width(), visible_size.height(), 0, 0,
                        visible_size.width(), visible_size.height());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAPutImage, false);
  }
  return ret == 0;
}

std::unique_ptr<ScopedVABuffer> VaapiWrapper::CreateVABuffer(VABufferType type,
                                                             size_t size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::CreateVABuffer");
  base::AutoLockMaybe auto_lock(va_lock_.get());
  TRACE_EVENT2("media,gpu", "VaapiWrapper::CreateVABufferLocked", "type", type,
               "size", size);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VAContextID context_id = type == VAProtectedSessionExecuteBufferType
                               ? va_protected_session_id_
                               : va_context_id_;
#else
  VAContextID context_id = va_context_id_;
#endif

  if (context_id == VA_INVALID_ID)
    return nullptr;
  return ScopedVABuffer::Create(va_lock_, va_display_, context_id, type, size);
}

uint64_t VaapiWrapper::GetEncodedChunkSize(VABufferID buffer_id,
                                           VASurfaceID sync_surface_id)
    NO_THREAD_SAFETY_ANALYSIS {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::GetEncodedChunkSize");
  std::optional<base::AutoLock> auto_lock =
      AutoLockOnlyIfNeeded(va_lock_.get());
  TRACE_EVENT0("media,gpu", "VaapiWrapper::GetEncodedChunkSizeLocked");
  // vaSyncSurface() is not necessary on Intel platforms as long as there is a
  // vaMapBuffer() like in ScopedVABufferMapping below.
  // vaSyncSurface() synchronizes all active workloads (potentially many, e.g.
  // for k-SVC encoding). On Intel, we'd rather use the more fine-grained
  // vaMapBuffer() in ScopedVABufferMapping below. see b/184312032.
  if (VaapiWrapper::GetImplementationType() != VAImplementation::kIntelI965 &&
      VaapiWrapper::GetImplementationType() != VAImplementation::kIntelIHD) {
    VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, 0u);
  }

  auto mapping = ScopedVABufferMapping::Create(auto_lock ? va_lock_ : nullptr,
                                               va_display_, buffer_id);
  if (!mapping) {
    return 0u;
  }

  uint64_t coded_data_size = 0;
  for (auto* buffer_segment =
           reinterpret_cast<VACodedBufferSegment*>(mapping->data());
       buffer_segment; buffer_segment = reinterpret_cast<VACodedBufferSegment*>(
                           buffer_segment->next)) {
    coded_data_size += buffer_segment->size;
  }
  return coded_data_size;
}

bool VaapiWrapper::DownloadFromVABuffer(
    VABufferID buffer_id,
    std::optional<VASurfaceID> sync_surface_id,
    uint8_t* target_ptr,
    size_t target_size,
    size_t* coded_data_size) NO_THREAD_SAFETY_ANALYSIS {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(target_ptr);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABuffer");
  std::optional<base::AutoLock> auto_lock =
      AutoLockOnlyIfNeeded(va_lock_.get());
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABufferLocked");
  // vaSyncSurface() is not necessary on Intel platforms as long as there is a
  // vaMapBuffer() like in ScopedVABufferMapping below, see b/184312032.
  // |sync_surface_id| will be nullopt because it has been synced already.
  // vaSyncSurface() is not executed in the case.
  if (sync_surface_id &&
      GetImplementationType() != VAImplementation::kIntelI965 &&
      GetImplementationType() != VAImplementation::kIntelIHD) {
    TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABuffer_SyncSurface");
    const VAStatus va_res = vaSyncSurface(va_display_, *sync_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, false);
  }

  auto mapping = ScopedVABufferMapping::Create(auto_lock ? va_lock_ : nullptr,
                                               va_display_, buffer_id);
  if (!mapping) {
    return false;
  }

  auto* buffer_segment =
      reinterpret_cast<VACodedBufferSegment*>(mapping->data());

  // memcpy calls should be fast, unlocking and relocking for unmapping might
  // cause another thread to acquire the lock and we'd have to wait delaying the
  // notification that the encode is done.
  {
    TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABuffer_copy");
    *coded_data_size = 0;

    while (buffer_segment) {
      DCHECK(buffer_segment->buf);

      if (buffer_segment->size > target_size) {
        LOG(ERROR) << "Insufficient output buffer size: " << target_size
                   << ", the buffer segment size: " << buffer_segment->size;
        break;
      }
      memcpy(target_ptr, buffer_segment->buf, buffer_segment->size);

      target_ptr += buffer_segment->size;
      target_size -= buffer_segment->size;
      *coded_data_size += buffer_segment->size;
      buffer_segment =
          reinterpret_cast<VACodedBufferSegment*>(buffer_segment->next);
    }
  }

  return buffer_segment == nullptr;
}

bool VaapiWrapper::GetVAEncMaxNumOfRefFrames(VideoCodecProfile profile,
                                             size_t* max_ref_frames) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const VAProfile va_profile = ProfileToVAProfile(profile);
  VAConfigAttrib attrib;
  attrib.type = VAConfigAttribEncMaxRefFrames;

  base::AutoLockMaybe auto_lock(va_lock_.get());
  VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile,
                                          va_entrypoint_, &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAGetConfigAttributes, false);

  *max_ref_frames = attrib.value;
  return true;
}

bool VaapiWrapper::GetSupportedPackedHeaders(VideoCodecProfile profile,
                                             bool& packed_sps,
                                             bool& packed_pps,
                                             bool& packed_slice) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const VAProfile va_profile = ProfileToVAProfile(profile);
  VAConfigAttrib attrib{};
  attrib.type = VAConfigAttribEncPackedHeaders;
  base::AutoLockMaybe auto_lock(va_lock_.get());
  const VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile,
                                                va_entrypoint_, &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAGetConfigAttributes, false);
  packed_sps = attrib.value & VA_ENC_PACKED_HEADER_SEQUENCE;
  packed_pps = attrib.value & VA_ENC_PACKED_HEADER_PICTURE;
  packed_slice = attrib.value & VA_ENC_PACKED_HEADER_SLICE;

  return true;
}

bool VaapiWrapper::GetMinAV1SegmentSize(VideoCodecProfile profile,
                                        uint32_t& min_seg_size) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const VAProfile va_profile = ProfileToVAProfile(profile);
  VAConfigAttrib attrib{};
  attrib.type = VAConfigAttribEncAV1Ext1;
  base::AutoLockMaybe auto_lock(va_lock_.get());
  const VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile,
                                                va_entrypoint_, &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAGetConfigAttributes, false);

  min_seg_size = reinterpret_cast<VAConfigAttribValEncAV1Ext1*>(&attrib.value)
                     ->bits.min_segid_block_size_accepted;

  return true;
}

bool VaapiWrapper::BlitSurface(VASurfaceID va_surface_src_id,
                               const gfx::Size& va_surface_src_size,
                               VASurfaceID va_surface_dst_id,
                               const gfx::Size& va_surface_dst_size,
                               std::optional<gfx::Rect> src_rect,
                               std::optional<gfx::Rect> dest_rect
#if BUILDFLAG(IS_CHROMEOS_ASH)
                               ,
                               VAProtectedSessionID va_protected_session_id
#endif
) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(mode_, kVideoProcess);
  base::AutoLockMaybe auto_lock(va_lock_.get());

  // Create a buffer for VPP if it has not been created.
  if (!va_buffer_for_vpp_) {
    DCHECK_NE(VA_INVALID_ID, va_context_id_);
    va_buffer_for_vpp_ =
        ScopedVABuffer::Create(va_lock_, va_display_, va_context_id_,
                               VAProcPipelineParameterBufferType,
                               sizeof(VAProcPipelineParameterBuffer));
    if (!va_buffer_for_vpp_)
      return false;
  }

  // Note that since we store pointers to these regions in our mapping below,
  // these may be accessed after the Unmap() below. These must therefore live
  // until the end of the function.
  VARectangle input_region;
  VARectangle output_region;
  {
    auto mapping = ScopedVABufferMapping::Create(va_lock_, va_display_,
                                                 va_buffer_for_vpp_->id());
    if (!mapping) {
      return false;
    }
    auto* pipeline_param =
        reinterpret_cast<VAProcPipelineParameterBuffer*>(mapping->data());

    memset(pipeline_param, 0, sizeof *pipeline_param);
    if (!src_rect)
      src_rect.emplace(gfx::Rect(va_surface_src_size));
    if (!dest_rect)
      dest_rect.emplace(gfx::Rect(va_surface_dst_size));

    input_region.x = src_rect->x();
    input_region.y = src_rect->y();
    input_region.width = src_rect->width();
    input_region.height = src_rect->height();
    pipeline_param->surface_region = &input_region;
    pipeline_param->surface = va_surface_src_id;
    pipeline_param->surface_color_standard = VAProcColorStandardNone;

    output_region.x = dest_rect->x();
    output_region.y = dest_rect->y();
    output_region.width = dest_rect->width();
    output_region.height = dest_rect->height();
    pipeline_param->output_region = &output_region;
    pipeline_param->output_background_color = 0xff000000;
    pipeline_param->output_color_standard = VAProcColorStandardNone;
    // SFC-AVS is the default iHD driver scaling setting on all Intel platforms,
    // however, it has limitation on the older GPU before Gen12, EU-Bilinear is
    // reconmmended on Intel GPUs before Gen12.
    if (IsGen8Gpu() || IsGen9Gpu() || IsGen95Gpu() || IsGen11Gpu()) {
      // EU-bilinear.
      pipeline_param->filter_flags =
          VA_FILTER_SCALING_HQ | VA_FILTER_INTERPOLATION_BILINEAR;
    } else {
      // SFC-AVS on Intel iHD driver.
      pipeline_param->filter_flags = VA_FILTER_SCALING_DEFAULT;
    }
    pipeline_param->rotation_state = VA_ROTATION_NONE;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (va_protected_session_id != VA_INVALID_ID) {
    const VAStatus va_res = vaAttachProtectedSession(
        va_display_, va_context_id_, va_protected_session_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAAttachProtectedSession,
                         false);
  }

  absl::Cleanup protected_session_detacher =
      [va_protected_session_id, this]()
          EXCLUSIVE_LOCKS_REQUIRED(va_lock_.get()) {
            if (va_protected_session_id == VA_INVALID_ID) {
              return;
            }
            VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
            vaDetachProtectedSession(va_display_, va_context_id_);
          };
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  TRACE_EVENT2("media,gpu", "VaapiWrapper::BlitSurface", "src_rect",
               src_rect->ToString(), "dest_rect", dest_rect->ToString());

  VAStatus va_res =
      vaBeginPicture(va_display_, va_context_id_, va_surface_dst_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVABeginPicture, false);

  VABufferID va_buffer_id = va_buffer_for_vpp_->id();
  va_res = vaRenderPicture(va_display_, va_context_id_, &va_buffer_id, 1);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVARenderPicture_Vpp, false);
  va_res = vaEndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAEndPicture, false);

  return true;
}

// static
bool VaapiWrapper::allow_disabling_global_lock_ = false;

// static
void VaapiWrapper::PreSandboxInitialization(bool allow_disabling_global_lock) {
  allow_disabling_global_lock_ = allow_disabling_global_lock;

  VADisplayStateSingleton::PreSandboxInitialization();

  const std::string va_suffix(std::to_string(VA_MAJOR_VERSION + 1));
  StubPathMap paths;

  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  paths[kModuleVa_prot].push_back(std::string("libva.so.") + va_suffix);
#endif

  // InitializeStubs dlopen()s VA-API libraries and loads the required symbols.
  static bool result = InitializeStubs(paths);
  if (!result) {
    static const char kErrorMsg[] = "Failed to initialize VAAPI libs";
#if BUILDFLAG(IS_CHROMEOS)
    // When Chrome runs on Linux with target_os="chromeos", do not log error
    // message without VAAPI libraries.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS()) << kErrorMsg;
#else
    DVLOG(1) << kErrorMsg;
#endif
  }

  // VASupportedProfiles::Get creates VADisplayStateSingleton and in so doing
  // driver associated libraries are dlopen(), to know:
  // i965_drv_video.so
  // hybrid_drv_video.so (platforms that support it)
  // libcmrt.so (platforms that support it)
  VASupportedProfiles::Get();
}

VaapiWrapper::VaapiWrapper(VADisplayStateHandle va_display_state_handle,
                           CodecMode mode)
    : mode_(mode),
      va_display_state_handle_(std::move(va_display_state_handle)),
      va_lock_(va_display_state_handle_ ? va_display_state_handle_->va_lock()
                                        : nullptr),
      va_display_(va_display_state_handle_
                      ? va_display_state_handle_->va_display()
                      : nullptr),
      va_profile_(VAProfileNone),
      va_entrypoint_(kVAEntrypointInvalid) {}

VaapiWrapper::~VaapiWrapper() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Destroy ScopedVABuffer before VaapiWrappers are destroyed to ensure
  // VADisplay is valid on ScopedVABuffer's destruction.
  va_buffer_for_vpp_.reset();
  DestroyPendingBuffers();
  DestroyContext();
  Deinitialize();
  if (mode_ == kDecode) {
    num_decoder_instances_.Decrement();
  }
}

bool VaapiWrapper::Initialize(VAProfile va_profile,
                              EncryptionScheme encryption_scheme) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  if (mode_ == kEncodeConstantQuantizationParameter) {
    DCHECK_NE(va_profile, VAProfileJPEGBaseline)
        << "JPEG Encoding doesn't support CQP bitrate control";
  }
  if (mode_ == kEncodeVariableBitrate) {
    DCHECK_NE(va_profile, VAProfileJPEGBaseline)
        << "JPEG Encoding doesn't support VBR bitrate control";
  }
#endif  // DCHECK_IS_ON()

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (encryption_scheme != EncryptionScheme::kUnencrypted &&
      mode_ != kDecodeProtected) {
    return false;
  }
#endif

  const VAEntrypoint entrypoint = GetDefaultVaEntryPoint(mode_, va_profile);

  base::AutoLockMaybe auto_lock(va_lock_.get());
  std::vector<VAConfigAttrib> required_attribs;
  if (!GetRequiredAttribs(va_lock_, va_display_, mode_, va_profile, entrypoint,
                          &required_attribs)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (encryption_scheme != EncryptionScheme::kUnencrypted) {
    DCHECK(!required_attribs.empty());
    // We need to adjust the attribute for encryption scheme.
    for (auto& attrib : required_attribs) {
      if (attrib.type == VAConfigAttribEncryption) {
        attrib.value = (encryption_scheme == EncryptionScheme::kCbcs)
                           ? VA_ENCRYPTION_TYPE_SUBSAMPLE_CBC
                           : VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint,
                     required_attribs.empty() ? nullptr : &required_attribs[0],
                     required_attribs.size(), &va_config_id_);
  va_profile_ = va_profile;
  va_entrypoint_ = entrypoint;

  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  return true;
}

void VaapiWrapper::Deinitialize() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (va_protected_session_id_ != VA_INVALID_ID) {
      VAStatus va_res =
          vaDestroyProtectedSession(va_display_, va_protected_session_id_);
      VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyProtectedSession);
      va_res = vaDestroyConfig(va_display_, va_protected_config_id_);
      VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyConfig);
    }
#endif
    if (va_config_id_ != VA_INVALID_ID) {
      const VAStatus va_res = vaDestroyConfig(va_display_, va_config_id_);
      VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyConfig);
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    va_protected_session_id_ = VA_INVALID_ID;
    va_protected_config_id_ = VA_INVALID_ID;
#endif
    va_config_id_ = VA_INVALID_ID;
    va_display_ = nullptr;
  }
}

void VaapiWrapper::VaInitialize(
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  report_error_to_uma_cb_ = report_error_to_uma_cb;

  DCHECK(va_lock_);
  if (!UseGlobalVaapiLock(va_display_state_handle_->implementation_type())) {
    va_lock_ = nullptr;
  }
}

bool VaapiWrapper::HasContext() const {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return va_context_id_ != VA_INVALID_ID;
}

void VaapiWrapper::DestroyContext() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoLockMaybe auto_lock(va_lock_.get());
  DVLOG(2) << "Destroying context";

  if (va_context_id_ != VA_INVALID_ID) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (va_protected_session_id_ != VA_INVALID_ID) {
      const VAStatus va_res =
          vaDetachProtectedSession(va_display_, va_context_id_);
      VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADetachProtectedSession);
    }
#endif
    const VAStatus va_res = vaDestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyContext);
  }

  va_context_id_ = VA_INVALID_ID;
}

bool VaapiWrapper::CreateSurfaces(
    unsigned int va_format,
    const gfx::Size& size,
    const std::vector<SurfaceUsageHint>& usage_hints,
    size_t num_surfaces,
    std::vector<VASurfaceID>* va_surfaces) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Creating " << num_surfaces << " " << size.ToString()
           << " surfaces";
  DCHECK_NE(va_format, kInvalidVaRtFormat);
  DCHECK(va_surfaces->empty());

  va_surfaces->resize(num_surfaces);
  VASurfaceAttrib attribute;
  memset(&attribute, 0, sizeof(attribute));
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  attribute.value.value.i = 0;
  for (SurfaceUsageHint usage_hint : usage_hints)
    attribute.value.value.i |= static_cast<int32_t>(usage_hint);
  static_assert(std::is_same<decltype(attribute.value.value.i), int32_t>::value,
                "attribute.value.value.i is not int32_t");
  static_assert(std::is_same<std::underlying_type<SurfaceUsageHint>::type,
                             int32_t>::value,
                "The underlying type of SurfaceUsageHint is not int32_t");

  VAStatus va_res;
  {
    base::AutoLockMaybe auto_lock(va_lock_.get());
    va_res = vaCreateSurfaces(
        va_display_, va_format, base::checked_cast<unsigned int>(size.width()),
        base::checked_cast<unsigned int>(size.height()), va_surfaces->data(),
        num_surfaces, &attribute, 1u);
  }
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVACreateSurfaces_Allocating);
  return va_res == VA_STATUS_SUCCESS;
}

std::vector<std::unique_ptr<ScopedVASurface>>
VaapiWrapper::CreateScopedVASurfaces(
    unsigned int va_rt_format,
    const gfx::Size& size,
    const std::vector<SurfaceUsageHint>& usage_hints,
    size_t num_surfaces,
    const std::optional<gfx::Size>& visible_size,
    const std::optional<uint32_t>& va_fourcc) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (kInvalidVaRtFormat == va_rt_format) {
    LOG(ERROR) << "Invalid VA RT format to CreateScopedVASurface";
    return {};
  }

  if (size.IsEmpty()) {
    LOG(ERROR) << "Invalid visible size input to CreateScopedVASurface";
    return {};
  }

  VASurfaceAttrib attribs[2];
  unsigned int num_attribs = 1;
  memset(attribs, 0, sizeof(attribs));
  attribs[0].type = VASurfaceAttribUsageHint;
  attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribs[0].value.type = VAGenericValueTypeInteger;
  attribs[0].value.value.i = 0;
  for (SurfaceUsageHint usage_hint : usage_hints)
    attribs[0].value.value.i |= static_cast<int32_t>(usage_hint);

  if (va_fourcc) {
    num_attribs += 1;
    attribs[1].type = VASurfaceAttribPixelFormat;
    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].value.type = VAGenericValueTypeInteger;
    attribs[1].value.value.i = base::checked_cast<int32_t>(*va_fourcc);
  }
  base::AutoLockMaybe auto_lock(va_lock_.get());
  std::vector<VASurfaceID> va_surface_ids(num_surfaces, VA_INVALID_ID);
  const VAStatus va_res = vaCreateSurfaces(
      va_display_, va_rt_format, base::checked_cast<unsigned int>(size.width()),
      base::checked_cast<unsigned int>(size.height()), va_surface_ids.data(),
      num_surfaces, attribs, num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateSurfaces_Allocating,
                       std::vector<std::unique_ptr<ScopedVASurface>>{});

  DCHECK(!base::Contains(va_surface_ids, VA_INVALID_ID))
      << "Invalid VA surface id after vaCreateSurfaces";

  DCHECK(!visible_size.has_value() || !visible_size->IsEmpty());
  std::vector<std::unique_ptr<ScopedVASurface>> scoped_va_surfaces;
  scoped_va_surfaces.reserve(num_surfaces);
  for (const VASurfaceID va_surface_id : va_surface_ids) {
    auto scoped_va_surface = std::make_unique<ScopedVASurface>(
        this, va_surface_id, visible_size.has_value() ? *visible_size : size,
        va_rt_format);
    DCHECK(scoped_va_surface->IsValid());
    scoped_va_surfaces.push_back(std::move(scoped_va_surface));
  }

  return scoped_va_surfaces;
}

void VaapiWrapper::DestroySurfaces(std::vector<VASurfaceID> va_surfaces) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << "Destroying " << va_surfaces.size() << " surfaces";

  // vaDestroySurfaces() makes no guarantees about VA_INVALID_SURFACE.
  std::erase(va_surfaces, VA_INVALID_SURFACE);
  if (va_surfaces.empty())
    return;

  base::AutoLockMaybe auto_lock(va_lock_.get());
  const VAStatus va_res =
      vaDestroySurfaces(va_display_, va_surfaces.data(), va_surfaces.size());
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroySurfaces);
}

void VaapiWrapper::DestroySurface(VASurfaceID va_surface_id) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (va_surface_id == VA_INVALID_SURFACE)
    return;
  DVLOG(3) << __func__ << " " << va_surface_id;
  base::AutoLockMaybe auto_lock(va_lock_.get());
  const VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_id, 1);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroySurfaces);
}

bool VaapiWrapper::Execute_Locked(VASurfaceID va_surface_id,
                                  const std::vector<VABufferID>& va_buffers) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::Execute_Locked");
  MAYBE_ASSERT_ACQUIRED(va_lock_);

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_buffers_.size();
  DVLOG(4) << "Target VA surface " << va_surface_id;
  const auto decode_start_time = base::TimeTicks::Now();

  // Get ready to execute for given surface.
  VAStatus va_res = vaBeginPicture(va_display_, va_context_id_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVABeginPicture, false);

  if (!va_buffers.empty()) {
    // vaRenderPicture() needs a non-const pointer, possibly unnecessarily.
    VABufferID* va_buffers_data = const_cast<VABufferID*>(va_buffers.data());
    va_res = vaRenderPicture(va_display_, va_context_id_, va_buffers_data,
                             base::checked_cast<int>(va_buffers.size()));
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVARenderPicture_VABuffers,
                         false);
  }

  // Instruct HW codec to start processing the submitted commands. In theory,
  // this shouldn't be blocking, relying on vaSyncSurface() instead, however
  // evidence points to it actually waiting for the job to be done.
  va_res = vaEndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAEndPicture, false);

  if (IsModeDecoding(mode_) && va_profile_ != VAProfileNone &&
      va_profile_ != VAProfileJPEGBaseline) {
    UMA_HISTOGRAM_TIMES("Media.PlatformVideoDecoding.Decode",
                        base::TimeTicks::Now() - decode_start_time);
  }

  return true;
}

bool VaapiWrapper::SubmitBuffer_Locked(const VABufferDescriptor& va_buffer) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffer_Locked");
  MAYBE_ASSERT_ACQUIRED(va_lock_);

  DCHECK(IsValidVABufferType(va_buffer.type));
  absl::Cleanup pending_buffers_destroyer_on_failure =
      [this]() EXCLUSIVE_LOCKS_REQUIRED(va_lock_.get()) {
        VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        DestroyPendingBuffers_Locked();
      };
  unsigned int va_buffer_size;
  // We use a null |va_buffer|.data for testing: it signals that we want this
  // SubmitBuffer_Locked() call to fail.
  if (!va_buffer.data || !base::CheckedNumeric<size_t>(va_buffer.size)
                              .AssignIfValid(&va_buffer_size)) {
    return false;
  }

  VABufferID buffer_id;
  {
    TRACE_EVENT2("media,gpu",
                 "VaapiWrapper::SubmitBuffer_Locked_vaCreateBuffer", "type",
                 va_buffer.type, "size", va_buffer_size);
    // The type of |data| in vaCreateBuffer() is void*, though a driver must not
    // change the |data| buffer. We execute const_cast to limit the type
    // mismatch. https://github.com/intel/libva/issues/597
    const VAStatus va_res = vaCreateBuffer(
        va_display_, va_context_id_, va_buffer.type, va_buffer_size, 1,
        const_cast<void*>(va_buffer.data.get()), &buffer_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateBuffer, false);
  }

  pending_va_buffers_.push_back(buffer_id);
  std::move(pending_buffers_destroyer_on_failure).Cancel();
  return true;
}

bool VaapiWrapper::MapAndCopy_Locked(VABufferID va_buffer_id,
                                     const VABufferDescriptor& va_buffer) {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MAYBE_ASSERT_ACQUIRED(va_lock_);

  DCHECK_NE(va_buffer_id, VA_INVALID_ID);
  DCHECK(IsValidVABufferType(va_buffer.type));
  DCHECK(va_buffer.data);

  auto mapping =
      ScopedVABufferMapping::Create(va_lock_, va_display_, va_buffer_id);
  if (!mapping) {
    return false;
  }

  return memcpy(mapping->data(), va_buffer.data, va_buffer.size);
}

void VaapiWrapper::MaybeSetLowQualityEncoding_Locked() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsModeEncoding(mode_));
  MAYBE_ASSERT_ACQUIRED(va_lock_);

  // Query if encoding quality (VAConfigAttribEncQualityRange) is supported, and
  // if so, use the associated value for lowest quality and power consumption.
  VAConfigAttrib attrib{};
  attrib.type = VAConfigAttribEncQualityRange;
  const VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile_,
                                                va_entrypoint_, &attrib, 1);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaGetConfigAttributes failed: " << vaProfileStr(va_profile_);
    return;
  }
  // From libva's va.h: 'A value less than or equal to 1 means that the
  // encoder only has a single "quality setting,"'.
  if (attrib.value == VA_ATTRIB_NOT_SUPPORTED || attrib.value <= 1u)
    return;

  const size_t temp_size = sizeof(VAEncMiscParameterBuffer) +
                           sizeof(VAEncMiscParameterBufferQualityLevel);
  std::vector<char> temp(temp_size);

  auto* const va_buffer =
      reinterpret_cast<VAEncMiscParameterBuffer*>(temp.data());
  va_buffer->type = VAEncMiscParameterTypeQualityLevel;
  auto* const enc_quality =
      reinterpret_cast<VAEncMiscParameterBufferQualityLevel*>(va_buffer->data);
  enc_quality->quality_level = attrib.value;

  const bool success =
      SubmitBuffer_Locked({VAEncMiscParameterBufferType, temp_size, va_buffer});
  LOG_IF(ERROR, !success) << "Error setting encoding quality to "
                          << enc_quality->quality_level;
}

bool VaapiWrapper::MaybeAttachProtectedSession_Locked() {
  VAAPI_CHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MAYBE_ASSERT_ACQUIRED(va_lock_);
  if (va_context_id_ == VA_INVALID_ID)
    return true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (va_protected_session_id_ == VA_INVALID_ID)
    return true;

  VAStatus va_res = vaAttachProtectedSession(va_display_, va_context_id_,
                                             va_protected_session_id_);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVAAttachProtectedSession);
  return va_res == VA_STATUS_SUCCESS;
#else
  return true;
#endif
}

}  // namespace media
