// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/av1_decoder.h"

#include <linux/media/av1-ctrls.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/base/video_types.h"
#include "media/filters/ivf_parser.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/test/av1_pix_fmt.h"
#include "third_party/libgav1/src/src/warp_prediction.h"

namespace media {

namespace v4l2_test {

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

// TODO(stevecho): Remove this provision when av1-ctrls.h includes linux/bits.h.
#ifndef BIT
#define BIT(nr) (1U << (nr))
#endif

inline void conditionally_set_flags(__u8* flags,
                                    const bool condition,
                                    const bool mask) {
  *flags |= (condition ? mask : 0);
}

// Section 5.9.11. Loop filter params syntax in AV1 spec.
// https://aomediacodec.github.io/av1-spec/av1-spec.pdf
// Note that |update_ref_delta| and |update_mode_delta| flags in the spec
// are not needed for V4L2 AV1 API.
// TODO(stevecho): sanity check data structures in libgav1 against the AV1 spec.
void FillLoopFilterParams(struct v4l2_av1_loop_filter* v4l2_lf,
                          const libgav1::LoopFilter& lf) {
  conditionally_set_flags(&v4l2_lf->flags, lf.delta_enabled,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_ENABLED);
  conditionally_set_flags(&v4l2_lf->flags, lf.delta_update,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_UPDATE);

  static_assert(std::size(decltype(v4l2_lf->level){}) == libgav1::kFrameLfCount,
                "Invalid size of loop filter level (strength) array");
  for (size_t i = 0; i < libgav1::kFrameLfCount; i++)
    v4l2_lf->level[i] = base::checked_cast<__u8>(lf.level[i]);

  v4l2_lf->sharpness = lf.sharpness;

  static_assert(std::size(decltype(v4l2_lf->ref_deltas){}) ==
                    libgav1::kNumReferenceFrameTypes,
                "Invalid size of ref deltas array");
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
    v4l2_lf->ref_deltas[i] = lf.ref_deltas[i];

  static_assert(std::size(decltype(v4l2_lf->mode_deltas){}) ==
                    libgav1::kLoopFilterMaxModeDeltas,
                "Invalid size of mode deltas array");
  for (size_t i = 0; i < libgav1::kLoopFilterMaxModeDeltas; i++)
    v4l2_lf->mode_deltas[i] = lf.mode_deltas[i];
}

// Section 5.9.18. Loop filter delta parameters syntax.
// Note that |delta_lf_res| in |v4l2_av1_loop_filter| corresponds to
// |delta_lf.scale| in the frame header defined in libgav1.
void FillLoopFilterDeltaParams(struct v4l2_av1_loop_filter* v4l2_lf,
                               const libgav1::Delta& delta_lf) {
  conditionally_set_flags(&v4l2_lf->flags, delta_lf.present,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_PRESENT);

  v4l2_lf->delta_lf_res = delta_lf.scale;
  v4l2_lf->delta_lf_multi = delta_lf.multi;
}

// Section 5.9.12. Quantization params syntax
void FillQuantizationParams(struct v4l2_av1_quantization* v4l2_quant,
                            const libgav1::QuantizerParameters& quant) {
  conditionally_set_flags(&v4l2_quant->flags, quant.use_matrix,
                          V4L2_AV1_QUANTIZATION_FLAG_USING_QMATRIX);

  v4l2_quant->base_q_idx = quant.base_index;

  // Note that quant.delta_ac[0] is useless as it is always 0 according to
  // libgav1.
  v4l2_quant->delta_q_y_dc = quant.delta_dc[0];

  v4l2_quant->delta_q_u_dc = quant.delta_dc[1];
  v4l2_quant->delta_q_u_ac = quant.delta_ac[1];

  v4l2_quant->delta_q_v_dc = quant.delta_dc[2];
  v4l2_quant->delta_q_v_ac = quant.delta_ac[2];

  if (!quant.use_matrix)
    return;

  v4l2_quant->qm_y = base::checked_cast<uint8_t>(quant.matrix_level[0]);
  v4l2_quant->qm_u = base::checked_cast<uint8_t>(quant.matrix_level[1]);
  v4l2_quant->qm_v = base::checked_cast<uint8_t>(quant.matrix_level[2]);
}

// Section 5.9.17. Quantizer index delta parameters syntax
void FillQuantizerIndexDeltaParams(
    struct v4l2_av1_quantization* v4l2_quant,
    const absl::optional<libgav1::ObuSequenceHeader>& seq_header,
    const libgav1::ObuFrameHeader& frm_header) {
  // |diff_uv_delta| in the spec doesn't exist in libgav1,
  // because libgav1 infers it using the following logic.
  const bool diff_uv_delta = (frm_header.quantizer.base_index != 0) &&
                             (!seq_header->color_config.is_monochrome) &&
                             (seq_header->color_config.separate_uv_delta_q);
  conditionally_set_flags(&v4l2_quant->flags, diff_uv_delta,
                          V4L2_AV1_QUANTIZATION_FLAG_DIFF_UV_DELTA);

  conditionally_set_flags(&v4l2_quant->flags, frm_header.delta_q.present,
                          V4L2_AV1_QUANTIZATION_FLAG_DELTA_Q_PRESENT);

  // |scale| is used to store |delta_q_res| value. This is because libgav1 uses
  // the same struct |Delta| both for quantizer index delta parameters and loop
  // filter delta parameters.
  v4l2_quant->delta_q_res = frm_header.delta_q.scale;
}

// Section 5.9.14. Segmentation params syntax
void FillSegmentationParams(struct v4l2_av1_segmentation* v4l2_seg,
                            const libgav1::Segmentation& seg) {
  conditionally_set_flags(&v4l2_seg->flags, seg.enabled,
                          V4L2_AV1_SEGMENTATION_FLAG_ENABLED);
  conditionally_set_flags(&v4l2_seg->flags, seg.update_map,
                          V4L2_AV1_SEGMENTATION_FLAG_UPDATE_MAP);
  conditionally_set_flags(&v4l2_seg->flags, seg.update_data,
                          V4L2_AV1_SEGMENTATION_FLAG_TEMPORAL_UPDATE);
  conditionally_set_flags(&v4l2_seg->flags, seg.temporal_update,
                          V4L2_AV1_SEGMENTATION_FLAG_UPDATE_DATA);
  conditionally_set_flags(&v4l2_seg->flags, seg.segment_id_pre_skip,
                          V4L2_AV1_SEGMENTATION_FLAG_SEG_ID_PRE_SKIP);

  static_assert(
      std::size(decltype(v4l2_seg->feature_enabled){}) == libgav1::kMaxSegments,
      "Invalid size of |feature_enabled| array in |v4l2_av1_segmentation| "
      "struct");

  static_assert(
      std::size(decltype(v4l2_seg->feature_data){}) == libgav1::kMaxSegments &&
          std::extent<decltype(v4l2_seg->feature_data), 0>::value ==
              libgav1::kSegmentFeatureMax,
      "Invalid size of |feature_data| array in |v4l2_av1_segmentation| struct");

  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j) {
      v4l2_seg->feature_enabled[i] |= (seg.feature_enabled[i][j] << j);
      v4l2_seg->feature_data[i][j] = seg.feature_data[i][j];
    }
  }

