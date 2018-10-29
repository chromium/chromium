// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_wrapper.h"

#include <dlfcn.h>
#include <string.h>

#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_version.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#include "media/base/media_switches.h"

// Auto-generated for dlopen libva libraries
#include "media/gpu/vaapi/va_stubs.h"

#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_implementation.h"

#if defined(USE_X11)
#include <va/va_x11.h>
#include "ui/gfx/x/x11_types.h"  // nogncheck
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

#define LOG_VA_ERROR_AND_REPORT(va_error, err_msg)                  \
  do {                                                              \
    LOG(ERROR) << err_msg << " VA error: " << vaErrorStr(va_error); \
    report_error_to_uma_cb_.Run();                                  \
  } while (0)

#define VA_LOG_ON_ERROR(va_error, err_msg)        \
  do {                                            \
    if ((va_error) != VA_STATUS_SUCCESS)          \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg); \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_error, err_msg, ret) \
  do {                                               \
    if ((va_error) != VA_STATUS_SUCCESS) {           \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);    \
      return (ret);                                  \
    }                                                \
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
    case gfx::BufferFormat::UYVY_422:
      return VA_FOURCC_UYVY;
    case gfx::BufferFormat::YVU_420:
      return VA_FOURCC_YV12;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VA_FOURCC_NV12;
    default:
      NOTREACHED();
      return 0;
  }
}

