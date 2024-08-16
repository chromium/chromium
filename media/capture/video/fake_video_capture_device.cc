// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/fake_video_capture_device.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/gpu_memory_buffer_utils.h"
#include "skia/ext/font_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkFont.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {

namespace {

// Sweep at 600 deg/sec.
static const float kPacmanAngularVelocity = 600;
// Beep every 500 ms.
static const int kBeepInterval = 500;
// Gradient travels from bottom to top in 5 seconds.
static const float kGradientFrequency = 1.f / 5;

static const double kMinPan = 100.0;
static const double kMaxPan = 400.0;
static const double kPanStep = 1.0;

static const double kMinTilt = 100.0;
static const double kMaxTilt = 400.0;
static const double kTiltStep = 1.0;

static const double kMinZoom = 100.0;
static const double kMaxZoom = 400.0;
static const double kZoomStep = 1.0;

static const double kMinExposureTime = 10.0;
static const double kMaxExposureTime = 100.0;
static const double kExposureTimeStep = 5.0;

static const double kMinFocusDistance = 10.0;
static const double kMaxFocusDistance = 100.0;
static const double kFocusDistanceStep = 5.0;

// Larger int means better.
enum class PixelFormatMatchType : int {
  INCOMPATIBLE = 0,
  SUPPORTED_THROUGH_CONVERSION = 1,
  EXACT = 2
};

PixelFormatMatchType DetermineFormatMatchType(
    VideoPixelFormat supported_format,
    VideoPixelFormat requested_format) {
  if (requested_format == PIXEL_FORMAT_I420 &&
      supported_format == PIXEL_FORMAT_MJPEG) {
    return PixelFormatMatchType::SUPPORTED_THROUGH_CONVERSION;
  }
  return (requested_format == supported_format)
             ? PixelFormatMatchType::EXACT
             : PixelFormatMatchType::INCOMPATIBLE;
}

VideoCaptureFormat FindClosestSupportedFormat(
    const VideoCaptureFormat& requested_format,
    const VideoCaptureFormats& supported_formats,
    bool video_capture_use_gmb) {
  DCHECK(!supported_formats.empty());
  int best_index = 0;
  PixelFormatMatchType best_format_match = PixelFormatMatchType::INCOMPATIBLE;
  int best_width_mismatch = std::numeric_limits<int>::max();
  float best_frame_rate_mismatch = std::numeric_limits<float>::max();
  for (int i = 0; i < static_cast<int>(supported_formats.size()); i++) {
    const auto& supported_format = supported_formats[i];
    PixelFormatMatchType current_format_match = DetermineFormatMatchType(
        supported_format.pixel_format, requested_format.pixel_format);
    if (current_format_match < best_format_match) {
      continue;
    }
    if (supported_format.frame_size.width() <
        requested_format.frame_size.width())
      continue;
    const int current_width_mismatch = supported_format.frame_size.width() -
                                       requested_format.frame_size.width();
    if (current_width_mismatch > best_width_mismatch)
      continue;
    const float current_frame_rate_mismatch =
        std::abs(supported_format.frame_rate - requested_format.frame_rate);
    if (current_width_mismatch < best_width_mismatch) {
      best_width_mismatch = current_width_mismatch;
      best_frame_rate_mismatch = current_frame_rate_mismatch;
      best_index = i;
      continue;
    }
    DCHECK_EQ(best_frame_rate_mismatch, current_frame_rate_mismatch);
    if (current_frame_rate_mismatch < best_frame_rate_mismatch) {
      best_frame_rate_mismatch = current_frame_rate_mismatch;
      best_index = i;
    }
  }

  VideoCaptureFormat format = supported_formats[best_index];
  // We use NV12 as the underlying opaque pixel format for GpuMemoryBuffer
  // frames.
  if (video_capture_use_gmb) {
    format.pixel_format = PIXEL_FORMAT_NV12;
  }

  return format;
}

gfx::ColorSpace GetDefaultColorSpace(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV12A:
    case PIXEL_FORMAT_NV16:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_NV24:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_P010LE:
    case PIXEL_FORMAT_P210LE:
    case PIXEL_FORMAT_P410LE:
    case PIXEL_FORMAT_Y16:
    case PIXEL_FORMAT_I422A:
    case PIXEL_FORMAT_I444A:
    case PIXEL_FORMAT_YUV420AP10:
    case PIXEL_FORMAT_YUV422AP10:
    case PIXEL_FORMAT_YUV444AP10:
      return gfx::ColorSpace::CreateREC601();
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_XR30:
    case PIXEL_FORMAT_XB30:
    case PIXEL_FORMAT_BGRA:
    case PIXEL_FORMAT_RGBAF16:
      return gfx::ColorSpace::CreateSRGB();
    case PIXEL_FORMAT_UNKNOWN:
      return gfx::ColorSpace();
  }
  return gfx::ColorSpace();
}

}  // anonymous namespace

