// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "remoting/codec/utils.h"
#include "remoting/codec/webrtc_video_encoder_av1.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/test/frame_generator/differ_frame_generator.h"
#include "remoting/test/frame_generator/file_frame_generator.h"
#include "remoting/test/frame_generator/headless_frame_generator.h"
#include "third_party/libyuv/include/libyuv/compare.h"
#include "third_party/webrtc/api/environment/environment_factory.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/video/encoded_image.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/api/video_codecs/builtin_video_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_decoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

namespace {

constexpr int kDefaultFrames = 100;
constexpr double kDefaultFps = 30.0;

enum class Codec {
  kAv1,
  kVp8,
  kVp9,
};

enum class Profile {
  k0,
  k1,
};

enum class ColorSpace {
  kI420,
  kI444,
};

struct EncoderConfig {
  std::string name;
  Codec codec = Codec::kAv1;
  Profile profile = Profile::k0;
  ColorSpace color_space = ColorSpace::kI420;
  int encoder_speed = WebrtcVideoEncoderAV1::kDefaultEncoderSpeed;
  bool use_active_map = false;
  int bitrate_kbps = 0;
};

struct BenchmarkConfig {
  int frames = kDefaultFrames;
  double fps = kDefaultFps;
  int bitrate_kbps = 0;
  std::vector<webrtc::DesktopSize> sizes;
  std::vector<std::string> scenarios;
  std::vector<EncoderConfig> encoders;
  base::FilePath frame_dir;
  base::FilePath chrome_path;
  base::FilePath output_json_path;
};

struct RuntimeParams {
  double fps = kDefaultFps;
  webrtc::DesktopSize size;
  std::string scenario;
};

struct FrameMetric {
  size_t frame_index = 0;
  double encode_time_ms = 0.0;
  size_t size_bytes = 0;
  int quantizer = 0;
  bool key_frame = false;
  webrtc::scoped_refptr<webrtc::EncodedImageBuffer> encoded_data;
};

struct QualityStats {
  size_t frames_decoded = 0;
  size_t dirty_frames_decoded = 0;
  double mean_psnr_db = 0.0;
  double min_psnr_db = 0.0;
  double final_frame_psnr_db = 0.0;
  double dirty_region_mean_psnr_db = 0.0;
  double dirty_region_min_psnr_db = 0.0;
  double mean_ssim = 0.0;
  double min_ssim = 0.0;
  double final_frame_ssim = 0.0;
};

struct RunResults {
  base::TimeTicks start_time;
  base::TimeTicks end_time;
  int64_t total_pixels = 0;
  int64_t updated_pixels = 0;
  std::vector<FrameMetric> frame_metrics;
  QualityStats quality_stats;
};

struct LatencyStats {
  size_t count = 0;
  double total_ms = 0.0;
  double mean_ms = 0.0;
  double p50_ms = 0.0;
  double p90_ms = 0.0;
  double p99_ms = 0.0;
  double max_ms = 0.0;
};

struct SizeStats {
  size_t count = 0;
  size_t total_bytes = 0;
  double avg_bytes = 0.0;
  size_t max_bytes = 0;
};

struct QuantizerStats {
  size_t count = 0;
  double avg_qp = 0.0;
  int min_qp = 0;
  int max_qp = 0;
};

std::string CodecToString(Codec codec) {
  switch (codec) {
    case Codec::kAv1:
      return "av1";
    case Codec::kVp8:
      return "vp8";
    case Codec::kVp9:
      return "vp9";
  }
}

std::string DefaultEncoderName(const EncoderConfig& config) {
  int profile_num = (config.profile == Profile::k1) ? 1 : 0;
  switch (config.codec) {
    case Codec::kAv1:
      return base::StringPrintf("av1_p%d_s%d_%s", profile_num,
                                config.encoder_speed,
                                config.use_active_map ? "am" : "no_am");
    case Codec::kVp9:
      return base::StringPrintf("vp9_p%d_s%d", profile_num,
                                config.encoder_speed);
    case Codec::kVp8:
      return "vp8_p0";
  }
}

// Returns the target bitrate (in kbps) for a benchmark run. If an explicit
// bitrate was configured (via the top-level or per-encoder JSON "bitrate_kbps"
// field), that value is used directly. Otherwise, delegates to
// EncoderBitrateFilter to compute the resolution-scaled minimum target bitrate
// (2500 kbps per megapixel with a 1000 kbps floor).
int GetTargetBitrateKbps(int configured_bitrate_kbps,
                         const webrtc::DesktopSize& size) {
  if (configured_bitrate_kbps > 0) {
    return configured_bitrate_kbps;
  }
  EncoderBitrateFilter bitrate_filter(
      EncoderBitrateFilter::kDefaultMinimumBitrateKbpsPerMegapixel);
  bitrate_filter.SetFrameSize(size.width(), size.height());
  return bitrate_filter.GetTargetBitrateKbps();
}

std::unique_ptr<WebrtcVideoEncoder> CreateEncoder(const EncoderConfig& config) {
  std::unique_ptr<WebrtcVideoEncoder> encoder;
  switch (config.codec) {
    case Codec::kAv1:
      encoder = std::make_unique<WebrtcVideoEncoderAV1>();
      break;
    case Codec::kVp8:
      encoder = WebrtcVideoEncoderVpx::CreateForVP8();
      break;
    case Codec::kVp9:
      encoder = WebrtcVideoEncoderVpx::CreateForVP9();
      break;
  }

  if (encoder) {
    encoder->SetLosslessColor(config.profile == Profile::k1);
    if (config.codec == Codec::kAv1) {
      encoder->SetUseActiveMap(config.use_active_map);
      encoder->SetEncoderSpeed(config.encoder_speed);
    } else if (config.codec == Codec::kVp9) {
      encoder->SetEncoderSpeed(config.encoder_speed);
    }
  }

  return encoder;
}

double ComputePercentileFromSorted(const std::vector<double>& sorted_values,
                                   double percentile) {
  if (sorted_values.empty()) {
    return 0.0;
  }
  double index = (percentile / 100.0) * (sorted_values.size() - 1);
  size_t lower = static_cast<size_t>(std::floor(index));
  size_t upper = static_cast<size_t>(std::ceil(index));
  double weight = index - lower;
  return sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight;
}

LatencyStats ComputeLatencyStats(const std::vector<double>& times_ms) {
  LatencyStats stats;
  stats.count = times_ms.size();
  if (times_ms.empty()) {
    return stats;
  }
  std::vector<double> sorted_times = times_ms;
  std::ranges::sort(sorted_times);
  stats.total_ms =
      std::accumulate(sorted_times.begin(), sorted_times.end(), 0.0);
  stats.mean_ms = stats.total_ms / stats.count;
  stats.p50_ms = ComputePercentileFromSorted(sorted_times, 50.0);
  stats.p90_ms = ComputePercentileFromSorted(sorted_times, 90.0);
  stats.p99_ms = ComputePercentileFromSorted(sorted_times, 99.0);
  stats.max_ms = sorted_times.back();
  return stats;
}

SizeStats ComputeSizeStats(const std::vector<size_t>& sizes) {
  SizeStats stats;
  stats.count = sizes.size();
  if (sizes.empty()) {
    return stats;
  }
  stats.total_bytes =
      std::accumulate(sizes.begin(), sizes.end(), static_cast<size_t>(0));
  stats.avg_bytes = static_cast<double>(stats.total_bytes) / stats.count;
  stats.max_bytes = *std::ranges::max_element(sizes);
  return stats;
}

QuantizerStats ComputeQuantizerStats(const std::vector<int>& quantizers) {
  QuantizerStats stats;
  stats.count = quantizers.size();
  if (quantizers.empty()) {
    return stats;
  }
  int64_t sum =
      std::accumulate(quantizers.begin(), quantizers.end(), int64_t{0});
  stats.avg_qp = static_cast<double>(sum) / stats.count;
  auto [min_it, max_it] = std::ranges::minmax_element(quantizers);
  stats.min_qp = *min_it;
  stats.max_qp = *max_it;
  return stats;
}

std::vector<std::unique_ptr<webrtc::SharedDesktopFrame>> LoadFrames(
    const BenchmarkConfig& config,
    const std::string& scenario,
    const webrtc::DesktopSize& size) {
  std::unique_ptr<FrameGenerator> generator;
  if (!config.frame_dir.empty()) {
    generator = std::make_unique<FileFrameGenerator>(config.frame_dir, size);
  } else {
    auto headless = std::make_unique<HeadlessFrameGenerator>(
        scenario, size, config.frames, config.fps);
    if (!config.chrome_path.empty()) {
      headless->SetChromePath(config.chrome_path);
    }
    if (!headless->Initialize()) {
      LOG(ERROR) << "Failed to initialize headless frame generator.";
      return {};
    }
    generator = std::move(headless);
  }

  // Wrap with differ to pre-calculate updated regions.
  generator = std::make_unique<DifferFrameGenerator>(std::move(generator));

  std::cout << "Pre-loading up to " << config.frames << " frames for scenario '"
            << scenario << "' at " << size.width() << "x" << size.height()
            << "..." << std::endl;

  std::vector<std::unique_ptr<webrtc::SharedDesktopFrame>> frames;
  frames.reserve(config.frames);
  for (int i = 0; i < config.frames; ++i) {
    auto frame = generator->GenerateFrame();
    if (!frame) {
      break;
    }
    frames.push_back(webrtc::SharedDesktopFrame::Wrap(std::move(frame)));
  }

  return frames;
}

class BenchmarkVideoDecoder {
 public:
  static std::unique_ptr<BenchmarkVideoDecoder> Create(Codec codec) {
    std::unique_ptr<webrtc::VideoDecoderFactory> decoder_factory =
        webrtc::CreateBuiltinVideoDecoderFactory();
    std::string codec_name;
    webrtc::VideoCodecType codec_type = webrtc::kVideoCodecAV1;
    switch (codec) {
      case Codec::kAv1:
        codec_name = "AV1";
        codec_type = webrtc::kVideoCodecAV1;
        break;
      case Codec::kVp9:
        codec_name = "VP9";
        codec_type = webrtc::kVideoCodecVP9;
        break;
      case Codec::kVp8:
        codec_name = "VP8";
        codec_type = webrtc::kVideoCodecVP8;
        break;
    }
    std::unique_ptr<webrtc::VideoDecoder> decoder = decoder_factory->Create(
        webrtc::CreateEnvironment(), webrtc::SdpVideoFormat(codec_name));
    if (!decoder) {
      LOG(ERROR) << "Failed to create decoder for quality assessment.";
      return nullptr;
    }

    webrtc::VideoDecoder::Settings dec_settings;
    dec_settings.set_codec_type(codec_type);
    dec_settings.set_number_of_cores(4);
    if (!decoder->Configure(dec_settings)) {
      LOG(ERROR) << "Failed to configure decoder for quality assessment.";
      return nullptr;
    }

    return base::WrapUnique(new BenchmarkVideoDecoder(std::move(decoder)));
  }

