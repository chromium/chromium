// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/h264_decoder.h"

#include <va/va.h>

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "media/base/subsample_entry.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/test/h264_dpb.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/h264_parser.h"

namespace media::vaapi_test {

namespace {

struct PicNumDescCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_num > b->pic_num;
  }
};

struct LongTermPicNumAscCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->long_term_pic_num < b->long_term_pic_num;
  }
};

struct POCAscCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_order_cnt < b->pic_order_cnt;
  }
};

struct POCDescCompare {
  bool operator()(const scoped_refptr<H264Picture>& a,
                  const scoped_refptr<H264Picture>& b) const {
    return a->pic_order_cnt > b->pic_order_cnt;
  }
};

bool FillH264PictureFromSliceHeader(const H264SPS* sps,
                                    const H264SliceHeader& slice_hdr,
                                    H264Picture* pic) {
  DCHECK(pic);

  pic->idr = slice_hdr.idr_pic_flag;
  if (pic->idr)
    pic->idr_pic_id = slice_hdr.idr_pic_id;

  if (slice_hdr.field_pic_flag) {
    pic->field = slice_hdr.bottom_field_flag ? H264Picture::FIELD_BOTTOM
                                             : H264Picture::FIELD_TOP;
  } else {
    pic->field = H264Picture::FIELD_NONE;
  }

  if (pic->field != H264Picture::FIELD_NONE) {
    VLOG(1) << "Interlaced video not supported.";
    return false;
  }

  pic->nal_ref_idc = slice_hdr.nal_ref_idc;
  pic->ref = slice_hdr.nal_ref_idc != 0;
  // This assumes non-interlaced stream.
  pic->frame_num = pic->pic_num = slice_hdr.frame_num;

  if (!sps)
    return false;

  pic->pic_order_cnt_type = sps->pic_order_cnt_type;
  switch (pic->pic_order_cnt_type) {
    case 0:
      pic->pic_order_cnt_lsb = slice_hdr.pic_order_cnt_lsb;
      pic->delta_pic_order_cnt_bottom = slice_hdr.delta_pic_order_cnt_bottom;
      break;

    case 1:
      pic->delta_pic_order_cnt0 = slice_hdr.delta_pic_order_cnt0;
      pic->delta_pic_order_cnt1 = slice_hdr.delta_pic_order_cnt1;
      break;

    case 2:
      break;

    default:
      NOTREACHED();
  }
  return true;
}

}  // namespace

H264Decoder::H264Decoder(const uint8_t* stream_data,
                         size_t stream_len,
                         const VaapiDevice& va_device,
                         SharedVASurface::FetchPolicy fetch_policy)
    : VideoDecoder::VideoDecoder(va_device, fetch_policy),
      parser_(std::make_unique<H264Parser>()),
      curr_sps_id_(-1),
      curr_pps_id_(-1),
      curr_slice_hdr_(nullptr),
      curr_nalu_(nullptr),
      curr_picture_(nullptr),
      is_stream_over_(false),
      va_wrapper_(va_device) {
  parser_->SetStream(stream_data, stream_len);

  LOG_ASSERT(GetStreamMetadata()) << "Stream contains no Sequence Parameters!";

  ExtractSliceHeader();
}

H264Decoder::~H264Decoder() {
  dpb_.Clear();
  last_decoded_surface_.reset();
}

VideoDecoder::Result H264Decoder::DecodeNextFrame() {
  while (!is_stream_over_ && output_queue.empty())
    DecodeNextFrameInStream();

  if (is_stream_over_ && output_queue.empty())
    return VideoDecoder::kEOStream;

  last_decoded_surface_ = output_queue.front()->surface;
  VLOG(4) << "Outputting frame poc: " << output_queue.front()->pic_order_cnt;
  last_decoded_frame_visible_ = !output_queue.front()->nonexisting;
  output_queue.pop();
  return VideoDecoder::kOk;
}

// H264 does not guarantee frames appear in the order they need to be shown in.
// This method merely decodes the next frame in the stream, it's up to the
// caller to actually figure out how to order them.
void H264Decoder::DecodeNextFrameInStream() {
  StartNewFrame();
  ProcessSlice();

  while (true) {
    curr_nalu_ = std::make_unique<H264NALU>();
    H264Parser::Result parser_result =
        parser_->AdvanceToNextNALU(curr_nalu_.get());
    if (parser_result == H264Parser::kEOStream) {
      is_stream_over_ = true;
      break;
    }

    if (curr_nalu_->nal_unit_type == H264NALU::kNonIDRSlice ||
        curr_nalu_->nal_unit_type == H264NALU::kIDRSlice) {
      ExtractSliceHeader();

      if (IsNewFrame())
        break;

      ProcessSlice();
    } else if (curr_nalu_->nal_unit_type == H264NALU::kSPS) {
      UpdateSequenceParams();
    } else if (curr_nalu_->nal_unit_type == H264NALU::kPPS) {
      UpdatePictureParams();
    } else {
      VLOG(4) << "Skipping NALU type " << curr_nalu_->nal_unit_type;
    }
  }

  DecodeFrame();

  FinishPicture(curr_picture_);

  if (is_stream_over_) {
    FlushDPB();
  }
}

void H264Decoder::ProcessSlice() {
  if (!curr_slice_hdr_->field_pic_flag) {
    max_pic_num_ = max_frame_num_;
  } else {
    max_pic_num_ = 2 * max_frame_num_;
  }

  H264Picture::Vector ref_pic_list0, ref_pic_list1;
  LOG_ASSERT(ModifyReferencePicLists(curr_slice_hdr_.get(), &ref_pic_list0,
                                     &ref_pic_list1))
      << "Error modify reference pic lists!";

  const H264PPS* pic_params = parser_->GetPPS(curr_pps_id_);
  LOG_ASSERT(pic_params) << "No picture params for ID " << curr_pps_id_;

  va_wrapper_.SubmitSlice(
      pic_params, curr_slice_hdr_.get(), ref_pic_list0, ref_pic_list1,
      curr_picture_.get(), curr_slice_hdr_->nalu_data,
      curr_slice_hdr_->nalu_size, parser_->GetCurrentSubsamples());
}

