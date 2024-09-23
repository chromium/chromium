// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fake_video_capture_device_factory.h"

#include <array>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/media_switches.h"
#include "media/base/video_facing.h"
#include "media/capture/capture_switches.h"

namespace {

// Cap the frame rate command line input to reasonable values.
static const float kFakeCaptureMinFrameRate = 5.0f;
static const float kFakeCaptureMaxFrameRate = 60.0f;

// Cap the device count command line input to reasonable values.
static const int kFakeCaptureMinDeviceCount = 0;
static const int kFakeCaptureMaxDeviceCount = 10;
static const int kDefaultDeviceCount = 1;

constexpr char kDefaultDeviceIdMask[] = "/dev/video%d";
static const media::FakeVideoCaptureDevice::DeliveryMode kDefaultDeliveryMode =
    media::FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
static constexpr std::array<gfx::Size, 6> kDefaultResolutions{
    {gfx::Size(96, 96), gfx::Size(320, 240), gfx::Size(640, 480),
     gfx::Size(1280, 720), gfx::Size(1920, 1080), gfx::Size(3840, 2160)}};
static constexpr std::array<float, 1> kDefaultFrameRates{{20.0f}};

static const double kInitialPan = 100.0;
static const double kInitialTilt = 100.0;
static const double kInitialZoom = 100.0;
static const double kInitialExposureTime = 50.0;
static const double kInitialFocusDistance = 50.0;

static const media::VideoPixelFormat kSupportedPixelFormats[] = {
    media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_Y16,
    media::PIXEL_FORMAT_MJPEG, media::PIXEL_FORMAT_NV12};

template <typename TElement, size_t TSize>
std::vector<TElement> ArrayToVector(const std::array<TElement, TSize>& arr) {
  return std::vector<TElement>(arr.begin(), arr.end());
}

media::VideoPixelFormat GetPixelFormatFromDeviceIndex(int device_index) {
  if (device_index == 1)
    return media::PIXEL_FORMAT_Y16;
  if (device_index == 2)
    return media::PIXEL_FORMAT_MJPEG;
#if BUILDFLAG(IS_WIN)
  if (media::IsMediaFoundationD3D11VideoCaptureEnabled() &&
      switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    return media::PIXEL_FORMAT_NV12;
  } else {
    return media::PIXEL_FORMAT_I420;
  }
#else
  return media::PIXEL_FORMAT_I420;
#endif
}

void AppendAllCombinationsToFormatsContainer(
    const std::vector<media::VideoPixelFormat>& pixel_formats,
    const std::vector<gfx::Size>& resolutions,
    const std::vector<float>& frame_rates,
    media::VideoCaptureFormats* output) {
  for (const auto& pixel_format : pixel_formats) {
    for (const auto& resolution : resolutions) {
      for (const auto& frame_rate : frame_rates)
        output->emplace_back(resolution, frame_rate, pixel_format);
    }
  }
}

class ErrorFakeDevice : public media::VideoCaptureDevice {
 public:
  // VideoCaptureDevice implementation.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override {
    client->OnError(media::VideoCaptureError::
                        kErrorFakeDeviceIntentionallyEmittingErrorEvent,
                    FROM_HERE, "Device has no supported formats.");
  }

  void StopAndDeAllocate() override {}
  void GetPhotoState(GetPhotoStateCallback callback) override {}
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override {}
  void TakePhoto(TakePhotoCallback callback) override {}
};

}  // anonymous namespace

