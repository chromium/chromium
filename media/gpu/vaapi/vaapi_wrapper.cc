// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_wrapper.h"

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_str.h>
#include <va/va_version.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/bits.h"
#include "base/callback_helpers.h"
#include "base/cpu.h"
#include "base/environment.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"

// Auto-generated for dlopen libva libraries
#include "media/gpu/vaapi/va_stubs.h"

#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"  // nogncheck

typedef XID Drawable;

extern "C" {
#include "media/gpu/vaapi/va_x11.sigs"
}
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

using media_gpu_vaapi::kModuleVa;
using media_gpu_vaapi::kModuleVa_drm;
#if defined(USE_X11)
using media_gpu_vaapi::kModuleVa_x11;
#endif
using media_gpu_vaapi::InitializeStubs;
using media_gpu_vaapi::IsVaInitialized;
#if defined(USE_X11)
using media_gpu_vaapi::IsVa_x11Initialized;
#endif
using media_gpu_vaapi::IsVa_drmInitialized;
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
  kVAPutSurface = 15,
  kVAQueryConfigAttributes = 16,
  kVAQueryImageFormats = 17,
  kVAQuerySurfaceAttributes = 18,
  kVAQueryVideoProcPipelineCaps = 19,
  kVARenderPicture_VABuffers = 20,
  kVARenderPicture_Vpp = 21,
  kVASyncSurface = 22,
  kVATerminate = 23,
  kVAUnmapBuffer = 24,
  // Anything else is captured in this last entry.
  kOtherVAFunction = 25,
  kMaxValue = kOtherVAFunction,
};

void ReportVaapiErrorToUMA(const std::string& histogram_name,
                           VaapiFunctions value) {
  UMA_HISTOGRAM_ENUMERATION(histogram_name, value);
}

constexpr std::array<const char*,
                     static_cast<size_t>(VaapiFunctions::kMaxValue) + 1>
    kVaapiFunctionNames = {"vaBeginPicture",
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
                           "vaPutSurface",
                           "vaQueryConfigAttributes",
                           "vaQueryImageFormats",
                           "vaQuerySurfaceAttributes",
                           "vaQueryVideoProcPipelineCaps",
                           "vaRenderPicture (|pending_va_buffers_|)",
                           "vaRenderPicture using Vpp",
                           "vaSyncSurface",
                           "vaTerminate",
                           "vaUnmapBuffer",
                           "Other VA function"};

// Translates |function| into a human readable string for logging.
const char* VaapiFunctionName(VaapiFunctions function) {
  DCHECK(function <= VaapiFunctions::kMaxValue);
  return kVaapiFunctionNames[static_cast<size_t>(function)];
}

}  // namespace media

#define LOG_VA_ERROR_AND_REPORT(va_error, function)              \
  do {                                                           \
    LOG(ERROR) << VaapiFunctionName(function)                    \
               << " failed, VA error: " << vaErrorStr(va_error); \
    report_error_to_uma_cb_.Run(function);                       \
  } while (0)

#define VA_LOG_ON_ERROR(va_error, function)        \
  do {                                             \
    if ((va_error) != VA_STATUS_SUCCESS)           \
      LOG_VA_ERROR_AND_REPORT(va_error, function); \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_error, function, ret) \
  do {                                                \
    if ((va_error) != VA_STATUS_SUCCESS) {            \
      LOG_VA_ERROR_AND_REPORT(va_error, function);    \
      return (ret);                                   \
    }                                                 \
    DVLOG(3) << VaapiFunctionName(function);          \
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
      NOTREACHED() << gfx::BufferFormatToString(fmt);
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
  }
  return media::VAImplementation::kOther;
}

}  // namespace

namespace media {

namespace {
// VAEntrypoint is an enumeration starting from 1, but has no "invalid" value.
constexpr VAEntrypoint kVAEntrypointInvalid = static_cast<VAEntrypoint>(0);

// Returns true if the SoC has a Gen9 GPU. CPU model ID's are referenced from
// the following file in the kernel source: arch/x86/include/asm/intel-family.h.
bool IsGen9Gpu() {
  constexpr int kPentiumAndLaterFamily = 0x06;
  constexpr int kSkyLakeModelId = 0x5E;
  constexpr int kSkyLake_LModelId = 0x4E;
  constexpr int kApolloLakeModelId = 0x5c;
  static base::NoDestructor<base::CPU> cpuid;
  static const bool is_gen9_gpu = cpuid->family() == kPentiumAndLaterFamily &&
                                  (cpuid->model() == kSkyLakeModelId ||
                                   cpuid->model() == kSkyLake_LModelId ||
                                   cpuid->model() == kApolloLakeModelId);
  return is_gen9_gpu;
}

// Returns true if the SoC has a 9.5 GPU. CPU model IDs are referenced from the
// following file in the kernel source:  arch/x86/include/asm/intel-family.h.
bool IsGen95Gpu() {
  constexpr int kPentiumAndLaterFamily = 0x06;
  constexpr int kKabyLakeModelId = 0x9E;
  // Amber Lake, Whiskey Lake and some Comet Lake CPU IDs are the same as KBL L.
  constexpr int kKabyLake_LModelId = 0x8E;
  constexpr int kGeminiLakeModelId = 0x7A;
  constexpr int kCometLakeModelId = 0xA5;
  constexpr int kCometLake_LModelId = 0xA6;
  static base::NoDestructor<base::CPU> cpuid;
  static const bool is_gen95_gpu = cpuid->family() == kPentiumAndLaterFamily &&
                                   (cpuid->model() == kKabyLakeModelId ||
                                    cpuid->model() == kKabyLake_LModelId ||
                                    cpuid->model() == kGeminiLakeModelId ||
                                    cpuid->model() == kCometLakeModelId ||
                                    cpuid->model() == kCometLake_LModelId);
  return is_gen95_gpu;
}

bool IsModeEncoding(VaapiWrapper::CodecMode mode) {
  return mode == VaapiWrapper::CodecMode::kEncode ||
         mode == VaapiWrapper::CodecMode::kEncodeConstantQuantizationParameter;
}

bool GetNV12VisibleWidthBytes(int visible_width,
                              uint32_t plane,
                              size_t* bytes) {
  if (plane == 0) {
    *bytes = base::checked_cast<size_t>(visible_width);
    return true;
  }

  *bytes = base::checked_cast<size_t>(visible_width);
  return visible_width % 2 == 0 ||
         base::CheckAdd<int>(visible_width, 1).AssignIfValid(bytes);
}

// Fill 0 on VAImage's non visible area.
bool ClearNV12Padding(const VAImage& image,
                      const gfx::Size& visible_size,
                      uint8_t* data) {
  DCHECK_EQ(2u, image.num_planes);
  DCHECK_EQ(image.format.fourcc, static_cast<uint32_t>(VA_FOURCC_NV12));

  size_t visible_width_bytes[2] = {};
  if (!GetNV12VisibleWidthBytes(visible_size.width(), 0u,
                                &visible_width_bytes[0]) ||
      !GetNV12VisibleWidthBytes(visible_size.width(), 1u,
                                &visible_width_bytes[1])) {
    return false;
  }

  for (uint32_t plane = 0; plane < image.num_planes; plane++) {
    size_t row_bytes = base::strict_cast<size_t>(image.pitches[plane]);
    if (row_bytes == visible_width_bytes[plane])
      continue;

    CHECK_GT(row_bytes, visible_width_bytes[plane]);
    int visible_height = visible_size.height();
    if (plane == 1 && !(base::CheckAdd<int>(visible_size.height(), 1) / 2)
                           .AssignIfValid(&visible_height)) {
      return false;
    }

    const size_t padding_bytes = row_bytes - visible_width_bytes[plane];
    uint8_t* plane_data = data + image.offsets[plane];
    for (int row = 0; row < visible_height; row++, plane_data += row_bytes)
      memset(plane_data + visible_width_bytes[plane], 0, padding_bytes);

    CHECK_GE(base::strict_cast<int>(image.height), visible_height);
    size_t image_height = base::strict_cast<size_t>(image.height);
    if (plane == 1 && !(base::CheckAdd<size_t>(image.height, 1) / 2)
                           .AssignIfValid(&image_height)) {
      return false;
    }

    base::CheckedNumeric<size_t> remaining_area(image_height);
    remaining_area -= base::checked_cast<size_t>(visible_height);
    remaining_area *= row_bytes;
    if (!remaining_area.IsValid())
      return false;
    memset(plane_data, 0, remaining_area.ValueOrDie());
  }

  return true;
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
  });
  return *kMediaToVAProfileMap;
}

