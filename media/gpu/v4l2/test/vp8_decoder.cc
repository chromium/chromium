// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/v4l2/test/vp8_decoder.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/test/v4l2_ioctl_shim.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp8_parser.h"

namespace {
constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_VP8_FRAME;

constexpr size_t kVp8FrameLast = 0;
constexpr size_t kVp8FrameGolden = 1;
constexpr size_t kVp8FrameAltref = 2;

using TypeOfVp8RefType = std::underlying_type_t<media::Vp8RefType>;

static_assert(kVp8FrameLast ==
                  base::strict_cast<TypeOfVp8RefType>(media::VP8_FRAME_LAST),
              "Invalid index value for Last reference frame");

static_assert(kVp8FrameGolden ==
                  base::strict_cast<TypeOfVp8RefType>(media::VP8_FRAME_GOLDEN),
              "Invalid index value for Golden reference frame");

static_assert(kVp8FrameAltref ==
                  base::strict_cast<TypeOfVp8RefType>(media::VP8_FRAME_ALTREF),
              "Invalid index value for Altref reference frame");

// The resolution encoded in the bitstream is required for queue creation. Note
// that parsing ivf file and parsing the first frame VP8 parser happen
// again later in the code. This is intentionally duplicated.
const gfx::Size GetResolutionFromBitstream(
    const base::MemoryMappedFile& stream) {
  media::IvfParser ivf_parser{};
  media::IvfFileHeader ivf_file_header{};

  if (!ivf_parser.Initialize(stream.data(), stream.length(),
                             &ivf_file_header)) {
    LOG(FATAL) << "Couldn't initialize IVF parser.";
  }

  media::IvfFrameHeader ivf_frame_header{};
  const uint8_t* ivf_frame_data;

  if (!ivf_parser.ParseNextFrame(&ivf_frame_header, &ivf_frame_data)) {
    LOG(FATAL) << "Failed to parse the first frame with IVF parser.";
  }

  VLOG(2) << "Ivf file header: " << ivf_file_header.width << " x "
          << ivf_file_header.height;

  media::Vp8Parser vp8_parser;
  media::Vp8FrameHeader vp8_frame_header;
  vp8_parser.ParseFrame(ivf_frame_data, ivf_frame_header.frame_size,
                        &vp8_frame_header);

  return gfx::Size(vp8_frame_header.width, vp8_frame_header.height);
}

// Section 9.4. Loop filter type and levels syntax in VP8 specs.
// https://datatracker.ietf.org/doc/rfc6386/
struct v4l2_vp8_loop_filter FillV4L2VP8LoopFilterHeader(
    const media::Vp8LoopFilterHeader& vp8_lf_hdr) {
  struct v4l2_vp8_loop_filter v4l2_lf = {};

  v4l2_lf.sharpness_level = vp8_lf_hdr.sharpness_level;
  v4l2_lf.level = vp8_lf_hdr.level;
  if (vp8_lf_hdr.type == 1)
    v4l2_lf.flags |= V4L2_VP8_LF_FILTER_TYPE_SIMPLE;
  if (vp8_lf_hdr.loop_filter_adj_enable)
    v4l2_lf.flags |= V4L2_VP8_LF_ADJ_ENABLE;
  if (vp8_lf_hdr.mode_ref_lf_delta_update)
    v4l2_lf.flags |= V4L2_VP8_LF_DELTA_UPDATE;

  static_assert(
      std::size(decltype(v4l2_lf.ref_frm_delta){}) == media::kNumBlockContexts,
      "Invalid size of ref_frm_delta");

  static_assert(
      std::size(decltype(v4l2_lf.mb_mode_delta){}) == media::kNumBlockContexts,
      "Invalid size of mb_mode_delta");

  media::SafeArrayMemcpy(v4l2_lf.ref_frm_delta, vp8_lf_hdr.ref_frame_delta);
  media::SafeArrayMemcpy(v4l2_lf.mb_mode_delta, vp8_lf_hdr.mb_mode_delta);

  return v4l2_lf;
}

// Section 9.6. Dequantization indices.
struct v4l2_vp8_quantization FillV4L2Vp8QuantizationHeader(
    const media::Vp8QuantizationHeader& vp8_quantization_hdr) {
  struct v4l2_vp8_quantization v4l2_quant = {};

  v4l2_quant.y_ac_qi = base::checked_cast<__u8>(vp8_quantization_hdr.y_ac_qi);
  v4l2_quant.y_dc_delta =
      base::checked_cast<__s8>(vp8_quantization_hdr.y_dc_delta);
  v4l2_quant.y2_dc_delta =
      base::checked_cast<__s8>(vp8_quantization_hdr.y2_dc_delta);
  v4l2_quant.y2_ac_delta =
      base::checked_cast<__s8>(vp8_quantization_hdr.y2_ac_delta);
  v4l2_quant.uv_dc_delta =
      base::checked_cast<__s8>(vp8_quantization_hdr.uv_dc_delta);
  v4l2_quant.uv_ac_delta =
      base::checked_cast<__s8>(vp8_quantization_hdr.uv_ac_delta);

  return v4l2_quant;
}

// Section 9.9.  DCT Coefficient Probability Update
struct v4l2_vp8_entropy FillV4L2VP8EntropyHeader(
    const media::Vp8EntropyHeader& vp8_entropy_hdr) {
  struct v4l2_vp8_entropy v4l2_entr = {};

  static_assert(
      std::size(decltype(v4l2_entr.coeff_probs){}) == media::kNumBlockTypes,
      "Invalid size of coeff_probs");

  static_assert(
      std::size(decltype(v4l2_entr.y_mode_probs){}) == media::kNumYModeProbs,
      "Invalid size of y_mode_probs");

  static_assert(
      std::size(decltype(v4l2_entr.uv_mode_probs){}) == media::kNumUVModeProbs,
      "Invalid size of uv_mode_probs");

  static_assert(
      std::size(decltype(v4l2_entr.mv_probs){}) == media::kNumMVContexts,
      "Invalid size of mv_probs");

  media::SafeArrayMemcpy(v4l2_entr.coeff_probs, vp8_entropy_hdr.coeff_probs);
  media::SafeArrayMemcpy(v4l2_entr.y_mode_probs, vp8_entropy_hdr.y_mode_probs);
  media::SafeArrayMemcpy(v4l2_entr.uv_mode_probs,
                         vp8_entropy_hdr.uv_mode_probs);
  media::SafeArrayMemcpy(v4l2_entr.mv_probs, vp8_entropy_hdr.mv_probs);
  return v4l2_entr;
}

// Section 9.3. Segment-Based Adjustments
struct v4l2_vp8_segment FillV4L2VP8SegmentationHeader(
    const media::Vp8SegmentationHeader& vp8_segmentation_hdr) {
  struct v4l2_vp8_segment v4l2_segment = {};
  if (vp8_segmentation_hdr.segmentation_enabled)
    v4l2_segment.flags |= V4L2_VP8_SEGMENT_FLAG_ENABLED;
  if (vp8_segmentation_hdr.update_mb_segmentation_map)
    v4l2_segment.flags |= V4L2_VP8_SEGMENT_FLAG_UPDATE_MAP;
  if (vp8_segmentation_hdr.update_segment_feature_data)
    v4l2_segment.flags |= V4L2_VP8_SEGMENT_FLAG_UPDATE_FEATURE_DATA;
  if (vp8_segmentation_hdr.segment_feature_mode ==
      media::Vp8SegmentationHeader::FEATURE_MODE_DELTA) {
    v4l2_segment.flags |= V4L2_VP8_SEGMENT_FLAG_DELTA_VALUE_MODE;
  }

  static_assert(
      std::size(decltype(v4l2_segment.quant_update){}) == media::kMaxMBSegments,
      "Invalid size of quant_update");

  static_assert(
      std::size(decltype(v4l2_segment.lf_update){}) == media::kMaxMBSegments,
      "Invalid size of lf_update");

  static_assert(std::size(decltype(v4l2_segment.segment_probs){}) ==
                    media::kNumMBFeatureTreeProbs,
                "Invalid size of segment_probs");

  media::SafeArrayMemcpy(v4l2_segment.quant_update,
                         vp8_segmentation_hdr.quantizer_update_value);
  media::SafeArrayMemcpy(v4l2_segment.lf_update,
                         vp8_segmentation_hdr.lf_update_value);
  media::SafeArrayMemcpy(v4l2_segment.segment_probs,
                         vp8_segmentation_hdr.segment_prob);
  v4l2_segment.padding = 0;

  return v4l2_segment;
}

// Checks if the buffer slot holding the reference frame is not used by other
// frames
bool IsBufferSlotInUse(
    const media::Vp8FrameHeader& frame_hdr,
    const std::array<scoped_refptr<media::v4l2_test::MmappedBuffer>,
                     media::kNumVp8ReferenceBuffers>& ref_frames,
    size_t curr_ref_frame_index) {
  for (size_t i = 0; i < media::kNumVp8ReferenceBuffers; i++) {
    // Skips |curr_ref_frame_index| to avoid comparing against itself and
    // removing it
    if (i == curr_ref_frame_index)
      continue;

    bool is_frame_not_refreshed = false;
    switch (i) {
      case kVp8FrameAltref:
        is_frame_not_refreshed = !frame_hdr.refresh_alternate_frame &&
                                 frame_hdr.copy_buffer_to_alternate ==
                                     media::Vp8FrameHeader::NO_ALT_REFRESH;
        break;
      case kVp8FrameGolden:
        is_frame_not_refreshed = !frame_hdr.refresh_golden_frame &&
                                 frame_hdr.copy_buffer_to_golden ==
                                     media::Vp8FrameHeader::NO_GOLDEN_REFRESH;
        break;
      case kVp8FrameLast:
        is_frame_not_refreshed = !frame_hdr.refresh_last;
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid reference frame index";
    }
    const bool is_candidate_in_use =
        (ref_frames[i]->buffer_id() ==
         ref_frames[curr_ref_frame_index]->buffer_id());

    if (is_frame_not_refreshed && is_candidate_in_use)
      return true;
  }
  return false;
}

}  // namespace
namespace media {
namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 6;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

Vp8Decoder::Vp8Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       gfx::Size display_resolution)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution),
      ivf_parser_(std::move(ivf_parser)),
      vp8_parser_(std::make_unique<Vp8Parser>()) {
  DCHECK(v4l2_ioctl_);
  DCHECK(v4l2_ioctl_->QueryCtrl(V4L2_CID_STATELESS_VP8_FRAME));

  std::fill(ref_frames_.begin(), ref_frames_.end(), nullptr);
}