  BenchmarkVideoDecoder(const BenchmarkVideoDecoder&) = delete;
  BenchmarkVideoDecoder& operator=(const BenchmarkVideoDecoder&) = delete;

  ~BenchmarkVideoDecoder() {
    if (decoder_) {
      decoder_->Release();
    }
  }

  std::optional<webrtc::VideoFrame> Decode(
      const webrtc::DesktopSize& size,
      size_t frame_index,
      bool key_frame,
      webrtc::scoped_refptr<webrtc::EncodedImageBuffer> encoded_data) {
    const uint32_t rtp_timestamp = static_cast<uint32_t>(frame_index * 3000);
    webrtc::EncodedImage encoded_image;
    encoded_image.SetEncodedData(std::move(encoded_data));
    encoded_image._encodedWidth = size.width();
    encoded_image._encodedHeight = size.height();
    encoded_image.set_frame_type(
        key_frame ? webrtc::VideoFrameType::kVideoFrameKey
                  : webrtc::VideoFrameType::kVideoFrameDelta);
    encoded_image.SetRtpTimestamp(rtp_timestamp);

    if (decoder_->Decode(encoded_image, /*render_time_ms=*/0) != 0) {
      LOG(ERROR) << "Failed to decode frame " << frame_index;
      return std::nullopt;
    }

    std::optional<webrtc::VideoFrame> decoded_frame = callback_.TakeFrame();
    if (!decoded_frame.has_value() ||
        decoded_frame->rtp_timestamp() != rtp_timestamp) {
      return std::nullopt;
    }
    return decoded_frame;
  }

 private:
  class FrameCaptureCallback : public webrtc::DecodedImageCallback {
   public:
    using webrtc::DecodedImageCallback::Decoded;

    int32_t Decoded(webrtc::VideoFrame& decoded_image) override {
      last_frame_ = decoded_image;
      return 0;
    }

    std::optional<webrtc::VideoFrame> TakeFrame() {
      auto out = std::move(last_frame_);
      last_frame_.reset();
      return out;
    }

   private:
    std::optional<webrtc::VideoFrame> last_frame_;
  };

  explicit BenchmarkVideoDecoder(std::unique_ptr<webrtc::VideoDecoder> decoder)
      : decoder_(std::move(decoder)) {
    decoder_->RegisterDecodeCompleteCallback(&callback_);
  }