namespace media {

FakeVideoCaptureDeviceSettings::FakeVideoCaptureDeviceSettings() = default;

FakeVideoCaptureDeviceSettings::~FakeVideoCaptureDeviceSettings() = default;

FakeVideoCaptureDeviceSettings::FakeVideoCaptureDeviceSettings(
    const FakeVideoCaptureDeviceSettings& other) = default;

constexpr char
    FakeVideoCaptureDeviceFactory::kDeviceConfigForGetPhotoStateFails[];
constexpr char
    FakeVideoCaptureDeviceFactory::kDeviceConfigForSetPhotoOptionsFails[];
constexpr char FakeVideoCaptureDeviceFactory::kDeviceConfigForTakePhotoFails[];

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory() {
  // The default |devices_config_| is the one obtained from an empty options
  // string.
  ParseFakeDevicesConfigFromOptionsString("", &devices_config_);
#if BUILDFLAG(IS_WIN)
  if (media::IsMediaFoundationD3D11VideoCaptureEnabled() &&
      switches::IsVideoCaptureUseGpuMemoryBufferEnabled()) {
    dxgi_device_manager_ = DXGIDeviceManager::Create(luid_);
  }
#endif
}

FakeVideoCaptureDeviceFactory::~FakeVideoCaptureDeviceFactory() = default;

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateDeviceWithSettings(
    const FakeVideoCaptureDeviceSettings& settings,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support) {
  if (settings.supported_formats.empty())
    return CreateErrorDevice();

  for (const auto& entry : settings.supported_formats) {
    bool pixel_format_supported = false;
    for (const auto& supported_pixel_format : kSupportedPixelFormats) {
      if (entry.pixel_format == supported_pixel_format) {
        pixel_format_supported = true;
        break;
      }
    }
    if (!pixel_format_supported) {
      DLOG(ERROR) << "Requested an unsupported pixel format "
                  << VideoPixelFormatToString(entry.pixel_format);
      return nullptr;
    }
  }

  const VideoCaptureFormat& initial_format = settings.supported_formats.front();
  auto device_state = std::make_unique<FakeDeviceState>(
      kInitialPan, kInitialTilt, kInitialZoom, kInitialExposureTime,
      kInitialFocusDistance, initial_format.frame_rate,
      initial_format.pixel_format);

  auto photo_frame_painter = std::make_unique<PacmanFramePainter>(
      PacmanFramePainter::Format::SK_N32, device_state.get());
  auto photo_device = std::make_unique<FakePhotoDevice>(
      std::move(photo_frame_painter), device_state.get(),
      settings.photo_device_config);

  return std::make_unique<FakeVideoCaptureDevice>(
      settings.supported_formats,
      std::make_unique<FrameDelivererFactory>(
          settings.delivery_mode, device_state.get(), std::move(gmb_support)),
      std::move(photo_device), std::move(device_state));
}

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateDeviceWithDefaultResolutions(
    VideoPixelFormat pixel_format,
    FakeVideoCaptureDevice::DeliveryMode delivery_mode,
    float frame_rate,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support) {
  FakeVideoCaptureDeviceSettings settings;
  settings.delivery_mode = delivery_mode;
  for (const gfx::Size& resolution : kDefaultResolutions)
    settings.supported_formats.emplace_back(resolution, frame_rate,
                                            pixel_format);
  return CreateDeviceWithSettings(settings, std::move(gmb_support));
}

// static
std::unique_ptr<VideoCaptureDevice>
FakeVideoCaptureDeviceFactory::CreateErrorDevice() {
  return std::make_unique<ErrorFakeDevice>();
}

void FakeVideoCaptureDeviceFactory::SetToDefaultDevicesConfig(
    int device_count) {
  devices_config_.clear();
  ParseFakeDevicesConfigFromOptionsString(
      base::StringPrintf("device-count=%d", device_count), &devices_config_);
}

void FakeVideoCaptureDeviceFactory::SetToCustomDevicesConfig(
    const std::vector<FakeVideoCaptureDeviceSettings>& config) {
  devices_config_ = config;
}

VideoCaptureErrorOrDevice FakeVideoCaptureDeviceFactory::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto& entry : devices_config_) {
    if (device_descriptor.device_id != entry.device_id)
      continue;
    auto device = CreateDeviceWithSettings(entry);
    return device ? VideoCaptureErrorOrDevice(std::move(device))
                  : VideoCaptureErrorOrDevice(
                        VideoCaptureError::
                            kErrorFakeDeviceIntentionallyEmittingErrorEvent);
  }
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kErrorFakeDeviceIntentionallyEmittingErrorEvent);
}

