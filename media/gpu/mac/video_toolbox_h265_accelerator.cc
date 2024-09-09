// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/mac/video_toolbox_h265_accelerator.h"

#include <array>
#include <utility>

#include "base/numerics/byte_conversions.h"
#include "media/base/media_log.h"

namespace media {

namespace {
constexpr size_t kNALUHeaderLength = 4;
}  // namespace

VideoToolboxH265Accelerator::VideoToolboxH265Accelerator(
    std::unique_ptr<MediaLog> media_log,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxH265Accelerator::~VideoToolboxH265Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H265Picture> VideoToolboxH265Accelerator::CreateH265Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<H265Picture>();
}

void VideoToolboxH265Accelerator::ProcessVPS(
    const H265VPS* vps,
    base::span<const uint8_t> vps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " vps_video_parameter_set_id=" << vps->vps_video_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_vps_data_[vps->vps_video_parameter_set_id] =
      std::vector<uint8_t>(vps_nalu_data.begin(), vps_nalu_data.end());
  if (vps->aux_alpha_layer_id) {
    alpha_vps_ids_.insert(vps->vps_video_parameter_set_id);
  } else {
    alpha_vps_ids_.erase(vps->vps_video_parameter_set_id);
  }
}

void VideoToolboxH265Accelerator::ProcessSPS(
    const H265SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id
           << " sps_video_parameter_set_id=" << sps->sps_video_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_sps_data_[sps->sps_seq_parameter_set_id] =
      std::vector<uint8_t>(sps_nalu_data.begin(), sps_nalu_data.end());
}

void VideoToolboxH265Accelerator::ProcessPPS(
    const H265PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DVLOG(3) << __func__ << ":"
           << " pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id
           << " pps_seq_parameter_set_id=" << pps->pps_seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  seen_pps_data_[pps->pps_pic_parameter_set_id] =
      std::vector<uint8_t>(pps_nalu_data.begin(), pps_nalu_data.end());
}

bool VideoToolboxH265Accelerator::ExtractParameterSetData(
    const char* parameter_set_name,
    const base::flat_set<int>& parameter_set_ids,
    const base::flat_map<int, std::vector<uint8_t>>& seen_parameter_set_data,
    base::flat_map<int, std::vector<uint8_t>>* active_parameter_set_data_out,
    std::vector<const uint8_t*>* parameter_set_data_out,
    std::vector<size_t>* parameter_set_size_out) {
  for (int parameter_set_id : parameter_set_ids) {
    // Check that the ID is valid.
    const auto it = seen_parameter_set_data.find(parameter_set_id);
    if (it == seen_parameter_set_data.end()) {
      MEDIA_LOG(ERROR, media_log_.get())
          << "Missing " << parameter_set_name << " " << parameter_set_id;
      return false;
    }
    // Update active parameter set data.
    (*active_parameter_set_data_out)[parameter_set_id] = it->second;
    // Extract the parameter set data.
    parameter_set_data_out->push_back(it->second.data());
    parameter_set_size_out->push_back(it->second.size());
  }
  return true;
}

bool VideoToolboxH265Accelerator::CreateFormat(scoped_refptr<H265Picture> pic) {
  active_vps_data_.clear();
  active_sps_data_.clear();
  active_pps_data_.clear();

  // Gather parameter sets and update active parameter set data.
  std::vector<const uint8_t*> parameter_set_data;
  std::vector<size_t> parameter_set_size;
  if (!ExtractParameterSetData("VPS", frame_vps_ids_, seen_vps_data_,
                               &active_vps_data_, &parameter_set_data,
                               &parameter_set_size)) {
    return false;
  }
  if (!ExtractParameterSetData("SPS", frame_sps_ids_, seen_sps_data_,
                               &active_sps_data_, &parameter_set_data,
                               &parameter_set_size)) {
    return false;
  }
  if (!ExtractParameterSetData("PPS", frame_pps_ids_, seen_pps_data_,
                               &active_pps_data_, &parameter_set_data,
                               &parameter_set_size)) {
    return false;
  }

  // Create the format description.
  active_format_.reset();

  OSStatus status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(
      /*allocator=*/kCFAllocatorDefault,
      /*parameterSetCount=*/parameter_set_data.size(),
      /*parameterSetPointers=*/parameter_set_data.data(),
      /*parameterSetSizes=*/parameter_set_size.data(),
      /*NALUnitHeaderLength=*/kNALUHeaderLength,
      /*extensions=*/nullptr,
      /*formatDescriptionOut=*/active_format_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMVideoFormatDescriptionCreateFromHEVCParameterSets()";
    return false;
  }

  // Record session metadata.
  active_session_metadata_ = VideoToolboxDecompressionSessionMetadata{
      /*allow_software_decoding=*/true,
      /*bit_depth=*/frame_bit_depth_,
      /*chroma_sampling=*/frame_chroma_sampling_,
      /*has_alpha=*/frame_has_alpha_,
      /*visible_rect=*/pic->visible_rect()};

  return true;
}

VideoToolboxH265Accelerator::Status
VideoToolboxH265Accelerator::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__ << ":"
           << " sps_video_parameter_set_id=" << sps->sps_video_parameter_set_id
           << " sps_seq_parameter_set_id=" << sps->sps_seq_parameter_set_id
           << " pps_pic_parameter_set_id=" << pps->pps_pic_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetFrameData();