  // Must be declared before `decoder_` so `callback_` outlives `decoder_`
  // during member destruction.
  FrameCaptureCallback callback_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

struct DecodedYuvFrame {
  webrtc::scoped_refptr<const webrtc::PlanarYuv8Buffer> buffer;
  bool is_i444 = false;
};

std::optional<DecodedYuvFrame> ExtractDecodedYuvFrame(
    const webrtc::VideoFrame& decoded_frame,
    const webrtc::DesktopSize& size,
    ColorSpace target_color_space) {
  auto buf = decoded_frame.video_frame_buffer();
  if (!buf || buf->width() < size.width() || buf->height() < size.height()) {
    return std::nullopt;
  }
  if (target_color_space == ColorSpace::kI444 &&
      buf->type() == webrtc::VideoFrameBuffer::Type::kI444) {
    const webrtc::I444BufferInterface* dec_i444 = buf->GetI444();
    if (!dec_i444) {
      return std::nullopt;
    }
    return DecodedYuvFrame{
        .buffer =
            webrtc::scoped_refptr<const webrtc::PlanarYuv8Buffer>(dec_i444),
        .is_i444 = true,
    };
  }
  webrtc::scoped_refptr<webrtc::I420BufferInterface> dec_i420 = buf->ToI420();
  if (!dec_i420) {
    return std::nullopt;
  }
  return DecodedYuvFrame{
      .buffer = std::move(dec_i420),
      .is_i444 = false,
  };
}

double ComputeFullFramePsnr(const webrtc::DesktopSize& size,
                            bool is_i444,
                            base::span<const uint8_t> ref_y,
                            base::span<const uint8_t> ref_u,
                            base::span<const uint8_t> ref_v,
                            const webrtc::PlanarYuv8Buffer& dec_buf) {
  const int width = size.width();
  const int height = size.height();
  const int uv_w = is_i444 ? width : (width + 1) / 2;
  const int uv_h = is_i444 ? height : (height + 1) / 2;

  uint64_t sse_y = libyuv::ComputeSumSquareErrorPlane(
      ref_y.data(), width, dec_buf.DataY(), dec_buf.StrideY(), width, height);
  uint64_t sse_u = libyuv::ComputeSumSquareErrorPlane(
      ref_u.data(), uv_w, dec_buf.DataU(), dec_buf.StrideU(), uv_w, uv_h);
  uint64_t sse_v = libyuv::ComputeSumSquareErrorPlane(
      ref_v.data(), uv_w, dec_buf.DataV(), dec_buf.StrideV(), uv_w, uv_h);
  uint64_t total_samples = static_cast<uint64_t>(width) * height +
                           2ULL * static_cast<uint64_t>(uv_w) * uv_h;
  return libyuv::SumSquareErrorToPsnr(sse_y + sse_u + sse_v, total_samples);
}

std::optional<double> ComputeDirtyRegionPsnr(
    const webrtc::DesktopRegion& updated_region,
    base::span<const uint8_t> ref_y,
    int ref_stride_y,
    base::span<const uint8_t> dec_y,
    int dec_stride_y) {
  if (updated_region.is_empty()) {
    return std::nullopt;
  }
  uint64_t dirty_sse_y = 0;
  uint64_t dirty_samples = 0;
  for (webrtc::DesktopRegion::Iterator r(updated_region); !r.IsAtEnd();
       r.Advance()) {
    const webrtc::DesktopRect& rect = r.rect();
    const size_t ref_offset =
        base::checked_cast<size_t>(rect.top()) * ref_stride_y +
        base::checked_cast<size_t>(rect.left());
    const size_t dec_offset =
        base::checked_cast<size_t>(rect.top()) * dec_stride_y +
        base::checked_cast<size_t>(rect.left());
    dirty_sse_y += libyuv::ComputeSumSquareErrorPlane(
        ref_y.subspan(ref_offset).data(), ref_stride_y,
        dec_y.subspan(dec_offset).data(), dec_stride_y, rect.width(),
        rect.height());
    dirty_samples += static_cast<uint64_t>(rect.width()) * rect.height();
  }
  if (dirty_samples == 0) {
    return std::nullopt;
  }
  return libyuv::SumSquareErrorToPsnr(dirty_sse_y, dirty_samples);
}

QualityStats EvaluateDecodedQuality(
    const EncoderConfig& encoder_config,
    const webrtc::DesktopSize& size,
    base::span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
        preloaded_frames,
    base::span<FrameMetric> frame_metrics) {
  QualityStats stats;
  if (frame_metrics.empty() || preloaded_frames.size() < frame_metrics.size()) {
    return stats;
  }

  std::unique_ptr<BenchmarkVideoDecoder> decoder =
      BenchmarkVideoDecoder::Create(encoder_config.codec);
  if (!decoder) {
    return stats;
  }

  const int width = size.width();
  const int height = size.height();
  const webrtc::DesktopRect full_rect =
      webrtc::DesktopRect::MakeWH(width, height);
  const size_t y_rows = static_cast<size_t>(height);
  const size_t y_plane_size = static_cast<size_t>(width) * y_rows;
  std::vector<uint8_t> ref_y(y_plane_size);
  std::vector<uint8_t> ref_u(y_plane_size);
  std::vector<uint8_t> ref_v(y_plane_size);

  double sum_psnr = 0.0;
  double min_psnr = 128.0;
  double final_psnr = 0.0;
  double sum_dirty_psnr = 0.0;
  double min_dirty_psnr = 128.0;
  double sum_ssim = 0.0;
  double min_ssim = 1.0;
  double final_ssim = 0.0;
  size_t ssim_count = 0;

  for (size_t i = 0; i < frame_metrics.size(); ++i) {
    auto& m = frame_metrics[i];
    webrtc::scoped_refptr<webrtc::EncodedImageBuffer> encoded_data =
        std::move(m.encoded_data);
    if (!encoded_data || encoded_data->size() == 0 ||
        m.frame_index >= preloaded_frames.size()) {
      continue;
    }

    std::optional<webrtc::VideoFrame> decoded_frame = decoder->Decode(
        size, m.frame_index, m.key_frame, std::move(encoded_data));
    if (!decoded_frame.has_value()) {
      continue;
    }

    std::optional<DecodedYuvFrame> decoded_yuv = ExtractDecodedYuvFrame(
        *decoded_frame, size, encoder_config.color_space);
    if (!decoded_yuv.has_value()) {
      continue;
    }

    const webrtc::DesktopFrame& src_frame = *preloaded_frames[m.frame_index];
    const int uv_w = decoded_yuv->is_i444 ? width : (width + 1) / 2;
    ConvertDesktopFrameRectToYuv(src_frame, full_rect, decoded_yuv->is_i444,
                                 ref_y, width, ref_u, uv_w, ref_v, uv_w);

    const webrtc::PlanarYuv8Buffer& dec_buf = *decoded_yuv->buffer;
    double frame_psnr = ComputeFullFramePsnr(size, decoded_yuv->is_i444, ref_y,
                                             ref_u, ref_v, dec_buf);

    sum_psnr += frame_psnr;
    min_psnr = std::min(min_psnr, frame_psnr);
    final_psnr = frame_psnr;
    stats.frames_decoded++;

    // Compute Y-plane PSNR specifically over delta-frame updated_region()
    // rectangles to catch skipped active-map blocks or dirty-rect conversion
    // bugs without dilution from unchanged background pixels or initial
    // keyframe CBR quantization.
    if (!m.key_frame) {
      // SAFETY: `dec_buf` is a `webrtc::PlanarYuv8Buffer` whose height was
      // verified to be at least `height` (`y_rows`) in
      // `ExtractDecodedYuvFrame()`, guaranteeing at least
      // `dec_buf.StrideY() * y_rows` valid bytes.
      auto dec_y_span = UNSAFE_BUFFERS(base::span(
          dec_buf.DataY(), static_cast<size_t>(dec_buf.StrideY()) * y_rows));
      if (std::optional<double> dirty_psnr =
              ComputeDirtyRegionPsnr(src_frame.updated_region(), ref_y, width,
                                     dec_y_span, dec_buf.StrideY())) {
        sum_dirty_psnr += *dirty_psnr;
        min_dirty_psnr = std::min(min_dirty_psnr, *dirty_psnr);
        stats.dirty_frames_decoded++;
      }
    }

    // Compute Y-plane SSIM on keyframe, final frame, and every 5th frame.
    if (m.key_frame || i + 1 == frame_metrics.size() || (i % 5 == 0)) {
      double frame_ssim =
          libyuv::CalcFrameSsim(ref_y.data(), width, dec_buf.DataY(),
                                dec_buf.StrideY(), width, height);
      sum_ssim += frame_ssim;
      min_ssim = std::min(min_ssim, frame_ssim);
      final_ssim = frame_ssim;
      ssim_count++;
    }
  }

  if (stats.frames_decoded > 0) {
    stats.mean_psnr_db = sum_psnr / stats.frames_decoded;
    stats.min_psnr_db = min_psnr;
    stats.final_frame_psnr_db = final_psnr;
  }
  if (stats.dirty_frames_decoded > 0) {
    stats.dirty_region_mean_psnr_db =
        sum_dirty_psnr / stats.dirty_frames_decoded;
    stats.dirty_region_min_psnr_db = min_dirty_psnr;
  }
  if (ssim_count > 0) {
    stats.mean_ssim = sum_ssim / ssim_count;
    stats.min_ssim = min_ssim;
    stats.final_frame_ssim = final_ssim;
  }
  return stats;
}

base::DictValue SummarizeAndPrintResults(const EncoderConfig& encoder_config,
                                         const RuntimeParams& runtime_params,
                                         const RunResults& run_results) {
  const auto& frame_metrics = run_results.frame_metrics;
  const auto& quality_stats = run_results.quality_stats;
  std::vector<double> all_times_ms;
  std::vector<double> keyframe_times_ms;
  std::vector<double> delta_times_ms;

  std::vector<size_t> all_sizes;
  std::vector<size_t> keyframe_sizes;
  std::vector<size_t> delta_sizes;

  std::vector<int> all_quantizers;
  std::vector<int> delta_quantizers;

  all_times_ms.reserve(frame_metrics.size());
  all_sizes.reserve(frame_metrics.size());
  all_quantizers.reserve(frame_metrics.size());

  for (const auto& m : frame_metrics) {
    all_times_ms.push_back(m.encode_time_ms);
    all_sizes.push_back(m.size_bytes);
    all_quantizers.push_back(m.quantizer);
    if (m.key_frame) {
      keyframe_times_ms.push_back(m.encode_time_ms);
      keyframe_sizes.push_back(m.size_bytes);
    } else {
      delta_times_ms.push_back(m.encode_time_ms);
      delta_sizes.push_back(m.size_bytes);
      delta_quantizers.push_back(m.quantizer);
    }
  }

  LatencyStats all_latency = ComputeLatencyStats(all_times_ms);
  LatencyStats kf_latency = ComputeLatencyStats(keyframe_times_ms);
  LatencyStats delta_latency = ComputeLatencyStats(delta_times_ms);

  SizeStats all_size_stats = ComputeSizeStats(all_sizes);
  SizeStats kf_size_stats = ComputeSizeStats(keyframe_sizes);
  SizeStats delta_size_stats = ComputeSizeStats(delta_sizes);

  QuantizerStats all_qp_stats = ComputeQuantizerStats(all_quantizers);
  QuantizerStats delta_qp_stats = ComputeQuantizerStats(delta_quantizers);

  double wall_clock_ms =
      (run_results.end_time - run_results.start_time).InMillisecondsF();
  double wall_clock_fps = (wall_clock_ms > 0.0 && !frame_metrics.empty())
                              ? (frame_metrics.size() * 1000.0) / wall_clock_ms
                              : 0.0;
  double encoder_fps =
      (all_latency.total_ms > 0.0 && !frame_metrics.empty())
          ? (frame_metrics.size() * 1000.0) / all_latency.total_ms
          : 0.0;
  double updated_area_pct =
      (run_results.total_pixels > 0)
          ? (run_results.updated_pixels * 100.0) / run_results.total_pixels
          : 0.0;
  double achieved_bitrate_kbps =
      (!frame_metrics.empty() && runtime_params.fps > 0.0)
          ? (static_cast<double>(all_size_stats.total_bytes) * 8.0 *
             runtime_params.fps) /
                (frame_metrics.size() * 1000.0)
          : 0.0;

  std::ios old_state(nullptr);
  old_state.copyfmt(std::cout);
  std::cout << std::fixed << std::setprecision(2);

  std::cout << "=================================================="
            << std::endl;
  std::cout << "RESULTS: " << encoder_config.name << " ("
            << CodecToString(encoder_config.codec) << ", "
            << runtime_params.size.width() << "x"
            << runtime_params.size.height() << ", " << runtime_params.scenario
            << ", " << runtime_params.fps << " FPS)" << std::endl;
  std::cout << "Config:  Profile "
            << (encoder_config.profile == Profile::k1 ? "1 (I444)" : "0 (I420)")
            << " | Speed " << encoder_config.encoder_speed << " | Active Map: "
            << (encoder_config.use_active_map ? "enabled" : "disabled")
            << " | Target Bitrate: " << encoder_config.bitrate_kbps << " kbps"
            << std::endl;
  std::cout << "--------------------------------------------------"
            << std::endl;
  std::cout << "Frames encoded:           " << frame_metrics.size() << " ("
            << kf_latency.count << " key, " << delta_latency.count << " delta)"
            << std::endl;
  std::cout << "Average updated area:     " << updated_area_pct << "%"
            << std::endl;
  std::cout << "Wall-clock time:          " << wall_clock_ms << " ms ("
            << wall_clock_fps << " FPS)" << std::endl;
  std::cout << "Encoder-only time:        " << all_latency.total_ms << " ms ("
            << encoder_fps << " FPS)" << std::endl;
  std::cout << "--------------------------------------------------"
            << std::endl;
  std::cout << "Encode Latency (All):     mean=" << all_latency.mean_ms
            << " ms | p50=" << all_latency.p50_ms
            << " ms | p90=" << all_latency.p90_ms
            << " ms | p99=" << all_latency.p99_ms
            << " ms | max=" << all_latency.max_ms << " ms" << std::endl;
  if (kf_latency.count > 0) {
    std::cout << "Encode Latency (Key):     mean=" << kf_latency.mean_ms
              << " ms | max=" << kf_latency.max_ms << " ms" << std::endl;
  }
  if (delta_latency.count > 0) {
    std::cout << "Encode Latency (Delta):   mean=" << delta_latency.mean_ms
              << " ms | p50=" << delta_latency.p50_ms
              << " ms | p90=" << delta_latency.p90_ms
              << " ms | p99=" << delta_latency.p99_ms
              << " ms | max=" << delta_latency.max_ms << " ms" << std::endl;
  }
  std::cout << "--------------------------------------------------"
            << std::endl;
  std::cout << "Stream Bitrate:           " << achieved_bitrate_kbps
            << " kbps (target: " << encoder_config.bitrate_kbps << " kbps)"
            << std::endl;
  std::cout << "Total Encoded Size:       " << all_size_stats.total_bytes
            << " bytes (" << (all_size_stats.total_bytes / 1024.0) << " KiB)"
            << std::endl;
  std::cout << "Frame Size (All):         avg=" << all_size_stats.avg_bytes
            << " B | max=" << all_size_stats.max_bytes << " B" << std::endl;
  if (kf_size_stats.count > 0) {
    std::cout << "Frame Size (Key):         avg=" << kf_size_stats.avg_bytes
              << " B | max=" << kf_size_stats.max_bytes << " B" << std::endl;
  }
  if (delta_size_stats.count > 0) {
    std::cout << "Frame Size (Delta):       avg=" << delta_size_stats.avg_bytes
              << " B | max=" << delta_size_stats.max_bytes << " B" << std::endl;
  }
  if (all_qp_stats.count > 0) {
    std::cout << "Quantizer / QP (All):     avg=" << all_qp_stats.avg_qp
              << " | min=" << all_qp_stats.min_qp
              << " | max=" << all_qp_stats.max_qp << std::endl;
  }
  if (delta_qp_stats.count > 0) {
    std::cout << "Quantizer / QP (Delta):   avg=" << delta_qp_stats.avg_qp
              << " | min=" << delta_qp_stats.min_qp
              << " | max=" << delta_qp_stats.max_qp << std::endl;
  }
  if (quality_stats.frames_decoded > 0) {
    std::cout << "Decoded Quality (PSNR):   mean=" << quality_stats.mean_psnr_db
              << " dB | min=" << quality_stats.min_psnr_db
              << " dB | final=" << quality_stats.final_frame_psnr_db << " dB";
    if (quality_stats.dirty_frames_decoded > 0) {
      std::cout << " | dirty_mean=" << quality_stats.dirty_region_mean_psnr_db
                << " dB | dirty_min=" << quality_stats.dirty_region_min_psnr_db
                << " dB";
    }
    std::cout << std::endl;
    std::cout << std::setprecision(4)
              << "Decoded Quality (SSIM):   mean=" << quality_stats.mean_ssim
              << " | min=" << quality_stats.min_ssim
              << " | final=" << quality_stats.final_frame_ssim
              << std::setprecision(2) << std::endl;
  }
  std::cout << "=================================================="
            << std::endl;

  std::cout.copyfmt(old_state);

  base::DictValue result;
  result.Set("encoder_name", encoder_config.name);
  result.Set("codec", CodecToString(encoder_config.codec));
  result.Set("profile", encoder_config.profile == Profile::k1 ? 1 : 0);
  result.Set("color_space",
             encoder_config.color_space == ColorSpace::kI444 ? "I444" : "I420");
  result.Set("encoder_speed", encoder_config.encoder_speed);
  result.Set("use_active_map", encoder_config.use_active_map);
  result.Set("target_bitrate_kbps", encoder_config.bitrate_kbps);
  result.Set("scenario", runtime_params.scenario);
  result.Set("width", runtime_params.size.width());
  result.Set("height", runtime_params.size.height());
  result.Set("target_fps", runtime_params.fps);

  result.Set("frames_encoded", static_cast<int>(frame_metrics.size()));
  result.Set("keyframes_encoded", static_cast<int>(kf_latency.count));
  result.Set("delta_frames_encoded", static_cast<int>(delta_latency.count));
  result.Set("updated_area_percent", updated_area_pct);
  result.Set("wall_clock_time_ms", wall_clock_ms);
  result.Set("wall_clock_fps", wall_clock_fps);
  result.Set("encoder_time_ms", all_latency.total_ms);
  result.Set("encoder_fps", encoder_fps);
  result.Set("achieved_bitrate_kbps", achieved_bitrate_kbps);
  result.Set("total_encoded_bytes",
             static_cast<double>(all_size_stats.total_bytes));

  auto latency_to_dict = [](const LatencyStats& s) {
    base::DictValue d;
    d.Set("count", static_cast<int>(s.count));
    d.Set("total_ms", s.total_ms);
    d.Set("mean_ms", s.mean_ms);
    d.Set("p50_ms", s.p50_ms);
    d.Set("p90_ms", s.p90_ms);
    d.Set("p99_ms", s.p99_ms);
    d.Set("max_ms", s.max_ms);
    return d;
  };
  result.Set("latency_all", latency_to_dict(all_latency));
  result.Set("latency_keyframe", latency_to_dict(kf_latency));
  result.Set("latency_delta", latency_to_dict(delta_latency));

  auto size_to_dict = [](const SizeStats& s) {
    base::DictValue d;
    d.Set("count", static_cast<int>(s.count));
    d.Set("total_bytes", static_cast<double>(s.total_bytes));
    d.Set("avg_bytes", s.avg_bytes);
    d.Set("max_bytes", static_cast<double>(s.max_bytes));
    return d;
  };
  result.Set("frame_size_all", size_to_dict(all_size_stats));
  result.Set("frame_size_keyframe", size_to_dict(kf_size_stats));
  result.Set("frame_size_delta", size_to_dict(delta_size_stats));

  auto qp_to_dict = [](const QuantizerStats& s) {
    base::DictValue d;
    d.Set("count", static_cast<int>(s.count));
    d.Set("avg_qp", s.avg_qp);
    d.Set("min_qp", s.min_qp);
    d.Set("max_qp", s.max_qp);
    return d;
  };
  result.Set("quantizer_all", qp_to_dict(all_qp_stats));
  result.Set("quantizer_delta", qp_to_dict(delta_qp_stats));

  base::DictValue quality_dict;
  quality_dict.Set("frames_decoded",
                   static_cast<int>(quality_stats.frames_decoded));
  quality_dict.Set("dirty_frames_decoded",
                   static_cast<int>(quality_stats.dirty_frames_decoded));
  quality_dict.Set("mean_psnr_db", quality_stats.mean_psnr_db);
  quality_dict.Set("min_psnr_db", quality_stats.min_psnr_db);
  quality_dict.Set("final_frame_psnr_db", quality_stats.final_frame_psnr_db);
  quality_dict.Set("dirty_region_mean_psnr_db",
                   quality_stats.dirty_region_mean_psnr_db);
  quality_dict.Set("dirty_region_min_psnr_db",
                   quality_stats.dirty_region_min_psnr_db);
  quality_dict.Set("mean_ssim", quality_stats.mean_ssim);
  quality_dict.Set("min_ssim", quality_stats.min_ssim);
  quality_dict.Set("final_frame_ssim", quality_stats.final_frame_ssim);
  result.Set("quality", std::move(quality_dict));

  return result;
}

class EncoderBenchmark {
 public:
  EncoderBenchmark() {
    capture_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    encode_task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  ~EncoderBenchmark() {
    // Flush both runners to ensure no use-after-free.
    base::RunLoop capture_loop;
    capture_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           capture_loop.QuitClosure());
    capture_loop.Run();

    base::RunLoop encode_loop;
    encode_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::DestroyEncoderOnEncodeThread,
                       base::Unretained(this)),
        encode_loop.QuitClosure());
    encode_loop.Run();
  }