void H264Decoder::UpdateSequenceParams() {
  LOG_ASSERT(parser_->ParseSPS(&curr_sps_id_) == H264Parser::kOk)
      << "Error parsing SPS NALU";

  const H264SPS* sps = parser_->GetSPS(curr_sps_id_);
  LOG_ASSERT(sps) << "SPS not present for ID " << curr_sps_id_;

  const gfx::Rect new_visible_rect =
      sps->GetVisibleRect().value_or(gfx::Rect());
  if (visible_rect_ != new_visible_rect) {
    VLOG(2) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }

  const gfx::Size new_pic_size = sps->GetCodedSize().value_or(gfx::Size());
  LOG_ASSERT(!new_pic_size.IsEmpty()) << "Invalid picture size";

  const int width_mb = base::checked_cast<int>(new_pic_size.width()) / 16;
  const int height_mb = base::checked_cast<int>(new_pic_size.height()) / 16;

  LOG_ASSERT(std::numeric_limits<int>::max() / width_mb > height_mb)
      << "Picture size is too big: " << new_pic_size.ToString();

  // Spec A.3.1 and A.3.2
  // For Baseline, Constrained Baseline and Main profile, the indicated level is
  // Level 1b if level_idc is equal to 11 and constraint_set3_flag is equal to 1
  uint8_t level = base::checked_cast<uint8_t>(sps->level_idc);
  if ((sps->profile_idc == H264SPS::kProfileIDCBaseline ||
       sps->profile_idc == H264SPS::kProfileIDCConstrainedBaseline ||
       sps->profile_idc == H264SPS::kProfileIDCMain) &&
      level == 11 && sps->constraint_set3_flag) {
    level = 9;  // Level 1b
  }
  int max_dpb_mbs = base::checked_cast<int>(H264LevelToMaxDpbMbs(level));
  LOG_ASSERT(max_dpb_mbs) << "Invalid profile level " << level;

  // MaxDpbFrames from level limits per spec.
  size_t max_dpb_frames = std::min(max_dpb_mbs / (width_mb * height_mb),
                                   static_cast<int>(H264DPB::kDPBMaxSize));
  VLOG(1) << "MaxDpbFrames: " << max_dpb_frames
          << ", max_num_ref_frames: " << sps->max_num_ref_frames
          << ", max_dec_frame_buffering: " << sps->max_dec_frame_buffering;

  // Set DPB size to at least the level limit, or what the stream requires.
  size_t max_dpb_size =
      std::max(static_cast<int>(max_dpb_frames),
               std::max(sps->max_num_ref_frames, sps->max_dec_frame_buffering));
  // Some non-conforming streams specify more frames are needed than the current
  // level limit. Allow this, but only up to the maximum number of reference
  // frames allowed per spec.
  LOG_ASSERT(max_dpb_size <= max_dpb_frames)
      << "Invalid stream, DPB size > MaxDpbFrames";
  LOG_ASSERT(max_dpb_size != 0 && max_dpb_size <= H264DPB::kDPBMaxSize)
      << "Invalid DPB size: " << max_dpb_size;

  LOG_ASSERT(sps->chroma_format_idc == 1) << "Only YUV 4:2:0 is supported";

  dpb_.set_max_num_pics(max_dpb_size);

  UpdateMaxNumReorderFrames(sps);
}

void H264Decoder::UpdatePictureParams() {
  LOG_ASSERT(parser_->ParsePPS(&curr_pps_id_) == H264Parser::kOk)
      << "Error parsing PPS NALU";
}

void H264Decoder::DecodeFrame() {
  va_wrapper_.SubmitDecode(curr_picture_);
}