uint32_t BufferFormatToVARTFormat(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::UYVY_422:
      return VA_RT_FORMAT_YUV422;
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return VA_RT_FORMAT_RGB32;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VA_RT_FORMAT_YUV420;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

namespace media {

namespace {

// Maximum framerate of encoded profile. This value is an arbitary limit
// and not taken from HW documentation.
constexpr int kMaxEncoderFramerate = 30;

// A map between VideoCodecProfile and VAProfile.
static const struct {
  VideoCodecProfile profile;
  VAProfile va_profile;
} kProfileMap[] = {
    {H264PROFILE_BASELINE, VAProfileH264Baseline},
    {H264PROFILE_MAIN, VAProfileH264Main},
    // TODO(posciak): See if we can/want to support other variants of
    // H264PROFILE_HIGH*.
    {H264PROFILE_HIGH, VAProfileH264High},
    {VP8PROFILE_ANY, VAProfileVP8Version0_3},
    {VP9PROFILE_PROFILE0, VAProfileVP9Profile0},
    {VP9PROFILE_PROFILE1, VAProfileVP9Profile1},
    {VP9PROFILE_PROFILE2, VAProfileVP9Profile2},
    {VP9PROFILE_PROFILE3, VAProfileVP9Profile3},
};

static const struct {
  std::string va_driver;
  std::string cpu_family;
  VaapiWrapper::CodecMode mode;
  std::vector<VAProfile> va_profiles;
} kBlackListMap[]{
    // TODO(hiroh): Remove once Chrome supports unpacked header.
    // https://crbug.com/828482.
    {"Mesa Gallium driver",
     "AMD STONEY",
     VaapiWrapper::CodecMode::kEncode,
     {VAProfileH264Baseline, VAProfileH264Main, VAProfileH264High,
      VAProfileH264ConstrainedBaseline}},
};

bool IsBlackListedDriver(const std::string& va_vendor_string,
                         VaapiWrapper::CodecMode mode,
                         VAProfile va_profile) {
  for (const auto& info : kBlackListMap) {
    if (info.mode == mode &&
        base::StartsWith(va_vendor_string, info.va_driver,
                         base::CompareCase::SENSITIVE) &&
        va_vendor_string.find(info.cpu_family) != std::string::npos &&
        base::ContainsValue(info.va_profiles, va_profile)) {
      return true;
    }
  }

  // TODO(posciak): Remove once VP8 encoding is to be enabled by default.
  if (mode == VaapiWrapper::CodecMode::kEncode &&
      va_profile == VAProfileVP8Version0_3 &&
      !base::FeatureList::IsEnabled(kVaapiVP8Encoder)) {
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

  // |va_lock_| must be held on entry.
  bool Initialize();
  void Deinitialize(VAStatus* status);

  base::Lock* va_lock() { return &va_lock_; }
  VADisplay va_display() const { return va_display_; }
  const std::string& va_vendor_string() const { return va_vendor_string_; }

  void SetDrmFd(base::PlatformFile fd) { drm_fd_.reset(HANDLE_EINTR(dup(fd))); }

 private:
  friend class base::NoDestructor<VADisplayState>;

  VADisplayState();
  ~VADisplayState() = default;

  // Implementation of Initialize() called only once.
  bool InitializeOnce();

  // Protected by |va_lock_|.
  int refcount_;

  // Libva is not thread safe, so we have to do locking for it ourselves.
  // This lock is to be taken for the duration of all VA-API calls and for
  // the entire job submission sequence in ExecuteAndDestroyPendingBuffers().
  base::Lock va_lock_;

  // Drm fd used to obtain access to the driver interface by VA.
  base::ScopedFD drm_fd_;

  // The VADisplay handle.
  VADisplay va_display_;

  // True if vaInitialize() has been called successfully.
  bool va_initialized_;

  // String acquired by vaQueryVendorString().
  std::string va_vendor_string_;

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
  va_lock_.AssertAcquired();

  if (!IsVaInitialized() ||
#if defined(USE_X11)
      !IsVa_x11Initialized() ||
#endif
      !IsVa_drmInitialized()) {
    return false;
  }

  // Manual refcounting to ensure the rest of the method is called only once.
  if (refcount_++ > 0)
    return true;

  const bool success = InitializeOnce();
  UMA_HISTOGRAM_BOOLEAN("Media.VaapiWrapper.VADisplayStateInitializeSuccess",
                        success);
  return success;
}

bool VADisplayState::InitializeOnce() {
  switch (gl::GetGLImplementation()) {
    case gl::kGLImplementationEGLGLES2:
      va_display_ = vaGetDisplayDRM(drm_fd_.get());
      break;
    case gl::kGLImplementationDesktopGL:
#if defined(USE_X11)
      va_display_ = vaGetDisplay(gfx::GetXDisplay());
#else
      LOG(WARNING) << "VAAPI video acceleration not available without "
                      "DesktopGL (GLX).";
#endif  // USE_X11
      break;
    // Cannot infer platform from GL, try all available displays
    case gl::kGLImplementationNone:
#if defined(USE_X11)
      va_display_ = vaGetDisplay(gfx::GetXDisplay());
      if (vaDisplayIsValid(va_display_))
        break;
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

  // Set VA logging level to enable error messages, unless already set
  constexpr char libva_log_level_env[] = "LIBVA_MESSAGING_LEVEL";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!env->HasVar(libva_log_level_env))
    env->SetVar(libva_log_level_env, "1");

  // The VAAPI version.
  int major_version, minor_version;
  VAStatus va_res = vaInitialize(va_display_, &major_version, &minor_version);
  if (va_res != VA_STATUS_SUCCESS) {
    LOG(ERROR) << "vaInitialize failed: " << vaErrorStr(va_res);
    return false;
  }

  va_initialized_ = true;

  va_vendor_string_ = vaQueryVendorString(va_display_);
  DLOG_IF(WARNING, va_vendor_string_.empty())
      << "Vendor string empty or error reading.";
  DVLOG(1) << "VAAPI version: " << major_version << "." << minor_version << " "
           << va_vendor_string_;

  if (major_version != VA_MAJOR_VERSION || minor_version != VA_MINOR_VERSION) {
    LOG(ERROR) << "This build of Chromium requires VA-API version "
               << VA_MAJOR_VERSION << "." << VA_MINOR_VERSION
               << ", system version: " << major_version << "." << minor_version;
    return false;
  }
  return true;
}

void VADisplayState::Deinitialize(VAStatus* status) {
  va_lock_.AssertAcquired();
  if (--refcount_ > 0)
    return;

  // Must check if vaInitialize completed successfully, to work around a bug in
  // libva. The bug was fixed upstream:
  // http://lists.freedesktop.org/archives/libva/2013-July/001807.html
  // TODO(mgiuca): Remove this check, and the |va_initialized_| variable, once
  // the fix has rolled out sufficiently.
  if (va_initialized_ && va_display_)
    *status = vaTerminate(va_display_);
  va_initialized_ = false;
  va_display_ = nullptr;
}

static std::vector<VAConfigAttrib> GetRequiredAttribs(
    VaapiWrapper::CodecMode mode,
    VAProfile profile) {
  std::vector<VAConfigAttrib> required_attribs;

  // VAConfigAttribRTFormat is common to both encode and decode |mode|s.
  if (profile == VAProfileVP9Profile2 || profile == VAProfileVP9Profile3) {
    required_attribs.push_back(
        {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420_10BPP});
  } else {
    required_attribs.push_back({VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420});
  }

  if (mode != VaapiWrapper::kEncode)
    return required_attribs;

  // All encoding use constant bit rate except for JPEG.
  if (profile != VAProfileJPEGBaseline)
    required_attribs.push_back({VAConfigAttribRateControl, VA_RC_CBR});

  // VAConfigAttribEncPackedHeaders is H.264 specific.
  if (profile >= VAProfileH264Baseline && profile <= VAProfileH264High) {
    required_attribs.push_back(
        {VAConfigAttribEncPackedHeaders,
         VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE});
  }

  return required_attribs;
}

static VAEntrypoint GetVaEntryPoint(VaapiWrapper::CodecMode mode,
                                    VAProfile profile) {
  switch (mode) {
    case VaapiWrapper::kDecode:
      return VAEntrypointVLD;
    case VaapiWrapper::kEncode:
      if (profile == VAProfileJPEGBaseline)
        return VAEntrypointEncPicture;
      else
        return VAEntrypointEncSlice;
    case VaapiWrapper::kCodecModeMax:
      NOTREACHED();
      return VAEntrypointVLD;
  }
}

// This class encapsulates reading and giving access to the list of supported
// ProfileInfo entries, in a singleton way.
class VASupportedProfiles {
 public:
  struct ProfileInfo {
    VAProfile va_profile;
    gfx::Size max_resolution;
  };
  static const VASupportedProfiles& Get();

  const std::vector<ProfileInfo>& GetSupportedProfileInfosForCodecMode(
      VaapiWrapper::CodecMode mode) const;

  bool IsProfileSupported(VaapiWrapper::CodecMode mode,
                          VAProfile va_profile) const;

 private:
  friend class base::NoDestructor<VASupportedProfiles>;

  VASupportedProfiles();
  ~VASupportedProfiles() = default;

  bool GetSupportedVAProfiles(std::vector<VAProfile>* profiles) const;

  // Gets supported profile infos for |mode|.
  std::vector<ProfileInfo> GetSupportedProfileInfosForCodecModeInternal(
      VaapiWrapper::CodecMode mode) const;

  // |va_lock_| must be held on entry in the following _Locked methods.

  // Checks if |va_profile| supports |entrypoint| or not.
  bool IsEntrypointSupported_Locked(VAProfile va_profile,
                                    VAEntrypoint entrypoint) const;
  // Returns true if |va_profile| for |entrypoint| with |required_attribs| is
  // supported.
  bool AreAttribsSupported_Locked(
      VAProfile va_profile,
      VAEntrypoint entrypoint,
      const std::vector<VAConfigAttrib>& required_attribs) const;
  // Gets maximum resolution for |va_profile| and |entrypoint| with
  // |required_attribs|. If return value is true, |resolution| is the maximum
  // resolution.
  bool GetMaxResolution_Locked(VAProfile va_profile,
                               VAEntrypoint entrypoint,
                               std::vector<VAConfigAttrib>& required_attribs,
                               gfx::Size* resolution) const;
  std::vector<ProfileInfo> supported_profiles_[VaapiWrapper::kCodecModeMax];

  // Pointer to VADisplayState's members |va_lock_| and its |va_display_|.
  base::Lock* va_lock_;
  VADisplay va_display_;

  const base::Closure report_error_to_uma_cb_;

  DISALLOW_COPY_AND_ASSIGN(VASupportedProfiles);
};

// static
const VASupportedProfiles& VASupportedProfiles::Get() {
  static const base::NoDestructor<VASupportedProfiles> profile_infos;
  return *profile_infos;
}

const std::vector<VASupportedProfiles::ProfileInfo>&
VASupportedProfiles::GetSupportedProfileInfosForCodecMode(
    VaapiWrapper::CodecMode mode) const {
  return supported_profiles_[mode];
}

bool VASupportedProfiles::IsProfileSupported(VaapiWrapper::CodecMode mode,
                                             VAProfile va_profile) const {
  for (const auto& profile : supported_profiles_[mode]) {
    if (profile.va_profile == va_profile)
      return true;
  }
  return false;
}

VASupportedProfiles::VASupportedProfiles()
    : va_display_(nullptr), report_error_to_uma_cb_(base::DoNothing()) {
  VADisplayState* display_state = VADisplayState::Get();
  va_lock_ = display_state->va_lock();

  static_assert(arraysize(supported_profiles_) == VaapiWrapper::kCodecModeMax,
                "The array size of supported profile is incorrect.");
  {
    base::AutoLock auto_lock(*va_lock_);
    if (!display_state->Initialize())
      return;
  }

  va_display_ = display_state->va_display();
  DCHECK(va_display_) << "VADisplayState hasn't been properly Initialize()d";
  for (size_t i = 0; i < VaapiWrapper::kCodecModeMax; ++i) {
    supported_profiles_[i] = GetSupportedProfileInfosForCodecModeInternal(
        static_cast<VaapiWrapper::CodecMode>(i));
  }

  {
    base::AutoLock auto_lock(*va_lock_);
    VAStatus va_res = VA_STATUS_SUCCESS;
    display_state->Deinitialize(&va_res);
    VA_LOG_ON_ERROR(va_res, "VADisplayState::Deinitialize failed");
    va_display_ = nullptr;
  }
}

std::vector<VASupportedProfiles::ProfileInfo>
VASupportedProfiles::GetSupportedProfileInfosForCodecModeInternal(
    VaapiWrapper::CodecMode mode) const {
  std::vector<ProfileInfo> supported_profile_infos;
  std::vector<VAProfile> va_profiles;
  if (!GetSupportedVAProfiles(&va_profiles))
    return supported_profile_infos;

  base::AutoLock auto_lock(*va_lock_);
  const std::string& va_vendor_string =
      VADisplayState::Get()->va_vendor_string();
  for (const auto& va_profile : va_profiles) {
    VAEntrypoint entrypoint = GetVaEntryPoint(mode, va_profile);
    std::vector<VAConfigAttrib> required_attribs =
        GetRequiredAttribs(mode, va_profile);
    if (!IsEntrypointSupported_Locked(va_profile, entrypoint))
      continue;
    if (!AreAttribsSupported_Locked(va_profile, entrypoint, required_attribs))
      continue;
    if (IsBlackListedDriver(va_vendor_string, mode, va_profile))
      continue;

    ProfileInfo profile_info;
    if (!GetMaxResolution_Locked(va_profile, entrypoint, required_attribs,
                                 &profile_info.max_resolution)) {
      LOG(ERROR) << "GetMaxResolution failed for va_profile " << va_profile
                 << " and entrypoint " << entrypoint;
      continue;
    }
    profile_info.va_profile = va_profile;
    supported_profile_infos.push_back(profile_info);
  }
  return supported_profile_infos;
}

bool VASupportedProfiles::GetSupportedVAProfiles(
    std::vector<VAProfile>* profiles) const {
  base::AutoLock auto_lock(*va_lock_);
  // Query the driver for supported profiles.
  const int max_profiles = vaMaxNumProfiles(va_display_);
  std::vector<VAProfile> supported_profiles(
      base::checked_cast<size_t>(max_profiles));

  int num_supported_profiles;
  VAStatus va_res = vaQueryConfigProfiles(va_display_, &supported_profiles[0],
                                          &num_supported_profiles);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigProfiles failed", false);
  if (num_supported_profiles < 0 || num_supported_profiles > max_profiles) {
    LOG(ERROR) << "vaQueryConfigProfiles returned: " << num_supported_profiles;
    return false;
  }

  supported_profiles.resize(base::checked_cast<size_t>(num_supported_profiles));
  *profiles = std::move(supported_profiles);
  return true;
}

bool VASupportedProfiles::IsEntrypointSupported_Locked(
    VAProfile va_profile,
    VAEntrypoint entrypoint) const {
  va_lock_->AssertAcquired();
  // Query the driver for supported entrypoints.
  int max_entrypoints = vaMaxNumEntrypoints(va_display_);
  std::vector<VAEntrypoint> supported_entrypoints(
      base::checked_cast<size_t>(max_entrypoints));

  int num_supported_entrypoints;
  VAStatus va_res = vaQueryConfigEntrypoints(va_display_, va_profile,
                                             &supported_entrypoints[0],
                                             &num_supported_entrypoints);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigEntrypoints failed", false);
  if (num_supported_entrypoints < 0 ||
      num_supported_entrypoints > max_entrypoints) {
    LOG(ERROR) << "vaQueryConfigEntrypoints returned: "
               << num_supported_entrypoints;
    return false;
  }

  return base::ContainsValue(supported_entrypoints, entrypoint);
}

bool VASupportedProfiles::AreAttribsSupported_Locked(
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    const std::vector<VAConfigAttrib>& required_attribs) const {
  va_lock_->AssertAcquired();
  // Query the driver for required attributes.
  std::vector<VAConfigAttrib> attribs = required_attribs;
  for (size_t i = 0; i < required_attribs.size(); ++i)
    attribs[i].value = 0;

  VAStatus va_res = vaGetConfigAttributes(va_display_, va_profile, entrypoint,
                                          &attribs[0], attribs.size());
  VA_SUCCESS_OR_RETURN(va_res, "vaGetConfigAttributes failed", false);

  for (size_t i = 0; i < required_attribs.size(); ++i) {
    if (attribs[i].type != required_attribs[i].type ||
        (attribs[i].value & required_attribs[i].value) !=
            required_attribs[i].value) {
      DVLOG(1) << "Unsupported value " << required_attribs[i].value
               << " for attribute type " << required_attribs[i].type;
      return false;
    }
  }
  return true;
}

bool VASupportedProfiles::GetMaxResolution_Locked(
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    std::vector<VAConfigAttrib>& required_attribs,
    gfx::Size* resolution) const {
  va_lock_->AssertAcquired();
  VAConfigID va_config_id;
  VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  // Calls vaQuerySurfaceAttributes twice. The first time is to get the number
  // of attributes to prepare the space and the second time is to get all
  // attributes.
  unsigned int num_attribs;
  va_res = vaQuerySurfaceAttributes(va_display_, va_config_id, nullptr,
                                    &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, "vaQuerySurfaceAttributes failed", false);
  if (!num_attribs)
    return false;

  std::vector<VASurfaceAttrib> attrib_list(
      base::checked_cast<size_t>(num_attribs));

  va_res = vaQuerySurfaceAttributes(va_display_, va_config_id, &attrib_list[0],
                                    &num_attribs);
  VA_SUCCESS_OR_RETURN(va_res, "vaQuerySurfaceAttributes failed", false);

  resolution->SetSize(0, 0);
  for (const auto& attrib : attrib_list) {
    if (attrib.type == VASurfaceAttribMaxWidth)
      resolution->set_width(attrib.value.value.i);
    else if (attrib.type == VASurfaceAttribMaxHeight)
      resolution->set_height(attrib.value.value.i);
  }
  if (resolution->IsEmpty()) {
    LOG(ERROR) << "Wrong codec resolution: " << resolution->ToString();
    return false;
  }
  return true;
}

// Maps VideoCodecProfile enum values to VaProfile values. This function
// includes a workaround for https://crbug.com/345569: if va_profile is h264
// baseline and it is not supported, we try constrained baseline.
VAProfile ProfileToVAProfile(VideoCodecProfile profile,
                             VaapiWrapper::CodecMode mode) {
  VAProfile va_profile = VAProfileNone;
  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    if (kProfileMap[i].profile == profile) {
      va_profile = kProfileMap[i].va_profile;
      break;
    }
  }
  const VASupportedProfiles& supported_profiles = VASupportedProfiles::Get();
  if (!supported_profiles.IsProfileSupported(mode, va_profile) &&
      va_profile == VAProfileH264Baseline) {
    // https://crbug.com/345569: ProfileIDToVideoCodecProfile() currently strips
    // the information whether the profile is constrained or not, so we have no
    // way to know here. Try for baseline first, but if it is not supported,
    // try constrained baseline and hope this is what it actually is
    // (which in practice is true for a great majority of cases).
    if (supported_profiles.IsProfileSupported(
            mode, VAProfileH264ConstrainedBaseline)) {
      va_profile = VAProfileH264ConstrainedBaseline;
      DVLOG(1) << "Fall back to constrained baseline profile.";
    }
  }
  return va_profile;
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

 private:
  friend class base::NoDestructor<VASupportedImageFormats>;

  VASupportedImageFormats();
  ~VASupportedImageFormats() = default;

  // Initialize the list of supported image formats. The VA display should be
  // locked upon calling this function.
  bool InitSupportedImageFormats();

  std::vector<VAImageFormat> supported_formats_;
  const base::RepeatingClosure report_error_to_uma_cb_;

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

VASupportedImageFormats::VASupportedImageFormats()
    : report_error_to_uma_cb_(base::DoNothing()) {
  VADisplayState* display_state = VADisplayState::Get();
  base::Lock* va_lock = display_state->va_lock();
  base::AutoLock auto_lock(*va_lock);

  if (!display_state->Initialize())
    return;

  VADisplay va_display = display_state->va_display();
  DCHECK(va_display) << "VADisplayState hasn't been properly initialized";

  if (!InitSupportedImageFormats())
    LOG(ERROR) << "Failed to get supported image formats";

  VAStatus va_res = VA_STATUS_SUCCESS;
  display_state->Deinitialize(&va_res);
  VA_LOG_ON_ERROR(va_res, "VADisplayState::Deinitialize failed");
}

bool VASupportedImageFormats::InitSupportedImageFormats() {
  VADisplayState* display_state = VADisplayState::Get();
  display_state->va_lock()->AssertAcquired();
  VADisplay va_display = display_state->va_display();
  DCHECK(va_display) << "VADisplayState hasn't been properly initialized";

  // Query the driver for the max number of image formats and allocate space.
  const int max_image_formats = vaMaxNumImageFormats(va_display);
  if (max_image_formats < 0) {
    LOG(ERROR) << "vaMaxNumImageFormats returned: " << max_image_formats;
    return false;
  }
  supported_formats_.resize(static_cast<size_t>(max_image_formats));

  // Query the driver for the list of supported image formats.
  int num_image_formats;
  VAStatus va_res = vaQueryImageFormats(va_display, supported_formats_.data(),
                                        &num_image_formats);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryImageFormats failed", false);
  if (num_image_formats < 0 || num_image_formats > max_image_formats) {
    LOG(ERROR) << "vaQueryImageFormats returned: " << num_image_formats;
    supported_formats_.clear();
    return false;
  }

  // Resize the list to the actual number of formats returned by the driver.
  supported_formats_.resize(static_cast<size_t>(num_image_formats));
  return true;
}

}  // namespace

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::Create(
    CodecMode mode,
    VAProfile va_profile,
    const base::Closure& report_error_to_uma_cb) {
  if (!VASupportedProfiles::Get().IsProfileSupported(mode, va_profile)) {
    DVLOG(1) << "Unsupported va_profile: " << va_profile;
    return nullptr;
  }

  scoped_refptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper());
  if (vaapi_wrapper->VaInitialize(report_error_to_uma_cb)) {
    if (vaapi_wrapper->Initialize(mode, va_profile))
      return vaapi_wrapper;
  }
  LOG(ERROR) << "Failed to create VaapiWrapper for va_profile: " << va_profile;
  return nullptr;
}

// static
scoped_refptr<VaapiWrapper> VaapiWrapper::CreateForVideoCodec(
    CodecMode mode,
    VideoCodecProfile profile,
    const base::Closure& report_error_to_uma_cb) {
  VAProfile va_profile = ProfileToVAProfile(profile, mode);
  return Create(mode, va_profile, report_error_to_uma_cb);
}

// static
VideoEncodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedEncodeProfiles() {
  VideoEncodeAccelerator::SupportedProfiles profiles;
  const std::vector<VASupportedProfiles::ProfileInfo>& encode_profile_infos =
      VASupportedProfiles::Get().GetSupportedProfileInfosForCodecMode(kEncode);

  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    VAProfile va_profile = ProfileToVAProfile(kProfileMap[i].profile, kEncode);
    if (va_profile == VAProfileNone)
      continue;
    for (const auto& profile_info : encode_profile_infos) {
      if (profile_info.va_profile == va_profile) {
        VideoEncodeAccelerator::SupportedProfile profile;
        profile.profile = kProfileMap[i].profile;
        profile.max_resolution = profile_info.max_resolution;
        profile.max_framerate_numerator = kMaxEncoderFramerate;
        profile.max_framerate_denominator = 1;
        profiles.push_back(profile);
        break;
      }
    }
  }
  return profiles;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VaapiWrapper::GetSupportedDecodeProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles;
  const std::vector<VASupportedProfiles::ProfileInfo>& decode_profile_infos =
      VASupportedProfiles::Get().GetSupportedProfileInfosForCodecMode(kDecode);

  for (size_t i = 0; i < arraysize(kProfileMap); ++i) {
    VAProfile va_profile = ProfileToVAProfile(kProfileMap[i].profile, kDecode);
    if (va_profile == VAProfileNone)
      continue;
    for (const auto& profile_info : decode_profile_infos) {
      if (profile_info.va_profile == va_profile) {
        VideoDecodeAccelerator::SupportedProfile profile;
        profile.profile = kProfileMap[i].profile;
        profile.max_resolution = profile_info.max_resolution;
        profile.min_resolution.SetSize(16, 16);
        profiles.push_back(profile);
        break;
      }
    }
  }
  return profiles;
}

// static
bool VaapiWrapper::IsJpegDecodeSupported() {
  return VASupportedProfiles::Get().IsProfileSupported(kDecode,
                                                       VAProfileJPEGBaseline);
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

bool VaapiWrapper::CreateSurfaces(unsigned int va_format,
                                  const gfx::Size& size,
                                  size_t num_surfaces,
                                  std::vector<VASurfaceID>* va_surfaces) {
  base::AutoLock auto_lock(*va_lock_);
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";

  DCHECK(va_surfaces->empty());
  DCHECK(va_surface_ids_.empty());
  DCHECK_EQ(va_surface_format_, 0u);
  va_surface_ids_.resize(num_surfaces);

  // Allocate surfaces in driver.
  VAStatus va_res =
      vaCreateSurfaces(va_display_, va_format, size.width(), size.height(),
                       &va_surface_ids_[0], va_surface_ids_.size(), NULL, 0);

  VA_LOG_ON_ERROR(va_res, "vaCreateSurfaces failed");
  if (va_res != VA_STATUS_SUCCESS) {
    va_surface_ids_.clear();
    return false;
  }

  // And create a context associated with them.
  const bool success = CreateContext(va_format, size, va_surface_ids_);
  if (success)
    *va_surfaces = va_surface_ids_;
  else
    DestroySurfaces_Locked();
  return success;
}

bool VaapiWrapper::CreateContext(unsigned int va_format,
                                 const gfx::Size& size,
                                 const std::vector<VASurfaceID>& va_surfaces) {
  VAStatus va_res = vaCreateContext(
      va_display_, va_config_id_, size.width(), size.height(), VA_PROGRESSIVE,
      &va_surface_ids_[0], va_surface_ids_.size(), &va_context_id_);

  VA_LOG_ON_ERROR(va_res, "vaCreateContext failed");
  if (va_res == VA_STATUS_SUCCESS)
    va_surface_format_ = va_format;
  return va_res == VA_STATUS_SUCCESS;
}

void VaapiWrapper::DestroySurfaces() {
  base::AutoLock auto_lock(*va_lock_);
  DVLOG(2) << "Destroying " << va_surface_ids_.size() << " surfaces";

  DestroySurfaces_Locked();
}

scoped_refptr<VASurface> VaapiWrapper::CreateVASurfaceForPixmap(
    const scoped_refptr<gfx::NativePixmap>& pixmap) {
  // Create a VASurface for a NativePixmap by importing the underlying dmabufs.
  VASurfaceAttribExternalBuffers va_attrib_extbuf;
  memset(&va_attrib_extbuf, 0, sizeof(va_attrib_extbuf));

  va_attrib_extbuf.pixel_format =
      BufferFormatToVAFourCC(pixmap->GetBufferFormat());
  gfx::Size size = pixmap->GetBufferSize();
  va_attrib_extbuf.width = size.width();
  va_attrib_extbuf.height = size.height();

  size_t num_fds = pixmap->GetDmaBufFdCount();
  size_t num_planes =
      gfx::NumberOfPlanesForBufferFormat(pixmap->GetBufferFormat());
  if (num_fds == 0 || num_fds > num_planes) {
    LOG(ERROR) << "Invalid number of dmabuf fds: " << num_fds
               << " , planes: " << num_planes;
    return nullptr;
  }

  for (size_t i = 0; i < num_planes; ++i) {
    va_attrib_extbuf.pitches[i] = pixmap->GetDmaBufPitch(i);
    va_attrib_extbuf.offsets[i] = pixmap->GetDmaBufOffset(i);
    DVLOG(4) << "plane " << i << ": pitch: " << va_attrib_extbuf.pitches[i]
             << " offset: " << va_attrib_extbuf.offsets[i];
  }
  va_attrib_extbuf.num_planes = num_planes;

  std::vector<unsigned long> fds(num_fds);
  for (size_t i = 0; i < num_fds; ++i) {
    int dmabuf_fd = pixmap->GetDmaBufFd(i);
    if (dmabuf_fd < 0) {
      LOG(ERROR) << "Failed to get dmabuf from an Ozone NativePixmap";
      return nullptr;
    }
    fds[i] = dmabuf_fd;
  }
  va_attrib_extbuf.buffers = fds.data();
  va_attrib_extbuf.num_buffers = fds.size();

  va_attrib_extbuf.flags = 0;
  va_attrib_extbuf.private_data = NULL;

  std::vector<VASurfaceAttrib> va_attribs(2);

  va_attribs[0].type = VASurfaceAttribMemoryType;
  va_attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[0].value.type = VAGenericValueTypeInteger;
  va_attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;

  va_attribs[1].type = VASurfaceAttribExternalBufferDescriptor;
  va_attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
  va_attribs[1].value.type = VAGenericValueTypePointer;
  va_attribs[1].value.value.p = &va_attrib_extbuf;

  const unsigned int va_format =
      BufferFormatToVARTFormat(pixmap->GetBufferFormat());

  VASurfaceID va_surface_id = VA_INVALID_ID;
  {
    base::AutoLock auto_lock(*va_lock_);
    VAStatus va_res =
        vaCreateSurfaces(va_display_, va_format, size.width(), size.height(),
                         &va_surface_id, 1, &va_attribs[0], va_attribs.size());
    VA_SUCCESS_OR_RETURN(va_res, "Failed to create unowned VASurface", nullptr);
  }

  // It's safe to use Unretained() here, because the caller takes care of the
  // destruction order. All the surfaces will be destroyed before VaapiWrapper.
  return new VASurface(
      va_surface_id, size, va_format,
      base::Bind(&VaapiWrapper::DestroySurface, base::Unretained(this)));
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                const void* buffer) {
  DCHECK_LT(va_buffer_type, VABufferTypeMax);
  DCHECK(buffer);
  base::AutoLock auto_lock(*va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(va_display_, va_context_id_, va_buffer_type,
                                   size, 1, nullptr, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  ScopedVABufferMapping mapping(
      va_lock_, va_display_, buffer_id,
      base::BindOnce(base::IgnoreResult(&vaDestroyBuffer), va_display_));
  if (!mapping.IsValid())
    return false;

  // TODO(selcott): Investigate potentially faster alternatives to memcpy here
  // such as libyuv::CopyX and family.
  memcpy(mapping.data(), buffer, size);

  switch (va_buffer_type) {
    case VASliceParameterBufferType:
    case VASliceDataBufferType:
    case VAEncSliceParameterBufferType:
      pending_slice_bufs_.push_back(buffer_id);
      break;

    default:
      pending_va_bufs_.push_back(buffer_id);
      break;
  }

  return true;
}

bool VaapiWrapper::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType misc_param_type,
    size_t size,
    const void* buffer) {
  base::AutoLock auto_lock(*va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(
      va_display_, va_context_id_, VAEncMiscParameterBufferType,
      sizeof(VAEncMiscParameterBuffer) + size, 1, NULL, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  ScopedVABufferMapping mapping(
      va_lock_, va_display_, buffer_id,
      base::BindOnce(base::IgnoreResult(&vaDestroyBuffer), va_display_));
  if (!mapping.IsValid())
    return false;

  auto* params = reinterpret_cast<VAEncMiscParameterBuffer*>(mapping.data());
  params->type = misc_param_type;
  memcpy(params->data, buffer, size);

  pending_va_bufs_.push_back(buffer_id);
  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  base::AutoLock auto_lock(*va_lock_);

  for (const auto& pending_va_buf : pending_va_bufs_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_va_buf);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  for (const auto& pending_slice_buf : pending_slice_bufs_) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_slice_buf);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  pending_va_bufs_.clear();
  pending_slice_bufs_.clear();
}

bool VaapiWrapper::ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  bool result = Execute(va_surface_id);
  DestroyPendingBuffers();
  return result;
}

#if defined(USE_X11)
bool VaapiWrapper::PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                                        Pixmap x_pixmap,
                                        gfx::Size dest_size) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Put the data into an X Pixmap.
  va_res = vaPutSurface(va_display_,
                        va_surface_id,
                        x_pixmap,
                        0, 0, dest_size.width(), dest_size.height(),
                        0, 0, dest_size.width(), dest_size.height(),
                        NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, "Failed putting surface to pixmap", false);
  return true;
}
#endif  // USE_X11

std::unique_ptr<ScopedVAImage> VaapiWrapper::CreateVaImage(
    VASurfaceID va_surface_id,
    VAImageFormat* format,
    const gfx::Size& size) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", nullptr);

  auto scoped_image = std::make_unique<ScopedVAImage>(
      va_lock_, va_display_, va_surface_id, format, size);
  return scoped_image->IsValid() ? std::move(scoped_image) : nullptr;
}