// Maps a VideoCodecProfile |profile| to a VAProfile, or VAProfileNone.
VAProfile ProfileToVAProfile(VideoCodecProfile profile,
                             VaapiWrapper::CodecMode mode) {
  const auto& profiles = GetProfileCodecMap();
  const auto& maybe_profile = profiles.find(profile);
  if (maybe_profile == profiles.end())
    return VAProfileNone;
  return maybe_profile->second;
}

bool IsVAProfileSupported(VAProfile va_profile) {
  // VAProfileJPEGBaseline is always recognized but is not a video codec per se.
  const auto& profiles = GetProfileCodecMap();
  return va_profile == VAProfileJPEGBaseline ||
         std::find_if(profiles.begin(), profiles.end(),
                      [va_profile](const auto& entry) {
                        return entry.second == va_profile;
                      }) != profiles.end();
}

bool IsBlockedDriver(VaapiWrapper::CodecMode mode, VAProfile va_profile) {
  if (!IsModeEncoding(mode))
    return false;

  // TODO(posciak): Remove once VP8 encoding is to be enabled by default.
  if (va_profile == VAProfileVP8Version0_3 &&
      !base::FeatureList::IsEnabled(kVaapiVP8Encoder)) {
    return true;
  }

  // TODO(crbug.com/811912): Remove once VP9 encoding is enabled by default.
  if (va_profile == VAProfileVP9Profile0 &&
      !base::FeatureList::IsEnabled(kVaapiVP9Encoder)) {
    return true;
  }

  return false;
}

// This class is a wrapper around its |va_display_| (and its associated
// |va_lock_|) to guarantee mutual exclusion and singleton behaviour.
class VADisplayState {
 public:
  static VADisplayState* Get();

  // Initialize static data before sandbox is enabled.
  static void PreSandboxInitialization();

  bool Initialize();
  VAStatus Deinitialize();

  base::Lock* va_lock() { return &va_lock_; }
  VADisplay va_display() const { return va_display_; }
  VAImplementation implementation_type() const { return implementation_type_; }

  void SetDrmFd(base::PlatformFile fd) { drm_fd_.reset(HANDLE_EINTR(dup(fd))); }

 private:
  friend class base::NoDestructor<VADisplayState>;

  VADisplayState();
  ~VADisplayState() = default;

  // Implementation of Initialize() called only once.
  bool InitializeOnce() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);
  bool InitializeVaDisplay_Locked() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);
  bool InitializeVaDriver_Locked() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  int refcount_ GUARDED_BY(va_lock_);

  // Libva is not thread safe, so we have to do locking for it ourselves.
  // This lock is to be taken for the duration of all VA-API calls and for
  // the entire job submission sequence in ExecuteAndDestroyPendingBuffers().
  base::Lock va_lock_;

  // Drm fd used to obtain access to the driver interface by VA.
  base::ScopedFD drm_fd_;

  // The VADisplay handle. Valid between Initialize() and Deinitialize().
  VADisplay va_display_;

  // True if vaInitialize() has been called successfully, until Deinitialize().
  bool va_initialized_;

  // Enumerated version of vaQueryVendorString(). Valid after Initialize().
  VAImplementation implementation_type_ = VAImplementation::kInvalid;

  DISALLOW_COPY_AND_ASSIGN(VADisplayState);
};

// static
VADisplayState* VADisplayState::Get() {
  static base::NoDestructor<VADisplayState> display_state;
  return display_state.get();
}

// static
void VADisplayState::PreSandboxInitialization() {
  const char kDriRenderNode0Path[] = "/dev/dri/renderD128";
  base::File drm_file = base::File(
      base::FilePath::FromUTF8Unsafe(kDriRenderNode0Path),
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  if (drm_file.IsValid())
    VADisplayState::Get()->SetDrmFd(drm_file.GetPlatformFile());
}

VADisplayState::VADisplayState()
    : refcount_(0), va_display_(nullptr), va_initialized_(false) {}

bool VADisplayState::Initialize() {
  base::AutoLock auto_lock(va_lock_);

  bool libraries_initialized = IsVaInitialized() && IsVa_drmInitialized();
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    libraries_initialized = libraries_initialized && IsVa_x11Initialized();
#endif
  if (!libraries_initialized)
    return false;

  // Manual refcounting to ensure the rest of the method is called only once.
  if (refcount_++ > 0)
    return true;

  const bool success = InitializeOnce();
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiWrapper.VADisplayStateInitializeSuccess",
                        success);
  return success;
}

bool VADisplayState::InitializeVaDisplay_Locked() {
  switch (gl::GetGLImplementation()) {
    case gl::kGLImplementationEGLGLES2:
      va_display_ = vaGetDisplayDRM(drm_fd_.get());
      break;
    case gl::kGLImplementationDesktopGL:
#if defined(USE_X11)
      if (!features::IsUsingOzonePlatform()) {
        va_display_ = vaGetDisplay(gfx::GetXDisplay());
        if (!vaDisplayIsValid(va_display_))
          va_display_ = vaGetDisplayDRM(drm_fd_.get());
      }
#endif  // USE_X11
      break;
    case gl::kGLImplementationEGLANGLE:
#if defined(USE_X11)
      if (!features::IsUsingOzonePlatform())
        va_display_ = vaGetDisplay(gfx::GetXDisplay());
#endif  // USE_X11
      break;
    // Cannot infer platform from GL, try all available displays
    case gl::kGLImplementationNone:
#if defined(USE_X11)
      if (!features::IsUsingOzonePlatform()) {
        va_display_ = vaGetDisplay(gfx::GetXDisplay());
        if (vaDisplayIsValid(va_display_))
          break;
      }
#endif  // USE_X11
      va_display_ = vaGetDisplayDRM(drm_fd_.get());
      break;

    default:
      LOG(WARNING) << "VAAPI video acceleration not available for "
                   << gl::GetGLImplementationName(gl::GetGLImplementation());
      return false;
  }

  if (!vaDisplayIsValid(va_display_)) {
    LOG(ERROR) << "Could not get a valid VA display";
    return false;
  }

  return true;
}

bool VADisplayState::InitializeVaDriver_Locked() {
  // The VAAPI version.
  int major_version, minor_version;
  VAStatus va_res = vaInitialize(va_display_, &major_version, &minor_version);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaInitialize failed: " << vaErrorStr(va_res);
    return false;
  }
  const std::string va_vendor_string = vaQueryVendorString(va_display_);
  DLOG_IF(WARNING, va_vendor_string.empty())
      << "Vendor string empty or error reading.";
  DVLOG(1) << "VAAPI version: " << major_version << "." << minor_version << " "
           << va_vendor_string;
  implementation_type_ = VendorStringToImplementationType(va_vendor_string);

  va_initialized_ = true;

  // The VAAPI version is determined from what is loaded on the system by
  // calling vaInitialize(). Since the libva is now ABI-compatible, relax the
  // version check which helps in upgrading the libva, without breaking any
  // existing functionality. Make sure the system version is not older than
  // the version with which the chromium is built since libva is only
  // guaranteed to be backward (and not forward) compatible.
  if (VA_MAJOR_VERSION > major_version ||
      (VA_MAJOR_VERSION == major_version && VA_MINOR_VERSION > minor_version)) {
    LOG(ERROR) << "The system version " << major_version << "." << minor_version
               << " should be greater than or equal to "
               << VA_MAJOR_VERSION << "." << VA_MINOR_VERSION;
    return false;
  }
  return true;
}

bool VADisplayState::InitializeOnce() {
  static_assert(
      VA_MAJOR_VERSION >= 2 || (VA_MAJOR_VERSION == 1 && VA_MINOR_VERSION >= 1),
      "Requires VA-API >= 1.1.0");

  // Set VA logging level, unless already set.
  constexpr char libva_log_level_env[] = "LIBVA_MESSAGING_LEVEL";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(libva_log_level_env))
    env->SetVar(libva_log_level_env, "1");

  if (!InitializeVaDisplay_Locked() || !InitializeVaDriver_Locked())
    return false;

