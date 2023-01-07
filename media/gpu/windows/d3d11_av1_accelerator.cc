// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_av1_accelerator.h"

#include <windows.h>
#include <numeric>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"

namespace media {

using DecodeStatus = AV1Decoder::AV1Accelerator::Status;

class D3D11AV1Picture : public AV1Picture {
 public:
  explicit D3D11AV1Picture(D3D11PictureBuffer* d3d11_picture,
                           D3D11VideoDecoderClient* client,
                           bool apply_grain)
      : picture_buffer_(d3d11_picture),
        client_(client),
        apply_grain_(apply_grain),
        picture_index_(d3d11_picture->picture_index()) {
    picture_buffer_->set_in_picture_use(true);
  }

  bool apply_grain() const { return apply_grain_; }
  D3D11PictureBuffer* picture_buffer() const { return picture_buffer_; }

 protected:
  ~D3D11AV1Picture() override { picture_buffer_->set_in_picture_use(false); }

 private:
  scoped_refptr<AV1Picture> CreateDuplicate() override {
    // We've already sent off the base frame for rendering, so we can just stamp
    // |picture_buffer_| with the updated timestamp.
    client_->UpdateTimestamp(picture_buffer_);
    return this;
  }

  const raw_ptr<D3D11PictureBuffer> picture_buffer_;
  const raw_ptr<D3D11VideoDecoderClient> client_;
  const bool apply_grain_;
  const size_t picture_index_;
};

class D3D11AV1Accelerator::ScopedDecoderBuffer {
 public:
  ScopedDecoderBuffer(MediaLog* media_log,
                      VideoContextWrapper* context,
                      ID3D11VideoDecoder* decoder,
                      D3D11_VIDEO_DECODER_BUFFER_TYPE type)
      : media_log_(media_log),
        context_(context),
        decoder_(decoder),
        type_(type) {
    UINT size;
    uint8_t* buffer;
    driver_call_result_ = context_->GetDecoderBuffer(
        decoder_, type_, &size, reinterpret_cast<void**>(&buffer));
    if (FAILED(driver_call_result_)) {
      MEDIA_LOG(ERROR, media_log_)
          << "ScopedDecoderBuffer(" << type_
          << ")=" << logging::SystemErrorCodeToString(driver_call_result_);
      return;
    }

    buffer_ = base::span<uint8_t>(buffer, size);
  }
  ScopedDecoderBuffer(ScopedDecoderBuffer&& o)
      : media_log_(o.media_log_),
        context_(o.context_),
        decoder_(o.decoder_),
        type_(o.type_),
        buffer_(std::move(o.buffer_)) {
    DCHECK(o.buffer_.empty());
  }

  ~ScopedDecoderBuffer() { Commit(); }

  ScopedDecoderBuffer(const ScopedDecoderBuffer&) = delete;
  ScopedDecoderBuffer& operator=(const ScopedDecoderBuffer&) = delete;

  void Commit() {
    if (buffer_.empty())
      return;
    driver_call_result_ = context_->ReleaseDecoderBuffer(decoder_, type_);
    if (FAILED(driver_call_result_)) {
      MEDIA_LOG(ERROR, media_log_)
          << "~ScopedDecoderBuffer(" << type_
          << ")=" << logging::SystemErrorCodeToString(driver_call_result_);
    }
    buffer_ = base::span<uint8_t>();
  }

  bool empty() const { return buffer_.empty(); }
  uint8_t* data() const { return buffer_.data(); }
  size_t size() const { return buffer_.size(); }
  HRESULT error() const { return driver_call_result_; }

 private:
  const raw_ptr<MediaLog> media_log_;
  const raw_ptr<VideoContextWrapper> context_;
  const raw_ptr<ID3D11VideoDecoder> decoder_;
  const D3D11_VIDEO_DECODER_BUFFER_TYPE type_;
  base::span<uint8_t> buffer_;
  HRESULT driver_call_result_ = S_OK;
};

D3D11AV1Accelerator::D3D11AV1Accelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log->Clone()),
      video_device_(std::move(video_device)),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  client->SetDecoderCB(base::BindRepeating(
      &D3D11AV1Accelerator::SetVideoDecoder, base::Unretained(this)));
}

D3D11AV1Accelerator::~D3D11AV1Accelerator() {}

void D3D11AV1Accelerator::RecordFailure(const std::string& fail_type,
                                        D3D11Status error) {
  RecordFailure(fail_type, error.message(), error.code());
}