FakeDeviceState::FakeDeviceState(double pan,
                                 double tilt,
                                 double zoom,
                                 double exposure_time,
                                 double focus_distance,
                                 float frame_rate,
                                 VideoPixelFormat pixel_format)
    : pan(pan),
      tilt(tilt),
      zoom(zoom),
      exposure_time(exposure_time),
      focus_distance(focus_distance),
      format(gfx::Size(), frame_rate, pixel_format) {
  exposure_mode = (exposure_time >= 0.0f) ? mojom::MeteringMode::MANUAL
                                          : mojom::MeteringMode::CONTINUOUS;
  focus_mode = (focus_distance >= 0.0f) ? mojom::MeteringMode::MANUAL
                                        : mojom::MeteringMode::CONTINUOUS;
}

// Paints and delivers frames to a client, which is set via Initialize().
class FrameDeliverer {
 public:
  FrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter)
      : frame_painter_(std::move(frame_painter)) {}
  virtual ~FrameDeliverer() = default;
  virtual void Initialize(VideoPixelFormat pixel_format,
                          std::unique_ptr<VideoCaptureDevice::Client> client,
                          const FakeDeviceState* device_state) {
    client_ = std::move(client);
    device_state_ = device_state;
    client_->OnStarted();
  }
  virtual void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) = 0;

 protected:
  base::TimeDelta CalculateTimeSinceFirstInvocation(base::TimeTicks now) {
    if (first_ref_time_.is_null())
      first_ref_time_ = now;
    return now - first_ref_time_;
  }

  PacmanFramePainter* frame_painter() { return frame_painter_.get(); }
  const FakeDeviceState* device_state() { return device_state_; }
  VideoCaptureDevice::Client* client() { return client_.get(); }

 private:
  const std::unique_ptr<PacmanFramePainter> frame_painter_;
  raw_ptr<const FakeDeviceState> device_state_ = nullptr;
  std::unique_ptr<VideoCaptureDevice::Client> client_;
  base::TimeTicks first_ref_time_;
};

// Delivers frames using its own buffers via OnIncomingCapturedData().
class OwnBufferFrameDeliverer : public FrameDeliverer {
 public:
  OwnBufferFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~OwnBufferFrameDeliverer() override;

  // Implementation of FrameDeliverer
  void Initialize(VideoPixelFormat pixel_format,
                  std::unique_ptr<VideoCaptureDevice::Client> client,
                  const FakeDeviceState* device_state) override;
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;

 private:
  base::HeapArray<uint8_t> buffer_;
};

// Delivers frames using buffers provided by the client via
// OnIncomingCapturedBuffer().
class ClientBufferFrameDeliverer : public FrameDeliverer {
 public:
  ClientBufferFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~ClientBufferFrameDeliverer() override;

  // Implementation of FrameDeliverer
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;
};

class JpegEncodingFrameDeliverer : public FrameDeliverer {
 public:
  JpegEncodingFrameDeliverer(std::unique_ptr<PacmanFramePainter> frame_painter);
  ~JpegEncodingFrameDeliverer() override;

  // Implementation of FrameDeliveryStrategy
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;

 private:
  std::vector<uint8_t> sk_n32_buffer_;
  std::vector<unsigned char> jpeg_buffer_;
};

// Delivers frames using GpuMemoryBuffer buffers reserved from the client buffer
// pool via OnIncomingCapturedBuffer();
class GpuMemoryBufferFrameDeliverer : public FrameDeliverer {
 public:
  GpuMemoryBufferFrameDeliverer(
      std::unique_ptr<PacmanFramePainter> frame_painter,
      gpu::GpuMemoryBufferSupport* gmb_support);
  ~GpuMemoryBufferFrameDeliverer() override;

  // Implementation of FrameDeliveryStrategy
  void PaintAndDeliverNextFrame(base::TimeDelta timestamp_to_paint) override;

 private:
  raw_ptr<gpu::GpuMemoryBufferSupport> gmb_support_;
};

FrameDelivererFactory::FrameDelivererFactory(
    FakeVideoCaptureDevice::DeliveryMode delivery_mode,
    const FakeDeviceState* device_state,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support)
    : delivery_mode_(delivery_mode),
      device_state_(device_state),
      gmb_support_(gmb_support
                       ? std::move(gmb_support)
                       : std::make_unique<gpu::GpuMemoryBufferSupport>()) {}

FrameDelivererFactory::~FrameDelivererFactory() = default;

