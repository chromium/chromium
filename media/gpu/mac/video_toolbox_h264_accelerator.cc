// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h264_accelerator.h"

#include <array>
#include <utility>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "media/base/media_log.h"
#include "media/base/video_types.h"

namespace media {

namespace {

// Kill-switch: Remove after M145 is stable.
BASE_FEATURE(kResetDecoderForNonIDR, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

VideoToolboxH264Accelerator::VideoToolboxH264Accelerator(
    std::unique_ptr<MediaLog> media_log,
    DecodeCB decode_cb,
    OutputCB output_cb)
    : media_log_(std::move(media_log)),
      decode_cb_(std::move(decode_cb)),
      output_cb_(std::move(output_cb)),
      sps_tracker_("SPS", media_log_.get()),
      pps_tracker_("PPS", media_log_.get()) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VideoToolboxH264Accelerator::~VideoToolboxH264Accelerator() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

scoped_refptr<H264Picture> VideoToolboxH264Accelerator::CreateH264Picture() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::MakeRefCounted<H264Picture>();
}

void VideoToolboxH264Accelerator::ProcessSPS(
    const H264SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  DVLOG(3) << __func__
           << ": seq_parameter_set_id=" << sps->seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sps_tracker_.Process(sps->seq_parameter_set_id, sps_nalu_data);
}

void VideoToolboxH264Accelerator::ProcessPPS(
    const H264PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  DVLOG(3) << __func__ << ": pic_parameter_set_id=" << pps->pic_parameter_set_id
           << " seq_parameter_set_id=" << pps->seq_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pps_tracker_.Process(pps->pic_parameter_set_id, pps_nalu_data);
}

bool VideoToolboxH264Accelerator::CreateFormat() {
  // Gather parameter sets and update active parameter set data.
  std::vector<const uint8_t*> parameter_set_data;
  std::vector<size_t> parameter_set_size;
  if (!sps_tracker_.ExtractForFormat(parameter_set_data, parameter_set_size) ||
      !pps_tracker_.ExtractForFormat(parameter_set_data, parameter_set_size)) {
    return false;
  }

  // Create the format description.
  active_format_.reset();

  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      /*allocator=*/kCFAllocatorDefault,
      /*parameterSetCount=*/parameter_set_data.size(),
      /*parameterSetPointers=*/parameter_set_data.data(),
      /*parameterSetSizes=*/parameter_set_size.data(),
      /*NALUnitHeaderLength=*/kNALUHeaderLength,
      active_format_.InitializeInto());
  if (status != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, status, media_log_.get())
        << "CMVideoFormatDescriptionCreateFromH264ParameterSets()";
    return false;
  }

  return true;
}

VideoToolboxH264Accelerator::Status
VideoToolboxH264Accelerator::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  DVLOG(3) << __func__ << ": seq_parameter_set_id=" << sps->seq_parameter_set_id
           << " pic_parameter_set_id=" << pps->pic_parameter_set_id;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetFrameData();
  return Status::kOk;
}

VideoToolboxH264Accelerator::Status VideoToolboxH264Accelerator::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sps_tracker_.ReferenceInFrame(pps->seq_parameter_set_id);
  pps_tracker_.ReferenceInFrame(pps->pic_parameter_set_id);
  frame_slice_data_.push_back(UNSAFE_TODO(base::span(data, size)));

  return Status::kOk;
}

VideoToolboxH264Accelerator::Status VideoToolboxH264Accelerator::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Extract changed parameter sets and update active parameter set data.
  std::vector<base::span<const uint8_t>> combined_nalu_data;
  if (!sps_tracker_.ExtractForInbandUpdate(combined_nalu_data) ||
      !pps_tracker_.ExtractForInbandUpdate(combined_nalu_data)) {
    return Status::kFail;
  }

  // Create a new format description if we haven't initialized one yet, or if we
  // are at a keyframe and the parameter set data has changed.
  if (!active_format_ || (pic->idr && !combined_nalu_data.empty())) {
    combined_nalu_data.clear();
    if (!CreateFormat()) {
      return Status::kFail;
    }
  }

  // Append slice data.
  combined_nalu_data.insert(combined_nalu_data.end(), frame_slice_data_.begin(),
                            frame_slice_data_.end());

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample =
      VideoToolboxCreateSampleBufferFromNALUs(
          combined_nalu_data, active_format_.get(), media_log_.get());
  if (!sample) {
    return Status::kFail;
  }

  if (!pic->idr && first_decode_ &&
      base::FeatureList::IsEnabled(kResetDecoderForNonIDR)) {
    // Flag the sample if it's non-IDR and the first sample provided. This was
    // recommended by Apple to prevent corruption when seeking to SEI
    // recovery points. See https://crbug.com/451536366.
    CMSetAttachment(sample.get(),
                    kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding,
                    kCFBooleanTrue, kCMAttachmentMode_ShouldNotPropagate);
  }
  first_decode_ = false;

  VideoToolboxDecompressionSessionMetadata session_metadata = {
#if defined(ARCH_CPU_X86_FAMILY)
      // Allow software decoding on Intel hardware where the cutoff is around
      // 480p and breaks tests.
      /*allow_software_decoding=*/true,
#else
      /*allow_software_decoding=*/false,
#endif  // defined(ARCH_CPU_X86_FAMILY)
      /*bit_depth=*/8,
      /*chroma_sampling=*/VideoChromaSampling::k420,
      /*has_alpha=*/false,
      /*visible_rect=*/pic->visible_rect()};
  decode_cb_.Run(std::move(sample), session_metadata, std::move(pic));
  return Status::kOk;
}

bool VideoToolboxH264Accelerator::OutputPicture(
    scoped_refptr<H264Picture> pic) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't care about outputs, just pass them along.
  output_cb_.Run(std::move(pic));
  return true;
}

void VideoToolboxH264Accelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sps_tracker_.ResetActive();
  pps_tracker_.ResetActive();
  active_format_.reset();
  ResetFrameData();
  first_decode_ = true;
}

void VideoToolboxH264Accelerator::ResetFrameData() {
  DVLOG(4) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sps_tracker_.ResetFrame();
  pps_tracker_.ResetFrame();
  frame_slice_data_.clear();
}

}  // namespace media