  v4l2_seg->last_active_seg_id = seg.last_active_segment_id;
}

// Section 5.9.19. CDEF params syntax
void FillCdefParams(struct v4l2_av1_cdef* v4l2_cdef,
                    const libgav1::Cdef& cdef,
                    uint8_t color_bitdepth) {
  // Damping value parsed in libgav1 is from the spec + (bitdepth - 8).
  // All the strength values parsed in libgav1 are from the spec and left
  // shifted by (bitdepth - 8).
  CHECK_GE(color_bitdepth, 8u);
  const uint8_t coeff_shift = color_bitdepth - 8u;

  v4l2_cdef->damping_minus_3 =
      base::checked_cast<uint8_t>(cdef.damping - coeff_shift - 3u);

  v4l2_cdef->bits = cdef.bits;

  static_assert(std::size(decltype(v4l2_cdef->y_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->y_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_sec_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->uv_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->uv_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_sec_strength strength");

  SafeArrayMemcpy(v4l2_cdef->y_pri_strength, cdef.y_primary_strength);
  SafeArrayMemcpy(v4l2_cdef->y_sec_strength, cdef.y_secondary_strength);
  SafeArrayMemcpy(v4l2_cdef->uv_pri_strength, cdef.uv_primary_strength);
  SafeArrayMemcpy(v4l2_cdef->uv_sec_strength, cdef.uv_secondary_strength);
}

// 5.9.20. Loop restoration params syntax
void FillLoopRestorationParams(v4l2_av1_loop_restoration* v4l2_lr,
                               const libgav1::LoopRestoration& lr) {
  for (size_t i = 0; i < V4L2_AV1_NUM_PLANES_MAX; i++) {
    switch (lr.type[i]) {
      case libgav1::LoopRestorationType::kLoopRestorationTypeNone:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_NONE;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeWiener:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_WIENER;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SGRPROJ;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SWITCHABLE;
        break;
      default:
        NOTREACHED() << "Invalid loop restoration type";
    }

    if (v4l2_lr->frame_restoration_type[i] != V4L2_AV1_FRAME_RESTORE_NONE) {
      conditionally_set_flags(&v4l2_lr->flags, true,
                              V4L2_AV1_LOOP_RESTORATION_FLAG_USES_LR);

      conditionally_set_flags(&v4l2_lr->flags, i > 0,
                              V4L2_AV1_LOOP_RESTORATION_FLAG_USES_CHROMA_LR);
    }
  }

  const bool use_loop_restoration =
      std::find_if(std::begin(lr.type),
                   std::begin(lr.type) + libgav1::kMaxPlanes,
                   [](const auto type) {
                     return type != libgav1::kLoopRestorationTypeNone;
                   }) != (lr.type + libgav1::kMaxPlanes);

  if (!use_loop_restoration)
    return;

  DCHECK_GE(lr.unit_size_log2[0], lr.unit_size_log2[1]);
  DCHECK_LE(lr.unit_size_log2[0] - lr.unit_size_log2[1], 1);
  v4l2_lr->lr_unit_shift = lr.unit_size_log2[0] - 6;
  v4l2_lr->lr_uv_shift = lr.unit_size_log2[0] - lr.unit_size_log2[1];

  // AV1 spec (p.52) uses this formula with hard coded value 2.
  v4l2_lr->loop_restoration_size[0] =
      V4L2_AV1_RESTORATION_TILESIZE_MAX >> (2 - v4l2_lr->lr_unit_shift);
  v4l2_lr->loop_restoration_size[1] =
      v4l2_lr->loop_restoration_size[0] >> v4l2_lr->lr_uv_shift;
  v4l2_lr->loop_restoration_size[2] =
      v4l2_lr->loop_restoration_size[0] >> v4l2_lr->lr_uv_shift;
}

// Section 5.9.15. Tile info syntax
void FillTileInfo(v4l2_av1_tile_info* v4l2_ti, const libgav1::TileInfo& ti) {
  conditionally_set_flags(&v4l2_ti->flags, ti.uniform_spacing,
                          V4L2_AV1_TILE_INFO_FLAG_UNIFORM_TILE_SPACING);
  static_assert(std::size(decltype(v4l2_ti->mi_col_starts){}) ==
                    (libgav1::kMaxTileColumns + 1),
                "Size of |mi_col_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");

  for (size_t i = 0; i < libgav1::kMaxTileColumns + 1; i++)
    v4l2_ti->mi_col_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_column_start[i]);

  static_assert(std::size(decltype(v4l2_ti->mi_row_starts){}) ==
                    (libgav1::kMaxTileRows + 1),
                "Size of |mi_row_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");
  for (size_t i = 0; i < libgav1::kMaxTileRows + 1; i++)
    v4l2_ti->mi_row_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_row_start[i]);

  if (!ti.uniform_spacing) {
    // Confirmed that |kMaxTileColumns| is enough size for
    // |width_in_sbs_minus_1| and |kMaxTileRows| is enough size for
    // |height_in_sbs_minus_1|
    // https://b.corp.google.com/issues/187828854#comment19
    static_assert(
        std::size(decltype(v4l2_ti->width_in_sbs_minus_1){}) ==
            libgav1::kMaxTileColumns,
        "Size of |width_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileColumns; i++) {
      CHECK_GE(ti.tile_column_width_in_superblocks[i], 1);
      v4l2_ti->width_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
          ti.tile_column_width_in_superblocks[i] - 1);
    }

    static_assert(
        std::size(decltype(v4l2_ti->height_in_sbs_minus_1){}) ==
            libgav1::kMaxTileRows,
        "Size of |height_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileRows; i++) {
      CHECK_GE(ti.tile_row_height_in_superblocks[i], 1);
      v4l2_ti->height_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
          ti.tile_row_height_in_superblocks[i] - 1);
    }
  }

  v4l2_ti->tile_size_bytes = ti.tile_size_bytes;
  v4l2_ti->context_update_tile_id = ti.context_update_id;
  v4l2_ti->tile_cols = ti.tile_columns;
  v4l2_ti->tile_rows = ti.tile_rows;
}