std::unique_ptr<FrameDeliverer> FrameDelivererFactory::CreateFrameDeliverer(
    const VideoCaptureFormat& format,
    bool video_capture_use_gmb) {
  PacmanFramePainter::Format painter_format;
  switch (format.pixel_format) {
    case PIXEL_FORMAT_I420:
      painter_format = PacmanFramePainter::Format::I420;
      break;
    case PIXEL_FORMAT_Y16:
      painter_format = PacmanFramePainter::Format::Y16;
      break;
    case PIXEL_FORMAT_MJPEG:
      painter_format = PacmanFramePainter::Format::SK_N32;
      break;
    case PIXEL_FORMAT_NV12:
      painter_format = PacmanFramePainter::Format::NV12;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      painter_format = PacmanFramePainter::Format::I420;
  }
  auto frame_painter =
      std::make_unique<PacmanFramePainter>(painter_format, device_state_);

  FakeVideoCaptureDevice::DeliveryMode delivery_mode = delivery_mode_;
  if (format.pixel_format == PIXEL_FORMAT_MJPEG &&
      delivery_mode_ ==
          FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS) {
    DLOG(WARNING) << "PIXEL_FORMAT_MJPEG cannot be used in combination with "
                  << "USE_CLIENT_PROVIDED_BUFFERS. Switching to "
                     "USE_DEVICE_INTERNAL_BUFFERS.";
    delivery_mode =
        FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS;
  }
  if (video_capture_use_gmb) {
    DLOG(INFO) << "Forcing GpuMemoryBufferFrameDeliverer";
    delivery_mode =
        FakeVideoCaptureDevice::DeliveryMode::USE_GPU_MEMORY_BUFFERS;
  }

  switch (delivery_mode) {
    case FakeVideoCaptureDevice::DeliveryMode::USE_DEVICE_INTERNAL_BUFFERS:
      if (format.pixel_format == PIXEL_FORMAT_MJPEG) {
        return std::make_unique<JpegEncodingFrameDeliverer>(
            std::move(frame_painter));
      } else {
        return std::make_unique<OwnBufferFrameDeliverer>(
            std::move(frame_painter));
      }
    case FakeVideoCaptureDevice::DeliveryMode::USE_CLIENT_PROVIDED_BUFFERS:
      return std::make_unique<ClientBufferFrameDeliverer>(
          std::move(frame_painter));
    case FakeVideoCaptureDevice::DeliveryMode::USE_GPU_MEMORY_BUFFERS:
      return std::make_unique<GpuMemoryBufferFrameDeliverer>(
          std::move(frame_painter), gmb_support_.get());
  }
  NOTREACHED();
}

PacmanFramePainter::PacmanFramePainter(Format pixel_format,
                                       const FakeDeviceState* fake_device_state)
    : pixel_format_(pixel_format), fake_device_state_(fake_device_state) {}

void PacmanFramePainter::PaintFrame(base::TimeDelta elapsed_time,
                                    uint8_t* target_buffer,
                                    int bytes_per_row) {
  DrawPacman(elapsed_time, target_buffer, bytes_per_row);
  DrawGradientSquares(elapsed_time, target_buffer, bytes_per_row);
}

// Starting from top left, -45 deg gradient.  Value at point (row, column) is
// calculated as (top_left_value + (row + column) * step) % MAX_VALUE, where
// step is MAX_VALUE / (width + height).  MAX_VALUE is 255 (for 8 bit per
// component) or 65535 for Y16.
// This is handy for pixel tests where we use the squares to verify rendering.
void PacmanFramePainter::DrawGradientSquares(base::TimeDelta elapsed_time,
                                             uint8_t* target_buffer,
                                             int bytes_per_row) {
  const int width = fake_device_state_->format.frame_size.width();
  const int height = fake_device_state_->format.frame_size.height();
  const int stride = (bytes_per_row == 0) ? width : bytes_per_row;

  const int side = width / 16;  // square side length.
  DCHECK(side);
  const gfx::Point squares[] = {{0, 0},
                                {width - side, 0},
                                {0, height - side},
                                {width - side, height - side}};
  const float start =
      fmod(65536 * elapsed_time.InSecondsF() * kGradientFrequency, 65536);
  const float color_step = 65535 / static_cast<float>(width + height);
  for (const auto& corner : squares) {
    for (int y = corner.y(); y < corner.y() + side; ++y) {
      for (int x = corner.x(); x < corner.x() + side; ++x) {
        const unsigned int value =
            static_cast<unsigned int>(start + (x + y) * color_step) & 0xFFFF;
        size_t offset = (y * stride) + x;
        switch (pixel_format_) {
          case Format::Y16:
            target_buffer[offset * sizeof(uint16_t)] = value & 0xFF;
            target_buffer[offset * sizeof(uint16_t) + 1] = value >> 8;
            break;
          case Format::SK_N32:
            target_buffer[offset * sizeof(uint32_t) + 1] = value >> 8;
            target_buffer[offset * sizeof(uint32_t) + 2] = value >> 8;
            target_buffer[offset * sizeof(uint32_t) + 3] = value >> 8;
            break;
          case Format::I420:
          case Format::NV12:
            // I420 and NV12 has the same Y plane dimension.
            target_buffer[offset] = value >> 8;
            break;
        }
      }
    }
  }
}