Vp8Decoder::~Vp8Decoder() = default;

// static
std::unique_ptr<Vp8Decoder> Vp8Decoder::Create(
    const base::MemoryMappedFile& stream) {
  VLOG(2) << "Attempting to create decoder with codec "
          << media::FourccToString(kDriverCodecFourcc);

  // Set up video parser.
  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};

  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  const auto driver_codec_fourcc =
      media::v4l2_test::FileFourccToDriverFourcc(file_header.fourcc);

  if (driver_codec_fourcc != kDriverCodecFourcc) {
    VLOG(2) << "File fourcc (" << media::FourccToString(driver_codec_fourcc)
            << ") does not match expected fourcc("
            << media::FourccToString(kDriverCodecFourcc) << ").";
    return nullptr;
  }

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);

  const gfx::Size bitstream_coded_size = GetResolutionFromBitstream(stream);
  LOG(INFO) << "Ivf file header: "
            << gfx::Size(file_header.width, file_header.height).ToString();

  return base::WrapUnique(new Vp8Decoder(
      std::move(ivf_parser), std::move(v4l2_ioctl), bitstream_coded_size));
}

struct v4l2_ctrl_vp8_frame Vp8Decoder::SetupFrameHeaders(
    const Vp8FrameHeader& frame_hdr) {
  struct v4l2_ctrl_vp8_frame v4l2_frame_headers = {};

  v4l2_frame_headers.lf = FillV4L2VP8LoopFilterHeader(frame_hdr.loopfilter_hdr);
  v4l2_frame_headers.quant =
      FillV4L2Vp8QuantizationHeader(frame_hdr.quantization_hdr);

  v4l2_frame_headers.coder_state.range = frame_hdr.bool_dec_range;
  v4l2_frame_headers.coder_state.value = frame_hdr.bool_dec_value;
  v4l2_frame_headers.coder_state.bit_count = frame_hdr.bool_dec_count;

  v4l2_frame_headers.width = frame_hdr.width;
  v4l2_frame_headers.height = frame_hdr.height;

  v4l2_frame_headers.horizontal_scale = frame_hdr.horizontal_scale;
  v4l2_frame_headers.vertical_scale = frame_hdr.vertical_scale;

  v4l2_frame_headers.version = frame_hdr.version;
  v4l2_frame_headers.prob_skip_false = frame_hdr.prob_skip_false;
  v4l2_frame_headers.prob_intra = frame_hdr.prob_intra;
  v4l2_frame_headers.prob_last = frame_hdr.prob_last;
  v4l2_frame_headers.prob_gf = frame_hdr.prob_gf;
  v4l2_frame_headers.num_dct_parts = frame_hdr.num_of_dct_partitions;

  v4l2_frame_headers.first_part_size = frame_hdr.first_part_size;
  // https://lwn.net/Articles/793069/: macroblock_bit_offset is renamed to
  // first_part_header_bits
  v4l2_frame_headers.first_part_header_bits = frame_hdr.macroblock_bit_offset;

  if (frame_hdr.frame_type == media::Vp8FrameHeader::KEYFRAME)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_KEY_FRAME;
  if (frame_hdr.show_frame)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_SHOW_FRAME;
  if (frame_hdr.mb_no_skip_coeff)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_MB_NO_SKIP_COEFF;
  if (frame_hdr.sign_bias_golden)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_SIGN_BIAS_GOLDEN;
  if (frame_hdr.sign_bias_alternate)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_SIGN_BIAS_ALT;
  if (frame_hdr.is_experimental)
    v4l2_frame_headers.flags |= V4L2_VP8_FRAME_FLAG_EXPERIMENTAL;

  static_assert(std::size(decltype(v4l2_frame_headers.dct_part_sizes){}) ==
                    media::kMaxDCTPartitions,
                "Invalid size of dct_part_sizes");

  for (size_t i = 0; i < frame_hdr.num_of_dct_partitions &&
                     i < std::size(v4l2_frame_headers.dct_part_sizes);
       ++i) {
    v4l2_frame_headers.dct_part_sizes[i] =
        static_cast<size_t>(frame_hdr.dct_partition_sizes[i]);
  }

  v4l2_frame_headers.entropy = FillV4L2VP8EntropyHeader(frame_hdr.entropy_hdr);
  v4l2_frame_headers.segment =
      FillV4L2VP8SegmentationHeader(frame_hdr.segmentation_hdr);

  constexpr uint64_t kInvalidSurface = std::numeric_limits<uint32_t>::max();
  // We need to convert a reference frame's frame_number() (in  microseconds)
  // to reference ID (in nanoseconds). Technically, v4l2_timeval_to_ns() is
  // suggested to be used to convert timestamp to nanoseconds, but multiplying
  // the microseconds part of timestamp |tv_usec| by |kTimestampToNanoSecs| to
  // make it nanoseconds is also known to work. This is how it is implemented
  // in v4l2 video decode accelerator tests as well as in gstreamer.
  // https://www.kernel.org/doc/html/v5.10/userspace-api/media/v4l/dev-stateless-decoder.html#buffer-management-while-decoding
  constexpr size_t kTimestampToNanoSecs = 1000;
  v4l2_frame_headers.last_frame_ts =
      ref_frames_[kVp8FrameLast]
          ? (ref_frames_[kVp8FrameLast]->frame_number() * kTimestampToNanoSecs)
          : kInvalidSurface;
  v4l2_frame_headers.golden_frame_ts =
      ref_frames_[kVp8FrameGolden]
          ? (ref_frames_[kVp8FrameGolden]->frame_number() *
             kTimestampToNanoSecs)
          : kInvalidSurface;
  v4l2_frame_headers.alt_frame_ts =
      ref_frames_[kVp8FrameAltref]
          ? (ref_frames_[kVp8FrameAltref]->frame_number() *
             kTimestampToNanoSecs)
          : kInvalidSurface;

  return v4l2_frame_headers;
}