// Section 5.9.24. Global motion params syntax
void FillGlobalMotionParams(
    v4l2_av1_global_motion* v4l2_gm,
    const std::array<libgav1::GlobalMotion, libgav1::kNumReferenceFrameTypes>&
        gm_array) {
  // gm_array[0] (for kReferenceFrameIntra) is not used because global motion is
  // not relevant for intra frames
  for (size_t i = 1; i < libgav1::kNumReferenceFrameTypes; ++i) {
    // Copy |gm_array| to |gm| because SetupShear() updates the affine variables
    // of the |gm_array|.
    auto gm = gm_array[i];
    switch (gm.type) {
      case libgav1::kGlobalMotionTransformationTypeIdentity:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_IDENTITY;
        break;
      case libgav1::kGlobalMotionTransformationTypeTranslation:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_TRANSLATION;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_GLOBAL_MOTION_FLAG_IS_TRANSLATION);
        break;
      case libgav1::kGlobalMotionTransformationTypeRotZoom:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_ROTZOOM;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_GLOBAL_MOTION_FLAG_IS_ROT_ZOOM);
        break;
      case libgav1::kGlobalMotionTransformationTypeAffine:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_AFFINE;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_WARP_MODEL_AFFINE);
        break;
      default:
        NOTREACHED() << "Invalid global motion transformation type, "
                     << v4l2_gm->type[i];
    }

    conditionally_set_flags(
        &v4l2_gm->flags[i],
        gm.type != libgav1::kGlobalMotionTransformationTypeIdentity,
        V4L2_AV1_GLOBAL_MOTION_FLAG_IS_GLOBAL);

    constexpr auto kNumGlobalMotionParams = std::size(decltype(gm.params){});

    for (size_t j = 0; j < kNumGlobalMotionParams; ++j)
      v4l2_gm->params[i][j] = base::checked_cast<uint32_t>(gm.params[j]);

    v4l2_gm[i].invalid = !libgav1::SetupShear(&gm);
  }
}