void FakeVideoCaptureDeviceFactory::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<VideoCaptureDeviceInfo> devices_info;

  int entry_index = 0;
  for (const auto& entry : devices_config_) {
    VideoCaptureApi api =
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
#elif BUILDFLAG(IS_IOS)
        VideoCaptureApi::UNKNOWN;
#elif BUILDFLAG(IS_MAC)
        VideoCaptureApi::MACOSX_AVFOUNDATION;
#elif BUILDFLAG(IS_WIN)
        VideoCaptureApi::WIN_DIRECT_SHOW;
#elif BUILDFLAG(IS_ANDROID)
        VideoCaptureApi::ANDROID_API2_LEGACY;
#elif BUILDFLAG(IS_FUCHSIA)
        VideoCaptureApi::FUCHSIA_CAMERA3;
#else
#error Unsupported platform
#endif

    devices_info.emplace_back(VideoCaptureDeviceDescriptor(
        base::StringPrintf("fake_device_%d", entry_index), entry.device_id,
        /*model_id=*/std::string(), api,
        entry.photo_device_config.control_support,
        VideoCaptureTransportType::OTHER_TRANSPORT,
        media::MEDIA_VIDEO_FACING_NONE, entry.availability));

    devices_info.back().supported_formats =
        GetSupportedFormats(entry.device_id);
    entry_index++;
  }

  std::move(callback).Run(std::move(devices_info));
}

VideoCaptureFormats FakeVideoCaptureDeviceFactory::GetSupportedFormats(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VideoCaptureFormats supported_formats;
  for (const auto& entry : devices_config_) {
    if (device_id != entry.device_id)
      continue;
    supported_formats.insert(supported_formats.end(),
                             entry.supported_formats.begin(),
                             entry.supported_formats.end());
  }

  return supported_formats;
}