bool VaapiWrapper::UploadVideoFrameToSurface(
    const scoped_refptr<VideoFrame>& frame,
    VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);

  VAImage image;
  VAStatus va_res = vaDeriveImage(va_display_, va_surface_id, &image);
  VA_SUCCESS_OR_RETURN(va_res, "vaDeriveImage failed", false);
  base::ScopedClosureRunner vaimage_deleter(
      base::Bind(&DestroyVAImage, va_display_, image));

  if (image.format.fourcc != VA_FOURCC_NV12) {
    LOG(ERROR) << "Unsupported image format: " << image.format.fourcc;
    return false;
  }

  if (gfx::Rect(image.width, image.height) < gfx::Rect(frame->coded_size())) {
    LOG(ERROR) << "Buffer too small to fit the frame.";
    return false;
  }

  ScopedVABufferMapping mapping(va_lock_, va_display_, image.buf);
  if (!mapping.IsValid())
    return false;
  uint8_t* image_ptr = static_cast<uint8_t*>(mapping.data());

  int ret = 0;
  {
    base::AutoUnlock auto_unlock(*va_lock_);
    switch (frame->format()) {
      case PIXEL_FORMAT_I420:
        ret = libyuv::I420ToNV12(frame->data(VideoFrame::kYPlane),
                                 frame->stride(VideoFrame::kYPlane),
                                 frame->data(VideoFrame::kUPlane),
                                 frame->stride(VideoFrame::kUPlane),
                                 frame->data(VideoFrame::kVPlane),
                                 frame->stride(VideoFrame::kVPlane),
                                 image_ptr + image.offsets[0], image.pitches[0],
                                 image_ptr + image.offsets[1], image.pitches[1],
                                 image.width, image.height);
        break;
      case PIXEL_FORMAT_NV12:
        libyuv::CopyPlane(frame->data(VideoFrame::kYPlane),
                          frame->stride(VideoFrame::kYPlane),
                          image_ptr + image.offsets[0], image.pitches[0],
                          image.width, image.height);
        libyuv::CopyPlane(frame->data(VideoFrame::kUVPlane),
                          frame->stride(VideoFrame::kUVPlane),
                          image_ptr + image.offsets[1], image.pitches[1],
                          image.width, image.height / 2);
        break;
      default:
        LOG(ERROR) << "Unsupported pixel format: " << frame->format();
        return false;
    }
  }
  return ret == 0;
}