#if defined(USE_X11)
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      implementation_type_ == VAImplementation::kIntelIHD) {
    DCHECK(!features::IsUsingOzonePlatform());
    constexpr char libva_driver_impl_env[] = "LIBVA_DRIVER_NAME";
    // TODO(crbug/1116703) The libva intel-media driver has a known segfault in
    // vaPutSurface, so until this is fixed, fall back to the i965 driver. There
    // is discussion of the issue here:
    // https://github.com/intel/media-driver/issues/818
    if (!env->HasVar(libva_driver_impl_env))
      env->SetVar(libva_driver_impl_env, "i965");

    // Re-initialize with the new driver.
    va_display_ = nullptr;
    va_initialized_ = false;
    implementation_type_ = VAImplementation::kInvalid;

    if (!InitializeVaDisplay_Locked() || !InitializeVaDriver_Locked())
      return false;
  }
#endif  // USE_X11

  return true;
}

VAStatus VADisplayState::Deinitialize() {
  base::AutoLock auto_lock(va_lock_);
  VAStatus va_res = VA_STATUS_SUCCESS;

  if (--refcount_ > 0)
    return va_res;

  // Must check if vaInitialize completed successfully, to work around a bug in
  // libva. The bug was fixed upstream:
  // http://lists.freedesktop.org/archives/libva/2013-July/001807.html
  // TODO(mgiuca): Remove this check, and the |va_initialized_| variable, once
  // the fix has rolled out sufficiently.
  if (va_initialized_ && va_display_)
    va_res = vaTerminate(va_display_);
  va_initialized_ = false;
  va_display_ = nullptr;
  return va_res;
}

// Returns all the VAProfiles that the driver lists as supported, regardless of
// what Chrome supports or not.
std::vector<VAProfile> GetSupportedVAProfiles(const base::Lock* va_lock,
                                              VADisplay va_display) {
  va_lock->AssertAcquired();

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
  va_lock->AssertAcquired();

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
      {VAEntrypointEncSlice, VAEntrypointEncPicture,
       VAEntrypointEncSliceLP},  // kEncode.
      {VAEntrypointEncSlice,
       VAEntrypointEncSliceLP},  // kEncodeConstantQuantizationParameter.
      {VAEntrypointVideoProc}    // kVideoProcess.
  };
  static_assert(base::size(kAllowedEntryPoints) == VaapiWrapper::kCodecModeMax,
                "");

  std::vector<VAEntrypoint> entrypoints;
  std::copy_if(va_entrypoints.begin(), va_entrypoints.end(),
               std::back_inserter(entrypoints),
               [&kAllowedEntryPoints, mode](VAEntrypoint entry_point) {
                 return base::Contains(kAllowedEntryPoints[mode], entry_point);
               });
  return entrypoints;
}