  if (pic->no_rasl_output_flag_ &&
      (slice_hdr->nal_unit_type == H265NALU::RASL_N ||
       slice_hdr->nal_unit_type == H265NALU::RASL_R)) {
    // Drop this RASL frame, otherwise VideoToolbox will fail to decode it.
    drop_frame_ = true;
    return Status::kOk;
  }

  // Update frame state.
  frame_is_keyframe_ = slice_hdr->irap_pic;
  frame_bit_depth_ = sps->bit_depth_y;
  frame_chroma_sampling_ = sps->GetChromaSampling();
  frame_has_alpha_ = alpha_vps_ids_.contains(sps->sps_video_parameter_set_id);

  return Status::kOk;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitSlice(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list0,
    const H265Picture::Vector& ref_pic_list1,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (drop_frame_) {
    return Status::kOk;
  }

  frame_vps_ids_.insert(sps->sps_video_parameter_set_id);
  frame_sps_ids_.insert(pps->pps_seq_parameter_set_id);
  frame_pps_ids_.insert(pps->pps_pic_parameter_set_id);
  frame_slice_data_.push_back(base::make_span(data, size));

  return Status::kOk;
}

bool VideoToolboxH265Accelerator::ExtractChangedParameterSetData(
    const char* parameter_set_name,
    const base::flat_set<int>& parameter_set_ids,
    const base::flat_map<int, std::vector<uint8_t>>& seen_parameter_set_data,
    base::flat_map<int, std::vector<uint8_t>>* active_parameter_set_data_out,
    std::vector<base::span<const uint8_t>>* parameter_set_data_out) {
  for (int parameter_set_id : parameter_set_ids) {
    // Check that the ID is valid.
    const auto seen_it = seen_parameter_set_data.find(parameter_set_id);
    if (seen_it == seen_parameter_set_data.end()) {
      MEDIA_LOG(ERROR, media_log_.get())
          << "Missing " << parameter_set_name << " " << parameter_set_id;
      return false;
    }
    // Check if the data has changed.
    const auto active_it =
        active_parameter_set_data_out->find(parameter_set_id);
    if (active_it == active_parameter_set_data_out->end() ||
        active_it->second != seen_it->second) {
      // Update active parameter set data.
      (*active_parameter_set_data_out)[parameter_set_id] = seen_it->second;
      // Extract the parameter set data.
      parameter_set_data_out->push_back(base::make_span(seen_it->second));
    }
  }
  return true;
}

VideoToolboxH265Accelerator::Status VideoToolboxH265Accelerator::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (drop_frame_) {
    return Status::kOk;
  }

  // Extract changed parameter sets and update active parameter set data.
  std::vector<base::span<const uint8_t>> combined_nalu_data;
  if (!ExtractChangedParameterSetData("VPS", frame_vps_ids_, seen_vps_data_,
                                      &active_vps_data_, &combined_nalu_data)) {
    return Status::kFail;
  }
  if (!ExtractChangedParameterSetData("SPS", frame_sps_ids_, seen_sps_data_,
                                      &active_sps_data_, &combined_nalu_data)) {
    return Status::kFail;
  }
  if (!ExtractChangedParameterSetData("PPS", frame_pps_ids_, seen_pps_data_,
                                      &active_pps_data_, &combined_nalu_data)) {
    return Status::kFail;
  }