void H264Decoder::FinishPicture(scoped_refptr<H264Picture> pic) {
  // Finish processing the picture.
  // Start by storing previous picture data for later use.
  if (pic->ref) {
    ReferencePictureMarking(pic);
    prev_ref_has_memmgmnt5_ = pic->mem_mgmt_5;
    prev_ref_top_field_order_cnt_ = pic->top_field_order_cnt;
    prev_ref_pic_order_cnt_msb_ = pic->pic_order_cnt_msb;
    prev_ref_pic_order_cnt_lsb_ = pic->pic_order_cnt_lsb;
    prev_ref_field_ = pic->field;
    prev_ref_frame_num_ = pic->frame_num;
  }
  prev_frame_num_ = pic->frame_num;
  prev_has_memmgmnt5_ = pic->mem_mgmt_5;
  prev_frame_num_offset_ = pic->frame_num_offset;

  dpb_.DeleteUnused();

  VLOG(4) << "Finishing picture frame_num: " << pic->frame_num
          << ", poc: " << pic->pic_order_cnt
          << ", entries in DPB: " << dpb_.size();
  if (recovery_frame_cnt_) {
    // This is the first picture after the recovery point SEI message. Computes
    // the frame_num of the frame that should be output from (Spec D.2.8).
    recovery_frame_num_ =
        (*recovery_frame_cnt_ + pic->frame_num) % max_frame_num_;
    VLOG(3) << "recovery_frame_num_" << *recovery_frame_num_;
    recovery_frame_cnt_.reset();
  }

  // Get all pictures that haven't been outputted yet.
  H264Picture::Vector not_outputted;
  dpb_.GetNotOutputtedPicsAppending(&not_outputted);
  // Include the one we've just decoded.
  not_outputted.push_back(pic);

  // Sort in output order.
  std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

  // Try to output as many pictures as we can. A picture can be output,
  // if the number of decoded and not yet outputted pictures that would remain
  // in DPB afterwards would at least be equal to max_num_reorder_frames.
  // If the outputted picture is not a reference picture, it doesn't have
  // to remain in the DPB and can be removed.
  auto output_candidate = not_outputted.begin();
  size_t num_remaining = not_outputted.size();
  while (num_remaining > max_num_reorder_frames_ ||
         // If the condition below is used, this is an invalid stream. We should
         // not be forced to output beyond max_num_reorder_frames in order to
         // make room in DPB to store the current picture (if we need to do so).
         // However, if this happens, ignore max_num_reorder_frames and try
         // to output more. This may cause out-of-order output, but is not
         // fatal, and better than failing instead.
         ((dpb_.IsFull() && (!pic->outputted || pic->ref)) && num_remaining)) {
    VLOG_IF(1, num_remaining <= max_num_reorder_frames_)
        << "Invalid stream: max_num_reorder_frames not preserved";

    if (!recovery_frame_num_ ||
        // If we are decoding ahead to reach a SEI recovery point, skip
        // outputting all pictures before it, to avoid outputting corrupted
        // frames.
        (*output_candidate)->frame_num == *recovery_frame_num_) {
      recovery_frame_num_ = std::nullopt;
      output_queue.push(*output_candidate);
      (*output_candidate)->outputted = true;
    }

    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      int outputted_poc = (*output_candidate)->pic_order_cnt;
      if (outputted_poc != pic->pic_order_cnt) {
        dpb_.DeleteByPOC(outputted_poc);
      }
    }

    ++output_candidate;
    --num_remaining;
  }

  // If we haven't managed to output the picture that we just decoded, or if
  // it's a reference picture, we have to store it in DPB.
  if (!pic->outputted || pic->ref) {
    if (dpb_.IsFull()) {
      // If we haven't managed to output anything to free up space in DPB
      // to store this picture, it's an error in the stream.
      VLOG(1) << "Could not free up space in DPB!";
    }

    dpb_.StorePic(std::move(pic));
  }
}

bool H264Decoder::GetStreamMetadata() {
  bool found_sps = false;
  while (true) {
    curr_nalu_ = std::make_unique<H264NALU>();
    H264Parser::Result parser_result =
        parser_->AdvanceToNextNALU(curr_nalu_.get());
    if (parser_result == H264Parser::kEOStream)
      return found_sps;

    if (curr_nalu_->nal_unit_type == H264NALU::kNonIDRSlice ||
        curr_nalu_->nal_unit_type == H264NALU::kIDRSlice) {
      return found_sps;
    } else if (curr_nalu_->nal_unit_type == H264NALU::kSPS) {
      found_sps = true;
      UpdateSequenceParams();
    } else if (curr_nalu_->nal_unit_type == H264NALU::kPPS) {
      UpdatePictureParams();
    } else {
      VLOG(4) << "Skipping NALU type " << curr_nalu_->nal_unit_type;
    }
  }
}

bool H264Decoder::IsNewFrame() {
  if (curr_slice_hdr_->frame_num != curr_picture_->frame_num ||
      curr_slice_hdr_->pic_parameter_set_id != curr_pps_id_ ||
      curr_slice_hdr_->nal_ref_idc != curr_picture_->nal_ref_idc ||
      curr_slice_hdr_->idr_pic_flag != curr_picture_->idr ||
      (curr_slice_hdr_->idr_pic_flag &&
       (curr_slice_hdr_->idr_pic_id != curr_picture_->idr_pic_id ||
        curr_slice_hdr_->first_mb_in_slice == 0))) {
    return true;
  }

  const H264SPS* sequence_params = parser_->GetSPS(curr_sps_id_);
  if (!sequence_params)
    return false;

  if (sequence_params->pic_order_cnt_type ==
      curr_picture_->pic_order_cnt_type) {
    if (curr_picture_->pic_order_cnt_type == 0) {
      if (curr_slice_hdr_->pic_order_cnt_lsb !=
              curr_picture_->pic_order_cnt_lsb ||
          curr_slice_hdr_->delta_pic_order_cnt_bottom !=
              curr_picture_->delta_pic_order_cnt_bottom) {
        return true;
      }
    } else if (curr_picture_->pic_order_cnt_type == 1) {
      if (curr_slice_hdr_->delta_pic_order_cnt0 !=
              curr_picture_->delta_pic_order_cnt0 ||
          curr_slice_hdr_->delta_pic_order_cnt1 !=
              curr_picture_->delta_pic_order_cnt1) {
        return true;
      }
    }
  }

  return false;
}

void H264Decoder::ExtractSliceHeader() {
  curr_slice_hdr_ = std::make_unique<H264SliceHeader>();
  H264Parser::Result result =
      parser_->ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get());
  LOG_ASSERT(result == H264Parser::kOk)
      << "Error parsing slice header! Parser returned " << result;
}

void H264Decoder::StartNewFrame() {
  if (curr_slice_hdr_->idr_pic_flag) {
    if (!curr_slice_hdr_->no_output_of_prior_pics_flag)
      FlushDPB();

    dpb_.Clear();
  }

  curr_pps_id_ = curr_slice_hdr_->pic_parameter_set_id;
  const H264PPS* picture_params = parser_->GetPPS(curr_pps_id_);
  LOG_ASSERT(picture_params)
      << "Error extracting picture parameters for ID " << curr_pps_id_;

  curr_sps_id_ = picture_params->seq_parameter_set_id;
  const H264SPS* sequence_params = parser_->GetSPS(curr_sps_id_);
  LOG_ASSERT(sequence_params)
      << "Error extracting sequence parameters for ID" << curr_sps_id_;

  curr_picture_ = va_wrapper_.CreatePicture(sequence_params);

  max_frame_num_ = 1 << (sequence_params->log2_max_frame_num_minus4 + 4);
  int frame_num = curr_slice_hdr_->frame_num;

  if (curr_slice_hdr_->idr_pic_flag)
    prev_ref_frame_num_ = 0;

  if (frame_num != prev_ref_frame_num_ &&
      frame_num != (prev_ref_frame_num_ + 1) % max_frame_num_) {
    LOG_ASSERT(HandleFrameNumGap(frame_num)) << "Error handling frame num gap";
  }

  LOG_ASSERT(InitCurrPicture(curr_slice_hdr_.get())) << "Error initializing"
                                                     << " picture.";

  UpdatePicNums(frame_num);
  ConstructReferencePicListsP();
  ConstructReferencePicListsB();

  va_wrapper_.SubmitFrameMetadata(sequence_params, picture_params, dpb_,
                                  ref_pic_list_p0_, ref_pic_list_b0_,
                                  ref_pic_list_b1_, curr_picture_);
}