bool GetRequiredAttribs(const base::Lock* va_lock,
                        VADisplay va_display,
                        VaapiWrapper::CodecMode mode,
                        VAProfile profile,
                        VAEntrypoint entrypoint,
                        std::vector<VAConfigAttrib>* required_attribs) {
  va_lock->AssertAcquired();

  // Choose a suitable VAConfigAttribRTFormat for every |mode|. For video
  // processing, the supported surface attribs may vary according to which RT
  // format is set.
  if (profile == VAProfileVP9Profile2 || profile == VAProfileVP9Profile3) {
    required_attribs->push_back(
        {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420_10BPP});
  } else {
    required_attribs->push_back({VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420});
  }

  if (!IsModeEncoding(mode))
    return true;

  if (profile != VAProfileJPEGBaseline) {
    if (mode == VaapiWrapper::kEncode)
      required_attribs->push_back({VAConfigAttribRateControl, VA_RC_CBR});
    if (mode == VaapiWrapper::kEncodeConstantQuantizationParameter)
      required_attribs->push_back({VAConfigAttribRateControl, VA_RC_CQP});
  }

  constexpr VAProfile kSupportedH264VaProfilesForEncoding[] = {
      VAProfileH264ConstrainedBaseline, VAProfileH264Main, VAProfileH264High};
  // VAConfigAttribEncPackedHeaders is H.264 specific.
  if (base::Contains(kSupportedH264VaProfilesForEncoding, profile)) {
    // Encode with Packed header if a driver supports.
    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribEncPackedHeaders;
    const VAStatus va_res =
        vaGetConfigAttributes(va_display, profile, entrypoint, &attrib, 1);
    if (va_res != VA_STATUS_SUCCESS) {
      LOG(ERROR) << "vaGetConfigAttributes failed for "
                 << vaProfileStr(profile);
      return false;
    }

    if (attrib.value != VA_ENC_PACKED_HEADER_NONE) {
      required_attribs->push_back(
          {VAConfigAttribEncPackedHeaders,
           VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE});
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
  va_lock->AssertAcquired();
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

  // Determines if |mode| supports |va_profile| (and |va_entrypoint| if defined
  // and valid). If so, returns a const pointer to its ProfileInfo, otherwise
  // returns nullptr.
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
  void FillSupportedProfileInfos(base::Lock* va_lock, VADisplay va_display);

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

  DISALLOW_COPY_AND_ASSIGN(VASupportedProfiles);
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
  auto iter = std::find_if(
      supported_profiles_[mode].begin(), supported_profiles_[mode].end(),
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
  VADisplayState* display_state = VADisplayState::Get();
  if (!display_state->Initialize())
    return;

  VADisplay va_display = display_state->va_display();
  DCHECK(va_display) << "VADisplayState hasn't been properly Initialize()d";

  FillSupportedProfileInfos(VADisplayState::Get()->va_lock(), va_display);

  const VAStatus va_res = display_state->Deinitialize();
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVATerminate);
}

void VASupportedProfiles::FillSupportedProfileInfos(base::Lock* va_lock,
                                                    VADisplay va_display) {
  base::AutoLock auto_lock(*va_lock);

  const std::vector<VAProfile> va_profiles =
      GetSupportedVAProfiles(va_lock, va_display);

  constexpr VaapiWrapper::CodecMode kWrapperModes[] = {
      VaapiWrapper::kDecode, VaapiWrapper::kEncode,
      VaapiWrapper::kEncodeConstantQuantizationParameter,
      VaapiWrapper::kVideoProcess};
  static_assert(base::size(kWrapperModes) == VaapiWrapper::kCodecModeMax, "");

  for (VaapiWrapper::CodecMode mode : kWrapperModes) {
    std::vector<ProfileInfo> supported_profile_infos;

    for (const auto& va_profile : va_profiles) {
      if (IsBlockedDriver(mode, va_profile))
        continue;

      if ((mode != VaapiWrapper::kVideoProcess) &&
          !IsVAProfileSupported(va_profile)) {
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
  va_lock->AssertAcquired();
  VAConfigID va_config_id;
  VAStatus va_res =
      vaCreateConfig(va_display, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  base::ScopedClosureRunner vaconfig_destroyer(base::BindOnce(
      [](VADisplay display, VAConfigID id) {
        if (id != VA_INVALID_ID) {
          VAStatus va_res = vaDestroyConfig(display, id);
          if (va_res != VA_STATUS_SUCCESS)
            LOG(ERROR) << "vaDestroyConfig failed. VA error: "
                       << vaErrorStr(va_res);
        }
      },
      va_display, va_config_id));

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

  // Create a new configuration to find the supported RT formats. We don't pass
  // required attributes here because we want the driver to tell us all the
  // supported RT formats.
  va_res = vaCreateConfig(va_display, va_profile, entrypoint, nullptr, 0,
                          &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  base::ScopedClosureRunner vaconfig_no_attribs_destroyer(base::BindOnce(
      [](VADisplay display, VAConfigID id) {
        if (id != VA_INVALID_ID) {
          VAStatus va_res = vaDestroyConfig(display, id);
          if (va_res != VA_STATUS_SUCCESS)
            LOG(ERROR) << "vaDestroyConfig failed. VA error: "
                       << vaErrorStr(va_res);
        }
      },
      va_display, va_config_id));
  profile_info->supported_internal_formats = {};
  size_t max_num_config_attributes;
  if (!base::CheckedNumeric<int>(vaMaxNumConfigAttributes(va_display))
           .AssignIfValid(&max_num_config_attributes)) {
    LOG(ERROR) << "Can't get the maximum number of config attributes";
    return false;
  }
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
    if (attrib.value & VA_RT_FORMAT_YUV444)
      profile_info->supported_internal_formats.yuv444 = true;
    break;
  }

  // Now work around some driver misreporting for JPEG decoding.
  if (va_profile == VAProfileJPEGBaseline && entrypoint == VAEntrypointVLD) {
    if (VADisplayState::Get()->implementation_type() ==
        VAImplementation::kMesaGallium) {
      // TODO(andrescj): the VAAPI state tracker in mesa does not report
      // VA_RT_FORMAT_YUV422 as being supported for JPEG decoding. However, it
      // is happy to allocate YUYV surfaces
      // (https://gitlab.freedesktop.org/mesa/mesa/commit/5608f442). Remove this
      // workaround once b/128337341 is resolved.
      profile_info->supported_internal_formats.yuv422 = true;
    }
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

void DestroyVAImage(VADisplay va_display, VAImage image) {
  if (image.image_id != VA_INVALID_ID)
    vaDestroyImage(va_display, image.image_id);
}

// This class encapsulates fetching the list of supported output image formats
// from the VAAPI driver, in a singleton way.
class VASupportedImageFormats {
 public:
  static const VASupportedImageFormats& Get();

  bool IsImageFormatSupported(const VAImageFormat& va_format) const;

  const std::vector<VAImageFormat>& GetSupportedImageFormats() const;

 private:
  friend class base::NoDestructor<VASupportedImageFormats>;

  VASupportedImageFormats();
  ~VASupportedImageFormats() = default;

  // Initialize the list of supported image formats.
  bool InitSupportedImageFormats_Locked() EXCLUSIVE_LOCKS_REQUIRED(va_lock_);

  // Pointer to VADisplayState's members |va_lock_| and its |va_display_|.
  base::Lock* va_lock_;
  VADisplay va_display_ GUARDED_BY(va_lock_);

  std::vector<VAImageFormat> supported_formats_;
  const ReportErrorToUMACB report_error_to_uma_cb_;

  DISALLOW_COPY_AND_ASSIGN(VASupportedImageFormats);
};

// static
const VASupportedImageFormats& VASupportedImageFormats::Get() {
  static const base::NoDestructor<VASupportedImageFormats> image_formats;
  return *image_formats;
}

bool VASupportedImageFormats::IsImageFormatSupported(
    const VAImageFormat& va_image_format) const {
  auto it = std::find_if(supported_formats_.begin(), supported_formats_.end(),
                         [&va_image_format](const VAImageFormat& format) {
                           return format.fourcc == va_image_format.fourcc;
                         });
  return it != supported_formats_.end();
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
    : va_lock_(VADisplayState::Get()->va_lock()),
      report_error_to_uma_cb_(base::DoNothing()) {
  VADisplayState* display_state = VADisplayState::Get();
  if (!display_state->Initialize())
    return;

  {
    base::AutoLock auto_lock(*va_lock_);
    va_display_ = display_state->va_display();
    DCHECK(va_display_) << "VADisplayState hasn't been properly initialized";

    if (!InitSupportedImageFormats_Locked())
      LOG(ERROR) << "Failed to get supported image formats";
  }

  const VAStatus va_res = display_state->Deinitialize();
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVATerminate);
}

bool VASupportedImageFormats::InitSupportedImageFormats_Locked() {
  va_lock_->AssertAcquired();

  // Query the driver for the max number of image formats and allocate space.
  const int max_image_formats = vaMaxNumImageFormats(va_display_);
  if (max_image_formats < 0) {
    LOG(ERROR) << "vaMaxNumImageFormats returned: " << max_image_formats;
    return false;
  }
  supported_formats_.resize(static_cast<size_t>(max_image_formats));

  // Query the driver for the list of supported image formats.
  int num_image_formats;
  const VAStatus va_res = vaQueryImageFormats(
      va_display_, supported_formats_.data(), &num_image_formats);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQueryImageFormats, false);
  if (num_image_formats < 0 || num_image_formats > max_image_formats) {
    LOG(ERROR) << "vaQueryImageFormats returned: " << num_image_formats;
    supported_formats_.clear();
    return false;
  }

  // Resize the list to the actual number of formats returned by the driver.
  supported_formats_.resize(static_cast<size_t>(num_image_formats));

  // Now work around some driver misreporting.
  if (VADisplayState::Get()->implementation_type() ==
      VAImplementation::kMesaGallium) {
    // TODO(andrescj): considering that the VAAPI state tracker in mesa can
    // convert from NV12 to IYUV when doing vaGetImage(), it's reasonable to
    // assume that IYUV/I420 is supported. However, it's not currently being
    // reported. See https://gitlab.freedesktop.org/mesa/mesa/commit/b0a44f10.
    // Remove this workaround once b/128340287 is resolved.
    if (std::find_if(supported_formats_.cbegin(), supported_formats_.cend(),
                     [](const VAImageFormat& format) {
                       return format.fourcc == VA_FOURCC_I420;
                     }) == supported_formats_.cend()) {
      VAImageFormat i420_format{};
      i420_format.fourcc = VA_FOURCC_I420;
      supported_formats_.push_back(i420_format);
    }
  }
  return true;
}

bool IsLowPowerEncSupported(VAProfile va_profile) {
  constexpr VAProfile kSupportedLowPowerEncodeProfiles[] = {
      VAProfileH264ConstrainedBaseline,
      VAProfileH264Main,
      VAProfileH264High,
      VAProfileVP9Profile0,
      VAProfileVP9Profile2};
  if (!base::Contains(kSupportedLowPowerEncodeProfiles, va_profile))
    return false;

  if ((IsGen95Gpu() || IsGen9Gpu()) &&
      !base::FeatureList::IsEnabled(kVaapiLowPowerEncoderGen9x)) {
    return false;
  }

  if (VASupportedProfiles::Get().IsProfileSupported(
          VaapiWrapper::kEncode, va_profile, VAEntrypointEncSliceLP)) {
    return true;
  }
  return false;
}

}  // namespace

NativePixmapAndSizeInfo::NativePixmapAndSizeInfo() = default;

NativePixmapAndSizeInfo::~NativePixmapAndSizeInfo() = default;

// static
VAImplementation VaapiWrapper::GetImplementationType() {
  return VADisplayState::Get()->implementation_type();
}

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::Create(
    CodecMode mode,
    VAProfile va_profile,
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  if (!VASupportedProfiles::Get().IsProfileSupported(mode, va_profile)) {
    DVLOG(1) << "Unsupported va_profile: " << vaProfileStr(va_profile);
    return nullptr;
  }

  scoped_refptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper(mode));
  if (vaapi_wrapper->VaInitialize(report_error_to_uma_cb)) {
    if (vaapi_wrapper->Initialize(mode, va_profile))
      return vaapi_wrapper;
  }
  LOG(ERROR) << "Failed to create VaapiWrapper for va_profile: "
             << vaProfileStr(va_profile);
  return nullptr;
}

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::CreateForVideoCodec(
    CodecMode mode,
    VideoCodecProfile profile,
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  const VAProfile va_profile = ProfileToVAProfile(profile, mode);
  return Create(mode, va_profile, report_error_to_uma_cb);
}

// static
VideoEncodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;

  for (const auto& media_to_va_profile_map_entry : GetProfileCodecMap()) {
    const VideoCodecProfile media_profile = media_to_va_profile_map_entry.first;
    const VAProfile va_profile = media_to_va_profile_map_entry.second;
    DCHECK(va_profile != VAProfileNone);

    const VASupportedProfiles::ProfileInfo* profile_info =
        VASupportedProfiles::Get().IsProfileSupported(kEncode, va_profile);
    if (!profile_info)
      continue;

    VideoEncodeAccelerator::SupportedProfile profile;
    profile.profile = media_profile;
    // Using VA-API for accelerated encoding frames smaller than a certain
    // size is less efficient than using a software encoder.
    const gfx::Size kMinEncodeResolution = gfx::Size(320 + 1, 240 + 1);
    profile.min_resolution = kMinEncodeResolution;
    profile.max_resolution = profile_info->max_resolution;
    // Maximum framerate of encoded profile. This value is an arbitrary
    // limit and not taken from HW documentation.
    constexpr int kMaxEncoderFramerate = 30;
    profile.max_framerate_numerator = kMaxEncoderFramerate;
    profile.max_framerate_denominator = 1;
    profiles.push_back(profile);
  }
  return profiles;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedDecodeProfiles(
    const gpu::GpuDriverBugWorkarounds& workarounds) {
  VideoDecodeAccelerator::SupportedProfiles profiles;

  for (const auto& media_to_va_profile_map_entry : GetProfileCodecMap()) {
    const VideoCodecProfile media_profile = media_to_va_profile_map_entry.first;
    const VAProfile va_profile = media_to_va_profile_map_entry.second;
    DCHECK(va_profile != VAProfileNone);

    if (media_profile == VP8PROFILE_ANY &&
        workarounds.disable_accelerated_vp8_decode) {
      continue;
    }

    const VASupportedProfiles::ProfileInfo* profile_info =
        VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
    if (!profile_info)
      continue;

    VideoDecodeAccelerator::SupportedProfile profile;
    profile.profile = media_profile;
    profile.max_resolution = profile_info->max_resolution;
    profile.min_resolution.SetSize(16, 16);
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
  static const base::NoDestructor<VaapiWrapper::InternalFormats>
      supported_internal_formats(
          VaapiWrapper::GetDecodeSupportedInternalFormats(va_profile));
  switch (rt_format) {
    case VA_RT_FORMAT_YUV420:
      return supported_internal_formats->yuv420;
    case VA_RT_FORMAT_YUV420_10:
      return supported_internal_formats->yuv420_10;
    case VA_RT_FORMAT_YUV422:
      return supported_internal_formats->yuv422;
    case VA_RT_FORMAT_YUV444:
      return supported_internal_formats->yuv444;
  }
  return false;
}

// static
bool VaapiWrapper::GetDecodeMinResolution(VAProfile va_profile,
                                          gfx::Size* min_size) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
  if (!profile_info)
    return false;
  *min_size = gfx::Size(std::max(1, profile_info->min_resolution.width()),
                        std::max(1, profile_info->min_resolution.height()));
  return true;
}

// static
bool VaapiWrapper::GetDecodeMaxResolution(VAProfile va_profile,
                                          gfx::Size* max_size) {
  const VASupportedProfiles::ProfileInfo* profile_info =
      VASupportedProfiles::Get().IsProfileSupported(kDecode, va_profile);
  if (!profile_info)
    return false;

  *max_size = profile_info->max_resolution;
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
      return false;
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
    // TODO(b/155939640): iHD v19.4 fails to allocate AYUV surfaces for the VPP
    // on gen 9.5.
    // (b/159896972): iHD v20.1.1 cannot create Y216 and Y416 images from a
    // decoded JPEG on gen 12. It is also failing to support Y800 format.
    if (preferred_fourcc == VA_FOURCC_AYUV ||
        preferred_fourcc == VA_FOURCC_Y216 ||
        preferred_fourcc == VA_FOURCC_Y416 ||
        preferred_fourcc == VA_FOURCC_Y800) {
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
  return gfx::Rect(profile_info->min_resolution.width(),
                   profile_info->min_resolution.height(),
                   profile_info->max_resolution.width(),
                   profile_info->max_resolution.height())
      .Contains(size.width(), size.height());
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
bool VaapiWrapper::IsVppSupportedForJpegDecodedSurfaceToFourCC(
    unsigned int rt_format,
    uint32_t fourcc) {
  if (!IsDecodingSupportedForInternalFormat(VAProfileJPEGBaseline, rt_format))
    return false;

  // Workaround: for Mesa VAAPI driver, VPP only supports internal surface
  // format for 4:2:0 JPEG image.
  DCHECK_NE(VAImplementation::kInvalid, GetImplementationType());
  if (GetImplementationType() == VAImplementation::kMesaGallium &&
      rt_format != VA_RT_FORMAT_YUV420) {
    return false;
  }

  return IsVppFormatSupported(fourcc);
}

// static
bool VaapiWrapper::IsJpegEncodeSupported() {
  return VASupportedProfiles::Get().IsProfileSupported(kEncode,
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
    case VaapiWrapper::kEncode:
    case VaapiWrapper::kEncodeConstantQuantizationParameter:
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
      return VAEntrypointVLD;
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
      NOTREACHED() << gfx::BufferFormatToString(fmt);
      return 0;
  }
}

bool VaapiWrapper::CreateContextAndSurfaces(
    unsigned int va_format,
    const gfx::Size& size,
    SurfaceUsageHint surface_usage_hint,
    size_t num_surfaces,
    std::vector<VASurfaceID>* va_surfaces) {
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";
  DCHECK(va_surfaces->empty());

  if (va_context_id_ != VA_INVALID_ID) {
    LOG(ERROR)
        << "The current context should be destroyed before creating a new one";
    return false;
  }

  if (!CreateSurfaces(va_format, size, surface_usage_hint, num_surfaces,
                      va_surfaces)) {
    return false;
  }

  const bool success = CreateContext(size);
  if (!success)
    DestroyContextAndSurfaces(*va_surfaces);
  return success;
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateContextAndScopedVASurface(
    unsigned int va_format,
    const gfx::Size& size,
    const base::Optional<gfx::Size>& visible_size) {
  if (va_context_id_ != VA_INVALID_ID) {
    LOG(ERROR) << "The current context should be destroyed before creating a "
                  "new one";
    return nullptr;
  }

  std::unique_ptr<ScopedVASurface> scoped_va_surface =
      CreateScopedVASurface(va_format, size, visible_size);
  if (!scoped_va_surface)
    return nullptr;

  if (CreateContext(size))
    return scoped_va_surface;

  DestroyContext();
  return nullptr;
}

void VaapiWrapper::DestroyContextAndSurfaces(
    std::vector<VASurfaceID> va_surfaces) {
  DestroyContext();
  DestroySurfaces(va_surfaces);
}

bool VaapiWrapper::CreateContext(const gfx::Size& size) {
  DVLOG(2) << "Creating context";
  base::AutoLock auto_lock(*va_lock_);

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
  const VAStatus va_res = vaCreateContext(
      va_display_, va_config_id_, picture_size.width(), picture_size.height(),
      flag, empty_va_surfaces_ids_pointer, empty_va_surfaces_ids_size,
      &va_context_id_);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVACreateContext);
  return va_res == VA_STATUS_SUCCESS;
}

scoped_refptr<VASurface> VaapiWrapper::CreateVASurfaceForPixmap(
    scoped_refptr<gfx::NativePixmap> pixmap) {
  const gfx::BufferFormat buffer_format = pixmap->GetBufferFormat();

  // Create a VASurface for a NativePixmap by importing the underlying dmabufs.
  const gfx::Size size = pixmap->GetBufferSize();
  VASurfaceAttribExternalBuffers va_attrib_extbuf{};
  va_attrib_extbuf.pixel_format = BufferFormatToVAFourCC(buffer_format);
  va_attrib_extbuf.width = size.width();
  va_attrib_extbuf.height = size.height();

  const size_t num_planes = pixmap->GetNumberOfPlanes();
  for (size_t i = 0; i < num_planes; ++i) {
    va_attrib_extbuf.pitches[i] = pixmap->GetDmaBufPitch(i);
    va_attrib_extbuf.offsets[i] = pixmap->GetDmaBufOffset(i);
    DVLOG(4) << "plane " << i << ": pitch: " << va_attrib_extbuf.pitches[i]
             << " offset: " << va_attrib_extbuf.offsets[i];
  }
  va_attrib_extbuf.num_planes = num_planes;

  if (pixmap->GetDmaBufFd(0) < 0) {
    LOG(ERROR) << "Failed to get dmabuf from an Ozone NativePixmap";
    return nullptr;
  }
  // We only have to pass the first file descriptor to a driver. A VA-API driver
  // shall create a VASurface from the single fd correctly.
  uintptr_t fd = base::checked_cast<uintptr_t>(pixmap->GetDmaBufFd(0));
  va_attrib_extbuf.buffers = &fd;
  va_attrib_extbuf.num_buffers = 1u;

  DCHECK_EQ(va_attrib_extbuf.flags, 0u);
  DCHECK_EQ(va_attrib_extbuf.private_data, nullptr);

  std::vector<VASurfaceAttrib> va_attribs(2);

  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = &va_attrib_extbuf;

  const unsigned int va_format = BufferFormatToVARTFormat(buffer_format);

  VASurfaceID va_surface_id = VA_INVALID_ID;
  {
    base::AutoLock auto_lock(*va_lock_);
    VAStatus va_res = vaCreateSurfaces(
        va_display_, va_format, base::checked_cast<unsigned int>(size.width()),
        base::checked_cast<unsigned int>(size.height()), &va_surface_id, 1,
        &va_attribs[0], va_attribs.size());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateSurfaces_Importing,
                         nullptr);
  }
  DVLOG(2) << __func__ << " " << va_surface_id;
  // VASurface shares an ownership of the buffer referred by the passed file
  // descriptor. We can release |pixmap| here.
  return new VASurface(va_surface_id, size, va_format,
                       base::BindOnce(&VaapiWrapper::DestroySurface, this));
}

std::unique_ptr<NativePixmapAndSizeInfo>
VaapiWrapper::ExportVASurfaceAsNativePixmapDmaBuf(
    const ScopedVASurface& scoped_va_surface) {
  if (!scoped_va_surface.IsValid()) {
    LOG(ERROR) << "Cannot export an invalid surface";
    return nullptr;
  }

  VADRMPRIMESurfaceDescriptor descriptor;
  {
    base::AutoLock auto_lock(*va_lock_);
    VAStatus va_res = vaSyncSurface(va_display_, scoped_va_surface.id());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, nullptr);
    va_res = vaExportSurfaceHandle(
        va_display_, scoped_va_surface.id(),
        VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
        &descriptor);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAExportSurfaceHandle,
                         nullptr);
  }

  // We only support one bo containing all the planes. The fd should be owned by
  // us: per va/va.h, "the exported handles are owned by the caller."
  //
  // TODO(crbug.com/974438): support multiple buffer objects so that this can
  // work in AMD.
  if (descriptor.num_objects != 1u) {
    DVLOG(1) << "Only surface descriptors with one bo are supported";
    NOTREACHED();
    return nullptr;
  }
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

    // Strictly speaking, we only have to dup() the fd for the planes after the
    // first one since we already own the first one, but we dup() regardless for
    // simplicity: |bo_fd| will be closed at the end of this method anyway.
    base::ScopedFD plane_fd(HANDLE_EINTR(dup(bo_fd.get())));
    PCHECK(plane_fd.is_valid());
    constexpr uint64_t kZeroSizeToPreventMapping = 0u;
    handle.planes.emplace_back(
        base::checked_cast<int>(descriptor.layers[layer].pitch[0]),
        base::checked_cast<int>(descriptor.layers[layer].offset[0]),
        kZeroSizeToPreventMapping,
        base::ScopedFD(HANDLE_EINTR(dup(bo_fd.get()))));
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
           .Contains(gfx::Rect(scoped_va_surface.size()))) {
    LOG(ERROR) << "A " << scoped_va_surface.size().ToString()
               << " ScopedVASurface cannot be contained by a "
               << exported_pixmap->va_surface_resolution.ToString()
               << " buffer";
    return nullptr;
  }
  exported_pixmap->pixmap = base::MakeRefCounted<gfx::NativePixmapDmaBuf>(
      scoped_va_surface.size(), buffer_format, std::move(handle));
  return exported_pixmap;
}

bool VaapiWrapper::SyncSurface(VASurfaceID va_surface_id) {
  DCHECK_NE(va_surface_id, VA_INVALID_ID);

  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, false);
  return true;
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                const void* data) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffer");
  base::AutoLock auto_lock(*va_lock_);
  return SubmitBuffer_Locked({va_buffer_type, size, data});
}

bool VaapiWrapper::SubmitBuffers(
    const std::vector<VABufferDescriptor>& va_buffers) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffers");
  base::AutoLock auto_lock(*va_lock_);
  for (const VABufferDescriptor& va_buffer : va_buffers) {
    if (!SubmitBuffer_Locked(va_buffer))
      return false;
  }
  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DestroyPendingBuffers");
  base::AutoLock auto_lock(*va_lock_);
  DestroyPendingBuffers_Locked();
}