void PacmanFramePainter::DrawPacman(base::TimeDelta elapsed_time,
                                    uint8_t* target_buffer,
                                    int bytes_per_row) {
  const int width = fake_device_state_->format.frame_size.width();
  const int height = fake_device_state_->format.frame_size.height();

  SkColorType colorspace = kAlpha_8_SkColorType;
  switch (pixel_format_) {
    case Format::I420:
    case Format::NV12:
      // Skia doesn't support painting in I420. Instead, paint an 8bpp
      // monochrome image to the beginning of |target_buffer|. This section of
      // |target_buffer| corresponds to the Y-plane of the YUV image. Do not
      // touch the U or V planes of |target_buffer|. Assuming they have been
      // initialized to 0, which corresponds to a green color tone, the result
      // will be an green-ish monochrome frame.
      //
      // NV12 has the same Y plane dimension as I420 and we don't touch UV
      // plane.
      colorspace = kAlpha_8_SkColorType;
      break;
    case Format::SK_N32:
      // SkColorType is RGBA on some platforms and BGRA on others.
      colorspace = kN32_SkColorType;
      break;
    case Format::Y16:
      // Skia doesn't support painting in Y16. Instead, paint an 8bpp monochrome
      // image to the beginning of |target_buffer|. Later, move the 8bit pixel
      // values to a position corresponding to the high byte values of 16bit
      // pixel values (assuming the byte order is little-endian).
      colorspace = kAlpha_8_SkColorType;
      break;
  }

  const SkImageInfo info =
      SkImageInfo::Make(width, height, colorspace, kOpaque_SkAlphaType);
  SkBitmap bitmap;
  bitmap.setInfo(info, bytes_per_row);
  bitmap.setPixels(target_buffer);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  SkFont font = skia::DefaultFont();
  font.setEdging(SkFont::Edging::kAlias);
  SkCanvas canvas(bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());

  const SkScalar unscaled_zoom = fake_device_state_->zoom / 100.f;
  const SkScalar translate_x =
      (fake_device_state_->pan - kMinPan) * (width / (kMaxPan - kMinPan));
  const SkScalar translate_y =
      (fake_device_state_->tilt - kMinTilt) * (height / (kMaxTilt - kMinTilt));
  SkMatrix matrix;
  matrix.setScale(unscaled_zoom, unscaled_zoom, width / 2, height / 2);
  matrix.setTranslateX(translate_x);
  matrix.setTranslateY(translate_y);
  canvas.setMatrix(matrix);

  // For the SK_N32 case, match the green color tone produced by the
  // I420 case.
  if (pixel_format_ == Format::SK_N32) {
    const SkRect full_frame = SkRect::MakeWH(width, height);
    paint.setARGB(255, 0, 127, 0);
    canvas.drawRect(full_frame, paint);
    paint.setColor(SkColors::kGreen);
  }

  // Draw a sweeping circle to show an animation.
  const float end_angle =
      fmod(kPacmanAngularVelocity * elapsed_time.InSecondsF(), 361);
  const int radius = std::min(width, height) / 4;
  const SkRect rect = SkRect::MakeXYWH(width / 2 - radius, height / 2 - radius,
                                       2 * radius, 2 * radius);
  canvas.drawArc(rect, 0, end_angle, true, paint);

  if (fake_device_state_->background_blur) {
    // Draw a shadow circle to show background blur.
    SkPaint circle_paint;
    circle_paint.setARGB(20, 0, 127, 0);
    canvas.drawCircle(rect.center(), radius * 3 / 2, circle_paint);
  }

  // Draw current time.
  const int milliseconds = elapsed_time.InMilliseconds() % 1000;
  const int seconds = elapsed_time.InSeconds() % 60;
  const int minutes = elapsed_time.InMinutes() % 60;
  const int hours = elapsed_time.InHours();
  const int frame_count = elapsed_time.InMilliseconds() *
                          fake_device_state_->format.frame_rate / 1000;

  const std::string time_string =
      base::StringPrintf("%d:%02d:%02d:%03d %d", hours, minutes, seconds,
                         milliseconds, frame_count);
  canvas.scale(3, 3);
  canvas.drawSimpleText(time_string.data(), time_string.length(),
                        SkTextEncoding::kUTF8, 30, 20, font, paint);

  if (pixel_format_ == Format::Y16) {
    // Use 8 bit bitmap rendered to first half of the buffer as high byte values
    // for the whole buffer. Low byte values are not important.
    for (int i = (width * height) - 1; i >= 0; --i)
      target_buffer[i * 2 + 1] = target_buffer[i];
  }
}