bool H264Decoder::InitCurrPicture(const H264SliceHeader* slice_hdr) {
  if (!FillH264PictureFromSliceHeader(parser_->GetSPS(curr_sps_id_), *slice_hdr,
                                      curr_picture_.get())) {
    return false;
  }

  if (!CalculatePicOrderCounts(curr_picture_))
    return false;

  curr_picture_->long_term_reference_flag = slice_hdr->long_term_reference_flag;
  curr_picture_->adaptive_ref_pic_marking_mode_flag =
      slice_hdr->adaptive_ref_pic_marking_mode_flag;

  // If the slice header indicates we will have to perform reference marking
  // process after this picture is decoded, store required data for that
  // purpose.
  if (slice_hdr->adaptive_ref_pic_marking_mode_flag) {
    static_assert(sizeof(curr_picture_->ref_pic_marking) ==
                      sizeof(slice_hdr->ref_pic_marking),
                  "Array sizes of ref pic marking do not match.");
    memcpy(curr_picture_->ref_pic_marking, slice_hdr->ref_pic_marking,
           sizeof(curr_picture_->ref_pic_marking));
  }

  curr_picture_->visible_rect = visible_rect_;

  return true;
}

void H264Decoder::ConstructReferencePicListsP() {
  // RefPicList0 (8.2.4.2.1) [[1] [2]], where:
  // [1] shortterm ref pics sorted by descending pic_num,
  // [2] longterm ref pics by ascending long_term_pic_num.
  ref_pic_list_p0_.clear();

  // First get the short ref pics...
  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_p0_);
  size_t num_short_refs = ref_pic_list_p0_.size();

  // and sort them to get [1].
  std::sort(ref_pic_list_p0_.begin(), ref_pic_list_p0_.end(),
            PicNumDescCompare());

  // Now get long term pics and sort them by long_term_pic_num to get [2].
  dpb_.GetLongTermRefPicsAppending(&ref_pic_list_p0_);
  std::sort(ref_pic_list_p0_.begin() + num_short_refs, ref_pic_list_p0_.end(),
            LongTermPicNumAscCompare());
}

void H264Decoder::ConstructReferencePicListsB() {
  // RefPicList0 (8.2.4.2.3) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC < curr_pic's POC sorted by descending POC,
  // [2] shortterm ref pics with POC > curr_pic's POC by ascending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.
  ref_pic_list_b0_.clear();
  ref_pic_list_b1_.clear();
  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_b0_);
  size_t num_short_refs = ref_pic_list_b0_.size();

  // First sort ascending, this will put [1] in right place and finish [2].
  std::sort(ref_pic_list_b0_.begin(), ref_pic_list_b0_.end(), POCAscCompare());

  // Find first with POC > curr_pic's POC to get first element in [2]...
  H264Picture::Vector::iterator iter;
  iter = std::upper_bound(ref_pic_list_b0_.begin(), ref_pic_list_b0_.end(),
                          curr_picture_.get(), POCAscCompare());

  // and sort [1] descending, thus finishing sequence [1] [2].
  std::sort(ref_pic_list_b0_.begin(), iter, POCDescCompare());

  // Now add [3] and sort by ascending long_term_pic_num.
  dpb_.GetLongTermRefPicsAppending(&ref_pic_list_b0_);
  std::sort(ref_pic_list_b0_.begin() + num_short_refs, ref_pic_list_b0_.end(),
            LongTermPicNumAscCompare());

  // RefPicList1 (8.2.4.2.4) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
  // [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.

  dpb_.GetShortTermRefPicsAppending(&ref_pic_list_b1_);
  num_short_refs = ref_pic_list_b1_.size();

  // First sort by descending POC.
  std::sort(ref_pic_list_b1_.begin(), ref_pic_list_b1_.end(), POCDescCompare());

  // Find first with POC < curr_pic's POC to get first element in [2]...
  iter = std::upper_bound(ref_pic_list_b1_.begin(), ref_pic_list_b1_.end(),
                          curr_picture_.get(), POCDescCompare());

  // and sort [1] ascending.
  std::sort(ref_pic_list_b1_.begin(), iter, POCAscCompare());

  // Now add [3] and sort by ascending long_term_pic_num
  dpb_.GetLongTermRefPicsAppending(&ref_pic_list_b1_);
  std::sort(ref_pic_list_b1_.begin() + num_short_refs, ref_pic_list_b1_.end(),
            LongTermPicNumAscCompare());

  // If lists identical, swap first two entries in RefPicList1 (spec 8.2.4.2.3)
  if (ref_pic_list_b1_.size() > 1 &&
      base::ranges::equal(ref_pic_list_b0_, ref_pic_list_b1_))
    std::swap(ref_pic_list_b1_[0], ref_pic_list_b1_[1]);
}