void VaapiWrapper::DestroyPendingBuffers_Locked() {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DestroyPendingBuffers_Locked");
  va_lock_->AssertAcquired();
  for (const auto& pending_va_buf : pending_va_buffers_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_va_buf);
    VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyBuffer);
  }
  pending_va_buffers_.clear();
}

bool VaapiWrapper::ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);
  bool result = Execute_Locked(va_surface_id, pending_va_buffers_);
  DestroyPendingBuffers_Locked();
  return result;
}

bool VaapiWrapper::MapAndCopyAndExecute(
    VASurfaceID va_surface_id,
    const std::vector<std::pair<VABufferID, VABufferDescriptor>>& va_buffers) {
  DCHECK_NE(va_surface_id, VA_INVALID_SURFACE);

  TRACE_EVENT0("media,gpu", "VaapiWrapper::MapAndCopyAndExecute");
  base::AutoLock auto_lock(*va_lock_);
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

#if defined(USE_X11)
bool VaapiWrapper::PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                                        Pixmap x_pixmap,
                                        gfx::Size dest_size) {
  DCHECK(!features::IsUsingOzonePlatform());
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, false);

  // Put the data into an X Pixmap.
  va_res = vaPutSurface(va_display_,
                        va_surface_id,
                        x_pixmap,
                        0, 0, dest_size.width(), dest_size.height(),
                        0, 0, dest_size.width(), dest_size.height(),
                        NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAPutSurface, false);
  return true;
}
#endif  // USE_X11