FakePhotoDevice::FakePhotoDevice(
    std::unique_ptr<PacmanFramePainter> sk_n32_painter,
    const FakeDeviceState* fake_device_state,
    const FakePhotoDeviceConfig& config)
    : sk_n32_painter_(std::move(sk_n32_painter)),
      fake_device_state_(fake_device_state),
      config_(config) {}

FakePhotoDevice::~FakePhotoDevice() = default;

void FakePhotoDevice::TakePhoto(VideoCaptureDevice::TakePhotoCallback callback,
                                base::TimeDelta elapsed_time) {
  if (config_.should_fail_take_photo)
    return;

  // Create a PNG-encoded frame and send it back to |callback|.
  auto required_sk_n32_buffer_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_ARGB, fake_device_state_->format.frame_size);
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[required_sk_n32_buffer_size]);
  memset(buffer.get(), 0, required_sk_n32_buffer_size);
  sk_n32_painter_->PaintFrame(elapsed_time, buffer.get());
  mojom::BlobPtr blob = mojom::Blob::New();
  const gfx::PNGCodec::ColorFormat encoding_source_format =
#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
      gfx::PNGCodec::FORMAT_RGBA;
#else
      gfx::PNGCodec::FORMAT_BGRA;
#endif
  const bool result = gfx::PNGCodec::Encode(
      buffer.get(), encoding_source_format,
      fake_device_state_->format.frame_size,
      VideoFrame::RowBytes(0 /* plane */, PIXEL_FORMAT_ARGB,
                           fake_device_state_->format.frame_size.width()),
      true /* discard_transparency */, std::vector<gfx::PNGCodec::Comment>(),
      &blob->data);
  DCHECK(result);

  blob->mime_type = "image/png";
  std::move(callback).Run(std::move(blob));
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(
    const VideoCaptureFormats& supported_formats,
    std::unique_ptr<FrameDelivererFactory> frame_deliverer_factory,
    std::unique_ptr<FakePhotoDevice> photo_device,
    std::unique_ptr<FakeDeviceState> device_state)
    : supported_formats_(supported_formats),
      device_state_(std::move(device_state)),
      photo_device_(std::move(photo_device)),
      frame_deliverer_factory_(std::move(frame_deliverer_factory)) {}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FakeVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool video_capture_use_gmb =
      (params.buffer_type == VideoCaptureBufferType::kGpuMemoryBuffer);
  VideoCaptureFormat selected_format = FindClosestSupportedFormat(
      params.requested_format, supported_formats_, video_capture_use_gmb);

  beep_time_ = base::TimeDelta();
  elapsed_time_ = base::TimeDelta();
  frame_deliverer_ = frame_deliverer_factory_->CreateFrameDeliverer(
      selected_format, video_capture_use_gmb);
  device_state_->format.frame_size = selected_format.frame_size;
  frame_deliverer_->Initialize(device_state_->format.pixel_format,
                               std::move(client), device_state_.get());
  current_session_id_++;
  BeepAndScheduleNextCapture(base::TimeTicks::Now());
}

void FakeVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Invalidate WeakPtr to stop the perpetual scheduling of tasks.
  weak_factory_.InvalidateWeakPtrs();
  frame_deliverer_.reset();
}

void FakeVideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  photo_device_->GetPhotoState(std::move(callback));
}