void H264Decoder::UpdatePicNums(int frame_num) {
  for (auto& pic : dpb_) {
    if (!pic->ref)
      continue;

    // 8.2.4.1. Assumes non-interlaced stream.
    DCHECK_EQ(pic->field, H264Picture::FIELD_NONE);
    if (pic->long_term) {
      pic->long_term_pic_num = pic->long_term_frame_idx;
    } else {
      if (pic->frame_num > frame_num)
        pic->frame_num_wrap = pic->frame_num - max_frame_num_;
      else
        pic->frame_num_wrap = pic->frame_num;

      pic->pic_num = pic->frame_num_wrap;
    }
  }
}

bool H264Decoder::InitNonexistingPicture(scoped_refptr<H264Picture> pic,
                                         int frame_num,
                                         bool ref) {
  pic->nonexisting = true;
  pic->nal_ref_idc = 1;
  pic->frame_num = pic->pic_num = frame_num;
  pic->adaptive_ref_pic_marking_mode_flag = false;
  pic->ref = ref;
  pic->long_term_reference_flag = false;
  pic->field = H264Picture::FIELD_NONE;

  return CalculatePicOrderCounts(pic);
}

bool H264Decoder::HandleFrameNumGap(int frame_num) {
  const H264SPS* sps = parser_->GetSPS(curr_sps_id_);
  if (!sps)
    return false;

  if (!sps->gaps_in_frame_num_value_allowed_flag) {
    VLOG(1) << "Invalid frame_num: " << frame_num;
    // TODO(b:129119729, b:146914440): Youtube android app sometimes sends an
    // invalid frame number after a seek. The sequence goes like:
    // Seek, SPS, PPS, IDR-frame, non-IDR, ... non-IDR with invalid number.
    // The only way to work around this reliably is to ignore this error.
    // Video playback is not affected, no artefacts are visible.
    // return false;
  }

  VLOG(2) << "Handling frame_num gap: " << prev_ref_frame_num_ << "->"
          << frame_num;

  // 7.4.3/7-23
  int unused_short_term_frame_num = (prev_ref_frame_num_ + 1) % max_frame_num_;
  while (unused_short_term_frame_num != frame_num) {
    scoped_refptr<H264Picture> pic = va_wrapper_.CreatePicture(sps);
    if (!InitNonexistingPicture(pic, unused_short_term_frame_num, true))
      return false;

    UpdatePicNums(unused_short_term_frame_num);

    FinishPicture(pic);

    unused_short_term_frame_num++;
    unused_short_term_frame_num %= max_frame_num_;
  }

  return true;
}

bool H264Decoder::CalculatePicOrderCounts(scoped_refptr<H264Picture> pic) {
  const H264SPS* sps = parser_->GetSPS(curr_sps_id_);
  if (!sps)
    return false;

  switch (pic->pic_order_cnt_type) {
    case 0: {
      // See spec 8.2.1.1.
      int prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;

      if (pic->idr) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        if (prev_ref_has_memmgmnt5_) {
          if (prev_ref_field_ != H264Picture::FIELD_BOTTOM) {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = prev_ref_top_field_order_cnt_;
          } else {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = 0;
          }
        } else {
          prev_pic_order_cnt_msb = prev_ref_pic_order_cnt_msb_;
          prev_pic_order_cnt_lsb = prev_ref_pic_order_cnt_lsb_;
        }
      }

      int max_pic_order_cnt_lsb =
          1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
      DCHECK_NE(max_pic_order_cnt_lsb, 0);
      if ((pic->pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - pic->pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb / 2)) {
        pic->pic_order_cnt_msb = prev_pic_order_cnt_msb + max_pic_order_cnt_lsb;
      } else if ((pic->pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
                 (pic->pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
                  max_pic_order_cnt_lsb / 2)) {
        pic->pic_order_cnt_msb = prev_pic_order_cnt_msb - max_pic_order_cnt_lsb;
      } else {
        pic->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      if (pic->field != H264Picture::FIELD_BOTTOM) {
        pic->top_field_order_cnt =
            pic->pic_order_cnt_msb + pic->pic_order_cnt_lsb;
      }

      if (pic->field != H264Picture::FIELD_TOP) {
        if (pic->field == H264Picture::FIELD_NONE) {
          pic->bottom_field_order_cnt =
              pic->top_field_order_cnt + pic->delta_pic_order_cnt_bottom;
        } else {
          pic->bottom_field_order_cnt =
              pic->pic_order_cnt_msb + pic->pic_order_cnt_lsb;
        }
      }
      break;
    }

    case 1: {
      // See spec 8.2.1.2.
      if (prev_has_memmgmnt5_) {
        prev_frame_num_offset_ = 0;
      }

      if (pic->idr) {
        pic->frame_num_offset = 0;
      } else if (prev_frame_num_ > pic->frame_num) {
        pic->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      } else {
        pic->frame_num_offset = prev_frame_num_offset_;
      }

      int abs_frame_num = 0;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0) {
        abs_frame_num = pic->frame_num_offset + pic->frame_num;
      } else {
        abs_frame_num = 0;
      }

      if (pic->nal_ref_idc == 0 && abs_frame_num > 0) {
        --abs_frame_num;
      }

      int expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          VLOG(1) << "Invalid num_ref_frames_in_pic_order_cnt_cycle "
                  << "in stream";
          return false;
        }

        int pic_order_cnt_cycle_cnt =
            (abs_frame_num - 1) / sps->num_ref_frames_in_pic_order_cnt_cycle;
        int frame_num_in_pic_order_cnt_cycle =
            (abs_frame_num - 1) % sps->num_ref_frames_in_pic_order_cnt_cycle;

        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
                                 sps->expected_delta_per_pic_order_cnt_cycle;
        // frame_num_in_pic_order_cnt_cycle is verified < 255 in parser
        for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i) {
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
        }
      }

      if (!pic->nal_ref_idc) {
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;
      }

      if (pic->field == H264Picture::FIELD_NONE) {
        pic->top_field_order_cnt =
            expected_pic_order_cnt + pic->delta_pic_order_cnt0;
        pic->bottom_field_order_cnt = pic->top_field_order_cnt +
                                      sps->offset_for_top_to_bottom_field +
                                      pic->delta_pic_order_cnt1;
      } else if (pic->field != H264Picture::FIELD_BOTTOM) {
        pic->top_field_order_cnt =
            expected_pic_order_cnt + pic->delta_pic_order_cnt0;
      } else {
        pic->bottom_field_order_cnt = expected_pic_order_cnt +
                                      sps->offset_for_top_to_bottom_field +
                                      pic->delta_pic_order_cnt0;
      }
      break;
    }

    case 2: {
      // See spec 8.2.1.3.
      if (prev_has_memmgmnt5_) {
        prev_frame_num_offset_ = 0;
      }

      if (pic->idr) {
        pic->frame_num_offset = 0;
      } else if (prev_frame_num_ > pic->frame_num) {
        pic->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      } else {
        pic->frame_num_offset = prev_frame_num_offset_;
      }

      int temp_pic_order_cnt;
      if (pic->idr) {
        temp_pic_order_cnt = 0;
      } else if (!pic->nal_ref_idc) {
        temp_pic_order_cnt = 2 * (pic->frame_num_offset + pic->frame_num) - 1;
      } else {
        temp_pic_order_cnt = 2 * (pic->frame_num_offset + pic->frame_num);
      }

      if (pic->field == H264Picture::FIELD_NONE) {
        pic->top_field_order_cnt = temp_pic_order_cnt;
        pic->bottom_field_order_cnt = temp_pic_order_cnt;
      } else if (pic->field == H264Picture::FIELD_BOTTOM) {
        pic->bottom_field_order_cnt = temp_pic_order_cnt;
      } else {
        pic->top_field_order_cnt = temp_pic_order_cnt;
      }
      break;
    }

    default:
      VLOG(1) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return false;
  }

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      pic->pic_order_cnt =
          std::min(pic->top_field_order_cnt, pic->bottom_field_order_cnt);
      break;
    case H264Picture::FIELD_TOP:
      pic->pic_order_cnt = pic->top_field_order_cnt;
      break;
    case H264Picture::FIELD_BOTTOM:
      pic->pic_order_cnt = pic->bottom_field_order_cnt;
      break;
  }

  return true;
}