void Vp8Decoder::UpdateReusableReferenceBufferSlots(
    const Vp8FrameHeader& frame_hdr,
    const size_t curr_ref_frame_index,
    std::set<int>& reusable_buffer_slots) {
  const auto reusable_candidate_buffer_id =
      ref_frames_[curr_ref_frame_index]->buffer_id();
  reusable_buffer_slots.insert(
      base::checked_cast<int>(reusable_candidate_buffer_id));

  bool is_buffer_slot_copied = false;
  switch (curr_ref_frame_index) {
    case kVp8FrameAltref:
      is_buffer_slot_copied =
          frame_hdr.copy_buffer_to_golden == Vp8FrameHeader::COPY_ALT_TO_GOLDEN;
      break;
    case kVp8FrameGolden:
      is_buffer_slot_copied = frame_hdr.copy_buffer_to_alternate ==
                              Vp8FrameHeader::COPY_GOLDEN_TO_ALT;
      break;
    case kVp8FrameLast:
      is_buffer_slot_copied = (frame_hdr.copy_buffer_to_alternate ==
                               Vp8FrameHeader::COPY_LAST_TO_ALT) ||
                              (frame_hdr.copy_buffer_to_golden ==
                               Vp8FrameHeader::COPY_LAST_TO_GOLDEN);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid reference frame index";
  }
  const bool is_buffer_slot_in_use =
      IsBufferSlotInUse(frame_hdr, ref_frames_, curr_ref_frame_index);

  if (is_buffer_slot_copied || is_buffer_slot_in_use)
    reusable_buffer_slots.erase(
        base::checked_cast<int>(reusable_candidate_buffer_id));
}

