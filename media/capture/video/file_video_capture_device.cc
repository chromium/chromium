// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/file_video_capture_device.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/capture/mojom/image_capture_types.h"
#include "media/capture/video/blob_utils.h"
#include "media/capture/video/gpu_memory_buffer_utils.h"
#include "media/capture/video_capture_types.h"
#include "media/parsers/jpeg_parser.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

int gcd(int a, int b) {
  int c;

  c = a % b;
  while (c != 0) {
    a = b;
    b = c;
    c = a % b;
  }

  return (b);
}

}  // namespace

static const int kY4MHeaderMaxSize = 200;
static const char kY4MSimpleFrameDelimiter[] = "FRAME";
static const int kY4MSimpleFrameDelimiterSize = 6;
static const float kMJpegFrameRate = 30.0f;

int ParseY4MInt(std::string_view token) {
  int temp_int;
  CHECK(base::StringToInt(token, &temp_int)) << token;
  return temp_int;
}

// Extract numerator and denominator out of a token that must have the aspect
// numerator:denominator, both integer numbers.
void ParseY4MRational(std::string_view token,
                      int* numerator,
                      int* denominator) {
  size_t index_divider = token.find(':');
  CHECK_NE(index_divider, token.npos);
  *numerator = ParseY4MInt(token.substr(0, index_divider));
  *denominator = ParseY4MInt(token.substr(index_divider + 1, token.length()));
  CHECK(*denominator);
}

// This function parses the ASCII string in |header| as belonging to a Y4M file,
// returning the collected format in |video_format|. For a non authoritative
// explanation of the header format, check
// http://wiki.multimedia.cx/index.php?title=YUV4MPEG2
// Restrictions: Only interlaced I420 pixel format is supported, and pixel
// aspect ratio is ignored.
// Implementation notes: Y4M header should end with an ASCII 0x20 (whitespace)
// character, however all examples mentioned in the Y4M header description end
// with a newline character instead. Also, some headers do _not_ specify pixel
// format, in this case it means I420.
// This code was inspired by third_party/libvpx/.../y4minput.* .
void ParseY4MTags(const std::string& file_header,
                  VideoCaptureFormat* video_format) {
  VideoCaptureFormat format;
  format.pixel_format = PIXEL_FORMAT_I420;
  size_t index = 0;
  size_t blank_position = 0;
  std::string_view token;
  while ((blank_position = file_header.find_first_of("\n ", index)) !=
         std::string::npos) {
    // Every token is supposed to have an identifier letter and a bunch of
    // information immediately after, which we extract into a |token| here.
    token = std::string_view(file_header)
                .substr(index + 1, blank_position - index - 1);
    CHECK(!token.empty());
    switch (file_header[index]) {
      case 'W':
        format.frame_size.set_width(ParseY4MInt(token));
        break;
      case 'H':
        format.frame_size.set_height(ParseY4MInt(token));
        break;
      case 'F': {
        // If the token is "FRAME", it means we have finished with the header.
        if (token[0] == 'R')
          break;
        int fps_numerator, fps_denominator;
        ParseY4MRational(token, &fps_numerator, &fps_denominator);
        format.frame_rate = fps_numerator / fps_denominator;
        break;
      }
      case 'I':
        // Interlacing is ignored, but we don't like mixed modes.
        CHECK_NE(token[0], 'm');
        break;
      case 'A':
        // Pixel aspect ratio ignored.
        break;
      case 'C':
        CHECK(token == "420" || token == "420jpeg" || token == "420mpeg2" ||
              token == "420paldv")
            << token;  // Only I420 is supported, and we fudge the variants.
        break;
      default:
        break;
    }
    // We're done if we have found a newline character right after the token.
    if (file_header[blank_position] == '\n')
      break;
    index = blank_position + 1;
  }
  // Last video format semantic correctness check before sending it back.
  CHECK(format.IsValid());
  *video_format = format;
}

class VideoFileParser {
 public:
  explicit VideoFileParser(const base::FilePath& file_path);
  virtual ~VideoFileParser();

  // Parses file header and collects format information in |capture_format|.
  virtual bool Initialize(VideoCaptureFormat* capture_format) = 0;

  // Gets the start pointer of next frame and stores current frame size in
  // |frame_size|.
  virtual base::span<const uint8_t> GetNextFrame() = 0;

