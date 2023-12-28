// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/resolution_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/filters/vp9_parser.h"
#include "media/parsers/vp8_parser.h"
#include "media/video/h264_parser.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {
namespace {

class Vp8ResolutionMonitor : public ResolutionMonitor {
 public:
  Vp8ResolutionMonitor() = default;
  absl::optional<gfx::Size> GetResolution(
      const DecoderBuffer& buffer) override {
    if (!buffer.is_key_frame()) {
      return current_resolution_;
    }

    Vp8Parser parser;
    Vp8FrameHeader frame_header;
    if (!parser.ParseFrame(buffer.data(), buffer.data_size(), &frame_header)) {
      DLOG(ERROR) << "Failed to parse vp8 stream";
      current_resolution_ = absl::nullopt;
    } else {
      current_resolution_ =
          gfx::Size(base::saturated_cast<int>(frame_header.width),
                    base::saturated_cast<int>(frame_header.height));
    }

    return current_resolution_;
  }
  VideoCodec codec() const override { return media::VideoCodec::kVP8; }

 private:
  absl::optional<gfx::Size> current_resolution_;
};

class Vp9ResolutionMonitor : public ResolutionMonitor {
 public:
  Vp9ResolutionMonitor() : parser_(/*parsing_compressed_header=*/false) {}

  ~Vp9ResolutionMonitor() override = default;

  absl::optional<gfx::Size> GetResolution(
      const DecoderBuffer& buffer) override {
    std::vector<uint32_t> frame_sizes;
    if (buffer.has_side_data()) {
      frame_sizes = buffer.side_data()->spatial_layers;
    }
    parser_.SetStream(buffer.data(),
                      base::checked_cast<off_t>(buffer.data_size()),
                      frame_sizes, /*stream_config=*/nullptr);

    gfx::Size frame_size;
    bool parse_error = false;
    // Get the maximum resolution in spatial layers.
    absl::optional<gfx::Size> max_resolution;
    while (GetNextFrameSize(frame_size, parse_error)) {
      if (max_resolution.value_or(gfx::Size()).GetArea() <
          frame_size.GetArea()) {
        max_resolution = frame_size;
      }
    }

    return parse_error ? absl::nullopt : max_resolution;
  }

  VideoCodec codec() const override { return media::VideoCodec::kVP9; }

 private:
  bool GetNextFrameSize(gfx::Size& frame_size, bool& parse_error) {
    Vp9FrameHeader frame_header;
    gfx::Size allocate_size;
    Vp9Parser::Result result = parser_.ParseNextFrame(
        &frame_header, &allocate_size, /*frame_decrypt_config=*/nullptr);
    switch (result) {
      case Vp9Parser::Result::kOk:
        frame_size.SetSize(frame_header.frame_width, frame_header.frame_height);
        return true;
      case Vp9Parser::Result::kEOStream:
        return false;
      case Vp9Parser::Result::kInvalidStream:
        DLOG(ERROR) << "Failed parsing vp9 frame";
        parse_error = true;
        return false;
    }
    NOTREACHED_NORETURN() << "Unexpected result: " << static_cast<int>(result);
  }

  Vp9Parser parser_;
};

class Av1ResolutionMonitor : public ResolutionMonitor {
 public:
  constexpr static unsigned int kDefaultOperatingPoint = 0;

  Av1ResolutionMonitor()
      : buffer_pool_(/*on_frame_buffer_size_changed=*/nullptr,
                     /*get_frame_buffer=*/nullptr,
                     /*release_frame_buffer=*/nullptr,
                     /*callback_private_data=*/nullptr) {}

  ~Av1ResolutionMonitor() override = default;

  absl::optional<gfx::Size> GetResolution(
      const DecoderBuffer& buffer) override {
    auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        buffer.data(), buffer.data_size(), kDefaultOperatingPoint,
        &buffer_pool_, &decoder_state_));
    if (current_sequence_header_) {
      parser->set_sequence_header(*current_sequence_header_);
    }

    absl::optional<gfx::Size> max_resolution;
    while (parser->HasData()) {
      libgav1::RefCountedBufferPtr current_frame;
      libgav1::StatusCode status_code = parser->ParseOneFrame(&current_frame);
      if (status_code != libgav1::kStatusOk) {
        DLOG(ERROR) << "Failed parsing av1 frame: "
                    << static_cast<int>(status_code);
        return absl::nullopt;
      }
      if (!current_frame) {
        // No frame is found. Finish the stream.
        break;
      }

      if (parser->sequence_header_changed() &&
          !UpdateCurrentSequenceHeader(parser->sequence_header())) {
        return absl::nullopt;
      }

      std::optional<gfx::Size> frame_size =
          GetFrameSizeFromHeader(parser->frame_header());
      if (!frame_size) {
        return absl::nullopt;
      }
      if (max_resolution.value_or(gfx::Size()).GetArea() <
          frame_size->GetArea()) {
        max_resolution = *frame_size;
      }

      decoder_state_.UpdateReferenceFrames(
          current_frame,
          base::strict_cast<int>(parser->frame_header().refresh_frame_flags));
    }