std::set<int> Vp8Decoder::RefreshReferenceSlots(
    const Vp8FrameHeader& frame_hdr,
    MmappedBuffer* buffer,
    std::set<uint32_t> queued_buffer_ids) {
  std::set<int> reusable_buffer_slots = {};

  if (frame_hdr.IsKeyframe()) {
    // For key frames, all referenced frame are refreshed/replaced by the
    // current reconstructed frame. Then all CAPTURE buffers can be reused
    // except the CAPTURE buffer holding the key frame.
    for (size_t i = 0; i < kNumberOfBuffersInCaptureQueue; i++) {
      if (!queued_buffer_ids.count(i)) {
        reusable_buffer_slots.insert(i);
      }
    }
    reusable_buffer_slots.erase(buffer->buffer_id());

    ref_frames_.fill(buffer);
    return reusable_buffer_slots;
  }

  if (frame_hdr.refresh_alternate_frame) {
    UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameAltref,
                                       reusable_buffer_slots);
    ref_frames_[kVp8FrameAltref] = buffer;
  } else {
    switch (frame_hdr.copy_buffer_to_alternate) {
      case Vp8FrameHeader::COPY_LAST_TO_ALT:
        DCHECK(ref_frames_[kVp8FrameLast]);
        UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameAltref,
                                           reusable_buffer_slots);
        ref_frames_[kVp8FrameAltref] = ref_frames_[kVp8FrameLast];
        break;
      case Vp8FrameHeader::COPY_GOLDEN_TO_ALT:
        DCHECK(ref_frames_[kVp8FrameGolden]);
        UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameAltref,
                                           reusable_buffer_slots);
        ref_frames_[kVp8FrameAltref] = ref_frames_[kVp8FrameGolden];
        break;
      case Vp8FrameHeader::NO_ALT_REFRESH:
        DCHECK(ref_frames_[kVp8FrameAltref]);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid flag to refresh altenate frame: "
                                  << frame_hdr.copy_buffer_to_alternate;
    }
  }

  if (frame_hdr.refresh_golden_frame) {
    UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameGolden,
                                       reusable_buffer_slots);
    ref_frames_[kVp8FrameGolden] = buffer;
  } else {
    switch (frame_hdr.copy_buffer_to_golden) {
      case Vp8FrameHeader::COPY_LAST_TO_GOLDEN:
        DCHECK(ref_frames_[kVp8FrameLast]);
        UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameGolden,
                                           reusable_buffer_slots);
        ref_frames_[kVp8FrameGolden] = ref_frames_[kVp8FrameLast];
        break;
      case Vp8FrameHeader::COPY_ALT_TO_GOLDEN:
        DCHECK(ref_frames_[kVp8FrameAltref]);
        UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameGolden,
                                           reusable_buffer_slots);
        ref_frames_[kVp8FrameGolden] = ref_frames_[kVp8FrameAltref];
        break;
      case Vp8FrameHeader::NO_GOLDEN_REFRESH:
        DCHECK(ref_frames_[kVp8FrameGolden]);
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid flag to refresh golden frame: "
                                  << frame_hdr.copy_buffer_to_golden;
    }
  }

  if (frame_hdr.refresh_last) {
    UpdateReusableReferenceBufferSlots(frame_hdr, kVp8FrameLast,
                                       reusable_buffer_slots);
    ref_frames_[kVp8FrameLast] = buffer;
  }

  DCHECK(ref_frames_[kVp8FrameLast]);

  return reusable_buffer_slots;
}