void H264Decoder::UpdateMaxNumReorderFrames(const H264SPS* sps) {
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    max_num_reorder_frames_ =
        base::checked_cast<size_t>(sps->max_num_reorder_frames);
  } else if (sps->constraint_set3_flag) {
    // max_num_reorder_frames not present, infer from profile/constraints
    // (see VUI semantics in spec).
    switch (sps->profile_idc) {
      case 44:
      case 86:
      case 100:
      case 110:
      case 122:
      case 244:
        max_num_reorder_frames_ = 0;
        break;
      default:
        max_num_reorder_frames_ = dpb_.max_num_pics();
        break;
    }
  } else {
    max_num_reorder_frames_ = dpb_.max_num_pics();
  }
}

bool H264Decoder::ModifyReferencePicLists(const H264SliceHeader* slice_hdr,
                                          H264Picture::Vector* ref_pic_list0,
                                          H264Picture::Vector* ref_pic_list1) {
  ref_pic_list0->clear();
  ref_pic_list1->clear();

  // Fill reference picture lists for B and S/SP slices.
  if (slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) {
    *ref_pic_list0 = ref_pic_list_p0_;
    return ModifyReferencePicList(slice_hdr, 0, ref_pic_list0);
  } else if (slice_hdr->IsBSlice()) {
    *ref_pic_list0 = ref_pic_list_b0_;
    *ref_pic_list1 = ref_pic_list_b1_;
    return ModifyReferencePicList(slice_hdr, 0, ref_pic_list0) &&
           ModifyReferencePicList(slice_hdr, 1, ref_pic_list1);
  }

  return true;
}