 protected:
  const base::FilePath file_path_;
  int frame_size_;
  size_t current_byte_index_;
  size_t first_frame_byte_index_;
};

class Y4mFileParser final : public VideoFileParser {
 public:
  explicit Y4mFileParser(const base::FilePath& file_path);

  Y4mFileParser(const Y4mFileParser&) = delete;
  Y4mFileParser& operator=(const Y4mFileParser&) = delete;

  // VideoFileParser implementation, class methods.
  ~Y4mFileParser() override;
  bool Initialize(VideoCaptureFormat* capture_format) override;
  base::span<const uint8_t> GetNextFrame() override;

 private:
  std::unique_ptr<base::File> file_;
  std::unique_ptr<uint8_t[]> video_frame_;
};

class MjpegFileParser final : public VideoFileParser {
 public:
  explicit MjpegFileParser(const base::FilePath& file_path);

  MjpegFileParser(const MjpegFileParser&) = delete;
  MjpegFileParser& operator=(const MjpegFileParser&) = delete;

  // VideoFileParser implementation, class methods.
  ~MjpegFileParser() override;
  bool Initialize(VideoCaptureFormat* capture_format) override;
  base::span<const uint8_t> GetNextFrame() override;

 private:
  std::unique_ptr<base::MemoryMappedFile> mapped_file_;
};

VideoFileParser::VideoFileParser(const base::FilePath& file_path)
    : file_path_(file_path),
      frame_size_(0),
      current_byte_index_(0),
      first_frame_byte_index_(0) {}

VideoFileParser::~VideoFileParser() = default;

Y4mFileParser::Y4mFileParser(const base::FilePath& file_path)
    : VideoFileParser(file_path) {}

Y4mFileParser::~Y4mFileParser() = default;

bool Y4mFileParser::Initialize(VideoCaptureFormat* capture_format) {
  file_ = std::make_unique<base::File>(
      file_path_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file_->IsValid()) {
    DLOG(ERROR) << file_path_.value() << ", error: "
                << base::File::ErrorToString(file_->error_details());
    return false;
  }

  std::string header(kY4MHeaderMaxSize, '\0');
  file_->Read(0, &header[0], header.size());
  const size_t header_end = header.find(kY4MSimpleFrameDelimiter);
  CHECK_NE(header_end, header.npos);

  ParseY4MTags(header, capture_format);
  first_frame_byte_index_ = header_end + kY4MSimpleFrameDelimiterSize;
  current_byte_index_ = first_frame_byte_index_;
  frame_size_ = VideoFrame::AllocationSize(capture_format->pixel_format,
                                           capture_format->frame_size);
  return true;
}

base::span<const uint8_t> Y4mFileParser::GetNextFrame() {
  if (!video_frame_)
    video_frame_ = std::make_unique<uint8_t[]>(frame_size_);
  int result =
      file_->Read(current_byte_index_,
                  reinterpret_cast<char*>(video_frame_.get()), frame_size_);

  // If we passed EOF to base::File, it will return 0 read characters. In that
  // case, reset the pointer and read again.
  if (result != frame_size_) {
    CHECK_EQ(result, 0);
    current_byte_index_ = first_frame_byte_index_;
    CHECK_EQ(
        file_->Read(current_byte_index_,
                    reinterpret_cast<char*>(video_frame_.get()), frame_size_),
        frame_size_);
  } else {
    current_byte_index_ += frame_size_ + kY4MSimpleFrameDelimiterSize;
  }
  return base::make_span(video_frame_.get(),
                         base::checked_cast<size_t>(frame_size_));
}

MjpegFileParser::MjpegFileParser(const base::FilePath& file_path)
    : VideoFileParser(file_path) {}

MjpegFileParser::~MjpegFileParser() = default;

bool MjpegFileParser::Initialize(VideoCaptureFormat* capture_format) {
  mapped_file_ = std::make_unique<base::MemoryMappedFile>();

  if (!mapped_file_->Initialize(file_path_) || !mapped_file_->IsValid()) {
    LOG(ERROR) << "File memory map error: " << file_path_.value();
    return false;
  }

  JpegParseResult result;
  if (!ParseJpegStream(mapped_file_->bytes(), &result)) {
    return false;
  }

  frame_size_ = result.image_size;
  if (frame_size_ > base::checked_cast<int>(mapped_file_->length())) {
    LOG(ERROR) << "File is incomplete";
    return false;
  }

  VideoCaptureFormat format;
  format.pixel_format = PIXEL_FORMAT_MJPEG;
  format.frame_size.set_width(result.frame_header.visible_width);
  format.frame_size.set_height(result.frame_header.visible_height);
  format.frame_rate = kMJpegFrameRate;
  if (!format.IsValid())
    return false;
  *capture_format = format;
  return true;
}

