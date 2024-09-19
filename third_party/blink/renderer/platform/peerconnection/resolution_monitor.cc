// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/peerconnection/resolution_monitor.h"

#include <bitset>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/vp9_parser.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/common_video/h264/h264_common.h"
#include "third_party/webrtc/common_video/h264/sps_parser.h"

namespace blink {

namespace {

class Vp8ResolutionMonitor : public ResolutionMonitor {
 public:
  Vp8ResolutionMonitor() = default;
  std::optional<gfx::Size> GetResolution(
      const media::DecoderBuffer& buffer) override {
    if (!buffer.is_key_frame()) {
      return current_resolution_;
    }

    media::Vp8Parser parser;
    media::Vp8FrameHeader frame_header;
    if (!parser.ParseFrame(buffer.data(), buffer.size(), &frame_header)) {
      DLOG(ERROR) << "Failed to parse vp8 stream";
      current_resolution_ = std::nullopt;
    } else {
      current_resolution_ =
          gfx::Size(base::saturated_cast<int>(frame_header.width),
                    base::saturated_cast<int>(frame_header.height));
    }

    return current_resolution_;
  }
  media::VideoCodec codec() const override { return media::VideoCodec::kVP8; }

 private:
  std::optional<gfx::Size> current_resolution_;
};

class Vp9ResolutionMonitor : public ResolutionMonitor {
 public:
  Vp9ResolutionMonitor() : parser_(/*parsing_compressed_header=*/false) {}

  ~Vp9ResolutionMonitor() override = default;

  std::optional<gfx::Size> GetResolution(
      const media::DecoderBuffer& buffer) override {
    std::vector<uint32_t> frame_sizes;
    if (buffer.has_side_data()) {
      frame_sizes = buffer.side_data()->spatial_layers;
    }
    parser_.SetStream(buffer.data(), base::checked_cast<off_t>(buffer.size()),
                      frame_sizes, /*stream_config=*/nullptr);

    gfx::Size frame_size;
    bool parse_error = false;
    // Get the maximum resolution in spatial layers.
    std::optional<gfx::Size> max_resolution;
    while (GetNextFrameSize(frame_size, parse_error)) {
      if (max_resolution.value_or(gfx::Size()).GetArea() <
          frame_size.GetArea()) {
        max_resolution = frame_size;
      }
    }

    return parse_error ? std::nullopt : max_resolution;
  }

  media::VideoCodec codec() const override { return media::VideoCodec::kVP9; }

 private:
  bool GetNextFrameSize(gfx::Size& frame_size, bool& parse_error) {
    media::Vp9FrameHeader frame_header;
    gfx::Size allocate_size;
    media::Vp9Parser::Result result = parser_.ParseNextFrame(
        &frame_header, &allocate_size, /*frame_decrypt_config=*/nullptr);
    switch (result) {
      case media::Vp9Parser::Result::kOk:
        frame_size.SetSize(frame_header.frame_width, frame_header.frame_height);
        return true;
      case media::Vp9Parser::Result::kEOStream:
        return false;
      case media::Vp9Parser::Result::kInvalidStream:
        DLOG(ERROR) << "Failed parsing vp9 frame";
        parse_error = true;
        return false;
    }
    NOTREACHED() << "Unexpected result: " << static_cast<int>(result);
  }

  media::Vp9Parser parser_;
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

  std::optional<gfx::Size> GetResolution(
      const media::DecoderBuffer& buffer) override {
    auto parser = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        buffer.data(), buffer.size(), kDefaultOperatingPoint, &buffer_pool_,
        &decoder_state_));
    if (current_sequence_header_) {
      parser->set_sequence_header(*current_sequence_header_);
    }

    std::optional<gfx::Size> max_resolution;
    while (parser->HasData()) {
      libgav1::RefCountedBufferPtr current_frame;
      libgav1::StatusCode status_code = parser->ParseOneFrame(&current_frame);
      if (status_code != libgav1::kStatusOk) {
        DLOG(ERROR) << "Failed parsing av1 frame: "
                    << static_cast<int>(status_code);
        return std::nullopt;
      }
      if (!current_frame) {
        // No frame is found. Finish the stream.
        break;
      }

      if (parser->sequence_header_changed() &&
          !UpdateCurrentSequenceHeader(parser->sequence_header())) {
        return std::nullopt;
      }

      std::optional<gfx::Size> frame_size =
          GetFrameSizeFromHeader(parser->frame_header());
      if (!frame_size) {
        return std::nullopt;
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

  media::VideoCodec codec() const override { return media::VideoCodec::kAV1; }

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
      return std::nullopt;
    }
    return gfx::Size(show_frame->frame_width(), show_frame->frame_height());
  }

  std::optional<libgav1::ObuSequenceHeader> current_sequence_header_;
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

  std::optional<gfx::Size> GetResolution(
      const media::DecoderBuffer& buffer) override {
    if (!buffer.is_key_frame()) {
      return current_resolution_;
    }

    std::optional<gfx::Size> resolution;
    rtc::ArrayView<const uint8_t> webrtc_buffer(buffer);
    std::vector<webrtc::H264::NaluIndex> nalu_indices =
        webrtc::H264::FindNaluIndices(webrtc_buffer);
    for (const auto& nalu_index : nalu_indices) {
      if (nalu_index.payload_size < webrtc::H264::kNaluTypeSize) {
        DLOG(ERROR) << "H.264 SPS NALU size too small for parsing NALU type.";
        return std::nullopt;
      }
      auto nalu_payload = webrtc_buffer.subview(nalu_index.payload_start_offset,
                                                nalu_index.payload_size);
      if (webrtc::H264::ParseNaluType(nalu_payload[0]) ==
          webrtc::H264::NaluType::kSps) {
        // Parse without NALU header.
        std::optional<webrtc::SpsParser::SpsState> sps =
            webrtc::SpsParser::ParseSps(
                nalu_payload.subview(webrtc::H264::kNaluTypeSize));
        if (!sps || !sps->width || !sps->height) {
          DLOG(ERROR) << "Failed parsing H.264 SPS.";
          return std::nullopt;
        }
        resolution = gfx::Size(sps->width, sps->height);
        break;
      }
    }

    current_resolution_ = resolution;
    return current_resolution_;
  }

  media::VideoCodec codec() const override { return media::VideoCodec::kH264; }

 private:
  std::optional<gfx::Size> current_resolution_;
};
}  // namespace

ResolutionMonitor::~ResolutionMonitor() = default;

// static
std::unique_ptr<ResolutionMonitor> ResolutionMonitor::Create(
    media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kH264:
      return std::make_unique<H264ResolutionMonitor>();
    case media::VideoCodec::kVP8:
      return std::make_unique<Vp8ResolutionMonitor>();
    case media::VideoCodec::kVP9:
      return std::make_unique<Vp9ResolutionMonitor>();
    case media::VideoCodec::kAV1:
      return std::make_unique<Av1ResolutionMonitor>();
    default:
      DLOG(ERROR) << "Unsupported codec: " << media::GetCodecName(codec);
      return nullptr;
  }
}

}  // namespace blink