bool H264Decoder::ModifyReferencePicList(const H264SliceHeader* slice_hdr,
                                         int list,
                                         H264Picture::Vector* ref_pic_listx) {
  bool ref_pic_list_modification_flag_lX;
  int num_ref_idx_lX_active_minus1;
  const H264ModificationOfPicNum* list_mod;

  // This can process either ref_pic_list0 or ref_pic_list1, depending on
  // the list argument. Set up pointers to proper list to be processed here.
  if (list == 0) {
    ref_pic_list_modification_flag_lX =
        slice_hdr->ref_pic_list_modification_flag_l0;
    num_ref_idx_lX_active_minus1 = slice_hdr->num_ref_idx_l0_active_minus1;
    list_mod = slice_hdr->ref_list_l0_modifications;
  } else {
    ref_pic_list_modification_flag_lX =
        slice_hdr->ref_pic_list_modification_flag_l1;
    num_ref_idx_lX_active_minus1 = slice_hdr->num_ref_idx_l1_active_minus1;
    list_mod = slice_hdr->ref_list_l1_modifications;
  }

  // Resize the list to the size requested in the slice header.
  // Note that per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to
  // indicate there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference and will be
  // initialized to nullptr, which must be handled by clients.
  DCHECK_GE(num_ref_idx_lX_active_minus1, 0);
  size_t original_size = ref_pic_listx->size();
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);
  for (int i = original_size; i < num_ref_idx_lX_active_minus1 + 1; i++) {
    scoped_refptr<H264Picture> nonref_pic =
        base::WrapRefCounted(new H264Picture(nullptr));
    LOG_ASSERT(InitNonexistingPicture(nonref_pic, 0, false));
    (*ref_pic_listx)[i] = nonref_pic;
  }

  if (!ref_pic_list_modification_flag_lX)
    return true;

  // Spec 8.2.4.3:
  // Reorder pictures on the list in a way specified in the stream.
  int pic_num_lx_pred = curr_picture_->pic_num;
  int ref_idx_lx = 0;
  int pic_num_lx_no_wrap;
  int pic_num_lx;
  bool done = false;
  scoped_refptr<H264Picture> pic;
  for (int i = 0; i < H264SliceHeader::kRefListModSize && !done; ++i) {
    switch (list_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        // Modify short reference picture position.
        if (list_mod->modification_of_pic_nums_idc == 0) {
          // Subtract given value from predicted PicNum.
          pic_num_lx_no_wrap =
              pic_num_lx_pred -
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes < 0 as result
          // of subtraction.
          if (pic_num_lx_no_wrap < 0)
            pic_num_lx_no_wrap += max_pic_num_;
        } else {
          // Add given value to predicted PicNum.
          pic_num_lx_no_wrap =
              pic_num_lx_pred +
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes >= max_pic_num_ as result
          // of the addition.
          if (pic_num_lx_no_wrap >= max_pic_num_)
            pic_num_lx_no_wrap -= max_pic_num_;
        }

        // For use in next iteration.
        pic_num_lx_pred = pic_num_lx_no_wrap;

        if (pic_num_lx_no_wrap > curr_picture_->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - max_pic_num_;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  H264SliceHeader::kRefListModSize);
        pic = dpb_.GetShortRefPicByPicNum(pic_num_lx);
        if (!pic) {
          VLOG(1) << "Malformed stream, no pic num " << pic_num_lx;
          return false;
        }

        if (ref_idx_lx > num_ref_idx_lX_active_minus1) {
          VLOG(1) << "Bounds mismatch: expected " << ref_idx_lx
                  << " <= " << num_ref_idx_lX_active_minus1;
          return false;
        }

        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          auto* src_pic = (*ref_pic_listx)[src].get();
          int src_pic_num_lx = src_pic ? PicNumF(*src_pic) : -1;
          if (src_pic_num_lx != pic_num_lx)
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 2:
        // Modify long term reference picture position.
        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  H264SliceHeader::kRefListModSize);
        pic = dpb_.GetLongRefPicByLongTermPicNum(list_mod->long_term_pic_num);
        if (!pic) {
          VLOG(1) << "Malformed stream, no pic num "
                  << list_mod->long_term_pic_num;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if ((*ref_pic_listx)[src] &&
              LongTermPicNumF(*(*ref_pic_listx)[src]) !=
                  static_cast<int>(list_mod->long_term_pic_num))
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 3:
        // End of modification list.
        done = true;
        break;

      default:
        // May be recoverable.
        VLOG(1) << "Invalid modification_of_pic_nums_idc="
                << list_mod->modification_of_pic_nums_idc << " in position "
                << i;
        break;
    }

    ++list_mod;
  }

  // Per NOTE 2 in 8.2.4.3.2, the ref_pic_listx size in the above loop is
  // temporarily made one element longer than the required final list.
  // Resize the list back to its required size.
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);

  return true;
}

// This method ensures that DPB does not overflow, either by removing
// reference pictures as specified in the stream, or using a sliding window
// procedure to remove the oldest one.
// It also performs marking and unmarking pictures as reference.
// See spac 8.2.5.1.
bool H264Decoder::ReferencePictureMarking(scoped_refptr<H264Picture> pic) {
  // If the current picture is an IDR, all reference pictures are unmarked.
  if (pic->idr) {
    dpb_.MarkAllUnusedForRef();

    if (pic->long_term_reference_flag) {
      pic->long_term = true;
      pic->long_term_frame_idx = 0;
      max_long_term_frame_idx_ = 0;
    } else {
      pic->long_term = false;
      max_long_term_frame_idx_ = -1;
    }

    return true;
  }

  // Not an IDR. If the stream contains instructions on how to discard pictures
  // from DPB and how to mark/unmark existing reference pictures, do so.
  // Otherwise, fall back to default sliding window process.
  if (pic->adaptive_ref_pic_marking_mode_flag) {
    DCHECK(!pic->nonexisting);
    return HandleMemoryManagementOps(pic);
  } else {
    return SlidingWindowPictureMarking();
  }
}