Vp8Decoder::ParseResult Vp8Decoder::ReadNextFrame(
    Vp8FrameHeader& vp8_frame_header) {
  IvfFrameHeader ivf_frame_header{};
  const uint8_t* ivf_frame_data;
  if (!ivf_parser_->ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
    return kEOStream;

  const bool result = vp8_parser_->ParseFrame(
      ivf_frame_data, ivf_frame_header.frame_size, &vp8_frame_header);

  return result ? Vp8Decoder::kOk : Vp8Decoder::kError;
}

VideoDecoder::Result Vp8Decoder::DecodeNextFrame(const int frame_number,
                                                 std::vector<uint8_t>& y_plane,
                                                 std::vector<uint8_t>& u_plane,
                                                 std::vector<uint8_t>& v_plane,
                                                 gfx::Size& size,
                                                 BitDepth& bit_depth) {
  Vp8FrameHeader frame_hdr{};

  Vp8Decoder::ParseResult parser_res = ReadNextFrame(frame_hdr);
  switch (parser_res) {
    case Vp8Decoder::kEOStream:
      return VideoDecoder::kEOStream;
    case Vp8Decoder::kError:
      return VideoDecoder::kError;
    case Vp8Decoder::kOk:
      break;
  }

  const bool is_OUTPUT_queue_new = !OUTPUT_queue_;
  if (!OUTPUT_queue_) {
    CreateOUTPUTQueue(kDriverCodecFourcc);
  }

  const bool resolution_changed =
      frame_hdr.width != OUTPUT_queue_->resolution().width() ||
      frame_hdr.height != OUTPUT_queue_->resolution().height();
  if (frame_hdr.IsKeyframe() && resolution_changed) {
    const gfx::Size new_resolution(frame_hdr.width, frame_hdr.height);
    LOG_ASSERT(!new_resolution.IsEmpty())
        << "New key frame resolution is empty.";

    HandleDynamicResolutionChange(new_resolution);
  } else {
    frame_hdr.width = OUTPUT_queue_->resolution().width();
    frame_hdr.height = OUTPUT_queue_->resolution().height();
  }

  VLOG_IF(2, !frame_hdr.show_frame) << "Not displaying frame";
  last_decoded_frame_visible_ = frame_hdr.show_frame;

  uint32_t buffer_id = 0;
  // Copies the frame data into the V4L2 buffer of OUTPUT |queue|.
  scoped_refptr<MmappedBuffer> OUTPUT_queue_buffer =
      OUTPUT_queue_->GetBuffer(buffer_id);
  OUTPUT_queue_buffer->mmapped_planes()[0].CopyIn(frame_hdr.data,
                                                  frame_hdr.frame_size);
  OUTPUT_queue_buffer->set_frame_number(frame_number);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, buffer_id)) {
    LOG(FATAL) << "VIDIOC_QBUF failed for OUTPUT queue.";
  }

  struct v4l2_ctrl_vp8_frame v4l2_frame_headers = SetupFrameHeaders(frame_hdr);

  // Set controls required by the OUTPUT format to enumerate the CAPTURE formats
  struct v4l2_ext_control ext_ctrl = {.id = V4L2_CID_STATELESS_VP8_FRAME,
                                      .size = sizeof(v4l2_frame_headers),
                                      .ptr = &v4l2_frame_headers};

  struct v4l2_ext_controls ext_ctrls = {.count = 1, .controls = &ext_ctrl};

  // Before the CAPTURE queue is set up the first frame must be parsed by the
  // driver. This is done so that when VIDIOC_G_FMT is called the frame
  // dimensions and format will be ready. Specifying V4L2_CTRL_WHICH_CUR_VAL
  // when VIDIOC_S_EXT_CTRLS processes the request immediately so that the frame
  // is parsed by the driver and the state is readied.
  v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls, is_OUTPUT_queue_new);
  v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_);

  if (!CAPTURE_queue_) {
    CreateCAPTUREQueue(kNumberOfBuffersInCaptureQueue);
  }

  v4l2_ioctl_->WaitForRequestCompletion(OUTPUT_queue_);

  v4l2_ioctl_->DQBuf(CAPTURE_queue_, &buffer_id);
  CAPTURE_queue_->DequeueBufferId(buffer_id);

  scoped_refptr<MmappedBuffer> buffer = CAPTURE_queue_->GetBuffer(buffer_id);
  bit_depth =
      ConvertToYUV(y_plane, u_plane, v_plane, OUTPUT_queue_->resolution(),
                   buffer->mmapped_planes(), CAPTURE_queue_->resolution(),
                   CAPTURE_queue_->fourcc());

  const std::set<int> reusable_buffer_slots = RefreshReferenceSlots(
      frame_hdr, CAPTURE_queue_->GetBuffer(buffer_id).get(),
      CAPTURE_queue_->queued_buffer_ids());

  for (const auto reusable_buffer_slot : reusable_buffer_slots) {
    if (v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_slot)) {
      // After decoding a key frame, all CAPTURE buffer slots can be reused and
      // queued, except the buffer holding the key frame. We want to avoid
      // queuing the CAPTURE buffer slots that are already queued from the
      // previous key frame. So we need to keep track of which buffers are
      // queued for all frames.
      CAPTURE_queue_->QueueBufferId(reusable_buffer_slot);
    } else {
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";
    }
  }

  v4l2_ioctl_->DQBuf(OUTPUT_queue_, &buffer_id);

  CHECK_EQ(buffer_id, uint32_t(0))
      << "Buffer ID of the buffer in OUTPUT queue is greater than size";

  v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_);

  return VideoDecoder::kOk;
}
}  // namespace v4l2_test
}  // namespace media