std::unique_ptr<ScopedVAImage> VaapiWrapper::CreateVaImage(
    VASurfaceID va_surface_id,
    VAImageFormat* format,
    const gfx::Size& size) {
  std::unique_ptr<ScopedVAImage> scoped_image;
  {
    base::AutoLock auto_lock(*va_lock_);

    VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, nullptr);

    scoped_image = std::make_unique<ScopedVAImage>(va_lock_, va_display_,
                                                   va_surface_id, format, size);
  }
  return scoped_image->IsValid() ? std::move(scoped_image) : nullptr;
}

bool VaapiWrapper::UploadVideoFrameToSurface(const VideoFrame& frame,
                                             VASurfaceID va_surface_id,
                                             const gfx::Size& va_surface_size) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::UploadVideoFrameToSurface");
  base::AutoLock auto_lock(*va_lock_);
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
  base::ScopedClosureRunner vaimage_deleter(
      base::Bind(&DestroyVAImage, va_display_, image));

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

  ScopedVABufferMapping mapping(va_lock_, va_display_, image.buf);
  if (!mapping.IsValid())
    return false;
  uint8_t* image_ptr = static_cast<uint8_t*>(mapping.data());

  if (!ClearNV12Padding(image, visible_size, image_ptr)) {
    LOG(ERROR) << "Failed to clear non visible area of VAImage";
    return false;
  }

  int ret = 0;
  {
    TRACE_EVENT0("media,gpu", "VaapiWrapper::UploadVideoFrameToSurface_copy");

    base::AutoUnlock auto_unlock(*va_lock_);
    switch (frame.format()) {
      case PIXEL_FORMAT_I420:
        ret = libyuv::I420ToNV12(
            frame.data(VideoFrame::kYPlane), frame.stride(VideoFrame::kYPlane),
            frame.data(VideoFrame::kUPlane), frame.stride(VideoFrame::kUPlane),
            frame.data(VideoFrame::kVPlane), frame.stride(VideoFrame::kVPlane),
            image_ptr + image.offsets[0], image.pitches[0],
            image_ptr + image.offsets[1], image.pitches[1],
            visible_size.width(), visible_size.height());
        break;
      case PIXEL_FORMAT_NV12: {
        int uv_width = visible_size.width();
        if (visible_size.width() % 2 != 0 &&
            !base::CheckAdd<int>(visible_size.width(), 1)
                 .AssignIfValid(&uv_width)) {
          return false;
        }

        int uv_height = 0;
        if (!(base::CheckAdd<int>(visible_size.height(), 1) / 2)
                 .AssignIfValid(&uv_height)) {
          return false;
        }

        libyuv::CopyPlane(frame.data(VideoFrame::kYPlane),
                          frame.stride(VideoFrame::kYPlane),
                          image_ptr + image.offsets[0], image.pitches[0],
                          visible_size.width(), visible_size.height());
        libyuv::CopyPlane(frame.data(VideoFrame::kUVPlane),
                          frame.stride(VideoFrame::kUVPlane),
                          image_ptr + image.offsets[1], image.pitches[1],
                          uv_width, uv_height);
      } break;
      default:
        LOG(ERROR) << "Unsupported pixel format: " << frame.format();
        return false;
    }
  }
  if (needs_va_put_image) {
    const VAStatus va_res =
        vaPutImage(va_display_, va_surface_id, image.image_id, 0, 0,
                   visible_size.width(), visible_size.height(), 0, 0,
                   visible_size.width(), visible_size.height());
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAPutImage, false);
  }
  return ret == 0;
}