void FakePhotoDevice::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  if (config_.should_fail_get_photo_capabilities)
    return;

  mojom::PhotoStatePtr photo_state = mojo::CreateEmptyPhotoState();

  photo_state->current_white_balance_mode = mojom::MeteringMode::NONE;

  photo_state->supported_exposure_modes.push_back(mojom::MeteringMode::MANUAL);
  photo_state->supported_exposure_modes.push_back(
      mojom::MeteringMode::CONTINUOUS);
  photo_state->current_exposure_mode = fake_device_state_->exposure_mode;

  photo_state->exposure_compensation = mojom::Range::New();

  photo_state->exposure_time = mojom::Range::New();
  photo_state->exposure_time->current = fake_device_state_->exposure_time;
  photo_state->exposure_time->max = kMaxExposureTime;
  photo_state->exposure_time->min = kMinExposureTime;
  photo_state->exposure_time->step = kExposureTimeStep;

  photo_state->color_temperature = mojom::Range::New();
  photo_state->iso = mojom::Range::New();
  photo_state->iso->current = 100.0;
  photo_state->iso->max = 100.0;
  photo_state->iso->min = 100.0;
  photo_state->iso->step = 0.0;

  photo_state->brightness = media::mojom::Range::New();
  photo_state->contrast = media::mojom::Range::New();
  photo_state->saturation = media::mojom::Range::New();
  photo_state->sharpness = media::mojom::Range::New();

  photo_state->supported_focus_modes.push_back(mojom::MeteringMode::MANUAL);
  photo_state->supported_focus_modes.push_back(mojom::MeteringMode::CONTINUOUS);
  photo_state->current_focus_mode = fake_device_state_->focus_mode;

  photo_state->focus_distance = mojom::Range::New();
  photo_state->focus_distance->current = fake_device_state_->focus_distance;
  photo_state->focus_distance->max = kMaxFocusDistance;
  photo_state->focus_distance->min = kMinFocusDistance;
  photo_state->focus_distance->step = kFocusDistanceStep;

  photo_state->pan = mojom::Range::New();
  if (config_.control_support.pan) {
    photo_state->pan->current = fake_device_state_->pan;
    photo_state->pan->max = kMaxPan;
    photo_state->pan->min = kMinPan;
    photo_state->pan->step = kPanStep;
  }

  photo_state->tilt = mojom::Range::New();
  if (config_.control_support.tilt) {
    photo_state->tilt->current = fake_device_state_->tilt;
    photo_state->tilt->max = kMaxTilt;
    photo_state->tilt->min = kMinTilt;
    photo_state->tilt->step = kTiltStep;
  }

  photo_state->zoom = mojom::Range::New();
  if (config_.control_support.zoom) {
    photo_state->zoom->current = fake_device_state_->zoom;
    photo_state->zoom->max = kMaxZoom;
    photo_state->zoom->min = kMinZoom;
    photo_state->zoom->step = kZoomStep;
  }

  photo_state->supports_torch = false;
  photo_state->torch = false;

  photo_state->red_eye_reduction = mojom::RedEyeReduction::NEVER;
  photo_state->height = mojom::Range::New();
  photo_state->height->current = fake_device_state_->format.frame_size.height();
  photo_state->height->max = 1080.0;
  photo_state->height->min = 96.0;
  photo_state->height->step = 1.0;
  photo_state->width = mojom::Range::New();
  photo_state->width->current = fake_device_state_->format.frame_size.width();
  photo_state->width->max = 1920.0;
  photo_state->width->min = 96.0;
  photo_state->width->step = 1.0;

  photo_state->supported_background_blur_modes = {
      mojom::BackgroundBlurMode::OFF, mojom::BackgroundBlurMode::BLUR};
  photo_state->background_blur_mode = fake_device_state_->background_blur
                                          ? mojom::BackgroundBlurMode::BLUR
                                          : mojom::BackgroundBlurMode::OFF;

  photo_state->supported_background_segmentation_mask_states = {false, true};
  photo_state->current_background_segmentation_mask_state =
      fake_device_state_->background_segmentation_mask;

  photo_state->supported_eye_gaze_correction_modes = {
      mojom::EyeGazeCorrectionMode::OFF, mojom::EyeGazeCorrectionMode::ON};
  photo_state->current_eye_gaze_correction_mode =
      fake_device_state_->eye_gaze_correction
          ? mojom::EyeGazeCorrectionMode::ON
          : mojom::EyeGazeCorrectionMode::OFF;

  photo_state->supported_face_framing_modes = {mojom::MeteringMode::NONE,
                                               mojom::MeteringMode::CONTINUOUS};
  photo_state->current_face_framing_mode = fake_device_state_->face_framing
                                               ? mojom::MeteringMode::CONTINUOUS
                                               : mojom::MeteringMode::NONE;

  std::move(callback).Run(std::move(photo_state));
}

void FakeVideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                             SetPhotoOptionsCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  photo_device_->SetPhotoOptions(std::move(settings), std::move(callback),
                                 device_state_.get());
}

void FakePhotoDevice::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback,
    FakeDeviceState* device_state_write_access) {
  if (config_.should_fail_set_photo_options)
    return;

  if (settings->has_pan) {
    device_state_write_access->pan =
        std::clamp(settings->pan, kMinPan, kMaxPan);
  }
  if (settings->has_tilt) {
    device_state_write_access->tilt =
        std::clamp(settings->tilt, kMinTilt, kMaxTilt);
  }
  if (settings->has_zoom) {
    device_state_write_access->zoom =
        std::clamp(settings->zoom, kMinZoom, kMaxZoom);
  }
  if (settings->has_exposure_time) {
    device_state_write_access->exposure_time =
        std::clamp(settings->exposure_time, kMinExposureTime, kMaxExposureTime);
  }

  if (settings->has_focus_distance) {
    device_state_write_access->focus_distance = std::clamp(
        settings->focus_distance, kMinFocusDistance, kMaxFocusDistance);
  }

  if (settings->has_background_blur_mode) {
    switch (settings->background_blur_mode) {
      case mojom::BackgroundBlurMode::OFF:
        device_state_write_access->background_blur = false;
        break;
      case mojom::BackgroundBlurMode::BLUR:
        device_state_write_access->background_blur = true;
        break;
    }
  }

  if (settings->background_segmentation_mask_state.has_value()) {
    device_state_write_access->background_segmentation_mask =
        settings->background_segmentation_mask_state.value();
  }

  if (settings->eye_gaze_correction_mode.has_value()) {
    switch (settings->eye_gaze_correction_mode.value()) {
      case mojom::EyeGazeCorrectionMode::OFF:
        device_state_write_access->eye_gaze_correction = false;
        break;
      case mojom::EyeGazeCorrectionMode::ON:
        device_state_write_access->eye_gaze_correction = true;
        break;
      case mojom::EyeGazeCorrectionMode::STARE:
        return;  // Not a supported fake eye gaze correction mode.
    }
  }

  if (settings->has_face_framing_mode) {
    switch (settings->face_framing_mode) {
      case mojom::MeteringMode::NONE:
        device_state_write_access->face_framing = false;
        break;
      case mojom::MeteringMode::CONTINUOUS:
        device_state_write_access->face_framing = true;
        break;
      case mojom::MeteringMode::MANUAL:
      case mojom::MeteringMode::SINGLE_SHOT:
        return;  // Not a supported face framing mode.
    }
  }

  std::move(callback).Run(true);
}

void FakeVideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  photo_device_->TakePhoto(
      base::BindPostTaskToCurrentDefault(std::move(callback)), elapsed_time_);
}

OwnBufferFrameDeliverer::OwnBufferFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

OwnBufferFrameDeliverer::~OwnBufferFrameDeliverer() = default;

void OwnBufferFrameDeliverer::Initialize(
    VideoPixelFormat pixel_format,
    std::unique_ptr<VideoCaptureDevice::Client> client,
    const FakeDeviceState* device_state) {
  FrameDeliverer::Initialize(pixel_format, std::move(client), device_state);
  buffer_ = base::HeapArray<uint8_t>::Uninit(VideoFrame::AllocationSize(
      pixel_format, device_state->format.frame_size));
}

void OwnBufferFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;
  const auto& frame_format = device_state()->format;
  const size_t frame_size = VideoFrame::AllocationSize(
      frame_format.pixel_format, frame_format.frame_size);
  memset(buffer_.data(), 0, frame_size);
  frame_painter()->PaintFrame(timestamp_to_paint, buffer_.data());
  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedData(
      buffer_.data(), frame_size, device_state()->format,
      GetDefaultColorSpace(device_state()->format.pixel_format),
      0 /* rotation */, false /* flip_y */, now,
      CalculateTimeSinceFirstInvocation(now), std::nullopt);
}

ClientBufferFrameDeliverer::ClientBufferFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

ClientBufferFrameDeliverer::~ClientBufferFrameDeliverer() = default;

void ClientBufferFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;

  const int arbitrary_frame_feedback_id = 0;
  VideoCaptureDevice::Client::Buffer capture_buffer;
  const auto reserve_result = client()->ReserveOutputBuffer(
      device_state()->format.frame_size, device_state()->format.pixel_format,
      arbitrary_frame_feedback_id, &capture_buffer,
      /*require_new_buffer_id=*/nullptr, /*retire_old_buffer_id=*/nullptr);
  if (reserve_result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    client()->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reserve_result));
    return;
  }
  auto buffer_access =
      capture_buffer.handle_provider->GetHandleForInProcessAccess();
  DCHECK(buffer_access->data()) << "Buffer has NO backing memory";

  uint8_t* data_ptr = buffer_access->data();
  memset(data_ptr, 0, buffer_access->mapped_size());
  frame_painter()->PaintFrame(timestamp_to_paint, data_ptr);
  buffer_access.reset();  // Can't outlive `capture_buffer.handle_provider'.

  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedBuffer(
      std::move(capture_buffer), device_state()->format, now,
      CalculateTimeSinceFirstInvocation(now), std::nullopt);
}

JpegEncodingFrameDeliverer::JpegEncodingFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter)
    : FrameDeliverer(std::move(frame_painter)) {}

JpegEncodingFrameDeliverer::~JpegEncodingFrameDeliverer() = default;

void JpegEncodingFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;

  auto required_sk_n32_buffer_size = VideoFrame::AllocationSize(
      PIXEL_FORMAT_ARGB, device_state()->format.frame_size);
  sk_n32_buffer_.resize(required_sk_n32_buffer_size);
  memset(&sk_n32_buffer_[0], 0, required_sk_n32_buffer_size);

  frame_painter()->PaintFrame(timestamp_to_paint, &sk_n32_buffer_[0]);

  static const int kQuality = 75;
  SkImageInfo info = SkImageInfo::MakeN32(
      device_state()->format.frame_size.width(),
      device_state()->format.frame_size.height(), kOpaque_SkAlphaType);
  SkPixmap src(info, &sk_n32_buffer_[0],
               VideoFrame::RowBytes(0 /* plane */, PIXEL_FORMAT_ARGB,
                                    device_state()->format.frame_size.width()));
  bool success = gfx::JPEGCodec::Encode(src, kQuality, &jpeg_buffer_);
  if (!success) {
    DLOG(ERROR) << "Jpeg encoding failed";
    return;
  }

  const size_t frame_size = jpeg_buffer_.size();
  base::TimeTicks now = base::TimeTicks::Now();
  client()->OnIncomingCapturedData(
      &jpeg_buffer_[0], frame_size, device_state()->format,
      gfx::ColorSpace::CreateJpeg(), 0 /* rotation */, false /* flip_y */, now,
      CalculateTimeSinceFirstInvocation(now), std::nullopt);
}