    return max_resolution;
  }

  VideoCodec codec() const override { return VideoCodec::kAV1; }

 private:
  // Returns true iff the current decode sequence has multiple spatial layers.
  bool IsSpatialLayerDecoding(int operating_point_idc) const {
    // Spec 6.4.1.
    constexpr int kTemporalLayerBitMaskBits = 8;
    const int kUsedSpatialLayerBitMask =
        (operating_point_idc >> kTemporalLayerBitMaskBits) & 0b1111;
    // In case of an only temporal layer encoding e.g. L1T3, spatial layer#0 bit
    // is 1. We allow this case.
    return kUsedSpatialLayerBitMask > 1;
  }

  bool UpdateCurrentSequenceHeader(
      const libgav1::ObuSequenceHeader& sequence_header) {
    int operating_point_idc =
        sequence_header.operating_point_idc[kDefaultOperatingPoint];
    if (IsSpatialLayerDecoding(operating_point_idc)) {
      constexpr size_t kOperatingPointIdcBits = 12;
      DVLOG(1) << "Spatial layer decoding is not supported: "
               << "operating_point_idc="
               << std::bitset<kOperatingPointIdcBits>(operating_point_idc);
      return false;
    }

    current_sequence_header_ = sequence_header;
    return true;
  }

  std::optional<gfx::Size> GetFrameSizeFromHeader(
      const libgav1::ObuFrameHeader& frame_header) const {
    if (!frame_header.show_existing_frame) {
      return gfx::Size(frame_header.width, frame_header.height);
    }
    const size_t frame_to_show =
        base::checked_cast<size_t>(frame_header.frame_to_show);
    CHECK_LT(frame_to_show,
             static_cast<size_t>(libgav1::kNumReferenceFrameTypes));
    const libgav1::RefCountedBufferPtr show_frame =
        decoder_state_.reference_frame[frame_to_show];
    if (!show_frame) {
      DLOG(ERROR) << "Show existing frame references an invalid frame";
      return absl::nullopt;
    }
    return gfx::Size(show_frame->frame_width(), show_frame->frame_height());
  }

  absl::optional<libgav1::ObuSequenceHeader> current_sequence_header_;
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
};

// H264ResolutionMonitor has two assumptions.
// (1) SPS and PPS come in bundle with IDR.
// (2) The buffer has only one IDR and it associates with the SPS in the bundle.
// This is satisfied in WebRTC use case.
class H264ResolutionMonitor : public ResolutionMonitor {
 public:
  H264ResolutionMonitor() = default;
  ~H264ResolutionMonitor() override = default;

  absl::optional<gfx::Size> GetResolution(
      const DecoderBuffer& buffer) override {
    if (!buffer.is_key_frame()) {
      return current_resolution_;
    }

    H264Parser h264_parser;
    h264_parser.SetStream(buffer.data(),
                          base::checked_cast<off_t>(buffer.data_size()));
    size_t nalus_count = 0;
    H264NALU nalu;
    absl::optional<gfx::Size> resolution;
    bool parse_error = false;
    while (GetNextNALU(h264_parser, nalu, parse_error)) {
      nalus_count += 1;
      if (nalu.nal_unit_type == H264NALU::Type::kSPS) {
        int sps_id = 0;
        if (h264_parser.ParseSPS(&sps_id) != H264Parser::kOk) {
          DLOG(ERROR) << "Failed parsing h264 SPS";
          return absl::nullopt;
        }
        const H264SPS* sps = h264_parser.GetSPS(sps_id);
        if (!sps) {
          DLOG(ERROR) << "Failed getting h264 SPS: id=" << sps_id;
          return absl::nullopt;
        }
        absl::optional<gfx::Size> coded_size = sps->GetCodedSize();
        if (!coded_size) {
          DLOG(ERROR) << "Failed getting coded size";
          return absl::nullopt;
        }
        resolution = *coded_size;
      }
    }

    if (parse_error || nalus_count == 0) {
      return absl::nullopt;
    }

    current_resolution_ = resolution;
    return current_resolution_;
  }

  VideoCodec codec() const override { return VideoCodec::kH264; }

 private:
  bool GetNextNALU(H264Parser& h264_parser,
                   H264NALU& nalu,
                   bool& parse_error) const {
    H264Parser::Result result = h264_parser.AdvanceToNextNALU(&nalu);
    switch (result) {
      case H264Parser::Result::kOk:
        return true;
      case H264Parser::Result::kEOStream:
        return false;
      case H264Parser::Result::kInvalidStream:
      case H264Parser::Result::kUnsupportedStream:
        DLOG(ERROR) << "Failed parsing h264 NALU";
        parse_error = true;
        return false;
    }
    NOTREACHED_NORETURN() << "Unexpected result: " << static_cast<int>(result);
  }

  absl::optional<gfx::Size> current_resolution_;
};
}  // namespace

ResolutionMonitor::~ResolutionMonitor() = default;

// static
std::unique_ptr<ResolutionMonitor> ResolutionMonitor::Create(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return std::make_unique<H264ResolutionMonitor>();
    case VideoCodec::kVP8:
      return std::make_unique<Vp8ResolutionMonitor>();
    case VideoCodec::kVP9:
      return std::make_unique<Vp9ResolutionMonitor>();
    case VideoCodec::kAV1:
      return std::make_unique<Av1ResolutionMonitor>();
    // TODO(bugs.webrtc.org/13485): Add H265.
    default:
      DLOG(ERROR) << "Unsupported codec: " << GetCodecName(codec);
      return nullptr;
  }
}
}  // namespace media