  base::DictValue Run(
      const EncoderConfig& encoder_config,
      const RuntimeParams& runtime_params,
      base::span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
          preloaded_frames) {
    encoder_config_ = encoder_config;
    runtime_params_ = runtime_params;
    encoder_config_.bitrate_kbps = GetTargetBitrateKbps(
        encoder_config_.bitrate_kbps, runtime_params_.size);
    preloaded_frames_ = preloaded_frames;
    next_frame_index_ = 0;
    frames_encoded_ = 0;
    top_off_active_ = false;
    run_results_ = RunResults();
    run_results_.frame_metrics.reserve(preloaded_frames.size());

    if (preloaded_frames.empty()) {
      LOG(ERROR) << "No frames available to encode.";
      return base::DictValue();
    }

    frames_to_encode_ = static_cast<int>(preloaded_frames.size());

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

    // Ensure encoder is initialized and warmed up before starting the timer.
    // RunLoop::QuitClosure() is thread-safe and is invoked on
    // `encode_task_runner_` once the warm-up encode and encoder creation
    // complete.
    base::RunLoop init_loop;
    encode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::InitializeEncoderOnEncodeThread,
                       base::Unretained(this),
                       base::ScopedClosureRunner(init_loop.QuitClosure())));
    init_loop.Run();

    if (!encoder_initialized_) {
      LOG(ERROR) << "Failed to initialize encoder: " << encoder_config_.name;
      preloaded_frames_ = {};
      run_results_ = RunResults();
      return base::DictValue();
    }

    std::cout << "Running encoder '" << encoder_config_.name << "'..."
              << std::endl;
    run_results_.start_time = base::TimeTicks::Now();

    capture_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::CaptureNextFrameOnCaptureThread,
                       base::Unretained(this)));

    run_loop.Run();
    run_results_.end_time = base::TimeTicks::Now();

    // Destroy the encoder between runs to ensure clean state.
    base::RunLoop destroy_loop;
    encode_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::DestroyEncoderOnEncodeThread,
                       base::Unretained(this)),
        destroy_loop.QuitClosure());
    destroy_loop.Run();

    run_results_.quality_stats =
        EvaluateDecodedQuality(encoder_config_, runtime_params_.size,
                               preloaded_frames_, run_results_.frame_metrics);
    preloaded_frames_ = {};
    base::DictValue summary = SummarizeAndPrintResults(
        encoder_config_, runtime_params_, run_results_);
    run_results_ = RunResults();
    return summary;
  }

 private:
  void InitializeEncoderOnEncodeThread(
      base::ScopedClosureRunner on_initialized) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    warmup_encoder_ = CreateEncoder(encoder_config_);
    if (!warmup_encoder_) {
      encoder_initialized_ = false;
      return;
    }

    // Run a single warm-up encode on a temporary instance to warm up the
    // encode thread, SIMD dispatch tables, and allocator caches without
    // mutating the rate-control state of |encoder_|.
    WebrtcVideoEncoder::FrameParams warmup_params;
    warmup_params.bitrate_kbps = encoder_config_.bitrate_kbps;
    warmup_params.duration = base::Hertz(runtime_params_.fps);
    warmup_params.fps = static_cast<int>(std::round(runtime_params_.fps));
    warmup_params.key_frame = true;
    warmup_params.clear_active_map = true;
    warmup_encoder_->Encode(
        preloaded_frames_[0]->Share(), warmup_params,
        base::BindOnce(&EncoderBenchmark::OnWarmupEncodedOnEncodeThread,
                       base::Unretained(this), std::move(on_initialized)));
  }

  void OnWarmupEncodedOnEncodeThread(
      base::ScopedClosureRunner on_initialized,
      WebrtcVideoEncoder::EncodeResult /*result*/,
      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> /*frame*/) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    // Defer resetting |warmup_encoder_| and creating |encoder_| to a follow-up
    // task on |encode_task_runner_| so synchronous encoders finish unwinding
    // Encode() before |warmup_encoder_| is destroyed, and two encoder instances
    // are never alive simultaneously.
    encode_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::FinishWarmupOnEncodeThread,
                       base::Unretained(this), std::move(on_initialized)));
  }

  void FinishWarmupOnEncodeThread(base::ScopedClosureRunner on_initialized) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    warmup_encoder_.reset();
    encoder_ = CreateEncoder(encoder_config_);
    encoder_initialized_ = (encoder_ != nullptr);
  }

  void DestroyEncoderOnEncodeThread() {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    warmup_encoder_.reset();
    encoder_.reset();
    encoder_initialized_ = false;
  }

  void CaptureNextFrameOnCaptureThread() {
    DCHECK(capture_task_runner_->RunsTasksInCurrentSequence());
    CHECK_LT(next_frame_index_, preloaded_frames_.size());

    size_t frame_index = next_frame_index_++;
    std::unique_ptr<webrtc::DesktopFrame> frame =
        preloaded_frames_[frame_index]->Share();

    WebrtcVideoEncoder::FrameParams params;
    params.bitrate_kbps = encoder_config_.bitrate_kbps;
    params.duration = base::Hertz(runtime_params_.fps);
    params.fps = static_cast<int>(std::round(runtime_params_.fps));
    params.key_frame = (frame_index == 0);
    params.clear_active_map = !top_off_active_;

    // Stats are tracked on capture thread.
    // Use int64_t to prevent overflow for large dimensions.
    run_results_.total_pixels += static_cast<int64_t>(frame->size().width()) *
                                 static_cast<int64_t>(frame->size().height());
    for (webrtc::DesktopRegion::Iterator r(frame->updated_region());
         !r.IsAtEnd(); r.Advance()) {
      run_results_.updated_pixels += static_cast<int64_t>(r.rect().width()) *
                                     static_cast<int64_t>(r.rect().height());
    }

    encode_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EncoderBenchmark::EncodeFrameOnEncodeThread,
                                  base::Unretained(this), frame_index,
                                  std::move(frame), params));
  }

  void EncodeFrameOnEncodeThread(
      size_t frame_index,
      std::unique_ptr<webrtc::DesktopFrame> frame,
      const WebrtcVideoEncoder::FrameParams& params) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    if (!encoder_) {
      LOG(ERROR) << "Encoder was not initialized.";
      main_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EncoderBenchmark::OnFrameEncodedCompleteOnMainThread,
                         base::Unretained(this)));
      return;
    }
    encoder_->Encode(
        std::move(frame), params,
        base::BindOnce(&EncoderBenchmark::OnFrameEncodedOnEncodeThread,
                       base::Unretained(this), frame_index,
                       base::TimeTicks::Now()));
  }

  void OnFrameEncodedOnEncodeThread(
      size_t frame_index,
      base::TimeTicks encode_start_time,
      WebrtcVideoEncoder::EncodeResult result,
      std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame) {
    DCHECK(encode_task_runner_->RunsTasksInCurrentSequence());
    base::TimeDelta elapsed = base::TimeTicks::Now() - encode_start_time;
    if (result == WebrtcVideoEncoder::EncodeResult::SUCCEEDED && frame) {
      FrameMetric metric;
      metric.frame_index = frame_index;
      metric.encode_time_ms = elapsed.InMillisecondsF();
      metric.size_bytes = frame->data ? frame->data->size() : 0;
      metric.quantizer = frame->quantizer;
      metric.key_frame = frame->key_frame;
      metric.encoded_data = frame->data;
      run_results_.frame_metrics.push_back(std::move(metric));
      top_off_active_ = (frame->quantizer > WebrtcVideoEncoder::kMinQuantizer);
    } else {
      LOG(ERROR) << "Encoder failed to encode frame.";
    }
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&EncoderBenchmark::OnFrameEncodedCompleteOnMainThread,
                       base::Unretained(this)));
  }

  void OnFrameEncodedCompleteOnMainThread() {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    frames_encoded_++;
    if (frames_encoded_ >= frames_to_encode_) {
      quit_closure_.Run();
    } else {
      capture_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EncoderBenchmark::CaptureNextFrameOnCaptureThread,
                         base::Unretained(this)));
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::unique_ptr<WebrtcVideoEncoder> warmup_encoder_;
  std::unique_ptr<WebrtcVideoEncoder> encoder_;
  bool encoder_initialized_ = false;

  EncoderConfig encoder_config_;
  RuntimeParams runtime_params_;

  base::raw_span<const std::unique_ptr<webrtc::SharedDesktopFrame>>
      preloaded_frames_;
  size_t next_frame_index_ = 0;
  int frames_to_encode_ = 0;
  int frames_encoded_ = 0;
  bool top_off_active_ = false;
  RunResults run_results_;
  base::RepeatingClosure quit_closure_;
};