std::unique_ptr<ScopedVABuffer> VaapiWrapper::CreateVABuffer(VABufferType type,
                                                             size_t size) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::CreateVABuffer");
  base::AutoLock auto_lock(*va_lock_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::CreateVABufferLocked");

  return ScopedVABuffer::Create(va_lock_, va_display_, va_context_id_, type,
                                size);
}

uint64_t VaapiWrapper::GetEncodedChunkSize(VABufferID buffer_id,
                                           VASurfaceID sync_surface_id) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::GetEncodedChunkSize");
  base::AutoLock auto_lock(*va_lock_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::GetEncodedChunkSizeLocked");
  VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, 0u);

  ScopedVABufferMapping mapping(va_lock_, va_display_, buffer_id);
  if (!mapping.IsValid())
    return 0u;

  uint64_t coded_data_size = 0;
  for (auto* buffer_segment =
           reinterpret_cast<VACodedBufferSegment*>(mapping.data());
       buffer_segment; buffer_segment = reinterpret_cast<VACodedBufferSegment*>(
                           buffer_segment->next)) {
    coded_data_size += buffer_segment->size;
  }
  return coded_data_size;
}

bool VaapiWrapper::DownloadFromVABuffer(VABufferID buffer_id,
                                        VASurfaceID sync_surface_id,
                                        uint8_t* target_ptr,
                                        size_t target_size,
                                        size_t* coded_data_size) {
  DCHECK(target_ptr);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABuffer");
  base::AutoLock auto_lock(*va_lock_);
  TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABufferLocked");

  {
    TRACE_EVENT0("media,gpu", "VaapiWrapper::DownloadFromVABuffer_SyncSurface");
    const VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVASyncSurface, false);
  }

  ScopedVABufferMapping mapping(va_lock_, va_display_, buffer_id);
  if (!mapping.IsValid())
    return false;
  auto* buffer_segment =
      reinterpret_cast<VACodedBufferSegment*>(mapping.data());

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
  const VAProfile va_profile = ProfileToVAProfile(profile, CodecMode::kEncode);
  VAConfigAttrib attrib;
  attrib.type = VAConfigAttribEncMaxRefFrames;

  base::AutoLock auto_lock(*va_lock_);
  VAStatus va_res =
      vaGetConfigAttributes(va_display_, va_profile,
                            va_entrypoint_, &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAGetConfigAttributes, false);

  *max_ref_frames = attrib.value;
  return true;
}

bool VaapiWrapper::IsRotationSupported() {
  base::AutoLock auto_lock(*va_lock_);
  VAProcPipelineCaps pipeline_caps;
  memset(&pipeline_caps, 0, sizeof(pipeline_caps));
  VAStatus va_res = vaQueryVideoProcPipelineCaps(va_display_, va_context_id_,
                                                 nullptr, 0, &pipeline_caps);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVAQueryVideoProcPipelineCaps,
                       false);

  if (!pipeline_caps.rotation_flags) {
    DVLOG(2) << "VA-API driver doesn't support any rotation";
    return false;
  }
  return true;
}

bool VaapiWrapper::BlitSurface(const VASurface& va_surface_src,
                               const VASurface& va_surface_dest,
                               base::Optional<gfx::Rect> src_rect,
                               base::Optional<gfx::Rect> dest_rect,
                               VideoRotation rotation) {
  DCHECK_EQ(mode_, kVideoProcess);
  base::AutoLock auto_lock(*va_lock_);

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

  {
    ScopedVABufferMapping mapping(va_lock_, va_display_,
                                  va_buffer_for_vpp_->id());
    if (!mapping.IsValid())
      return false;
    auto* pipeline_param =
        reinterpret_cast<VAProcPipelineParameterBuffer*>(mapping.data());

    memset(pipeline_param, 0, sizeof *pipeline_param);
    if (!src_rect)
      src_rect.emplace(gfx::Rect(va_surface_src.size()));
    if (!dest_rect)
      dest_rect.emplace(gfx::Rect(va_surface_dest.size()));

    VARectangle input_region;
    input_region.x = src_rect->x();
    input_region.y = src_rect->y();
    input_region.width = src_rect->width();
    input_region.height = src_rect->height();
    pipeline_param->surface_region = &input_region;
    pipeline_param->surface = va_surface_src.id();
    pipeline_param->surface_color_standard = VAProcColorStandardNone;

    VARectangle output_region;
    output_region.x = dest_rect->x();
    output_region.y = dest_rect->y();
    output_region.width = dest_rect->width();
    output_region.height = dest_rect->height();
    pipeline_param->output_region = &output_region;
    pipeline_param->output_background_color = 0xff000000;
    pipeline_param->output_color_standard = VAProcColorStandardNone;
    pipeline_param->filter_flags = VA_FILTER_SCALING_DEFAULT;

    switch (rotation) {
      case VIDEO_ROTATION_0:
        pipeline_param->rotation_state = VA_ROTATION_NONE;
        break;
      case VIDEO_ROTATION_90:
        pipeline_param->rotation_state = VA_ROTATION_90;
        break;
      case VIDEO_ROTATION_180:
        pipeline_param->rotation_state = VA_ROTATION_180;
        break;
      case VIDEO_ROTATION_270:
        pipeline_param->rotation_state = VA_ROTATION_270;
        break;
    }

    VA_SUCCESS_OR_RETURN(mapping.Unmap(), VaapiFunctions::kVAUnmapBuffer,
                         false);
  }

  VA_SUCCESS_OR_RETURN(
      vaBeginPicture(va_display_, va_context_id_, va_surface_dest.id()),
      VaapiFunctions::kVABeginPicture, false);

  VABufferID va_buffer_id = va_buffer_for_vpp_->id();
  VA_SUCCESS_OR_RETURN(
      vaRenderPicture(va_display_, va_context_id_, &va_buffer_id, 1),
      VaapiFunctions::kVARenderPicture_Vpp, false);

  VA_SUCCESS_OR_RETURN(vaEndPicture(va_display_, va_context_id_),
                       VaapiFunctions::kVAEndPicture, false);

  return true;
}

// static
void VaapiWrapper::PreSandboxInitialization() {
  VADisplayState::PreSandboxInitialization();

  const std::string va_suffix(std::to_string(VA_MAJOR_VERSION + 1));
  StubPathMap paths;

  paths[kModuleVa].push_back(std::string("libva.so.") + va_suffix);
  paths[kModuleVa_drm].push_back(std::string("libva-drm.so.") + va_suffix);
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    paths[kModuleVa_x11].push_back(std::string("libva-x11.so.") + va_suffix);
#endif

  // InitializeStubs dlopen() VA-API libraries
  // libva.so
  // libva-x11.so (X11)
  // libva-drm.so (X11 and Ozone).
  static bool result = InitializeStubs(paths);
  if (!result) {
    static const char kErrorMsg[] = "Failed to initialize VAAPI libs";
#if defined(OS_CHROMEOS)
    // When Chrome runs on Linux with target_os="chromeos", do not log error
    // message without VAAPI libraries.
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS()) << kErrorMsg;
#else
    DVLOG(1) << kErrorMsg;
#endif
  }

  // VASupportedProfiles::Get creates VADisplayState and in so doing
  // driver associated libraries are dlopen(), to know:
  // i965_drv_video.so
  // hybrid_drv_video.so (platforms that support it)
  // libcmrt.so (platforms that support it)
  VASupportedProfiles::Get();
}

VaapiWrapper::VaapiWrapper(CodecMode mode)
    : mode_(mode),
      va_lock_(VADisplayState::Get()->va_lock()),
      va_display_(NULL),
      va_config_id_(VA_INVALID_ID),
      va_context_id_(VA_INVALID_ID) {}

VaapiWrapper::~VaapiWrapper() {
  // Destroy ScopedVABuffer before VaapiWrappers are destroyed to ensure
  // VADisplay is valid on ScopedVABuffer's destruction.
  va_buffer_for_vpp_.reset();
  DestroyPendingBuffers();
  DestroyContext();
  Deinitialize();
}