Av1Decoder::Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       std::unique_ptr<V4L2Queue> OUTPUT_queue,
                       std::unique_ptr<V4L2Queue> CAPTURE_queue)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl),
                                 std::move(OUTPUT_queue),
                                 std::move(CAPTURE_queue)),
      ivf_parser_(std::move(ivf_parser)),
      buffer_pool_(std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr)),
      state_(std::make_unique<libgav1::DecoderState>()) {}

Av1Decoder::~Av1Decoder() {
  // We destroy the state explicitly to ensure it's destroyed before the
  // |buffer_pool_|. The |buffer_pool_| checks that all the allocated frames
  // are released in its destructor.
  state_.reset();
  DCHECK(buffer_pool_);
}

// static
std::unique_ptr<Av1Decoder> Av1Decoder::Create(
    const base::MemoryMappedFile& stream) {
  constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_AV1_FRAME;

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

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>();

  // MM21 is an uncompressed opaque format that is produced by MediaTek
  // video decoders.
  constexpr uint32_t kUncompressedFourcc = v4l2_fourcc('M', 'M', '2', '1');

  // TODO(stevecho): this might need some driver patches to support AV1F
  if (!v4l2_ioctl->VerifyCapabilities(kDriverCodecFourcc,
                                      kUncompressedFourcc)) {
    LOG(ERROR) << "Device doesn't support the provided FourCCs.";
    return nullptr;
  }

  LOG(INFO) << "Ivf file header: " << file_header.width << " x "
            << file_header.height;

  // TODO(stevecho): might need to consider using more than 1 file descriptor
  // (fd) & buffer with the output queue for 4K60 requirement.
  // https://buganizer.corp.google.com/issues/202214561#comment31
  auto OUTPUT_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kDriverCodecFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/1,
      V4L2_MEMORY_MMAP, /*num_buffers=*/1);

  // TODO(stevecho): enable V4L2_MEMORY_DMABUF memory for CAPTURE queue.
  // |num_planes| represents separate memory buffers, not planes for Y, U, V.
  // https://www.kernel.org/doc/html/v5.16/userspace-api/media/v4l/pixfmt-v4l2-mplane.html#c.V4L.v4l2_plane_pix_format
  auto CAPTURE_queue = std::make_unique<V4L2Queue>(
      V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kUncompressedFourcc,
      gfx::Size(file_header.width, file_header.height), /*num_planes=*/2,
      V4L2_MEMORY_MMAP, /*num_buffers=*/10);

  return base::WrapUnique(
      new Av1Decoder(std::move(ivf_parser), std::move(v4l2_ioctl),
                     std::move(OUTPUT_queue), std::move(CAPTURE_queue)));
}