// static
void FakeVideoCaptureDeviceFactory::ParseFakeDevicesConfigFromOptionsString(
    const std::string options_string,
    std::vector<FakeVideoCaptureDeviceSettings>* config) {
  base::StringTokenizer option_tokenizer(options_string, ", ");
  option_tokenizer.set_quote_chars("\"");

  FakeVideoCaptureDevice::DeliveryMode delivery_mode = kDefaultDeliveryMode;
  std::vector<gfx::Size> resolutions = ArrayToVector(kDefaultResolutions);
  std::vector<float> frame_rates = ArrayToVector(kDefaultFrameRates);
  int device_count = kDefaultDeviceCount;
  FakePhotoDeviceConfig photo_device_config;
  FakeVideoCaptureDevice::DisplayMediaType display_media_type =
      FakeVideoCaptureDevice::DisplayMediaType::ANY;

  while (option_tokenizer.GetNext()) {
    std::vector<std::string_view> param = base::SplitStringPiece(
        option_tokenizer.token_piece(), "=", base::TRIM_WHITESPACE,
        base::SPLIT_WANT_NONEMPTY);

    if (param.size() != 2u) {
      LOG(WARNING) << "Forget a value '" << options_string
                   << "'? Use name=value for "
                   << switches::kUseFakeDeviceForMediaStream << ".";
      return;
    }

    if (base::EqualsCaseInsensitiveASCII(param.front(), "ownership") &&
        base::EqualsCaseInsensitiveASCII(param.back(), "client")) {
      delivery_mode =
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS;
    } else if (base::EqualsCaseInsensitiveASCII(param.front(), "fps")) {
      double parsed_fps = 0;
      if (base::StringToDouble(param.back(), &parsed_fps)) {
        float capped_frame_rate =
            std::max(kFakeCaptureMinFrameRate, static_cast<float>(parsed_fps));
        capped_frame_rate =
            std::min(kFakeCaptureMaxFrameRate, capped_frame_rate);
        frame_rates.clear();
        frame_rates.push_back(capped_frame_rate);
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "device-count")) {
      unsigned int count = 0;
      if (base::StringToUint(param.back(), &count)) {
        device_count = std::min(
            kFakeCaptureMaxDeviceCount,
            std::max(kFakeCaptureMinDeviceCount, static_cast<int>(count)));
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(), "config")) {
      const int device_index = 0;
      std::vector<VideoPixelFormat> pixel_formats;
      pixel_formats.push_back(GetPixelFormatFromDeviceIndex(device_index));
      FakeVideoCaptureDeviceSettings settings;
      settings.delivery_mode = delivery_mode;
      settings.device_id =
          base::StringPrintf(kDefaultDeviceIdMask, device_index);
      AppendAllCombinationsToFormatsContainer(
          pixel_formats, resolutions, frame_rates, &settings.supported_formats);

      if (param.back() == kDeviceConfigForGetPhotoStateFails) {
        settings.photo_device_config.should_fail_get_photo_capabilities = true;
        config->push_back(settings);
        return;
      }
      if (param.back() == kDeviceConfigForSetPhotoOptionsFails) {
        settings.photo_device_config.should_fail_set_photo_options = true;
        config->push_back(settings);
        return;
      }
      if (param.back() == kDeviceConfigForTakePhotoFails) {
        settings.photo_device_config.should_fail_take_photo = true;
        config->push_back(settings);
        return;
      }
      LOG(WARNING) << "Unknown config " << param.back();
      return;
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "display-media-type")) {
      if (base::EqualsCaseInsensitiveASCII(param.back(), "any")) {
        display_media_type = FakeVideoCaptureDevice::DisplayMediaType::ANY;
      } else if (base::EqualsCaseInsensitiveASCII(param.back(), "monitor")) {
        display_media_type = FakeVideoCaptureDevice::DisplayMediaType::MONITOR;
      } else if (base::EqualsCaseInsensitiveASCII(param.back(), "window")) {
        display_media_type = FakeVideoCaptureDevice::DisplayMediaType::WINDOW;
      } else if (base::EqualsCaseInsensitiveASCII(param.back(), "browser")) {
        display_media_type = FakeVideoCaptureDevice::DisplayMediaType::BROWSER;
      }
    } else if (base::EqualsCaseInsensitiveASCII(param.front(),
                                                "hardware-support")) {
      photo_device_config.control_support = VideoCaptureControlSupport();
      if (!base::EqualsCaseInsensitiveASCII(param.back(), "none")) {
        for (const std::string& support :
             base::SplitString(param.back(), "-", base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY)) {
          if (base::EqualsCaseInsensitiveASCII(support, "pan"))
            photo_device_config.control_support.pan = true;
          else if (base::EqualsCaseInsensitiveASCII(support, "tilt"))
            photo_device_config.control_support.tilt = true;
          else if (base::EqualsCaseInsensitiveASCII(support, "zoom"))
            photo_device_config.control_support.zoom = true;
          else
            LOG(WARNING) << "Unsupported hardware support " << support;
        }
      }
    }
  }

  for (int device_index = 0; device_index < device_count; device_index++) {
    std::vector<VideoPixelFormat> pixel_formats;
    pixel_formats.push_back(GetPixelFormatFromDeviceIndex(device_index));
    FakeVideoCaptureDeviceSettings settings;
    settings.delivery_mode = delivery_mode;
    settings.device_id = base::StringPrintf(kDefaultDeviceIdMask, device_index);
    AppendAllCombinationsToFormatsContainer(
        pixel_formats, resolutions, frame_rates, &settings.supported_formats);
    settings.photo_device_config = photo_device_config;
    settings.display_media_type = display_media_type;
    config->push_back(settings);
  }
}

#if BUILDFLAG(IS_WIN)
void FakeVideoCaptureDeviceFactory::OnGpuInfoUpdate(const CHROME_LUID& luid) {
  luid_ = luid;
  if (dxgi_device_manager_) {
    dxgi_device_manager_->OnGpuInfoUpdate(luid_);
  }
}

scoped_refptr<DXGIDeviceManager>
FakeVideoCaptureDeviceFactory::GetDxgiDeviceManager() {
  return dxgi_device_manager_;
}
#endif

}  // namespace media