bool VaapiWrapper::Initialize(CodecMode mode, VAProfile va_profile) {
#if DCHECK_IS_ON()
  if (mode == kEncodeConstantQuantizationParameter) {
    DCHECK_NE(va_profile, VAProfileJPEGBaseline)
        << "JPEG Encoding doesn't support CQP bitrate control";
  }
#endif  // DCHECK_IS_ON()

  const VAEntrypoint entrypoint = GetDefaultVaEntryPoint(mode, va_profile);

  base::AutoLock auto_lock(*va_lock_);
  std::vector<VAConfigAttrib> required_attribs;
  if (!GetRequiredAttribs(va_lock_, va_display_, mode, va_profile, entrypoint,
                          &required_attribs)) {
    return false;
  }

  const VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint,
                     required_attribs.empty() ? nullptr : &required_attribs[0],
                     required_attribs.size(), &va_config_id_);

  va_entrypoint_ = entrypoint;

  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateConfig, false);
  return true;
}

void VaapiWrapper::Deinitialize() {
  {
    base::AutoLock auto_lock(*va_lock_);
    if (va_config_id_ != VA_INVALID_ID) {
      VAStatus va_res = vaDestroyConfig(va_display_, va_config_id_);
      VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyConfig);
    }
    va_config_id_ = VA_INVALID_ID;
    va_display_ = nullptr;
  }

  const VAStatus va_res = VADisplayState::Get()->Deinitialize();
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVATerminate);
}

bool VaapiWrapper::VaInitialize(
    const ReportErrorToUMACB& report_error_to_uma_cb) {
  report_error_to_uma_cb_ = report_error_to_uma_cb;
  if (!VADisplayState::Get()->Initialize())
    return false;

  {
    base::AutoLock auto_lock(*va_lock_);
    va_display_ = VADisplayState::Get()->va_display();
    DCHECK(va_display_) << "VADisplayState hasn't been properly Initialize()d";
  }
  return true;
}

void VaapiWrapper::DestroyContext() {
  base::AutoLock auto_lock(*va_lock_);
  DVLOG(2) << "Destroying context";

  if (va_context_id_ != VA_INVALID_ID) {
    const VAStatus va_res = vaDestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroyContext);
  }

  va_context_id_ = VA_INVALID_ID;
}

bool VaapiWrapper::CreateSurfaces(unsigned int va_format,
                                  const gfx::Size& size,
                                  SurfaceUsageHint usage_hint,
                                  size_t num_surfaces,
                                  std::vector<VASurfaceID>* va_surfaces) {
  DVLOG(2) << "Creating " << num_surfaces << " " << size.ToString()
           << " surfaces";
  DCHECK_NE(va_format, kInvalidVaRtFormat);
  DCHECK(va_surfaces->empty());

  va_surfaces->resize(num_surfaces);
  VASurfaceAttrib attribute{};
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  switch (usage_hint) {
    case SurfaceUsageHint::kVideoDecoder:
      attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
      break;
    case SurfaceUsageHint::kVideoEncoder:
      attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
      break;
    case SurfaceUsageHint::kVideoProcessWrite:
      attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_VPP_WRITE;
      break;
    case SurfaceUsageHint::kGeneric:
      attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC;
      break;
  }

  VAStatus va_res;
  {
    base::AutoLock auto_lock(*va_lock_);
    va_res = vaCreateSurfaces(
        va_display_, va_format, base::checked_cast<unsigned int>(size.width()),
        base::checked_cast<unsigned int>(size.height()), va_surfaces->data(),
        num_surfaces, &attribute, 1u);
  }
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVACreateSurfaces_Allocating);
  return va_res == VA_STATUS_SUCCESS;
}

std::unique_ptr<ScopedVASurface> VaapiWrapper::CreateScopedVASurface(
    unsigned int va_rt_format,
    const gfx::Size& size,
    const base::Optional<gfx::Size>& visible_size) {
  if (kInvalidVaRtFormat == va_rt_format) {
    LOG(ERROR) << "Invalid VA RT format to CreateScopedVASurface";
    return nullptr;
  }

  if (size.IsEmpty()) {
    LOG(ERROR) << "Invalid visible size input to CreateScopedVASurface";
    return nullptr;
  }

  base::AutoLock auto_lock(*va_lock_);
  VASurfaceID va_surface_id = VA_INVALID_ID;
  VAStatus va_res = vaCreateSurfaces(
      va_display_, va_rt_format, base::checked_cast<unsigned int>(size.width()),
      base::checked_cast<unsigned int>(size.height()), &va_surface_id, 1u, NULL,
      0);
  VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateSurfaces_Allocating,
                       nullptr);

  DCHECK_NE(VA_INVALID_ID, va_surface_id)
      << "Invalid VA surface id after vaCreateSurfaces";

  DCHECK(!visible_size.has_value() || !visible_size->IsEmpty());
  auto scoped_va_surface = std::make_unique<ScopedVASurface>(
      this, va_surface_id, visible_size.has_value() ? *visible_size : size,
      va_rt_format);

  DCHECK(scoped_va_surface);
  DCHECK(scoped_va_surface->IsValid());
  return scoped_va_surface;
}

void VaapiWrapper::DestroySurfaces(std::vector<VASurfaceID> va_surfaces) {
  DVLOG(2) << "Destroying " << va_surfaces.size() << " surfaces";

  // vaDestroySurfaces() makes no guarantees about VA_INVALID_SURFACE.
  base::Erase(va_surfaces, VA_INVALID_SURFACE);
  if (va_surfaces.empty())
    return;

  base::AutoLock auto_lock(*va_lock_);
  const VAStatus va_res =
      vaDestroySurfaces(va_display_, va_surfaces.data(), va_surfaces.size());
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroySurfaces);
}

void VaapiWrapper::DestroySurface(VASurfaceID va_surface_id) {
  if (va_surface_id == VA_INVALID_SURFACE)
    return;
  DVLOG(2) << __func__ << " " << va_surface_id;
  base::AutoLock auto_lock(*va_lock_);
  const VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_id, 1);
  VA_LOG_ON_ERROR(va_res, VaapiFunctions::kVADestroySurfaces);
}

bool VaapiWrapper::Execute_Locked(VASurfaceID va_surface_id,
                                  const std::vector<VABufferID>& va_buffers) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::Execute_Locked");
  va_lock_->AssertAcquired();

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

  UMA_HISTOGRAM_TIMES("Media.PlatformVideoDecoding.Decode",
                      base::TimeTicks::Now() - decode_start_time);

  return true;
}

bool VaapiWrapper::SubmitBuffer_Locked(const VABufferDescriptor& va_buffer) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::SubmitBuffer_Locked");
  va_lock_->AssertAcquired();

  DCHECK_LT(va_buffer.type, VABufferTypeMax);
  DCHECK(va_buffer.data);

  unsigned int va_buffer_size;
  if (!base::CheckedNumeric<size_t>(va_buffer.size)
           .AssignIfValid(&va_buffer_size)) {
    return false;
  }

  VABufferID buffer_id;
  {
    TRACE_EVENT0("media,gpu",
                 "VaapiWrapper::SubmitBuffer_Locked_vaCreateBuffer");
    const VAStatus va_res =
        vaCreateBuffer(va_display_, va_context_id_, va_buffer.type,
                       va_buffer_size, 1, nullptr, &buffer_id);
    VA_SUCCESS_OR_RETURN(va_res, VaapiFunctions::kVACreateBuffer, false);
  }

  if (!MapAndCopy_Locked(buffer_id, va_buffer))
    return false;

  pending_va_buffers_.push_back(buffer_id);
  return true;
}

bool VaapiWrapper::MapAndCopy_Locked(VABufferID va_buffer_id,
                                     const VABufferDescriptor& va_buffer) {
  va_lock_->AssertAcquired();

  DCHECK_NE(va_buffer_id, VA_INVALID_ID);
  DCHECK_LT(va_buffer.type, VABufferTypeMax);
  DCHECK(va_buffer.data);

  ScopedVABufferMapping mapping(
      va_lock_, va_display_, va_buffer_id,
      base::BindOnce(base::IgnoreResult(&vaDestroyBuffer), va_display_));
  if (!mapping.IsValid())
    return false;

  return memcpy(mapping.data(), va_buffer.data, va_buffer.size);
}

}  // namespace media