Av1Decoder::ParsingResult Av1Decoder::ReadNextFrame(
    libgav1::RefCountedBufferPtr& current_frame) {
  if (!obu_parser_ || !obu_parser_->HasData()) {
    if (!ivf_parser_->ParseNextFrame(&ivf_frame_header_, &ivf_frame_data_))
      return ParsingResult::kEOStream;

    // The ObuParser has run out of data or did not exist in the first place. It
    // has no "replace the current buffer with a new buffer of a different size"
    // method; we must make a new parser.
    // (std::nothrow) is required for the base class Allocable of
    // libgav1::ObuParser
    obu_parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        ivf_frame_data_, ivf_frame_header_.frame_size, /*operating_point=*/0,
        buffer_pool_.get(), state_.get()));
    if (current_sequence_header_)
      obu_parser_->set_sequence_header(*current_sequence_header_);
  }

  const libgav1::StatusCode code = obu_parser_->ParseOneFrame(&current_frame);
  if (code != libgav1::kStatusOk) {
    LOG(ERROR) << "Error parsing OBU stream: " << libgav1::GetErrorString(code);
    return ParsingResult::kFailed;
  }
  return ParsingResult::kOk;
}

void Av1Decoder::CopyFrameData(const libgav1::ObuFrameHeader& frame_hdr,
                               std::unique_ptr<V4L2Queue>& queue) {
  CHECK_EQ(queue->num_buffers(), 1u)
      << "Only 1 buffer is expected to be used for OUTPUT queue for now.";

  CHECK_EQ(queue->num_planes(), 1u)
      << "Number of planes is expected to be 1 for OUTPUT queue.";

  scoped_refptr<MmapedBuffer> buffer = queue->GetBuffer(0);

  buffer->mmaped_planes()[0].CopyIn(ivf_frame_data_,
                                    ivf_frame_header_.frame_size);
}