void D3D11AV1Accelerator::RecordFailure(const std::string& fail_type,
                                        const std::string& message,
                                        D3D11Status::Codes reason) {
  MEDIA_LOG(ERROR, media_log_)
      << "DX11AV1Failure(" << fail_type << ")=" << message;
}

scoped_refptr<AV1Picture> D3D11AV1Accelerator::CreateAV1Picture(
    bool apply_grain) {
  D3D11PictureBuffer* picture_buffer = client_->GetPicture();
  return picture_buffer ? base::MakeRefCounted<D3D11AV1Picture>(
                              picture_buffer, client_, apply_grain)
                        : nullptr;
}

D3D11AV1Accelerator::ScopedDecoderBuffer D3D11AV1Accelerator::GetBuffer(
    D3D11_VIDEO_DECODER_BUFFER_TYPE type) {
  return ScopedDecoderBuffer(media_log_.get(), video_context_.get(),
                             video_decoder_.Get(), type);
}

bool D3D11AV1Accelerator::SubmitDecoderBuffer(
    const DXVA_PicParams_AV1& pic_params,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers) {
  // Buffer #1 - AV1 specific picture parameters.
  auto params_buffer = GetBuffer(D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
  if (params_buffer.empty() || params_buffer.size() < sizeof(pic_params)) {
    RecordFailure("SubmitDecoderBuffers",
                  logging::SystemErrorCodeToString(params_buffer.error()),
                  D3D11Status::Codes::kGetPicParamBufferFailed);
    return false;
  }

  memcpy(params_buffer.data(), &pic_params, sizeof(pic_params));

  // Buffer #2 - Slice control data.
  const auto tile_size = sizeof(DXVA_Tile_AV1) * tile_buffers.size();
  auto tile_buffer = GetBuffer(D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);
  if (tile_buffer.empty() || tile_buffer.size() < tile_size) {
    RecordFailure("SubmitDecoderBuffers",
                  logging::SystemErrorCodeToString(tile_buffer.error()),
                  D3D11Status::Codes::kGetSliceControlBufferFailed);
    return false;
  }

  auto* tiles = reinterpret_cast<DXVA_Tile_AV1*>(tile_buffer.data());

  // Buffer #3 - Tile buffer bitstream data.
  const size_t bitstream_size = std::accumulate(
      tile_buffers.begin(), tile_buffers.end(), 0,
      [](size_t acc, const auto& buffer) { return acc + buffer.size; });
  auto bitstream_buffer = GetBuffer(D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
  if (bitstream_buffer.empty() || bitstream_buffer.size() < bitstream_size) {
    RecordFailure("SubmitDecoderBuffers",
                  logging::SystemErrorCodeToString(bitstream_buffer.error()),
                  D3D11Status::Codes::kGetBitstreamBufferFailed);
    return false;
  }

  size_t tile_offset = 0;
  for (size_t i = 0; i < tile_buffers.size(); ++i) {
    const auto& tile = tile_buffers[i];
    tiles[i].DataOffset = tile_offset;
    tiles[i].DataSize = tile.size;
    tiles[i].row = i / pic_params.tiles.cols;
    tiles[i].column = i % pic_params.tiles.cols;
    tiles[i].anchor_frame = 0xFF;

    memcpy(bitstream_buffer.data() + tile_offset, tile.data, tile.size);
    tile_offset += tile.size;
  }

  // Commit the buffers we prepared above.
  params_buffer.Commit();
  tile_buffer.Commit();
  bitstream_buffer.Commit();

  constexpr int kBuffersCount = 3;
  VideoContextWrapper::VideoBufferWrapper buffers[kBuffersCount] = {};
  buffers[0].BufferType = D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
  buffers[0].DataSize = sizeof(pic_params);
  buffers[1].BufferType = D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
  buffers[1].DataSize = tile_size;
  buffers[2].BufferType = D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
  buffers[2].DataSize = bitstream_size;

  const auto hr = video_context_->SubmitDecoderBuffers(video_decoder_.Get(),
                                                       kBuffersCount, buffers);
  if (FAILED(hr)) {
    RecordFailure("SubmitDecoderBuffers", logging::SystemErrorCodeToString(hr),
                  D3D11Status::Codes::kSubmitDecoderBuffersFailed);
    return false;
  }

  return true;
}

DecodeStatus D3D11AV1Accelerator::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& seq_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  const D3D11AV1Picture* pic_ptr = static_cast<const D3D11AV1Picture*>(&pic);
  do {
    ID3D11VideoDecoderOutputView* output_view = nullptr;
    auto result = pic_ptr->picture_buffer()->AcquireOutputView();
    if (result.has_value()) {
      output_view = std::move(result).value();
    } else {
      RecordFailure("AcquireOutputView", std::move(result).error());
      return DecodeStatus::kFail;
    }
    const auto hr = video_context_->DecoderBeginFrame(video_decoder_.Get(),
                                                      output_view, 0, nullptr);
    if (SUCCEEDED(hr)) {
      break;
    } else if (hr == E_PENDING || hr == D3DERR_WASSTILLDRAWING) {
      base::PlatformThread::YieldCurrentThread();
    } else if (FAILED(hr)) {
      RecordFailure("DecoderBeginFrame", logging::SystemErrorCodeToString(hr),
                    D3D11Status::Codes::kDecoderBeginFrameFailed);
      return DecodeStatus::kFail;
    }
  } while (true);

  DXVA_PicParams_AV1 pic_params = {0};
  FillPicParams(pic_ptr->picture_buffer()->picture_index(),
                pic_ptr->apply_grain(), pic.frame_header, seq_header,
                ref_frames, &pic_params);

  if (!SubmitDecoderBuffer(pic_params, tile_buffers))
    return DecodeStatus::kFail;

  const auto hr = video_context_->DecoderEndFrame(video_decoder_.Get());
  if (FAILED(hr)) {
    RecordFailure("DecoderEndFrame", logging::SystemErrorCodeToString(hr),
                  D3D11Status::Codes::kDecoderEndFrameFailed);
    return DecodeStatus::kFail;
  }

  return DecodeStatus::kOk;
}

bool D3D11AV1Accelerator::OutputPicture(const AV1Picture& pic) {
  const auto* pic_ptr = static_cast<const D3D11AV1Picture*>(&pic);
  return client_->OutputResult(pic_ptr, pic_ptr->picture_buffer());
}

void D3D11AV1Accelerator::SetVideoDecoder(ComD3D11VideoDecoder video_decoder) {
  video_decoder_ = std::move(video_decoder);
}

void D3D11AV1Accelerator::FillPicParams(
    size_t picture_index,
    bool apply_grain,
    const libgav1::ObuFrameHeader& frame_header,
    const libgav1::ObuSequenceHeader& seq_header,
    const AV1ReferenceFrameVector& ref_frames,
    DXVA_PicParams_AV1* pp) {
  // Note: Unclear from documentation if DXVA wants these values -1. The docs
  // say they correspond to the "minus_1" variants... Microsoft's dav1d
  // implementation uses the full values.
  pp->width = frame_header.width;
  pp->height = frame_header.height;
  pp->max_width = seq_header.max_frame_width;
  pp->max_height = seq_header.max_frame_height;

  pp->CurrPicTextureIndex = picture_index;
  pp->superres_denom = frame_header.use_superres
                           ? frame_header.superres_scale_denominator
                           : libgav1::kSuperResScaleNumerator;
  pp->bitdepth = seq_header.color_config.bitdepth;
  pp->seq_profile = seq_header.profile;

  const auto& tile_info = frame_header.tile_info;
  pp->tiles.cols = tile_info.tile_columns;
  pp->tiles.rows = tile_info.tile_rows;
  pp->tiles.context_update_id = tile_info.context_update_id;

  if (tile_info.uniform_spacing) {
    // TODO(b/174802667): Just use tile_column_width_in_superblocks and
    // tile_row_height_in_superblocks once they're always populated by libgav1.
    const auto tile_width_sb =
        (tile_info.sb_columns + (1 << tile_info.tile_columns_log2) - 1) >>
        tile_info.tile_columns_log2;
    const int last_width_idx = tile_info.tile_columns - 1;
    for (int i = 0; i < last_width_idx; ++i)
      pp->tiles.widths[i] = tile_width_sb;
    pp->tiles.widths[last_width_idx] =
        tile_info.sb_columns - last_width_idx * tile_width_sb;

    const auto tile_height_sb =
        (tile_info.sb_rows + (1 << tile_info.tile_rows_log2) - 1) >>
        tile_info.tile_rows_log2;
    const int last_height_idx = tile_info.tile_rows - 1;
    for (int i = 0; i < last_height_idx; ++i)
      pp->tiles.heights[i] = tile_height_sb;
    pp->tiles.heights[last_height_idx] =
        tile_info.sb_rows - last_height_idx * tile_height_sb;
  } else {
    for (int i = 0; i < pp->tiles.cols; ++i) {
      pp->tiles.widths[i] =
          frame_header.tile_info.tile_column_width_in_superblocks[i];
    }
    for (int i = 0; i < pp->tiles.rows; ++i) {
      pp->tiles.heights[i] =
          frame_header.tile_info.tile_row_height_in_superblocks[i];
    }
  }

  pp->coding.use_128x128_superblock = seq_header.use_128x128_superblock;
  pp->coding.intra_edge_filter = seq_header.enable_intra_edge_filter;
  pp->coding.interintra_compound = seq_header.enable_interintra_compound;
  pp->coding.masked_compound = seq_header.enable_masked_compound;

  // Note: The ObuSequenceHeader has a |enable_warped_motion| field and the
  // ObuFrameHeader has a |allow_warped_motion|. Per the DXVA spec,
  // "[warped_motion] corresponds to the syntax element named
  // allow_warped_motion from the specification."
  pp->coding.warped_motion = frame_header.allow_warped_motion;

  pp->coding.dual_filter = seq_header.enable_dual_filter;
  pp->coding.jnt_comp = seq_header.enable_jnt_comp;

  // Another field in both the sequence and frame header, per the DXVA spec:
  // "[screen_content_tools] corresponds to the syntax element named
  // allow_screen_content_tools from the specification."
  pp->coding.screen_content_tools = frame_header.allow_screen_content_tools;

  // Another field in both the sequence and frame header, per the DXVA spec:
  // "[integer_mv] corresponds to the syntax element named force_integer_mv
  // from the specification."
  pp->coding.integer_mv = frame_header.force_integer_mv;

  pp->coding.cdef = seq_header.enable_cdef;
  pp->coding.restoration = seq_header.enable_restoration;
  pp->coding.film_grain = seq_header.film_grain_params_present;
  pp->coding.intrabc = frame_header.allow_intrabc;
  pp->coding.high_precision_mv = frame_header.allow_high_precision_mv;
  pp->coding.switchable_motion_mode = frame_header.is_motion_mode_switchable;
  pp->coding.filter_intra = seq_header.enable_filter_intra;
  pp->coding.disable_frame_end_update_cdf =
      !frame_header.enable_frame_end_update_cdf;
  pp->coding.disable_cdf_update = !frame_header.enable_cdf_update;
  pp->coding.reference_mode = frame_header.reference_mode_select;
  pp->coding.skip_mode = frame_header.skip_mode_present;
  pp->coding.reduced_tx_set = frame_header.reduced_tx_set;
  pp->coding.superres = frame_header.use_superres;
  pp->coding.tx_mode = frame_header.tx_mode;
  pp->coding.use_ref_frame_mvs = frame_header.use_ref_frame_mvs;
  pp->coding.enable_ref_frame_mvs = seq_header.enable_ref_frame_mvs;
  pp->coding.reference_frame_update =
      !(frame_header.show_existing_frame &&
        frame_header.frame_type == libgav1::kFrameKey);

  pp->format.frame_type = frame_header.frame_type;
  pp->format.show_frame = frame_header.show_frame;
  pp->format.showable_frame = frame_header.showable_frame;
  pp->format.subsampling_x = seq_header.color_config.subsampling_x;
  pp->format.subsampling_y = seq_header.color_config.subsampling_y;
  pp->format.mono_chrome = seq_header.color_config.is_monochrome;

  pp->primary_ref_frame = frame_header.primary_reference_frame;
  pp->order_hint = frame_header.order_hint;
  pp->order_hint_bits = seq_header.order_hint_bits;

  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes - 1; ++i) {
    if (libgav1::IsIntraFrame(frame_header.frame_type)) {
      pp->frame_refs[i].Index = 0xFF;
      continue;
    }

    const auto ref_idx = frame_header.reference_frame_index[i];
    const auto* rp =
        static_cast<const D3D11AV1Picture*>(ref_frames[ref_idx].get());
    if (!rp) {
      pp->frame_refs[i].Index = 0xFF;
      continue;
    }

    pp->frame_refs[i].width = rp->frame_header.width;
    pp->frame_refs[i].height = rp->frame_header.height;

    const auto& gm =
        frame_header.global_motion[libgav1::kReferenceFrameLast + i];
    for (size_t j = 0; j < 6; ++j)
      pp->frame_refs[i].wmmat[j] = gm.params[j];
    pp->frame_refs[i].wminvalid =
        gm.type == libgav1::kGlobalMotionTransformationTypeIdentity;

    pp->frame_refs[i].wmtype = gm.type;
    pp->frame_refs[i].Index = ref_idx;
  }

  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    const auto* rp = static_cast<const D3D11AV1Picture*>(ref_frames[i].get());
    pp->RefFrameMapTextureIndex[i] =
        rp ? rp->picture_buffer()->picture_index() : 0xFF;
  }

  pp->loop_filter.filter_level[0] = frame_header.loop_filter.level[0];
  pp->loop_filter.filter_level[1] = frame_header.loop_filter.level[1];
  pp->loop_filter.filter_level_u = frame_header.loop_filter.level[2];
  pp->loop_filter.filter_level_v = frame_header.loop_filter.level[3];
  pp->loop_filter.sharpness_level = frame_header.loop_filter.sharpness;
  pp->loop_filter.mode_ref_delta_enabled =
      frame_header.loop_filter.delta_enabled;
  pp->loop_filter.mode_ref_delta_update = frame_header.loop_filter.delta_update;
  pp->loop_filter.delta_lf_multi = frame_header.delta_lf.multi;
  pp->loop_filter.delta_lf_present = frame_header.delta_lf.present;

  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i)
    pp->loop_filter.ref_deltas[i] = frame_header.loop_filter.ref_deltas[i];
  pp->loop_filter.mode_deltas[0] = frame_header.loop_filter.mode_deltas[0];
  pp->loop_filter.mode_deltas[1] = frame_header.loop_filter.mode_deltas[1];
  pp->loop_filter.delta_lf_res = frame_header.delta_lf.scale;

  for (size_t i = 0; i < libgav1::kMaxPlanes; ++i) {
    constexpr uint8_t kD3D11LoopRestorationMapping[4] = {
        0,  // libgav1::kLoopRestorationTypeNone,
        3,  // libgav1::kLoopRestorationTypeSwitchable,
        1,  // libgav1::kLoopRestorationTypeWiener,
        2,  // libgav1::kLoopRestorationTypeSgrProj
    };

    pp->loop_filter.frame_restoration_type[i] =
        kD3D11LoopRestorationMapping[frame_header.loop_restoration.type[i]];
    pp->loop_filter.log2_restoration_unit_size[i] =
        frame_header.loop_restoration.unit_size_log2[i];
  }

  pp->quantization.delta_q_present = frame_header.delta_q.present;
  pp->quantization.delta_q_res = frame_header.delta_q.scale;
  pp->quantization.base_qindex = frame_header.quantizer.base_index;
  pp->quantization.y_dc_delta_q = frame_header.quantizer.delta_dc[0];
  pp->quantization.u_dc_delta_q = frame_header.quantizer.delta_dc[1];
  pp->quantization.v_dc_delta_q = frame_header.quantizer.delta_dc[2];
  pp->quantization.u_ac_delta_q = frame_header.quantizer.delta_ac[1];
  pp->quantization.v_ac_delta_q = frame_header.quantizer.delta_ac[2];
  pp->quantization.qm_y = frame_header.quantizer.use_matrix
                              ? frame_header.quantizer.matrix_level[0]
                              : 0xFF;
  pp->quantization.qm_u = frame_header.quantizer.use_matrix
                              ? frame_header.quantizer.matrix_level[1]
                              : 0xFF;
  pp->quantization.qm_v = frame_header.quantizer.use_matrix
                              ? frame_header.quantizer.matrix_level[2]
                              : 0xFF;

  // libgav1 stores the computed versions of the cdef values, so we must undo
  // the computation for DXVA. See ObuParser::ParseCdefParameters().
  const uint8_t coeff_shift = pp->bitdepth - 8;
  pp->cdef.damping = frame_header.cdef.damping - coeff_shift - 3u;
  pp->cdef.bits = frame_header.cdef.bits;
  for (size_t i = 0; i < libgav1::kMaxCdefStrengths; ++i) {
    // libgav1's computation will give values of |4| for secondary strengths
    // despite it being a two-bit entry with range 0-3, so check for this, and
    // subtract.
    // See https://aomediacodec.github.io/av1-spec/#cdef-params-syntax
    uint8_t y_str = frame_header.cdef.y_secondary_strength[i] >> coeff_shift;
    uint8_t uv_str = frame_header.cdef.uv_secondary_strength[i] >> coeff_shift;
    y_str = y_str == 4 ? 3 : y_str;
    uv_str = uv_str == 4 ? 3 : uv_str;
    pp->cdef.y_strengths[i].primary =
        frame_header.cdef.y_primary_strength[i] >> coeff_shift;
    pp->cdef.y_strengths[i].secondary = y_str;
    pp->cdef.uv_strengths[i].primary =
        frame_header.cdef.uv_primary_strength[i] >> coeff_shift;
    pp->cdef.uv_strengths[i].secondary = uv_str;
  }

  pp->interp_filter = frame_header.interpolation_filter;

  pp->segmentation.enabled = frame_header.segmentation.enabled;
  pp->segmentation.update_map = frame_header.segmentation.update_map;
  pp->segmentation.update_data = frame_header.segmentation.update_data;
  pp->segmentation.temporal_update = frame_header.segmentation.temporal_update;
  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j) {
      pp->segmentation.feature_mask[i].mask |=
          frame_header.segmentation.feature_enabled[i][j] << j;
      pp->segmentation.feature_data[i][j] =
          frame_header.segmentation.feature_data[i][j];
    }
  }

  if (apply_grain) {
    const auto& fg = frame_header.film_grain_params;
    pp->film_grain.apply_grain = fg.apply_grain;
    pp->film_grain.scaling_shift_minus8 = fg.chroma_scaling - 8;
    pp->film_grain.chroma_scaling_from_luma = fg.chroma_scaling_from_luma;
    pp->film_grain.ar_coeff_lag = fg.auto_regression_coeff_lag;
    pp->film_grain.ar_coeff_shift_minus6 = fg.auto_regression_shift - 6;
    pp->film_grain.grain_scale_shift = fg.grain_scale_shift;
    pp->film_grain.overlap_flag = fg.overlap_flag;
    pp->film_grain.clip_to_restricted_range = fg.clip_to_restricted_range;
    pp->film_grain.matrix_coeff_is_identity =
        seq_header.color_config.matrix_coefficients ==
        libgav1::kMatrixCoefficientsIdentity;
    pp->film_grain.grain_seed = fg.grain_seed;
    pp->film_grain.num_y_points = fg.num_y_points;
    for (uint8_t i = 0; i < fg.num_y_points; ++i) {
      pp->film_grain.scaling_points_y[i][0] = fg.point_y_value[i];
      pp->film_grain.scaling_points_y[i][1] = fg.point_y_scaling[i];
    }
    pp->film_grain.num_cb_points = fg.num_u_points;
    for (uint8_t i = 0; i < fg.num_u_points; ++i) {
      pp->film_grain.scaling_points_cb[i][0] = fg.point_u_value[i];
      pp->film_grain.scaling_points_cb[i][1] = fg.point_u_scaling[i];
    }
    pp->film_grain.num_cr_points = fg.num_v_points;
    for (uint8_t i = 0; i < fg.num_v_points; ++i) {
      pp->film_grain.scaling_points_cr[i][0] = fg.point_v_value[i];
      pp->film_grain.scaling_points_cr[i][1] = fg.point_v_scaling[i];
    }
    for (size_t i = 0; i < std::size(fg.auto_regression_coeff_y); ++i) {
      pp->film_grain.ar_coeffs_y[i] = fg.auto_regression_coeff_y[i] + 128;
    }
    for (size_t i = 0; i < std::size(fg.auto_regression_coeff_u); ++i) {
      pp->film_grain.ar_coeffs_cb[i] = fg.auto_regression_coeff_u[i] + 128;
      pp->film_grain.ar_coeffs_cr[i] = fg.auto_regression_coeff_v[i] + 128;
    }
    // libgav1 will provide the multipliers by subtracting 128 and the offsets
    // by subtracting 256. Restore values as DXVA spec requires values without
    // subtraction.
    if (fg.num_u_points > 0) {
      pp->film_grain.cb_mult = fg.u_multiplier + 128;
      pp->film_grain.cb_luma_mult = fg.u_luma_multiplier + 128;
      pp->film_grain.cb_offset = fg.u_offset + 256;
    }
    if (fg.num_v_points > 0) {
      pp->film_grain.cr_mult = fg.v_multiplier + 128;
      pp->film_grain.cr_luma_mult = fg.v_luma_multiplier + 128;
      pp->film_grain.cr_offset = fg.v_offset + 256;
    }
  }

  // StatusReportFeedbackNumber "should not be equal to 0"... but it crashes :|
  // pp->StatusReportFeedbackNumber = ++status_feedback_;
}

}  // namespace media