base::span<const uint8_t> MjpegFileParser::GetNextFrame() {
  base::span<const uint8_t> buf_span =
      mapped_file_->bytes().subspan(current_byte_index_);

  JpegParseResult result;
  if (!ParseJpegStream(buf_span, &result)) {
    return base::span<const uint8_t>();
  }
  int frame_size = frame_size_ = result.image_size;
  current_byte_index_ += frame_size_;
  // Reset the pointer to play repeatedly.
  if (current_byte_index_ >= mapped_file_->length())
    current_byte_index_ = first_frame_byte_index_;
  return buf_span.subspan(0u, base::checked_cast<size_t>(frame_size));
}

// static
bool FileVideoCaptureDevice::GetVideoCaptureFormat(
    const base::FilePath& file_path,
    VideoCaptureFormat* video_format) {
  std::unique_ptr<VideoFileParser> file_parser =
      GetVideoFileParser(file_path, video_format);
  return file_parser != nullptr;
}

// static
std::unique_ptr<VideoFileParser> FileVideoCaptureDevice::GetVideoFileParser(
    const base::FilePath& file_path,
    VideoCaptureFormat* video_format) {
  std::unique_ptr<VideoFileParser> file_parser;
  std::string file_name(file_path.value().begin(), file_path.value().end());

  if (base::EndsWith(file_name, "y4m", base::CompareCase::INSENSITIVE_ASCII)) {
    file_parser = std::make_unique<Y4mFileParser>(file_path);
  } else if (base::EndsWith(file_name, "mjpeg",
                            base::CompareCase::INSENSITIVE_ASCII)) {
    file_parser = std::make_unique<MjpegFileParser>(file_path);
  } else {
    LOG(ERROR) << "Unsupported file format.";
    return file_parser;
  }

  if (!file_parser->Initialize(video_format)) {
    file_parser.reset();
  }
  return file_parser;
}