std::set<int> Av1Decoder::RefreshReferenceSlots(
    uint8_t refresh_frame_flags,
    libgav1::RefCountedBufferPtr current_frame,
    scoped_refptr<MmapedBuffer> buffer,
    uint32_t last_queued_buffer_index) {
  state_->UpdateReferenceFrames(current_frame,
                                base::strict_cast<int>(refresh_frame_flags));

  static_assert(
      kAv1NumRefFrames == sizeof(refresh_frame_flags) * CHAR_BIT,
      "|refresh_frame_flags| size must be equal to |kAv1NumRefFrames|");

  const std::bitset<kAv1NumRefFrames> refresh_frame_slots(refresh_frame_flags);

  std::set<int> reusable_buffer_ids;

  constexpr uint8_t kRefreshFrameFlagsNone = 0;
  if (refresh_frame_flags == kRefreshFrameFlagsNone) {
    // Indicates to reuse currently decoded CAPTURE buffer.
    reusable_buffer_ids.insert(buffer->buffer_id());

    return reusable_buffer_ids;
  }

  constexpr uint8_t kRefreshFrameFlagsAll = 0xFF;
  if (refresh_frame_flags == kRefreshFrameFlagsAll) {
    // After decoding a key frame, all CAPTURE buffers can be reused except the
    // CAPTURE buffer corresponding to the key frame.
    for (size_t i = 0; i < kNumberOfBuffersInCaptureQueue; i++)
      reusable_buffer_ids.insert(i);

    reusable_buffer_ids.erase(buffer->buffer_id());

    // Note that the CAPTURE buffer for previous frame can be used as well,
    // but it is already queued again at this point.
    reusable_buffer_ids.erase(last_queued_buffer_index);

    // Updates to assign current key frame as a reference frame for all
    // reference frame slots in the reference frames list.
    ref_frames_.fill(buffer);

    return reusable_buffer_ids;
  }

  // More than one slot in |refresh_frame_flags| can be set.
  for (size_t i = 0; i < kAv1NumRefFrames; i++) {
    if (!refresh_frame_slots[i])
      continue;

    // It is not required to check whether existing reference frame slot is
    // already pointing to a reference frame. This is because reference
    // frame slots are empty only after the first key frame decoding.
    const uint16_t reusable_candidate_buffer_id = ref_frames_[i]->buffer_id();
    reusable_buffer_ids.insert(reusable_candidate_buffer_id);

    // Checks to make sure |reusable_candidate_buffer_id| is not used in
    // different reference frame slots in the reference frames list. If
    // |reusable_candidate_buffer_id| is already being used, then it is no
    // longer qualified as a reusable buffer. Thus, it is removed from
    // |reusable_buffer_ids|.
    for (size_t j = 0; j < kAv1NumRefFrames; j++) {
      const bool is_refresh_slot_not_used = (refresh_frame_slots[j] == false);
      const bool is_candidate_used =
          (ref_frames_[j]->buffer_id() == reusable_candidate_buffer_id);

      if (is_refresh_slot_not_used && is_candidate_used) {
        reusable_buffer_ids.erase(reusable_candidate_buffer_id);
        break;
      }
    }
    ref_frames_[i] = buffer;
  }

  return reusable_buffer_ids;
}