bool VaapiWrapper::CreateCodedBuffer(size_t size, VABufferID* buffer_id) {
  base::AutoLock auto_lock(*va_lock_);
  VAStatus va_res =
      vaCreateBuffer(va_display_, va_context_id_, VAEncCodedBufferType, size, 1,
                     NULL, buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a coded buffer", false);

  const auto is_new_entry = coded_buffers_.insert(*buffer_id).second;
  DCHECK(is_new_entry);
  return true;
}

bool VaapiWrapper::DownloadFromCodedBuffer(VABufferID buffer_id,
                                           VASurfaceID sync_surface_id,
                                           uint8_t* target_ptr,
                                           size_t target_size,
                                           size_t* coded_data_size) {
  DCHECK(target_ptr);
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  ScopedVABufferMapping mapping(va_lock_, va_display_, buffer_id);
  if (!mapping.IsValid())
    return false;
  auto* buffer_segment =
      reinterpret_cast<VACodedBufferSegment*>(mapping.data());

  {
    base::AutoUnlock auto_unlock(*va_lock_);
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
      *coded_data_size += buffer_segment->size;
      target_size -= buffer_segment->size;

      buffer_segment =
          reinterpret_cast<VACodedBufferSegment*>(buffer_segment->next);
    }
  }

  return buffer_segment == nullptr;
}