std::vector<uint8_t> FileVideoCaptureDevice::CropPTZRegion(
    const uint8_t* frame,
    size_t frame_buffer_size,
    VideoPixelFormat* final_pixel_format) {
  CHECK(frame);

  *final_pixel_format = capture_format_.pixel_format;

  const gfx::Size& frame_size = capture_format_.frame_size;
  uint32_t fourcc;
  std::unique_ptr<uint8_t[]> jpeg_to_i420_buffer_;
  switch (capture_format_.pixel_format) {
    case PIXEL_FORMAT_MJPEG:
      // |libyuv::ConvertToI420| don't support cropping MJPG into different
      // width and thus require transform to i420 first.
      if ([&frame, &frame_buffer_size, &frame_size, &jpeg_to_i420_buffer_]() {
            const size_t i420_buffer_size =
                VideoFrame::AllocationSize(PIXEL_FORMAT_I420, frame_size);
            jpeg_to_i420_buffer_ =
                std::make_unique<uint8_t[]>(i420_buffer_size);

            uint8_t* dst_yp = jpeg_to_i420_buffer_.get();
            uint8_t* dst_up =
                dst_yp + VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 0, frame_size)
                             .GetArea();
            uint8_t* dst_vp =
                dst_up + VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 1, frame_size)
                             .GetArea();
            int dst_yp_stride = frame_size.width();
            int dst_up_stride = dst_yp_stride / 2;
            int dst_vp_stride = dst_yp_stride / 2;

            return libyuv::ConvertToI420(
                frame, frame_buffer_size, dst_yp, dst_yp_stride, dst_up,
                dst_up_stride, dst_vp, dst_vp_stride, /* crop_x */ 0,
                /* crop_y */ 0,
                /* src_width */ frame_size.width(),
                /* src_height */ frame_size.height(),
                /* crop_width */ frame_size.width(),
                /* crop_height */ frame_size.height(),
                libyuv::RotationMode::kRotate0, libyuv::FOURCC_MJPG);
          }()) {
        LOG(ERROR) << "Failed to convert MJPEG to i420 for ptz transform";
      }
      frame = jpeg_to_i420_buffer_.get();
      frame_buffer_size =
          VideoFrame::AllocationSize(PIXEL_FORMAT_I420, frame_size);
      *final_pixel_format = PIXEL_FORMAT_I420;
      [[fallthrough]];
    case PIXEL_FORMAT_I420:
      fourcc = libyuv::FOURCC_I420;
      break;
    default:
      LOG(ERROR) << "Unsupported file format for ptz transform.";
      return {};
  }

  // Crop zoomed region.
  const int crop_width = (zoom_max_levels_ - zoom_) * aspect_ratio_numerator_;
  const int crop_height =
      (zoom_max_levels_ - zoom_) * aspect_ratio_denominator_;
  const gfx::Size crop_size(crop_width, crop_height);
  const int crop_x =
      std::min(pan_ * aspect_ratio_numerator_, frame_size.width() - crop_width);
  const int crop_y =
      std::min((zoom_max_levels_ - 1 - tilt_) * aspect_ratio_denominator_,
               frame_size.height() - crop_height);
  const size_t crop_buffer_size =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, crop_size);
  std::vector<uint8_t> crop_frame(crop_buffer_size);

  uint8_t* crop_yp = crop_frame.data();
  uint8_t* crop_up =
      crop_yp +
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 0, crop_size).GetArea();
  uint8_t* crop_vp =
      crop_up +
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 1, crop_size).GetArea();
  int crop_yp_stride = crop_width;
  int crop_up_stride = crop_yp_stride / 2;
  int crop_vp_stride = crop_yp_stride / 2;

  if (libyuv::ConvertToI420(frame, frame_buffer_size, crop_yp, crop_yp_stride,
                            crop_up, crop_up_stride, crop_vp, crop_vp_stride,
                            crop_x, crop_y, frame_size.width(),
                            frame_size.height(), crop_width, crop_height,
                            libyuv::RotationMode::kRotate0, fourcc)) {
    LOG(ERROR) << "Failed to crop image for ptz transform.";
    return {};
  }

  if (crop_size == frame_size)
    return crop_frame;

  // Scale cropped region to original size.
  const auto& scale_size = frame_size;
  const size_t scale_buffer_size =
      VideoFrame::AllocationSize(PIXEL_FORMAT_I420, scale_size);
  std::vector<uint8_t> scale_frame(scale_buffer_size);

  uint8_t* scale_yp = scale_frame.data();
  uint8_t* scale_up =
      scale_yp +
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 0, scale_size).GetArea();
  uint8_t* scale_vp =
      scale_up +
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 1, scale_size).GetArea();
  int scale_yp_stride = scale_size.width();
  int scale_up_stride = scale_yp_stride / 2;
  int scale_vp_stride = scale_yp_stride / 2;

  if (libyuv::I420Scale(crop_yp, crop_yp_stride, crop_up, crop_up_stride,
                        crop_vp, crop_vp_stride, crop_width, crop_height,
                        scale_yp, scale_yp_stride, scale_up, scale_up_stride,
                        scale_vp, scale_vp_stride, scale_size.width(),
                        scale_size.height(),
                        libyuv::FilterMode::kFilterBilinear)) {
    LOG(ERROR) << "Failed to scale image for ptz transform.";
    return {};
  }
  return scale_frame;
}

FileVideoCaptureDevice::FileVideoCaptureDevice(
    const base::FilePath& file_path,
    std::unique_ptr<gpu::GpuMemoryBufferSupport> gmb_support)
    : capture_thread_("CaptureThread"),
      file_path_(file_path),
      gmb_support_(gmb_support
                       ? std::move(gmb_support)
                       : std::make_unique<gpu::GpuMemoryBufferSupport>()) {}

FileVideoCaptureDevice::~FileVideoCaptureDevice() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  CHECK(!capture_thread_.IsRunning());
}

void FileVideoCaptureDevice::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(!capture_thread_.IsRunning());

  capture_thread_.Start();
  capture_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&FileVideoCaptureDevice::OnAllocateAndStart,
                     base::Unretained(this), params, std::move(client)));
}

void FileVideoCaptureDevice::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(capture_thread_.IsRunning());

  capture_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileVideoCaptureDevice::OnStopAndDeAllocate,
                                base::Unretained(this)));
  capture_thread_.Stop();
}