VideoDecoder::Result Av1Decoder::DecodeNextFrame(std::vector<char>& y_plane,
                                                 std::vector<char>& u_plane,
                                                 std::vector<char>& v_plane,
                                                 gfx::Size& size,
                                                 const int frame_number) {
  libgav1::RefCountedBufferPtr current_frame;
  const ParsingResult parser_res = ReadNextFrame(current_frame);

  if (parser_res != ParsingResult::kOk) {
    LOG_ASSERT(parser_res == ParsingResult::kEOStream)
        << "Failed to parse next frame.";
    return VideoDecoder::kEOStream;
  }

  libgav1::ObuFrameHeader current_frame_header = obu_parser_->frame_header();

  if (obu_parser_->sequence_header_changed())
    current_sequence_header_.emplace(obu_parser_->sequence_header());

  LOG_ASSERT(current_sequence_header_)
      << "Sequence header missing for decoding.";

  CopyFrameData(current_frame_header, OUTPUT_queue_);

  LOG_ASSERT(OUTPUT_queue_->num_buffers() == 1)
      << "Too many buffers in OUTPUT queue. It is currently designed to "
         "support only 1 request at a time.";

  OUTPUT_queue_->GetBuffer(0)->set_frame_number(frame_number);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for OUTPUT queue.";

  // TODO(b/230891887): use uint64_t when v4l2_timeval_to_ns() function is used.
  constexpr uint32_t kInvalidSurface = std::numeric_limits<uint32_t>::max();

  for (const auto ref_frame_index :
       current_frame_header.reference_frame_index) {
    LOG_ASSERT(ref_frame_index < kAv1NumRefFrames)
        << "Invalid reference frame index.\n";

    constexpr size_t kTimestampToNanoSecs = 1000;

    // |reference_id| is needed to use previously decoded frames
    // from reference frames list.
    const auto reference_id =
        ref_frames_[ref_frame_index]
            ? ref_frames_[ref_frame_index]->frame_number() *
                  kTimestampToNanoSecs
            : kInvalidSurface;

    // TODO(stevecho): add setup for frame parameters using |reference_id|
    // when av1 kernel header is ready.
    ANALYZER_ALLOW_UNUSED(reference_id);
  }

  // TODO(b/239618516): add ext_ctrl for V4L2_CID_STATELESS_AV1_SEQUENCE

  struct v4l2_ctrl_av1_frame_header v4l2_frame_params = {};

  FillLoopFilterParams(&v4l2_frame_params.loop_filter,
                       current_frame_header.loop_filter);

  FillLoopFilterDeltaParams(&v4l2_frame_params.loop_filter,
                            current_frame_header.delta_lf);

  FillQuantizationParams(&v4l2_frame_params.quantization,
                         current_frame_header.quantizer);

  FillQuantizerIndexDeltaParams(&v4l2_frame_params.quantization,
                                current_sequence_header_, current_frame_header);

  FillSegmentationParams(&v4l2_frame_params.segmentation,
                         current_frame_header.segmentation);

  const auto color_bitdepth = current_sequence_header_->color_config.bitdepth;
  FillCdefParams(&v4l2_frame_params.cdef, current_frame_header.cdef,
                 base::strict_cast<int8_t>(color_bitdepth));

  FillLoopRestorationParams(&v4l2_frame_params.loop_restoration,
                            current_frame_header.loop_restoration);

  FillTileInfo(&v4l2_frame_params.tile_info, current_frame_header.tile_info);

  FillGlobalMotionParams(&v4l2_frame_params.global_motion,
                         current_frame_header.global_motion);

  // TODO(stevecho): V4L2_CID_STATELESS_AV1_FRAME_HEADER is trending to be
  // changed to V4L2_CID_STATELESS_AV1_FRAME
  struct v4l2_ext_control ext_ctrl = {.id = V4L2_CID_STATELESS_AV1_FRAME_HEADER,
                                      .size = sizeof(v4l2_frame_params),
                                      .ptr = &v4l2_frame_params};

  struct v4l2_ext_controls ext_ctrls = {.count = 1, .controls = &ext_ctrl};

  if (!v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls))
    LOG(FATAL) << "VIDIOC_S_EXT_CTRLS failed.";

  if (!v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_))
    LOG(FATAL) << "MEDIA_REQUEST_IOC_QUEUE failed.";

  uint32_t index;

  if (!v4l2_ioctl_->DQBuf(CAPTURE_queue_, &index))
    LOG(FATAL) << "VIDIOC_DQBUF failed for CAPTURE queue.";

  scoped_refptr<MmapedBuffer> buffer = CAPTURE_queue_->GetBuffer(index);

  if (!v4l2_ioctl_->DQBuf(OUTPUT_queue_, &index))
    LOG(FATAL) << "VIDIOC_DQBUF failed for OUTPUT queue.";

  if (!v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_))
    LOG(FATAL) << "MEDIA_REQUEST_IOC_REINIT failed.";

  const std::set<int> reusable_buffer_ids =
      RefreshReferenceSlots(current_frame_header.refresh_frame_flags,
                            current_frame, CAPTURE_queue_->GetBuffer(index),
                            CAPTURE_queue_->last_queued_buffer_index());

  for (const auto reusable_buffer_id : reusable_buffer_ids) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_id))
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";

    if (!libgav1::IsIntraFrame(current_frame_header.frame_type))
      CAPTURE_queue_->set_last_queued_buffer_index(reusable_buffer_id);
  }

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