void PrintHelp(const char* executable_name) {
  std::cout << "Usage: " << executable_name << R"( [options]

Options:
  --config=<path>        Path to JSON benchmark configuration file
  --chrome-path=<path>   Override path to the Chrome/Chromium binary
  --frame-dir=<path>     Override path to pre-generated PNG frames
  --output-json=<path>   Write benchmark results to the specified JSON file
  --help                 Print this help message and JSON schema

Example JSON configuration:
{
  "frames": 100,
  "fps": 30.0,
  "bitrate_kbps": 5000,
  "sizes": ["1920x1080", "2560x1440", "3840x2160"],
  "scenarios": ["desktop_clock", "scroll_vertical"],
  "encoders": [
    {
      "name": "av1_s11_no_am",
      "codec": "av1",
      "profile": 0,
      "encoder_speed": 11,
      "use_active_map": false
    },
    {
      "name": "vp9_s9_i420",
      "codec": "vp9",
      "profile": 0,
      "encoder_speed": 9
    },
    {
      "name": "vp8_default",
      "codec": "vp8"
    }
  ]
}
)";
}

std::vector<std::string> GetDefaultScenarios() {
  return {"desktop_clock", "scroll_vertical", "scroll_horizontal",
          "moving_window", "busy_developer",  "low_motion",
          "high_motion",   "chaos_stress"};
}