void FileVideoCaptureDevice::GetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(capture_thread_.IsRunning());

  capture_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileVideoCaptureDevice::OnGetPhotoState,
                                base::Unretained(this), std::move(callback)));
}

void FileVideoCaptureDevice::OnGetPhotoState(GetPhotoStateCallback callback) {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());

  auto photo_capabilities = mojo::CreateEmptyPhotoState();

  int height = capture_format_.frame_size.height();
  photo_capabilities->height = mojom::Range::New(height, height, height, 0);
  int width = capture_format_.frame_size.width();
  photo_capabilities->width = mojom::Range::New(width, width, width, 0);

  if (zoom_max_levels_ > 0) {
    photo_capabilities->pan = mojom::Range::New();
    photo_capabilities->pan->current = pan_;
    photo_capabilities->pan->max = zoom_max_levels_ - 1;
    photo_capabilities->pan->min = 0;
    photo_capabilities->pan->step = 1;

    photo_capabilities->tilt = mojom::Range::New();
    photo_capabilities->tilt->current = tilt_;
    photo_capabilities->tilt->max = zoom_max_levels_ - 1;
    photo_capabilities->tilt->min = 0;
    photo_capabilities->tilt->step = 1;

    photo_capabilities->zoom = mojom::Range::New();
    photo_capabilities->zoom->current = zoom_;
    photo_capabilities->zoom->max = zoom_max_levels_ - 1;
    photo_capabilities->zoom->min = 0;
    photo_capabilities->zoom->step = 1;
  }

  std::move(callback).Run(std::move(photo_capabilities));
}

void FileVideoCaptureDevice::SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                                             SetPhotoOptionsCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(capture_thread_.IsRunning());

  capture_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileVideoCaptureDevice::OnSetPhotoOptions,
                                base::Unretained(this), std::move(settings),
                                std::move(callback)));
}

void FileVideoCaptureDevice::OnSetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());

  if (settings->has_height &&
      settings->height != capture_format_.frame_size.height()) {
    return;
  }

  if (settings->has_width &&
      settings->width != capture_format_.frame_size.width()) {
    return;
  }

  if (settings->has_torch && settings->torch)
    return;

  if (settings->has_red_eye_reduction && settings->red_eye_reduction)
    return;

  if (settings->has_exposure_compensation || settings->has_exposure_time ||
      settings->has_color_temperature || settings->has_iso ||
      settings->has_brightness || settings->has_contrast ||
      settings->has_saturation || settings->has_sharpness ||
      settings->has_focus_distance || settings->has_fill_light_mode) {
    return;
  }

  if (settings->has_pan) {
    pan_ = std::clamp(int(settings->pan), 0, zoom_max_levels_);
  }

  if (settings->has_tilt) {
    tilt_ = std::clamp(int(settings->tilt), 0, zoom_max_levels_);
  }
  if (settings->has_zoom) {
    zoom_ = std::clamp(int(settings->zoom), 0, zoom_max_levels_);
  }

  std::move(callback).Run(true);
}

void FileVideoCaptureDevice::TakePhoto(TakePhotoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(lock_);

  take_photo_callbacks_.push(std::move(callback));
}

void FileVideoCaptureDevice::OnAllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());

  client_ = std::move(client);

  if (params.buffer_type == VideoCaptureBufferType::kGpuMemoryBuffer)
    video_capture_use_gmb_ = true;

  DCHECK(!file_parser_);
  file_parser_ = GetVideoFileParser(file_path_, &capture_format_);
  if (!file_parser_) {
    client_->OnError(
        VideoCaptureError::kFileVideoCaptureDeviceCouldNotOpenVideoFile,
        FROM_HERE, "Could not open Video file");
    return;
  }

  zoom_max_levels_ = gcd(capture_format_.frame_size.width(),
                         capture_format_.frame_size.height());
  aspect_ratio_numerator_ =
      capture_format_.frame_size.width() / zoom_max_levels_;
  aspect_ratio_denominator_ =
      capture_format_.frame_size.height() / zoom_max_levels_;
  zoom_ = 0;
  pan_ = 0;
  tilt_ = zoom_max_levels_ - 1;

  DVLOG(1) << "Opened video file " << capture_format_.frame_size.ToString()
           << ", fps: " << capture_format_.frame_rate;
  client_->OnStarted();

  capture_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileVideoCaptureDevice::OnCaptureTask,
                                base::Unretained(this)));
}