bool VaapiWrapper::DownloadAndDestroyCodedBuffer(VABufferID buffer_id,
                                                 VASurfaceID sync_surface_id,
                                                 uint8_t* target_ptr,
                                                 size_t target_size,
                                                 size_t* coded_data_size) {
  bool result = DownloadFromCodedBuffer(buffer_id, sync_surface_id, target_ptr,
                                        target_size, coded_data_size);

  VAStatus va_res = vaDestroyBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  const auto was_found = coded_buffers_.erase(buffer_id);
  DCHECK(was_found);

  return result;
}

void VaapiWrapper::DestroyCodedBuffers() {
  base::AutoLock auto_lock(*va_lock_);

  for (std::set<VABufferID>::const_iterator iter = coded_buffers_.begin();
       iter != coded_buffers_.end(); ++iter) {
    VAStatus va_res = vaDestroyBuffer(va_display_, *iter);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  coded_buffers_.clear();
}

bool VaapiWrapper::BlitSurface(
    const scoped_refptr<VASurface>& va_surface_src,
    const scoped_refptr<VASurface>& va_surface_dest) {
  base::AutoLock auto_lock(*va_lock_);

  // Initialize the post processing engine if not already done.
  if (va_vpp_buffer_id_ == VA_INVALID_ID) {
    if (!InitializeVpp_Locked())
      return false;
  }

  {
    ScopedVABufferMapping mapping(va_lock_, va_display_, va_vpp_buffer_id_);
    if (!mapping.IsValid())
      return false;
    auto* pipeline_param =
        reinterpret_cast<VAProcPipelineParameterBuffer*>(mapping.data());

    memset(pipeline_param, 0, sizeof *pipeline_param);
    const gfx::Size& src_size = va_surface_src->size();
    const gfx::Size& dest_size = va_surface_dest->size();

    VARectangle input_region;
    input_region.x = input_region.y = 0;
    input_region.width = src_size.width();
    input_region.height = src_size.height();
    pipeline_param->surface_region = &input_region;
    pipeline_param->surface = va_surface_src->id();
    pipeline_param->surface_color_standard = VAProcColorStandardNone;

    VARectangle output_region;
    output_region.x = output_region.y = 0;
    output_region.width = dest_size.width();
    output_region.height = dest_size.height();
    pipeline_param->output_region = &output_region;
    pipeline_param->output_background_color = 0xff000000;
    pipeline_param->output_color_standard = VAProcColorStandardNone;
    pipeline_param->filter_flags = VA_FILTER_SCALING_DEFAULT;

    VA_SUCCESS_OR_RETURN(mapping.Unmap(), "Couldn't unmap vpp buffer", false);
  }

  VA_SUCCESS_OR_RETURN(
      vaBeginPicture(va_display_, va_vpp_context_id_, va_surface_dest->id()),
      "Couldn't begin picture", false);

  VA_SUCCESS_OR_RETURN(
      vaRenderPicture(va_display_, va_vpp_context_id_, &va_vpp_buffer_id_, 1),
      "Couldn't render picture", false);

  VA_SUCCESS_OR_RETURN(vaEndPicture(va_display_, va_vpp_context_id_),
                       "Couldn't end picture", false);

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

VaapiWrapper::VaapiWrapper()
    : va_surface_format_(0),
      va_display_(NULL),
      va_config_id_(VA_INVALID_ID),
      va_context_id_(VA_INVALID_ID),
      va_vpp_config_id_(VA_INVALID_ID),
      va_vpp_context_id_(VA_INVALID_ID),
      va_vpp_buffer_id_(VA_INVALID_ID) {
  va_lock_ = VADisplayState::Get()->va_lock();
}

VaapiWrapper::~VaapiWrapper() {
  DestroyPendingBuffers();
  DestroyCodedBuffers();
  DestroySurfaces();
  DeinitializeVpp();
  Deinitialize();
}

bool VaapiWrapper::Initialize(CodecMode mode, VAProfile va_profile) {
  TryToSetVADisplayAttributeToLocalGPU();

  VAEntrypoint entrypoint = GetVaEntryPoint(mode, va_profile);
  std::vector<VAConfigAttrib> required_attribs =
      GetRequiredAttribs(mode, va_profile);
  base::AutoLock auto_lock(*va_lock_);
  VAStatus va_res =
      vaCreateConfig(va_display_, va_profile, entrypoint, &required_attribs[0],
                     required_attribs.size(), &va_config_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  return true;
}

void VaapiWrapper::Deinitialize() {
  base::AutoLock auto_lock(*va_lock_);

  if (va_config_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyConfig(va_display_, va_config_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyConfig failed");
  }

  VAStatus va_res = VA_STATUS_SUCCESS;
  VADisplayState::Get()->Deinitialize(&va_res);
  VA_LOG_ON_ERROR(va_res, "vaTerminate failed");

  va_config_id_ = VA_INVALID_ID;
  va_display_ = NULL;
}

bool VaapiWrapper::VaInitialize(const base::Closure& report_error_to_uma_cb) {
  report_error_to_uma_cb_ = report_error_to_uma_cb;
  {
    base::AutoLock auto_lock(*va_lock_);
    if (!VADisplayState::Get()->Initialize())
      return false;
  }

  va_display_ = VADisplayState::Get()->va_display();
  DCHECK(va_display_) << "VADisplayState hasn't been properly Initialize()d";
  return true;
}

void VaapiWrapper::DestroySurfaces_Locked() {
  va_lock_->AssertAcquired();

  if (va_context_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyContext failed");
  }

  if (!va_surface_ids_.empty()) {
    VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_ids_[0],
                                        va_surface_ids_.size());
    VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces failed");
  }

  va_surface_ids_.clear();
  va_context_id_ = VA_INVALID_ID;
  va_surface_format_ = 0;
}

void VaapiWrapper::DestroySurface(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(*va_lock_);

  VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_id, 1);
  VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces on surface failed");
}

bool VaapiWrapper::InitializeVpp_Locked() {
  va_lock_->AssertAcquired();

  VA_SUCCESS_OR_RETURN(
      vaCreateConfig(va_display_, VAProfileNone, VAEntrypointVideoProc, NULL, 0,
                     &va_vpp_config_id_),
      "Couldn't create config", false);

  // The size of the picture for the context is irrelevant in the case
  // of the VPP, just passing 1x1.
  VA_SUCCESS_OR_RETURN(vaCreateContext(va_display_, va_vpp_config_id_, 1, 1, 0,
                                       NULL, 0, &va_vpp_context_id_),
                       "Couldn't create context", false);

  VA_SUCCESS_OR_RETURN(vaCreateBuffer(va_display_, va_vpp_context_id_,
                                      VAProcPipelineParameterBufferType,
                                      sizeof(VAProcPipelineParameterBuffer), 1,
                                      NULL, &va_vpp_buffer_id_),
                       "Couldn't create buffer", false);

  return true;
}

void VaapiWrapper::DeinitializeVpp() {
  base::AutoLock auto_lock(*va_lock_);

  if (va_vpp_buffer_id_ != VA_INVALID_ID) {
    vaDestroyBuffer(va_display_, va_vpp_buffer_id_);
    va_vpp_buffer_id_ = VA_INVALID_ID;
  }
  if (va_vpp_context_id_ != VA_INVALID_ID) {
    vaDestroyContext(va_display_, va_vpp_context_id_);
    va_vpp_context_id_ = VA_INVALID_ID;
  }
  if (va_vpp_config_id_ != VA_INVALID_ID) {
    vaDestroyConfig(va_display_, va_vpp_config_id_);
    va_vpp_config_id_ = VA_INVALID_ID;
  }
}

bool VaapiWrapper::Execute(VASurfaceID va_surface_id) {
  TRACE_EVENT0("media,gpu", "VaapiWrapper::Execute");
  base::AutoLock auto_lock(*va_lock_);

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_bufs_.size();
  DVLOG(4) << "Pending slice bufs to commit: " << pending_slice_bufs_.size();
  DVLOG(4) << "Target VA surface " << va_surface_id;

  // Get ready to execute for given surface.
  VAStatus va_res = vaBeginPicture(va_display_, va_context_id_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaBeginPicture failed", false);

  if (pending_va_bufs_.size() > 0) {
    // Commit parameter and slice buffers.
    va_res = vaRenderPicture(va_display_, va_context_id_, &pending_va_bufs_[0],
                             pending_va_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for va_bufs failed", false);
  }

  if (pending_slice_bufs_.size() > 0) {
    va_res =
        vaRenderPicture(va_display_, va_context_id_, &pending_slice_bufs_[0],
                        pending_slice_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for slices failed", false);
  }

  // Instruct HW codec to start processing committed buffers.
  // Does not block and the job is not finished after this returns.
  va_res = vaEndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaEndPicture failed", false);

  return true;
}

void VaapiWrapper::TryToSetVADisplayAttributeToLocalGPU() {
  base::AutoLock auto_lock(*va_lock_);
  VADisplayAttribute item = {VADisplayAttribRenderMode,
                             1,   // At least support '_LOCAL_OVERLAY'.
                             -1,  // The maximum possible support 'ALL'.
                             VA_RENDER_MODE_LOCAL_GPU,
                             VA_DISPLAY_ATTRIB_SETTABLE};

  VAStatus va_res = vaSetDisplayAttributes(va_display_, &item, 1);
  if (va_res != VA_STATUS_SUCCESS)
    DVLOG(2) << "vaSetDisplayAttributes unsupported, ignoring by default.";
}

}  // namespace media