std::vector<webrtc::DesktopSize> GetDefaultSizes() {
  return {{1920, 1080}, {2560, 1440}, {3840, 2160}};
}

bool ParseSizeWxH(std::string_view size_str, webrtc::DesktopSize& out_size) {
  std::vector<std::string_view> dims = base::SplitStringPiece(
      size_str, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (dims.size() != 2) {
    LOG(ERROR) << "Invalid size '" << size_str
               << "'. Expected 'WxH' format (e.g., '1920x1080').";
    return false;
  }
  int width = 0;
  int height = 0;
  if (!base::StringToInt(dims[0], &width) ||
      !base::StringToInt(dims[1], &height) ||
      width < WebrtcVideoEncoder::kMinDimension ||
      height < WebrtcVideoEncoder::kMinDimension) {
    LOG(ERROR) << "Invalid size dimensions '" << size_str
               << "'. Width and height must be integers >= "
               << WebrtcVideoEncoder::kMinDimension << ".";
    return false;
  }
  out_size = webrtc::DesktopSize(width, height);
  return true;
}

bool ParseEncoderConfig(const base::DictValue& dict,
                        int default_bitrate_kbps,
                        EncoderConfig& out_config) {
  static constexpr auto kAllowedKeys = base::MakeFixedFlatSet<std::string_view>(
      {"name", "codec", "profile", "encoder_speed", "use_active_map",
       "bitrate_kbps"});
  for (const auto [key, _] : dict) {
    if (!kAllowedKeys.contains(key)) {
      LOG(ERROR) << "Unknown key '" << key << "' in encoder configuration.";
      return false;
    }
  }

  const std::string* codec_ptr = dict.FindString("codec");
  if (!codec_ptr) {
    LOG(ERROR) << "Encoder configuration is missing required 'codec' string.";
    return false;
  }
  std::string codec_str = base::ToLowerASCII(*codec_ptr);
  if (codec_str == "av1") {
    out_config.codec = Codec::kAv1;
  } else if (codec_str == "vp8") {
    out_config.codec = Codec::kVp8;
  } else if (codec_str == "vp9") {
    out_config.codec = Codec::kVp9;
  } else {
    LOG(ERROR) << "Invalid codec '" << *codec_ptr
               << "'. Supported codecs: av1, vp8, vp9.";
    return false;
  }

  int profile_val = 0;
  if (const base::Value* profile_value = dict.Find("profile")) {
    if (!profile_value->is_int()) {
      LOG(ERROR) << "'profile' must be an integer (0 or 1).";
      return false;
    }
    profile_val = profile_value->GetInt();
    if (profile_val != 0 && profile_val != 1) {
      LOG(ERROR) << "Invalid profile " << profile_val
                 << ". Only 0 and 1 are supported.";
      return false;
    }
  }
  if (out_config.codec == Codec::kVp8 && profile_val != 0) {
    LOG(ERROR) << "VP8 only supports profile 0.";
    return false;
  }
  out_config.profile = (profile_val == 1) ? Profile::k1 : Profile::k0;
  out_config.color_space = (out_config.profile == Profile::k1)
                               ? ColorSpace::kI444
                               : ColorSpace::kI420;

  if (const base::Value* speed_value = dict.Find("encoder_speed")) {
    if (!speed_value->is_int()) {
      LOG(ERROR) << "'encoder_speed' must be an integer.";
      return false;
    }
    int speed = speed_value->GetInt();
    if (out_config.codec == Codec::kAv1) {
      if (speed < WebrtcVideoEncoderAV1::kMinEncoderSpeed ||
          speed > WebrtcVideoEncoderAV1::kMaxEncoderSpeed) {
        LOG(ERROR) << "AV1 encoder_speed must be between "
                   << WebrtcVideoEncoderAV1::kMinEncoderSpeed << " and "
                   << WebrtcVideoEncoderAV1::kMaxEncoderSpeed << " (got "
                   << speed << ").";
        return false;
      }
      out_config.encoder_speed = speed;
    } else if (out_config.codec == Codec::kVp9) {
      if (speed < WebrtcVideoEncoderVpx::kVp9MinEncoderSpeed ||
          speed > WebrtcVideoEncoderVpx::kVp9MaxEncoderSpeed) {
        LOG(ERROR) << "VP9 encoder_speed must be between "
                   << WebrtcVideoEncoderVpx::kVp9MinEncoderSpeed << " and "
                   << WebrtcVideoEncoderVpx::kVp9MaxEncoderSpeed << " (got "
                   << speed << ").";
        return false;
      }
      out_config.encoder_speed = speed;
    } else {
      LOG(ERROR) << "VP8 does not support configurable 'encoder_speed'.";
      return false;
    }
  } else {
    switch (out_config.codec) {
      case Codec::kAv1:
        out_config.encoder_speed = WebrtcVideoEncoderAV1::kDefaultEncoderSpeed;
        break;
      case Codec::kVp9:
        out_config.encoder_speed =
            WebrtcVideoEncoderVpx::kVp9DefaultEncoderSpeed;
        break;
      case Codec::kVp8:
        out_config.encoder_speed =
            WebrtcVideoEncoderVpx::kVp8DefaultEncoderSpeed;
        break;
    }
  }

  if (const base::Value* active_map_value = dict.Find("use_active_map")) {
    if (!active_map_value->is_bool()) {
      LOG(ERROR) << "'use_active_map' must be a boolean.";
      return false;
    }
    if (out_config.codec != Codec::kAv1) {
      LOG(ERROR) << "'use_active_map' can only be configured for AV1 (always "
                    "enabled for VP8 and VP9).";
      return false;
    }
    out_config.use_active_map = active_map_value->GetBool();
  } else {
    out_config.use_active_map = (out_config.codec != Codec::kAv1);
  }

  out_config.bitrate_kbps = default_bitrate_kbps;
  if (const base::Value* bitrate_value = dict.Find("bitrate_kbps")) {
    if (!bitrate_value->is_int() || bitrate_value->GetInt() <= 0) {
      LOG(ERROR) << "'bitrate_kbps' must be a positive integer.";
      return false;
    }
    out_config.bitrate_kbps = bitrate_value->GetInt();
  }

  if (const std::string* name_ptr = dict.FindString("name")) {
    if (name_ptr->empty()) {
      LOG(ERROR) << "'name' cannot be empty when specified.";
      return false;
    }
    out_config.name = *name_ptr;
  } else {
    out_config.name = DefaultEncoderName(out_config);
  }

  return true;
}

bool LoadJsonConfigFile(const base::FilePath& config_path,
                        BenchmarkConfig& config) {
  std::string json_content;
  if (!base::ReadFileToString(config_path, &json_content)) {
    LOG(ERROR) << "Failed to read config file: " << config_path;
    return false;
  }

  auto parsed = base::JSONReader::ReadAndReturnValueWithError(
      json_content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!parsed.has_value()) {
    LOG(ERROR) << "Failed to parse JSON config " << config_path << " (line "
               << parsed.error().line << ", col " << parsed.error().column
               << "): " << parsed.error().message;
    return false;
  }
  if (!parsed->is_dict()) {
    LOG(ERROR) << "Root of JSON config must be an object.";
    return false;
  }

  const base::DictValue& root = parsed->GetDict();
  static constexpr auto kAllowedTopKeys =
      base::MakeFixedFlatSet<std::string_view>(
          {"frames", "fps", "bitrate_kbps", "sizes", "scenarios", "encoders",
           "frame_dir", "chrome_path", "output_json"});
  for (const auto [key, _] : root) {
    if (!kAllowedTopKeys.contains(key)) {
      LOG(ERROR) << "Unknown top-level key '" << key << "' in JSON config.";
      return false;
    }
  }

  if (const base::Value* frames_val = root.Find("frames")) {
    if (!frames_val->is_int() || frames_val->GetInt() <= 0) {
      LOG(ERROR) << "'frames' must be a positive integer.";
      return false;
    }
    config.frames = frames_val->GetInt();
  }

  if (const base::Value* fps_val = root.Find("fps")) {
    if ((!fps_val->is_double() && !fps_val->is_int()) ||
        fps_val->GetDouble() <= 0.0) {
      LOG(ERROR) << "'fps' must be a positive number.";
      return false;
    }
    config.fps = fps_val->GetDouble();
  }

  if (const base::Value* bitrate_val = root.Find("bitrate_kbps")) {
    if (!bitrate_val->is_int() || bitrate_val->GetInt() <= 0) {
      LOG(ERROR) << "'bitrate_kbps' must be a positive integer.";
      return false;
    }
    config.bitrate_kbps = bitrate_val->GetInt();
  }

  if (const std::string* frame_dir = root.FindString("frame_dir")) {
    config.frame_dir = base::FilePath::FromUTF8Unsafe(*frame_dir);
  }

  if (const std::string* chrome_path = root.FindString("chrome_path")) {
    config.chrome_path = base::FilePath::FromUTF8Unsafe(*chrome_path);
  }

  if (const std::string* output_json = root.FindString("output_json")) {
    config.output_json_path = base::FilePath::FromUTF8Unsafe(*output_json);
  }

  if (const base::Value* sizes_val = root.Find("sizes")) {
    if (!sizes_val->is_list() || sizes_val->GetList().empty()) {
      LOG(ERROR) << "'sizes' must be a non-empty array of 'WxH' strings.";
      return false;
    }
    config.sizes.clear();
    for (const base::Value& item : sizes_val->GetList()) {
      if (!item.is_string()) {
        LOG(ERROR) << "Each entry in 'sizes' must be a 'WxH' string.";
        return false;
      }
      webrtc::DesktopSize parsed_size;
      if (!ParseSizeWxH(item.GetString(), parsed_size)) {
        return false;
      }
      config.sizes.push_back(parsed_size);
    }
  }

  if (const base::Value* scenarios_val = root.Find("scenarios")) {
    if (!scenarios_val->is_list() || scenarios_val->GetList().empty()) {
      LOG(ERROR) << "'scenarios' must be a non-empty array of strings.";
      return false;
    }
    config.scenarios.clear();
    for (const base::Value& item : scenarios_val->GetList()) {
      if (!item.is_string() || item.GetString().empty()) {
        LOG(ERROR) << "Each entry in 'scenarios' must be a non-empty string.";
        return false;
      }
      if (item.GetString() == "all") {
        config.scenarios = GetDefaultScenarios();
        break;
      }
      config.scenarios.push_back(item.GetString());
    }
  }

  if (const base::Value* encoders_val = root.Find("encoders")) {
    if (!encoders_val->is_list() || encoders_val->GetList().empty()) {
      LOG(ERROR) << "'encoders' must be a non-empty array of encoder objects.";
      return false;
    }
    config.encoders.clear();
    for (const base::Value& item : encoders_val->GetList()) {
      if (!item.is_dict()) {
        LOG(ERROR) << "Each entry in 'encoders' must be a JSON object.";
        return false;
      }
      EncoderConfig encoder_config;
      if (!ParseEncoderConfig(item.GetDict(), config.bitrate_kbps,
                              encoder_config)) {
        return false;
      }
      config.encoders.push_back(std::move(encoder_config));
    }
  } else if (config.bitrate_kbps > 0) {
    for (auto& encoder_config : config.encoders) {
      encoder_config.bitrate_kbps = config.bitrate_kbps;
    }
  }

  return true;
}

bool InitializeBenchmarkConfig(const base::CommandLine* cmd_line,
                               BenchmarkConfig& config) {
  static constexpr const char* kRemovedSwitches[] = {
      "codec",    "profile", "encoder-speed", "use-active-map", "bitrate",
      "duration", "fps",     "max-frames",    "scenarios",      "sizes"};
  for (const char* removed : kRemovedSwitches) {
    if (cmd_line->HasSwitch(removed)) {
      LOG(ERROR) << "--" << removed
                 << " is no longer supported on the command line. Provide a "
                    "JSON config file via --config=<path> (see --help).";
      return false;
    }
  }

  config.sizes = GetDefaultSizes();
  config.scenarios = GetDefaultScenarios();
  EncoderConfig default_encoder;
  default_encoder.name = DefaultEncoderName(default_encoder);
  config.encoders = {default_encoder};

  if (cmd_line->HasSwitch("config")) {
    base::FilePath config_path = cmd_line->GetSwitchValuePath("config");
    if (!LoadJsonConfigFile(config_path, config)) {
      return false;
    }
  }

  if (cmd_line->HasSwitch("frame-dir")) {
    config.frame_dir = cmd_line->GetSwitchValuePath("frame-dir");
  }
  if (cmd_line->HasSwitch("chrome-path")) {
    config.chrome_path = cmd_line->GetSwitchValuePath("chrome-path");
  }
  if (cmd_line->HasSwitch("output-json")) {
    config.output_json_path = cmd_line->GetSwitchValuePath("output-json");
  }

  if (!config.frame_dir.empty()) {
    config.scenarios = {"user_provided_frames"};
  }

  return true;
}

}  // namespace

}  // namespace remoting

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  if (cmd_line->HasSwitch("help")) {
    remoting::PrintHelp(argv[0]);
    return 0;
  }

  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment;

  remoting::BenchmarkConfig config;
  if (!remoting::InitializeBenchmarkConfig(cmd_line, config)) {
    return 1;
  }

  remoting::EncoderBenchmark benchmark;
  base::ListValue all_results;

  for (const auto& scenario : config.scenarios) {
    for (const auto& size : config.sizes) {
      auto preloaded_frames = remoting::LoadFrames(config, scenario, size);
      if (preloaded_frames.empty()) {
        LOG(ERROR) << "No frames generated for scenario '" << scenario
                   << "' at " << size.width() << "x" << size.height() << ".";
        continue;
      }

      remoting::RuntimeParams runtime_params;
      runtime_params.fps = config.fps;
      runtime_params.size = size;
      runtime_params.scenario = scenario;

      for (const auto& encoder_config : config.encoders) {
        base::DictValue run_result =
            benchmark.Run(encoder_config, runtime_params, preloaded_frames);
        if (!run_result.empty()) {
          all_results.Append(std::move(run_result));
        }
      }
    }
  }

  if (!config.output_json_path.empty()) {
    base::DictValue root_output;
    root_output.Set("results", std::move(all_results));
    std::string json_output;
    if (base::JSONWriter::WriteWithOptions(
            root_output, base::JSONWriter::OPTIONS_PRETTY_PRINT,
            &json_output) &&
        base::WriteFile(config.output_json_path, json_output)) {
      std::cout << "Wrote benchmark results to " << config.output_json_path
                << std::endl;
    } else {
      LOG(ERROR) << "Failed to write JSON results to "
                 << config.output_json_path;
      return 1;
    }
  }

  return 0;
}