void FileVideoCaptureDevice::OnStopAndDeAllocate() {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  file_parser_.reset();
  client_.reset();
  next_frame_time_ = base::TimeTicks();
}

void FileVideoCaptureDevice::OnCaptureTask() {
  DCHECK(capture_thread_.task_runner()->BelongsToCurrentThread());
  if (!client_)
    return;
  base::AutoLock lock(lock_);

  // Give the captured frame to the client.
  base::span<const uint8_t> frame_span = file_parser_->GetNextFrame();
  CHECK(!frame_span.empty());

  VideoPixelFormat ptz_pixel_format;
  auto ptz_frame =
      CropPTZRegion(frame_span.data(), frame_span.size(), &ptz_pixel_format);

  VideoCaptureFormat ptz_format = capture_format_;
  ptz_format.pixel_format = ptz_pixel_format;

  CHECK(!ptz_frame.empty());

  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (first_ref_time_.is_null())
    first_ref_time_ = current_time;

  if (video_capture_use_gmb_) {
    const gfx::Size& buffer_size = capture_format_.frame_size;
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb;
    VideoCaptureDevice::Client::Buffer capture_buffer;
    auto reserve_result = AllocateNV12GpuMemoryBuffer(
        client_.get(), buffer_size, gmb_support_.get(), &gmb, &capture_buffer);
    if (reserve_result !=
        VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
      client_->OnFrameDropped(
          ConvertReservationFailureToFrameDropReason(reserve_result));
      DVLOG(2) << __func__ << " frame was dropped.";
      return;
    }
    ScopedNV12GpuMemoryBufferMapping scoped_mapping(std::move(gmb));
    const uint8_t* src_y_plane = ptz_frame.data();
    const uint8_t* src_u_plane =
        ptz_frame.data() +
        VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 0, buffer_size).GetArea();
    const uint8_t* src_v_plane =
        ptz_frame.data() +
        VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 0, buffer_size).GetArea() +
        VideoFrame::PlaneSize(PIXEL_FORMAT_I420, 1, buffer_size).GetArea();
    libyuv::I420ToNV12(
        src_y_plane, buffer_size.width(), src_u_plane, buffer_size.width() / 2,
        src_v_plane, buffer_size.width() / 2, scoped_mapping.y_plane(),
        scoped_mapping.y_stride(), scoped_mapping.uv_plane(),
        scoped_mapping.uv_stride(), buffer_size.width(), buffer_size.height());
    // When GpuMemoryBuffer is used, the frame data is opaque to the CPU for
    // most of the time.  Currently the only supported underlying format is
    // NV12.
    VideoCaptureFormat gmb_format = ptz_format;
    gmb_format.pixel_format = PIXEL_FORMAT_NV12;
    client_->OnIncomingCapturedBuffer(
        std::move(capture_buffer), gmb_format, current_time,
        current_time - first_ref_time_, std::nullopt);
  } else {
    // Leave the color space unset for compatibility purposes but this
    // information should be retrieved from the container when possible.
    client_->OnIncomingCapturedData(
        ptz_frame.data(), ptz_frame.size(), ptz_format, gfx::ColorSpace(),
        0 /* clockwise_rotation */, false /* flip_y */, current_time,
        current_time - first_ref_time_, std::nullopt);
  }

  // Process waiting photo callbacks
  while (!take_photo_callbacks_.empty()) {
    auto cb = std::move(take_photo_callbacks_.front());
    take_photo_callbacks_.pop();

    mojom::BlobPtr blob =
        RotateAndBlobify(ptz_frame.data(), ptz_frame.size(), ptz_format, 0);
    if (!blob)
      continue;

    std::move(cb).Run(std::move(blob));
  }

  // Reschedule next CaptureTask.
  const base::TimeDelta frame_interval =
      base::Microseconds(1E6 / capture_format_.frame_rate);
  if (next_frame_time_.is_null()) {
    next_frame_time_ = current_time + frame_interval;
  } else {
    next_frame_time_ += frame_interval;
    // Don't accumulate any debt if we are lagging behind - just post next frame
    // immediately and continue as normal.
    if (next_frame_time_ < current_time)
      next_frame_time_ = current_time;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FileVideoCaptureDevice::OnCaptureTask,
                     base::Unretained(this)),
      next_frame_time_ - current_time);
}

}  // namespace media