bool H264Decoder::HandleMemoryManagementOps(scoped_refptr<H264Picture> pic) {
  // 8.2.5.4
  for (size_t i = 0; i < std::size(pic->ref_pic_marking); ++i) {
    // Code below does not support interlaced stream (per-field pictures).
    H264DecRefPicMarking* ref_pic_marking = &pic->ref_pic_marking[i];
    scoped_refptr<H264Picture> to_mark;
    int pic_num_x;

    switch (ref_pic_marking->memory_mgmnt_control_operation) {
      case 0:
        // Normal end of operations' specification.
        return true;

      case 1:
        // Mark a short term reference picture as unused so it can be removed
        // if outputted.
        pic_num_x =
            pic->pic_num - (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          VLOG(1) << "Invalid short ref pic num to unmark";
          return false;
        }
        break;

      case 2:
        // Mark a long term reference picture as unused so it can be removed
        // if outputted.
        to_mark = dpb_.GetLongRefPicByLongTermPicNum(
            ref_pic_marking->long_term_pic_num);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          VLOG(1) << "Invalid long term ref pic num to unmark";
          return false;
        }
        break;

      case 3:
        // Mark a short term reference picture as long term reference.
        pic_num_x =
            pic->pic_num - (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          DCHECK(to_mark->ref && !to_mark->long_term);

          scoped_refptr<H264Picture> long_term_mark =
              dpb_.GetLongRefPicByLongTermIdx(
                  ref_pic_marking->long_term_frame_idx);
          if (long_term_mark) {
            long_term_mark->ref = false;
          }

          to_mark->long_term = true;
          to_mark->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        } else {
          VLOG(1) << "Invalid short term ref pic num to mark as long ref";
          return false;
        }
        break;

      case 4: {
        // Unmark all reference pictures with long_term_frame_idx over new max.
        max_long_term_frame_idx_ =
            ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        H264Picture::Vector long_terms;
        dpb_.GetLongTermRefPicsAppending(&long_terms);
        for (auto long_term_pic : long_terms) {
          DCHECK(long_term_pic->ref && long_term_pic->long_term);
          // Ok to cast, max_long_term_frame_idx is much smaller than 16bit.
          if (long_term_pic->long_term_frame_idx >
              static_cast<int>(max_long_term_frame_idx_))
            long_term_pic->ref = false;
        }
        break;
      }

      case 5:
        // Unmark all reference pictures.
        dpb_.MarkAllUnusedForRef();
        max_long_term_frame_idx_ = -1;
        pic->mem_mgmt_5 = true;
        break;

      case 6: {
        // Replace long term reference pictures with current picture.
        // First unmark if any existing with this long_term_frame_idx...
        H264Picture::Vector long_terms;
        dpb_.GetLongTermRefPicsAppending(&long_terms);
        for (auto long_term_pic : long_terms) {
          DCHECK(long_term_pic->ref && long_term_pic->long_term);
          // Ok to cast, long_term_frame_idx is much smaller than 16bit.
          if (long_term_pic->long_term_frame_idx ==
              static_cast<int>(ref_pic_marking->long_term_frame_idx))
            long_term_pic->ref = false;
        }

        // and mark the current one instead.
        pic->ref = true;
        pic->long_term = true;
        pic->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        break;
      }

      default:
        // Would indicate a bug in parser.
        NOTREACHED_IN_MIGRATION();
    }
  }

  return true;
}

bool H264Decoder::SlidingWindowPictureMarking() {
  const H264SPS* sps = parser_->GetSPS(curr_sps_id_);
  if (!sps)
    return false;

  // 8.2.5.3. Ensure the DPB doesn't overflow by discarding the oldest picture.
  int num_ref_pics = dpb_.CountRefPics();
  DCHECK_LE(num_ref_pics, std::max<int>(sps->max_num_ref_frames, 1));
  if (num_ref_pics == std::max<int>(sps->max_num_ref_frames, 1)) {
    // Max number of reference pics reached, need to remove one of the short
    // term ones. Find smallest frame_num_wrap short reference picture and mark
    // it as unused.
    scoped_refptr<H264Picture> to_unmark =
        dpb_.GetLowestFrameNumWrapShortRefPic();
    if (!to_unmark) {
      VLOG(1) << "Couldn't find a short ref picture to unmark";
      return false;
    }

    to_unmark->ref = false;
  }

  return true;
}

// See 8.2.4
int H264Decoder::PicNumF(const H264Picture& pic) const {
  if (!pic.long_term)
    return pic.pic_num;
  else
    return max_pic_num_;
}

// See 8.2.4
int H264Decoder::LongTermPicNumF(const H264Picture& pic) const {
  if (pic.ref && pic.long_term)
    return pic.long_term_pic_num;
  else
    return 2 * (max_long_term_frame_idx_ + 1);
}

// Shift elements on the |v| starting from |from| to |to|, inclusive,
// one position to the right and insert pic at |from|.
void H264Decoder::ShiftRightAndInsert(H264Picture::Vector* v,
                                      int from,
                                      int to,
                                      scoped_refptr<H264Picture> pic) {
  // Security checks, do not disable in Debug mode.
  CHECK(from <= to);
  CHECK(to <= std::numeric_limits<int>::max() - 2);
  // Additional checks. Debug mode ok.
  DCHECK(v);
  DCHECK(pic);
  DCHECK((to + 1 == static_cast<int>(v->size())) ||
         (to + 2 == static_cast<int>(v->size())));

  v->resize(to + 2);

  for (int i = to + 1; i > from; --i)
    (*v)[i] = (*v)[i - 1];

  (*v)[from] = std::move(pic);
}

uint32_t H264Decoder::H264LevelToMaxDpbMbs(uint8_t level) {
  switch (level) {
    case H264SPS::kLevelIDC1p0:
    case H264SPS::kLevelIDC1B:
      return 396;
    case H264SPS::kLevelIDC1p1:
      return 900;
    case H264SPS::kLevelIDC1p2:
    case H264SPS::kLevelIDC1p3:
    case H264SPS::kLevelIDC2p0:
      return 2376;
    case H264SPS::kLevelIDC2p1:
      return 4752;
    case H264SPS::kLevelIDC2p2:
    case H264SPS::kLevelIDC3p0:
      return 8100;
    case H264SPS::kLevelIDC3p1:
      return 18000;
    case H264SPS::kLevelIDC3p2:
      return 20480;
    case H264SPS::kLevelIDC4p0:
    case H264SPS::kLevelIDC4p1:
      return 32768;
    case H264SPS::kLevelIDC4p2:
      return 34816;
    case H264SPS::kLevelIDC5p0:
      return 110400;
    case H264SPS::kLevelIDC5p1:
    case H264SPS::kLevelIDC5p2:
      return 184320;
    case H264SPS::kLevelIDC6p1:
    case H264SPS::kLevelIDC6p2:
      return 696320;
    default:
      return 0;
  }
}

void H264Decoder::FlushDPB() {
  H264Picture::Vector not_outputted_vec;
  dpb_.GetNotOutputtedPicsAppending(&not_outputted_vec);
  std::sort(not_outputted_vec.begin(), not_outputted_vec.end(),
            POCDescCompare());
  while (!not_outputted_vec.empty()) {
    output_queue.push(not_outputted_vec.back());
    not_outputted_vec.back()->outputted = true;
    not_outputted_vec.pop_back();
  }
  dpb_.Clear();
}

}  // namespace media::vaapi_test