  // Create a new format description if necessary.
  // We assume that session metadata can only change at a keyframe.
  // TODO(crbug.com/40227557): It's not clear when it is better to inline the
  // parameter sets vs. creating a new format.
  if (!active_format_ || (combined_nalu_data.size() && frame_is_keyframe_)) {
    combined_nalu_data.clear();
    CreateFormat(pic);
  }

  // Append slice data.
  combined_nalu_data.insert(combined_nalu_data.end(), frame_slice_data_.begin(),
                            frame_slice_data_.end());

  // Determine the final size of the converted bitstream.
  size_t data_size = 0;
  for (const auto& nalu_data : combined_nalu_data) {
    data_size += kNALUHeaderLength + nalu_data.size();
  }

  // Allocate a buffer.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      /*structureAllocator=*/kCFAllocatorDefault,
      /*memoryBlock=*/nullptr,
      /*blockLength=*/data_size,
      /*blockAllocator=*/kCFAllocatorDefault,
      /*customBlockSource=*/nullptr,
      /*offsetToData=*/0,
      /*dataLength=*/data_size,
      /*flags=*/0, data.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferCreateWithMemoryBlock()";
    return Status::kFail;
  }

  status = CMBlockBufferAssureBlockMemory(data.get());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMBlockBufferAssureBlockMemory()";
    return Status::kFail;
  }

  // Copy each NALU into the buffer, prefixed with a length header.
  size_t offset = 0u;
  for (const auto& nalu_data : combined_nalu_data) {
    // Write length header.
    std::array<uint8_t, kNALUHeaderLength> header =
        base::U32ToBigEndian(static_cast<uint32_t>(nalu_data.size()));
    status = CMBlockBufferReplaceDataBytes(header.data(), data.get(), offset,
                                           header.size());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferReplaceDataBytes()";
      return Status::kFail;
    }
    offset += header.size();

    // Write NALU data.
    status = CMBlockBufferReplaceDataBytes(nalu_data.data(), data.get(), offset,
                                           nalu_data.size());
    if (status != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
          << "CMBlockBufferReplaceDataBytes()";
      return Status::kFail;
    }
    offset += nalu_data.size();
  }

  // Wrap in a sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(
      /*allocator=*/kCFAllocatorDefault,
      /*dataBuffer=*/data.get(),
      /*dataReady=*/true,
      /*makeDataReadyCallback=*/nullptr,
      /*makeDataReadyRefcon=*/nullptr,
      /*formatDescription=*/active_format_.get(),
      /*numSamples=*/1,
      /*numSampleTimingEntries=*/0,
      /*sampleTimingArray=*/nullptr,
      /*numSampleSizeEntries=*/1,
      /*sampleSizeArray=*/&data_size, sample.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMSampleBufferCreate()";
    return Status::kFail;
  }

  decode_cb_.Run(std::move(sample), active_session_metadata_, std::move(pic));
  return Status::kOk;
}

bool VideoToolboxH265Accelerator::OutputPicture(
    scoped_refptr<H265Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't care about outputs, just pass them along.
  output_cb_.Run(std::move(pic));
  return true;
}

void VideoToolboxH265Accelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The decompression session will probably also be reset, so we can't expect
  // it to know about any parameter sets. https://crbug.com/1493624
  active_vps_data_.clear();
  active_sps_data_.clear();
  active_pps_data_.clear();
  active_format_.reset();

  ResetFrameData();
}

void VideoToolboxH265Accelerator::ResetFrameData() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  frame_vps_ids_.clear();
  frame_sps_ids_.clear();
  frame_pps_ids_.clear();
  frame_slice_data_.clear();
  frame_bit_depth_ = 8;
  frame_chroma_sampling_ = VideoChromaSampling::k420;
  frame_is_keyframe_ = false;
  frame_has_alpha_ = false;
  drop_frame_ = false;
}

bool VideoToolboxH265Accelerator::IsChromaSamplingSupported(
    VideoChromaSampling format) {
  return true;
}

bool VideoToolboxH265Accelerator::IsAlphaLayerSupported() {
  return true;
}

}  // namespace media