GpuMemoryBufferFrameDeliverer::GpuMemoryBufferFrameDeliverer(
    std::unique_ptr<PacmanFramePainter> frame_painter,
    gpu::GpuMemoryBufferSupport* gmb_support)
    : FrameDeliverer(std::move(frame_painter)), gmb_support_(gmb_support) {}

GpuMemoryBufferFrameDeliverer::~GpuMemoryBufferFrameDeliverer() = default;

void GpuMemoryBufferFrameDeliverer::PaintAndDeliverNextFrame(
    base::TimeDelta timestamp_to_paint) {
  if (!client())
    return;

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb;
  VideoCaptureDevice::Client::Buffer capture_buffer;
  const gfx::Size& buffer_size = device_state()->format.frame_size;
  auto reserve_result = AllocateNV12GpuMemoryBuffer(
      client(), buffer_size, gmb_support_, &gmb, &capture_buffer);
  if (reserve_result != VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
    client()->OnFrameDropped(
        ConvertReservationFailureToFrameDropReason(reserve_result));
    return;
  }
#if BUILDFLAG(IS_WIN)
  // On windows the GMBs aren't mappable natively. Instead mapping only copies
  // data to a shared memory region. So a different mechanism is used for
  // writable access.
  auto buffer_access =
      capture_buffer.handle_provider->GetHandleForInProcessAccess();
  uint8_t* data_ptr = buffer_access->data();
  memset(data_ptr, 0, buffer_access->mapped_size());
  frame_painter()->PaintFrame(timestamp_to_paint, data_ptr,
                              buffer_size.width());
  // Need to destroy `handle` so that the changes are committed to the GMB.
  buffer_access.reset();
  // Premap always just in case the client requests it.
  if (capture_buffer.handle_provider->DuplicateAsUnsafeRegion().IsValid()) {
    capture_buffer.is_premapped = true;
  }
#else
  ScopedNV12GpuMemoryBufferMapping scoped_mapping(std::move(gmb));
  memset(scoped_mapping.y_plane(), 0,
         scoped_mapping.y_stride() * buffer_size.height());
  memset(scoped_mapping.uv_plane(), 0,
         scoped_mapping.uv_stride() * (buffer_size.height() / 2));
  frame_painter()->PaintFrame(timestamp_to_paint, scoped_mapping.y_plane(),
                              scoped_mapping.y_stride());
#endif  // if BUILDFLAG(IS_WIN)
  base::TimeTicks now = base::TimeTicks::Now();
  VideoCaptureFormat modified_format = device_state()->format;
  // When GpuMemoryBuffer is used, the frame data is opaque to the CPU for most
  // of the time.  Currently the only supported underlying format is NV12.
  modified_format.pixel_format = PIXEL_FORMAT_NV12;
  client()->OnIncomingCapturedBuffer(
      std::move(capture_buffer), modified_format, now,
      CalculateTimeSinceFirstInvocation(now), std::nullopt);
}

void FakeVideoCaptureDevice::BeepAndScheduleNextCapture(
    base::TimeTicks expected_execution_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::TimeDelta beep_interval = base::Milliseconds(kBeepInterval);
  const base::TimeDelta frame_interval =
      base::Microseconds(1e6 / device_state_->format.frame_rate);
  beep_time_ += frame_interval;
  elapsed_time_ += frame_interval;

  // Generate a synchronized beep twice per second.
  if (beep_time_ >= beep_interval) {
    FakeAudioInputStream::BeepOnce();
    beep_time_ -= beep_interval;
  }

  // Reschedule next CaptureTask.
  const base::TimeTicks current_time = base::TimeTicks::Now();
  // Don't accumulate any debt if we are lagging behind - just post the next
  // frame immediately and continue as normal.
  const base::TimeTicks next_execution_time =
      std::max(current_time, expected_execution_time + frame_interval);
  const base::TimeDelta delay = next_execution_time - current_time;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeVideoCaptureDevice::OnNextFrameDue,
                     weak_factory_.GetWeakPtr(), next_execution_time,
                     current_session_id_),
      delay);
}

void FakeVideoCaptureDevice::OnNextFrameDue(
    base::TimeTicks expected_execution_time,
    int session_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (session_id != current_session_id_)
    return;

  frame_deliverer_->PaintAndDeliverNextFrame(elapsed_time_);
  BeepAndScheduleNextCapture(expected_execution_time);
}

}  // namespace media
