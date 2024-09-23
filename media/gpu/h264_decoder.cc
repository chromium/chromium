// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h264_decoder.h"

#include <limits>
#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "media/base/media_switches.h"
#include "media/parsers/h264_level_limits.h"

namespace media {
namespace {

bool ParseBitDepth(const H264SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.2.1.1
  if (sps.bit_depth_luma_minus8 != sps.bit_depth_chroma_minus8) {
    DVLOG(1) << "H264Decoder doesn't support different bit depths between luma"
             << "and chroma, bit_depth_luma_minus8="
             << sps.bit_depth_luma_minus8
             << ", bit_depth_chroma_minus8=" << sps.bit_depth_chroma_minus8;
    return false;
  }
  DCHECK_GE(sps.bit_depth_luma_minus8, 0);
  DCHECK_LE(sps.bit_depth_luma_minus8, 6);
  switch (sps.bit_depth_luma_minus8) {
    case 0:
      bit_depth = 8u;
      break;
    case 2:
      bit_depth = 10u;
      break;
    case 4:
      bit_depth = 12u;
      break;
    case 6:
      bit_depth = 14u;
      break;
    default:
      DVLOG(1) << "Invalid bit depth: "
               << base::checked_cast<int>(sps.bit_depth_luma_minus8 + 8);
      return false;
  }
  return true;
}

bool IsValidBitDepth(uint8_t bit_depth, VideoCodecProfile profile) {
  // Spec A.2.
  switch (profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
      return bit_depth == 8u;
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
      return bit_depth == 8u || bit_depth == 10u;
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 12u ||
             bit_depth == 14u;
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
      // Spec G.10.1.
      return bit_depth == 8u;
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      // Spec H.10.1.1 and H.10.1.2.
      return bit_depth == 8u;
    default:
      NOTREACHED();
  }
}
}  // namespace

H264Decoder::H264Accelerator::H264Accelerator() = default;

H264Decoder::H264Accelerator::~H264Accelerator() = default;

scoped_refptr<H264Picture>
H264Decoder::H264Accelerator::CreateH264PictureSecure(uint64_t secure_handle) {
  return nullptr;
}

void H264Decoder::H264Accelerator::ProcessSPS(
    const H264SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {}

void H264Decoder::H264Accelerator::ProcessPPS(
    const H264PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {}

H264Decoder::H264Accelerator::Status
H264Decoder::H264Accelerator::ParseEncryptedSliceHeader(
    const std::vector<base::span<const uint8_t>>& data,
    const std::vector<SubsampleEntry>& subsamples,
    uint64_t secure_handle,
    H264SliceHeader* slice_header_out) {
  return H264Decoder::H264Accelerator::Status::kNotSupported;
}

H264Decoder::H264Accelerator::Status H264Decoder::H264Accelerator::SetStream(
    base::span<const uint8_t> stream,
    const DecryptConfig* decrypt_config) {
  return H264Decoder::H264Accelerator::Status::kNotSupported;
}

bool H264Decoder::H264Accelerator::RequiresRefLists() {
  return false;
}

H264Decoder::H264Decoder(std::unique_ptr<H264Accelerator> accelerator,
                         VideoCodecProfile profile,
                         const VideoColorSpace& container_color_space)
    : state_(State::kNeedStreamMetadata),
      container_color_space_(container_color_space),
      max_frame_num_(0),
      max_pic_num_(0),
      max_long_term_frame_idx_(0),
      max_num_reorder_frames_(0),
      // TODO(hiroh): Set profile to UNKNOWN.
      profile_(profile),
      accelerator_(std::move(accelerator)),
      requires_ref_lists_(accelerator_->RequiresRefLists()) {
  DCHECK(accelerator_);
  Reset();
}

H264Decoder::~H264Decoder() = default;

void H264Decoder::Reset() {
  curr_pic_ = nullptr;
  curr_nalu_ = nullptr;
  curr_slice_hdr_ = nullptr;
  curr_sps_id_ = -1;
  curr_pps_id_ = -1;

  prev_frame_num_ = -1;
  prev_ref_frame_num_ = -1;
  prev_frame_num_offset_ = -1;
  prev_has_memmgmnt5_ = false;

  prev_ref_has_memmgmnt5_ = false;
  prev_ref_top_field_order_cnt_ = -1;
  prev_ref_pic_order_cnt_msb_ = -1;
  prev_ref_pic_order_cnt_lsb_ = -1;
  prev_ref_field_ = H264Picture::FIELD_NONE;

  ref_pic_list_p0_.clear();
  ref_pic_list_b0_.clear();
  ref_pic_list_b1_.clear();
  dpb_.Clear();
  parser_.Reset();
  accelerator_->Reset();
  last_output_poc_ = std::numeric_limits<int>::min();

  prior_cencv1_nalus_.clear();
  prior_cencv1_subsamples_.clear();

  recovery_frame_num_.reset();
  recovery_frame_cnt_.reset();

  secure_handle_ = 0;

  // If we are in kDecoding, we can resume without processing an SPS.
  // The state becomes kDecoding again, (1) at the first IDR slice or (2) at
  // the first slice after the recovery point SEI.
  if (state_ == State::kDecoding)
    state_ = State::kAfterReset;
}

void H264Decoder::PrepareRefPicLists() {
  ConstructReferencePicListsP();
  ConstructReferencePicListsB();
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

H264Decoder::H264Accelerator::Status H264Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());

  return accelerator_->SubmitDecode(curr_pic_);
}

bool H264Decoder::InitNonexistingPicture(scoped_refptr<H264Picture> pic,
                                         int frame_num) {
  pic->nonexisting = true;
  pic->nal_ref_idc = 1;
  pic->frame_num = pic->pic_num = frame_num;
  pic->adaptive_ref_pic_marking_mode_flag = false;
  pic->ref = true;
  pic->long_term_reference_flag = false;
  pic->field = H264Picture::FIELD_NONE;

  return CalculatePicOrderCounts(pic);
}

bool H264Decoder::InitCurrPicture(const H264SliceHeader* slice_hdr) {
  if (!FillH264PictureFromSliceHeader(parser_.GetSPS(curr_sps_id_), *slice_hdr,
                                      curr_pic_.get())) {
    return false;
  }

  if (!CalculatePicOrderCounts(curr_pic_))
    return false;

  curr_pic_->long_term_reference_flag = slice_hdr->long_term_reference_flag;
  curr_pic_->adaptive_ref_pic_marking_mode_flag =
      slice_hdr->adaptive_ref_pic_marking_mode_flag;

  // If the slice header indicates we will have to perform reference marking
  // process after this picture is decoded, store required data for that
  // purpose.
  if (slice_hdr->adaptive_ref_pic_marking_mode_flag) {
    static_assert(sizeof(curr_pic_->ref_pic_marking) ==
                      sizeof(slice_hdr->ref_pic_marking),
                  "Array sizes of ref pic marking do not match.");
    memcpy(curr_pic_->ref_pic_marking, slice_hdr->ref_pic_marking,
           sizeof(curr_pic_->ref_pic_marking));
  }

  curr_pic_->set_visible_rect(visible_rect_);
  curr_pic_->set_bitstream_id(stream_id_);

  return true;
}

bool H264Decoder::CalculatePicOrderCounts(scoped_refptr<H264Picture> pic) {
  const H264SPS* sps = parser_.GetSPS(curr_sps_id_);
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
          prev_pic_order_cnt_msb = 0;
          prev_pic_order_cnt_lsb = prev_ref_top_field_order_cnt_;
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

      pic->top_field_order_cnt =
          pic->pic_order_cnt_msb + pic->pic_order_cnt_lsb;

      pic->bottom_field_order_cnt =
          pic->top_field_order_cnt + pic->delta_pic_order_cnt_bottom;

      break;
    }

    case 1: {
      // See spec 8.2.1.2.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (pic->idr)
        pic->frame_num_offset = 0;
      else if (prev_frame_num_ > pic->frame_num)
        pic->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        pic->frame_num_offset = prev_frame_num_offset_;

      int abs_frame_num = 0;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = pic->frame_num_offset + pic->frame_num;
      else
        abs_frame_num = 0;

      if (pic->nal_ref_idc == 0 && abs_frame_num > 0)
        --abs_frame_num;

      base::CheckedNumeric<int> expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          DVLOG(1) << "Invalid num_ref_frames_in_pic_order_cnt_cycle.";
          return false;
        }

        int pic_order_cnt_cycle_cnt =
            (abs_frame_num - 1) / sps->num_ref_frames_in_pic_order_cnt_cycle;
        int frame_num_in_pic_order_cnt_cycle =
            (abs_frame_num - 1) % sps->num_ref_frames_in_pic_order_cnt_cycle;

        expected_pic_order_cnt =
            base::CheckedNumeric<int>(pic_order_cnt_cycle_cnt) *
            sps->expected_delta_per_pic_order_cnt_cycle;

        // frame_num_in_pic_order_cnt_cycle is verified < 255 in parser
        for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }

      if (!pic->nal_ref_idc)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      base::CheckedNumeric<int> top_field_order_cnt =
          expected_pic_order_cnt + pic->delta_pic_order_cnt0;
      base::CheckedNumeric<int> bottom_field_order_cnt =
          top_field_order_cnt + sps->offset_for_top_to_bottom_field +
          pic->delta_pic_order_cnt1;

      if (!top_field_order_cnt.IsValid()) {
        DVLOG(1) << "Invalid top_field_order_cnt.";
        return false;
      }

      if (!bottom_field_order_cnt.IsValid()) {
        DVLOG(1) << "Invalid bottom_field_order_cnt.";
        return false;
      }

      pic->top_field_order_cnt = top_field_order_cnt.ValueOrDie();
      pic->bottom_field_order_cnt = bottom_field_order_cnt.ValueOrDie();
      break;
    }

    case 2: {
      // See spec 8.2.1.3.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (pic->idr)
        pic->frame_num_offset = 0;
      else if (prev_frame_num_ > pic->frame_num)
        pic->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        pic->frame_num_offset = prev_frame_num_offset_;

      int temp_pic_order_cnt;
      if (pic->idr) {
        temp_pic_order_cnt = 0;
      } else if (!pic->nal_ref_idc) {
        temp_pic_order_cnt = 2 * (pic->frame_num_offset + pic->frame_num) - 1;
      } else {
        temp_pic_order_cnt = 2 * (pic->frame_num_offset + pic->frame_num);
      }

      pic->top_field_order_cnt = temp_pic_order_cnt;
      pic->bottom_field_order_cnt = temp_pic_order_cnt;

      break;
    }

    default:
      DVLOG(1) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return false;
  }

  pic->pic_order_cnt =
      std::min(pic->top_field_order_cnt, pic->bottom_field_order_cnt);

  return true;
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
                          curr_pic_.get(), POCAscCompare());

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
                          curr_pic_.get(), POCDescCompare());

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

// See 8.2.4
int H264Decoder::PicNumF(const H264Picture& pic) {
  if (!pic.long_term)
    return pic.pic_num;
  else
    return max_pic_num_;
}

// See 8.2.4
int H264Decoder::LongTermPicNumF(const H264Picture& pic) {
  if (pic.ref && pic.long_term)
    return pic.long_term_pic_num;
  else
    return 2 * (max_long_term_frame_idx_ + 1);
}

// Shift elements on the |v| starting from |from| to |to|, inclusive,
// one position to the right and insert pic at |from|.
static void ShiftRightAndInsert(H264Picture::Vector* v,
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
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);

  if (!ref_pic_list_modification_flag_lX)
    return true;

  // Spec 8.2.4.3:
  // Reorder pictures on the list in a way specified in the stream.
  int pic_num_lx_pred = curr_pic_->pic_num;
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

        if (pic_num_lx_no_wrap > curr_pic_->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - max_pic_num_;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  H264SliceHeader::kRefListModSize);
        pic = dpb_.GetShortRefPicByPicNum(pic_num_lx);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num " << pic_num_lx;
          return false;
        }

        if (ref_idx_lx > num_ref_idx_lX_active_minus1) {
          DVLOG(1) << "Bounds mismatch: expected " << ref_idx_lx
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
          DVLOG(1) << "Malformed stream, no pic num "
                   << list_mod->long_term_pic_num;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (LongTermPicNumF(*(*ref_pic_listx)[src]) !=
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
        DVLOG(1) << "Invalid modification_of_pic_nums_idc="
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

bool H264Decoder::OutputPic(scoped_refptr<H264Picture> pic) {
  DCHECK(!pic->outputted);
  pic->outputted = true;

  // Set the color space for the picture.
  pic->set_colorspace(picture_color_space_);

  if (pic->nonexisting) {
    DVLOG(4) << "Skipping output, non-existing frame_num: " << pic->frame_num;
    return true;
  }

  DVLOG_IF(1, pic->pic_order_cnt < last_output_poc_)
      << "Outputting out of order, likely a broken stream: " << last_output_poc_
      << " -> " << pic->pic_order_cnt;
  last_output_poc_ = pic->pic_order_cnt;

  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt;
  return accelerator_->OutputPicture(pic);
}

void H264Decoder::ClearDPB() {
  // Clear DPB contents, marking the pictures as unused first.
  dpb_.Clear();
  last_output_poc_ = std::numeric_limits<int>::min();
}

bool H264Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  if (FinishPrevFrameIfPresent() != H264Accelerator::Status::kOk)
    return false;
  H264Picture::Vector to_output;
  dpb_.GetNotOutputtedPicsAppending(&to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  for (auto& pic : to_output) {
    if (!OutputPic(pic))
      return false;
  }
  return true;
}

bool H264Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  if (!OutputAllRemainingPics())
    return false;

  ClearDPB();
  DVLOG(2) << "Decoder flush finished";
  return true;
}

H264Decoder::H264Accelerator::Status H264Decoder::StartNewFrame(
    const H264SliceHeader* slice_hdr) {
  // TODO posciak: add handling of max_num_ref_frames per spec.
  CHECK(curr_pic_.get());
  DCHECK(slice_hdr);

  curr_pps_id_ = slice_hdr->pic_parameter_set_id;
  const H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  if (!pps)
    return H264Accelerator::Status::kFail;

  curr_sps_id_ = pps->seq_parameter_set_id;
  const H264SPS* sps = parser_.GetSPS(curr_sps_id_);
  if (!sps)
    return H264Accelerator::Status::kFail;

  max_frame_num_ = 1 << (sps->log2_max_frame_num_minus4 + 4);
  int frame_num = slice_hdr->frame_num;
  if (slice_hdr->idr_pic_flag)
    prev_ref_frame_num_ = 0;

  // 7.4.3
  if (frame_num != prev_ref_frame_num_ &&
      frame_num != (prev_ref_frame_num_ + 1) % max_frame_num_) {
    if (!HandleFrameNumGap(frame_num))
      return H264Accelerator::Status::kFail;
  }

  if (!InitCurrPicture(slice_hdr))
    return H264Accelerator::Status::kFail;

  UpdatePicNums(frame_num);

  if (requires_ref_lists_) {
    PrepareRefPicLists();
  }

  return accelerator_->SubmitFrameMetadata(sps, pps, dpb_, ref_pic_list_p0_,
                                           ref_pic_list_b0_, ref_pic_list_b1_,
                                           curr_pic_.get());
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
          // |to_mark| may be null for a variety of reasons. For example, the
          // video frame it refers to may have been dropped by the network, or
          // the bitstream is non-conformant and the frame it refers to is
          // already marked as "unused for reference," etc. In any case, it
          // should be safe to ignore this case and continue processing further
          // memory management control operations since the frame won't be used
          // for reference after this in any case.
          //
          // In real life, this case was observed in https://crbug.com/1394965.
          DVLOG(1) << "Invalid short ref pic num to unmark";
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
          // TODO(crbug.com/40251206): consider doing the same for mmco 2 when
          // we can have testing for it, as how we handle missing |to_mark| for
          // mmco 1.
          DVLOG(1) << "Invalid long term ref pic num to unmark";
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
          DVLOG(1) << "Invalid short term ref pic num to mark as long ref";
          return false;
        }
        break;

      case 4: {
        // Unmark all reference pictures with long_term_frame_idx over new max.
        max_long_term_frame_idx_ =
            ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        H264Picture::Vector long_terms;
        dpb_.GetLongTermRefPicsAppending(&long_terms);
        for (size_t long_term = 0; long_term < long_terms.size(); ++long_term) {
          scoped_refptr<H264Picture>& long_term_pic = long_terms[long_term];
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
        for (size_t long_term = 0; long_term < long_terms.size(); ++long_term) {
          scoped_refptr<H264Picture>& long_term_pic = long_terms[long_term];
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

bool H264Decoder::SlidingWindowPictureMarking() {
  const H264SPS* sps = parser_.GetSPS(curr_sps_id_);
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
      DVLOG(1) << "Couldn't find a short ref picture to unmark";
      return false;
    }

    to_unmark->ref = false;
  }

  return true;
}

bool H264Decoder::FinishPicture(scoped_refptr<H264Picture> pic) {
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

  // Remove unused (for reference or later output) pictures from DPB, marking
  // them as such.
  dpb_.DeleteUnused();

  DVLOG(4) << "Finishing picture frame_num: " << pic->frame_num
           << ", entries in DPB: " << dpb_.size();
  if (recovery_frame_cnt_) {
    // This is the first picture after the recovery point SEI message. Validate
    // `recovery_frame_cnt_` now that we are certain to have max_frame_num_.
    if (*recovery_frame_cnt_ >= max_frame_num_) {
      DVLOG(1) << "Invalid recovery_frame_cnt=" << *recovery_frame_cnt_
               << " (must be less than or equal to max_frame_num-1="
               << (max_frame_num_ - 1) << ")";
      return false;
    }

    // Compute the frame_num of the first frame that should be output (D.2.8).
    recovery_frame_num_ =
        (*recovery_frame_cnt_ + pic->frame_num) % max_frame_num_;
    DVLOG(3) << "recovery_frame_num_=" << *recovery_frame_num_;
    recovery_frame_cnt_.reset();
  }

  // The ownership of pic will either be transferred to DPB - if the picture is
  // still needed (for output and/or reference) - or we will release it
  // immediately if we manage to output it here and won't have to store it for
  // future reference.

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
    DVLOG_IF(1, num_remaining <= max_num_reorder_frames_)
        << "Invalid stream: max_num_reorder_frames not preserved";

    if (!recovery_frame_num_ ||
        // If we are decoding ahead to reach a SEI recovery point, skip
        // outputting all pictures before it, to avoid outputting corrupted
        // frames.
        (*output_candidate)->frame_num == *recovery_frame_num_) {
      recovery_frame_num_ = std::nullopt;
      if (!OutputPic(*output_candidate))
        return false;
    }

    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      if (*output_candidate != pic)
        dpb_.Delete(*output_candidate);
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
      DVLOG(1) << "Could not free up space in DPB!";
      return false;
    }

    dpb_.StorePic(std::move(pic));
  }

  secure_handle_ = 0;

  return true;
}

bool H264Decoder::UpdateMaxNumReorderFrames(const H264SPS* sps) {
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    max_num_reorder_frames_ =
        base::checked_cast<size_t>(sps->max_num_reorder_frames);
    if (max_num_reorder_frames_ > dpb_.max_num_pics()) {
      DVLOG(1)
          << "max_num_reorder_frames present, but larger than MaxDpbFrames ("
          << max_num_reorder_frames_ << " > " << dpb_.max_num_pics() << ")";
      max_num_reorder_frames_ = 0;
      return false;
    }
    return true;
  }

  // max_num_reorder_frames not present, infer from profile/constraints
  // (see VUI semantics in spec).
  if (sps->constraint_set3_flag) {
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

  return true;
}

bool H264Decoder::ProcessSPS(int sps_id, bool* need_new_buffers) {
  DVLOG(4) << "Processing SPS id:" << sps_id;

  const H264SPS* sps = parser_.GetSPS(sps_id);
  if (!sps)
    return false;

  *need_new_buffers = false;

  if (sps->frame_mbs_only_flag == 0) {
    DVLOG(1) << "frame_mbs_only_flag != 1 not supported";
    return false;
  }

  gfx::Size new_pic_size = sps->GetCodedSize().value_or(gfx::Size());
  if (new_pic_size.IsEmpty()) {
    DVLOG(1) << "Invalid picture size";
    return false;
  }

  int width_mb = new_pic_size.width() / 16;
  int height_mb = new_pic_size.height() / 16;

  // Verify that the values are not too large before multiplying.
  if (std::numeric_limits<int>::max() / width_mb < height_mb) {
    DVLOG(1) << "Picture size is too big: " << new_pic_size.ToString();
    return false;
  }

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
  if (max_dpb_mbs == 0)
    return false;

  // MaxDpbFrames from level limits per spec.
  size_t max_dpb_frames = std::min(max_dpb_mbs / (width_mb * height_mb),
                                   static_cast<int>(H264DPB::kDPBMaxSize));
  DVLOG(1) << "MaxDpbFrames: " << max_dpb_frames
           << ", max_num_ref_frames: " << sps->max_num_ref_frames
           << ", max_dec_frame_buffering: " << sps->max_dec_frame_buffering;

  // Set DPB size to at least the level limit, or what the stream requires.
  size_t max_dpb_size =
      std::max(static_cast<int>(max_dpb_frames),
               std::max(sps->max_num_ref_frames, sps->max_dec_frame_buffering));
  // Some non-conforming streams specify more frames are needed than the current
  // level limit. Allow this, but only up to the maximum number of reference
  // frames allowed per spec.
  DVLOG_IF(1, max_dpb_size > max_dpb_frames)
      << "Invalid stream, DPB size > MaxDpbFrames";
  if (max_dpb_size == 0 || max_dpb_size > H264DPB::kDPBMaxSize) {
    DVLOG(1) << "Invalid DPB size: " << max_dpb_size;
    return false;
  }

  VideoChromaSampling new_chroma_sampling = sps->GetChromaSampling();
  if (new_chroma_sampling != chroma_sampling_) {
    chroma_sampling_ = new_chroma_sampling;
  }

  if (chroma_sampling_ != VideoChromaSampling::k420) {
    DVLOG(1) << "Only YUV 4:2:0 is supported";
    return false;
  }

  VideoCodecProfile new_profile =
      H264Parser::ProfileIDCToVideoCodecProfile(sps->profile_idc);
  if (new_profile == VIDEO_CODEC_PROFILE_UNKNOWN) {
    return false;
  }
  uint8_t new_bit_depth = 0;
  if (!ParseBitDepth(*sps, new_bit_depth)) {
    return false;
  }
  if (!IsValidBitDepth(new_bit_depth, new_profile)) {
    DVLOG(1) << "Invalid bit depth=" << base::strict_cast<int>(new_bit_depth)
             << ", profile=" << GetProfileName(new_profile);
    return false;
  }

  VideoColorSpace new_color_space;
  // For H264, prefer the frame color space over the config.
  if (sps && sps->GetColorSpace().IsSpecified()) {
    new_color_space = sps->GetColorSpace();
  } else if (container_color_space_.IsSpecified()) {
    new_color_space = container_color_space_;
  }

  if (new_color_space.matrix == VideoColorSpace::MatrixID::RGB) {
    // Some H.264 videos contain a VUI that specifies a color matrix of GBR,
    // when they are actually ordinary YUV. H264 only supports 4:2:0 subsampling
    // and BGR should only be used with 4:4:4, hence default to Rec709. See
    // crbug.com/341266991.
    CHECK_NE(chroma_sampling_, VideoChromaSampling::k444);
    new_color_space = VideoColorSpace::REC709();
  }

  bool is_color_space_change = false;
  if (base::FeatureList::IsEnabled(kAVDColorSpaceChanges)) {
    is_color_space_change = new_color_space.IsSpecified() &&
                            new_color_space != picture_color_space_;
  }

  if (pic_size_ != new_pic_size || dpb_.max_num_pics() != max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth ||
      is_color_space_change) {
    if (!Flush()) {
      return false;
    }
    DVLOG(1) << "Codec profile: " << GetProfileName(new_profile)
             << ", level: " << base::strict_cast<int>(level)
             << ", DPB size: " << max_dpb_size
             << ", Picture size: " << new_pic_size.ToString()
             << ", bit depth: " << base::strict_cast<int>(new_bit_depth)
             << ", color_space: " << new_color_space.ToString();
    *need_new_buffers = true;
    profile_ = new_profile;
    bit_depth_ = new_bit_depth;
    pic_size_ = new_pic_size;
    picture_color_space_ = new_color_space;
    dpb_.set_max_num_pics(max_dpb_size);
  }

  gfx::Rect new_visible_rect = sps->GetVisibleRect().value_or(gfx::Rect());
  if (visible_rect_ != new_visible_rect) {
    DVLOG(2) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }

  if (!UpdateMaxNumReorderFrames(sps))
    return false;
  DVLOG(1) << "max_num_reorder_frames: " << max_num_reorder_frames_;

  return true;
}

H264Decoder::H264Accelerator::Status H264Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_) {
    H264Accelerator::Status result = DecodePicture();
    if (result != H264Accelerator::Status::kOk)
      return result;

    scoped_refptr<H264Picture> pic = curr_pic_;
    curr_pic_ = nullptr;
    if (!FinishPicture(pic))
      return H264Accelerator::Status::kFail;
  }

  return H264Accelerator::Status::kOk;
}

bool H264Decoder::HandleFrameNumGap(int frame_num) {
  const H264SPS* sps = parser_.GetSPS(curr_sps_id_);
  if (!sps)
    return false;

  if (!sps->gaps_in_frame_num_value_allowed_flag) {
    DVLOG(1) << "Invalid frame_num: " << frame_num;
    // TODO(b:129119729, b:146914440): Youtube android app sometimes sends an
    // invalid frame number after a seek. The sequence goes like:
    // Seek, SPS, PPS, IDR-frame, non-IDR, ... non-IDR with invalid number.
    // The only way to work around this reliably is to ignore this error.
    // Video playback is not affected, no artefacts are visible.
    return true;
  }

  DVLOG(2) << "Handling frame_num gap: " << prev_ref_frame_num_ << "->"
           << frame_num;

  // 7.4.3/7-23
  int unused_short_term_frame_num = (prev_ref_frame_num_ + 1) % max_frame_num_;
  while (unused_short_term_frame_num != frame_num) {
    auto pic = base::MakeRefCounted<H264Picture>();
    if (!InitNonexistingPicture(pic, unused_short_term_frame_num))
      return false;

    UpdatePicNums(unused_short_term_frame_num);

    if (!FinishPicture(pic))
      return false;

    unused_short_term_frame_num++;
    unused_short_term_frame_num %= max_frame_num_;
  }

  return true;
}

H264Decoder::H264Accelerator::Status H264Decoder::ProcessEncryptedSliceHeader(
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK(curr_nalu_);
  DCHECK(curr_slice_hdr_);
  std::vector<base::span<const uint8_t>> spans(prior_cencv1_nalus_.begin(),
                                               prior_cencv1_nalus_.end());
  spans.emplace_back(curr_nalu_->data.get(),
                     base::checked_cast<size_t>(curr_nalu_->size));
  std::vector<SubsampleEntry> all_subsamples(prior_cencv1_subsamples_.begin(),
                                             prior_cencv1_subsamples_.end());
  all_subsamples.insert(all_subsamples.end(), subsamples.begin(),
                        subsamples.end());
  auto rv = accelerator_->ParseEncryptedSliceHeader(
      spans, all_subsamples, secure_handle_, curr_slice_hdr_.get());
  // Return now if this isn't fully processed and don't store the NALU info
  // since we will get called again in the kTryAgain case, and on an error we
  // want to exist.
  if (rv != H264Accelerator::Status::kOk)
    return rv;

  // Insert this encrypted slice data as well in case this is a multi-slice
  // picture.
  prior_cencv1_nalus_.emplace_back(
      curr_nalu_->data.get(), base::checked_cast<size_t>(curr_nalu_->size));
  prior_cencv1_subsamples_.insert(prior_cencv1_subsamples_.end(),
                                  subsamples.begin(), subsamples.end());
  return rv;
}

H264Decoder::H264Accelerator::Status H264Decoder::PreprocessCurrentSlice() {
  const H264SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  if (IsNewPrimaryCodedPicture(curr_pic_.get(), curr_pps_id_,
                               parser_.GetSPS(curr_sps_id_), *slice_hdr)) {
    // New picture, so first finish the previous one before processing it.
    H264Accelerator::Status result = FinishPrevFrameIfPresent();
    if (result != H264Accelerator::Status::kOk)
      return result;

    DCHECK(!curr_pic_);

    if (slice_hdr->first_mb_in_slice != 0) {
      DVLOG(1) << "ASO/invalid stream, first_mb_in_slice: "
               << slice_hdr->first_mb_in_slice;
      return H264Accelerator::Status::kFail;
    }

    // If the new picture is an IDR, flush DPB.
    if (slice_hdr->idr_pic_flag) {
      // Output all remaining pictures, unless we are explicitly instructed
      // not to do so.
      if (!slice_hdr->no_output_of_prior_pics_flag) {
        if (!Flush())
          return H264Accelerator::Status::kFail;
      }
      dpb_.Clear();
      last_output_poc_ = std::numeric_limits<int>::min();
    }
  }

  return H264Accelerator::Status::kOk;
}

H264Decoder::H264Accelerator::Status H264Decoder::ProcessCurrentSlice() {
  DCHECK(curr_pic_);

  const H264SliceHeader* slice_hdr = curr_slice_hdr_.get();
  DCHECK(slice_hdr);

  if (slice_hdr->field_pic_flag == 0)
    max_pic_num_ = max_frame_num_;
  else
    max_pic_num_ = 2 * max_frame_num_;

  H264Picture::Vector ref_pic_list0, ref_pic_list1;
  // If we are using full sample encryption then we do not have the information
  // we need to update the ref pic lists here, but that's OK because the
  // accelerator doesn't actually need to submit them in this case.
  if (!slice_hdr->full_sample_encryption && requires_ref_lists_ &&
      !ModifyReferencePicLists(slice_hdr, &ref_pic_list0, &ref_pic_list1)) {
    return H264Accelerator::Status::kFail;
  }

  const H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  if (!pps)
    return H264Accelerator::Status::kFail;

  return accelerator_->SubmitSlice(pps, slice_hdr, ref_pic_list0, ref_pic_list1,
                                   curr_pic_.get(), slice_hdr->nalu_data,
                                   slice_hdr->nalu_size,
                                   parser_.GetCurrentSubsamples());
}

#define SET_ERROR_AND_RETURN()         \
  do {                                 \
    DVLOG(1) << "Error during decode"; \
    state_ = State::kError;            \
    return H264Decoder::kDecodeError;  \
  } while (0)

#define CHECK_ACCELERATOR_RESULT(func)             \
  do {                                             \
    H264Accelerator::Status result = (func);       \
    switch (result) {                              \
      case H264Accelerator::Status::kOk:           \
        break;                                     \
      case H264Accelerator::Status::kTryAgain:     \
        DVLOG(1) << #func " needs to try again";   \
        return H264Decoder::kTryAgain;             \
      case H264Accelerator::Status::kFail:         \
      case H264Accelerator::Status::kNotSupported: \
        SET_ERROR_AND_RETURN();                    \
    }                                              \
  } while (0)

void H264Decoder::SetStream(int32_t id, const DecoderBuffer& decoder_buffer) {
  const uint8_t* ptr = decoder_buffer.data();
  const size_t size = decoder_buffer.size();
  const DecryptConfig* decrypt_config = decoder_buffer.decrypt_config();

  DCHECK(ptr);
  DCHECK(size);
  DVLOG(4) << "New input stream id: " << id << " at: " << (void*)ptr
           << " size: " << size;
  stream_id_ = id;
  current_stream_ = ptr;
  current_stream_size_ = size;
  current_stream_has_been_changed_ = true;
  prior_cencv1_nalus_.clear();
  prior_cencv1_subsamples_.clear();
  if (decrypt_config) {
    parser_.SetEncryptedStream(ptr, size, decrypt_config->subsamples());
    current_decrypt_config_ = decrypt_config->Clone();
  } else {
    parser_.SetStream(ptr, size);
    current_decrypt_config_ = nullptr;
  }
  if (decoder_buffer.has_side_data() &&
      decoder_buffer.side_data()->secure_handle) {
    secure_handle_ = decoder_buffer.side_data()->secure_handle;
  } else {
    secure_handle_ = 0;
  }
}

H264Decoder::DecodeResult H264Decoder::Decode() {
  if (state_ == State::kError) {
    DVLOG(1) << "Decoder in error state";
    return kDecodeError;
  }

  if (current_stream_has_been_changed_) {
    // Calling H264Accelerator::SetStream() here instead of when the stream is
    // originally set in case the accelerator needs to return kTryAgain.
    H264Accelerator::Status result = accelerator_->SetStream(
        base::span<const uint8_t>(current_stream_.get(), current_stream_size_),
        current_decrypt_config_.get());
    switch (result) {
      case H264Accelerator::Status::kOk:
      case H264Accelerator::Status::kNotSupported:
        // kNotSupported means the accelerator can't handle this stream,
        // so everything will be done through the parser.
        break;
      case H264Accelerator::Status::kTryAgain:
        DVLOG(1) << "SetStream() needs to try again";
        return H264Decoder::kTryAgain;
      case H264Accelerator::Status::kFail:
        SET_ERROR_AND_RETURN();
    }

    // Reset the flag so that this is only called again next time SetStream()
    // is called.
    current_stream_has_been_changed_ = false;
  }

  while (true) {
    H264Parser::Result par_res;

    if (!curr_nalu_) {
      curr_nalu_ = std::make_unique<H264NALU>();
      par_res = parser_.AdvanceToNextNALU(curr_nalu_.get());
      if (par_res == H264Parser::kEOStream) {
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        return kRanOutOfStreamData;
      } else if (par_res != H264Parser::kOk) {
        SET_ERROR_AND_RETURN();
      }

      DVLOG(4) << "New NALU: " << static_cast<int>(curr_nalu_->nal_unit_type);
    }

    switch (curr_nalu_->nal_unit_type) {
      case H264NALU::kNonIDRSlice:
        // We can't resume from a non-IDR slice unless recovery point SEI
        // process is going.
        if (state_ == State::kError ||
            (state_ == State::kAfterReset && !recovery_frame_cnt_))
          break;

        [[fallthrough]];
      case H264NALU::kIDRSlice: {
        // TODO(posciak): the IDR may require an SPS that we don't have
        // available. For now we'd fail if that happens, but ideally we'd like
        // to keep going until the next SPS in the stream.
        if (state_ == State::kNeedStreamMetadata) {
          // We need an SPS, skip this IDR and keep looking.
          break;
        }

        // If after reset or waiting for a key, we should be able to recover
        // from an IDR. |state_|, |curr_slice_hdr_|, and |curr_pic_| are used
        // to keep track of what has previously been attempted, so that after
        // a retryable result is returned, subsequent calls to Decode() retry
        // the call that failed previously. If it succeeds (it may not if no
        // additional key has been provided, for example), then the remaining
        // steps will be executed.
        if (!curr_slice_hdr_) {
          curr_slice_hdr_ = std::make_unique<H264SliceHeader>();
          state_ = State::kParseSliceHeader;
        }

        if (state_ == State::kParseSliceHeader) {
          // Check if the slice header is encrypted.
          bool parsed_header = false;
          if (current_decrypt_config_) {
            const std::vector<SubsampleEntry>& subsamples =
                parser_.GetCurrentSubsamples();
            // There is only a single clear byte for the NALU information for
            // full sample encryption, and the rest is encrypted.
            if (!subsamples.empty() && subsamples[0].clear_bytes == 1) {
              CHECK_ACCELERATOR_RESULT(ProcessEncryptedSliceHeader(subsamples));
              parsed_header = true;
              curr_slice_hdr_->pic_parameter_set_id = last_parsed_pps_id_;
            }
          }
          if (!parsed_header) {
            par_res =
                parser_.ParseSliceHeader(*curr_nalu_, curr_slice_hdr_.get());
            if (par_res != H264Parser::kOk)
              SET_ERROR_AND_RETURN();
          }
          state_ = State::kTryPreprocessCurrentSlice;
        }

        if (state_ == State::kTryPreprocessCurrentSlice) {
          CHECK_ACCELERATOR_RESULT(PreprocessCurrentSlice());
          state_ = State::kEnsurePicture;
        }

        if (state_ == State::kEnsurePicture) {
          if (curr_pic_) {
            // |curr_pic_| already exists, so skip to ProcessCurrentSlice().
            state_ = State::kTryCurrentSlice;
          } else {
            // New picture/finished previous one, try to start a new one
            // or tell the client we need more surfaces.
            if (secure_handle_) {
              curr_pic_ = accelerator_->CreateH264PictureSecure(secure_handle_);
            } else {
              curr_pic_ = accelerator_->CreateH264Picture();
            }
            if (!curr_pic_)
              return kRanOutOfSurfaces;
            if (current_decrypt_config_)
              curr_pic_->set_decrypt_config(current_decrypt_config_->Clone());
            if (hdr_metadata_.has_value())
              curr_pic_->set_hdr_metadata(hdr_metadata_);

            state_ = State::kTryNewFrame;
          }
        }

        if (state_ == State::kTryNewFrame) {
          CHECK_ACCELERATOR_RESULT(StartNewFrame(curr_slice_hdr_.get()));
          state_ = State::kTryCurrentSlice;
        }

        DCHECK_EQ(state_, State::kTryCurrentSlice);
        CHECK_ACCELERATOR_RESULT(ProcessCurrentSlice());
        curr_slice_hdr_.reset();
        state_ = State::kDecoding;
        break;
      }

      case H264NALU::kSPS: {
        int sps_id;

        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        bool need_new_buffers = false;
        if (!ProcessSPS(sps_id, &need_new_buffers)) {
          SET_ERROR_AND_RETURN();
        }
        accelerator_->ProcessSPS(
            parser_.GetSPS(sps_id),
            base::span<const uint8_t>(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size)));

        if (state_ == State::kNeedStreamMetadata)
          state_ = State::kAfterReset;

        if (need_new_buffers) {
          curr_pic_ = nullptr;
          curr_nalu_ = nullptr;
          ref_pic_list_p0_.clear();
          ref_pic_list_b0_.clear();
          ref_pic_list_b1_.clear();
        }
        // Prefer config changes over color space changes.
        if (need_new_buffers) {
          return kConfigChange;
        }
        break;
      }

      case H264NALU::kPPS: {
        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        par_res = parser_.ParsePPS(&last_parsed_pps_id_);
        if (par_res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();
        accelerator_->ProcessPPS(
            parser_.GetPPS(last_parsed_pps_id_),
            base::span<const uint8_t>(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size)));
        break;
      }

      case H264NALU::kAUD:
      case H264NALU::kEOSeq:
      case H264NALU::kEOStream:
        if (state_ != State::kDecoding)
          break;

        CHECK_ACCELERATOR_RESULT(FinishPrevFrameIfPresent());
        break;

      case H264NALU::kSEIMessage: {
        if (current_decrypt_config_) {
          // If there are encrypted SEI NALUs as part of CENCv1, then we also
          // need to save those so we can send them into the accelerator so it
          // can decrypt the sample properly (otherwise it would be starting
          // partway into a block).
          const std::vector<SubsampleEntry>& subsamples =
              parser_.GetCurrentSubsamples();
          if (!subsamples.empty()) {
            prior_cencv1_nalus_.emplace_back(
                curr_nalu_->data.get(),
                base::checked_cast<size_t>(curr_nalu_->size));
            DCHECK_EQ(1u, subsamples.size());
            prior_cencv1_subsamples_.push_back(subsamples[0]);
            // Since the SEI is encrypted, do not try to parse it below as it
            // may fail or yield incorrect results.
            DVLOG(3) << "Skipping parsing of encrypted SEI NALU";
            break;
          }
        }
        H264SEI sei;
        if (parser_.ParseSEI(&sei) != H264Parser::kOk)
          break;

        for (auto& sei_msg : sei.msgs) {
          switch (sei_msg.type) {
            case H264SEIMessage::kSEIRecoveryPoint:
              // If we are after reset, we can also resume from a SEI recovery
              // point (spec D.2.8) if one is present. However, if we are
              // already in the process of handling one, skip any subsequent
              // ones until we are done processing.
              if (state_ == State::kAfterReset && !recovery_frame_cnt_ &&
                  !recovery_frame_num_) {
                recovery_frame_cnt_ = sei_msg.recovery_point.recovery_frame_cnt;

                if (0 > *recovery_frame_cnt_) {
                  DVLOG(1) << "Invalid recovery_frame_cnt="
                           << *recovery_frame_cnt_
                           << " (it must not be less then 0)";
                  SET_ERROR_AND_RETURN();
                }
                DVLOG(3) << "Recovery point SEI is found, recovery_frame_cnt_="
                         << *recovery_frame_cnt_;
              }
              break;
            case H264SEIMessage::kSEIContentLightLevelInfo:
              // H264 HDR metadata may appears in the below places:
              // 1. Container.
              // 2. Bitstream.
              // 3. Both container and bitstream.
              // Thus we should also extract HDR metadata here in case we
              // miss the information.
              if (!hdr_metadata_.has_value()) {
                hdr_metadata_.emplace();
              }
              hdr_metadata_->cta_861_3 =
                  sei_msg.content_light_level_info.ToGfx();
              break;
            case H264SEIMessage::kSEIMasteringDisplayInfo:
              if (!hdr_metadata_.has_value()) {
                hdr_metadata_.emplace();
              }
              hdr_metadata_->smpte_st_2086 =
                  sei_msg.mastering_display_info.ToGfx();
              break;
            default:
              break;
          }
        }
        break;
      }

      default:
        DVLOG(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    DVLOG(4) << "NALU done";
    curr_nalu_.reset();
  }
}

gfx::Size H264Decoder::GetPicSize() const {
  return pic_size_;
}

gfx::Rect H264Decoder::GetVisibleRect() const {
  return visible_rect_;
}

VideoCodecProfile H264Decoder::GetProfile() const {
  return profile_;
}

uint8_t H264Decoder::GetBitDepth() const {
  return bit_depth_;
}

VideoChromaSampling H264Decoder::GetChromaSampling() const {
  return chroma_sampling_;
}

VideoColorSpace H264Decoder::GetVideoColorSpace() const {
  return picture_color_space_;
}

std::optional<gfx::HDRMetadata> H264Decoder::GetHDRMetadata() const {
  return hdr_metadata_;
}

size_t H264Decoder::GetRequiredNumOfPictures() const {
  constexpr size_t kPicsInPipeline = limits::kMaxVideoFrames + 1;
  return GetNumReferenceFrames() + kPicsInPipeline;
}

size_t H264Decoder::GetNumReferenceFrames() const {
  // Use the maximum number of pictures in the Decoded Picture Buffer.
  return dpb_.max_num_pics();
}

// static
bool H264Decoder::FillH264PictureFromSliceHeader(
    const H264SPS* sps,
    const H264SliceHeader& slice_hdr,
    H264Picture* pic) {
  DCHECK(pic);

  pic->idr = slice_hdr.idr_pic_flag;
  if (pic->idr)
    pic->idr_pic_id = slice_hdr.idr_pic_id;

  if (!slice_hdr.field_pic_flag) {
    pic->field = H264Picture::FIELD_NONE;
  } else {
    DVLOG(1) << "Interlaced video not supported.";
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

// static
bool H264Decoder::IsNewPrimaryCodedPicture(const H264Picture* curr_pic,
                                           int curr_pps_id,
                                           const H264SPS* sps,
                                           const H264SliceHeader& slice_hdr) {
  if (!curr_pic)
    return true;

  // 7.4.1.2.4, assumes non-interlaced.
  if (slice_hdr.frame_num != curr_pic->frame_num ||
      slice_hdr.pic_parameter_set_id != curr_pps_id ||
      slice_hdr.nal_ref_idc != curr_pic->nal_ref_idc ||
      slice_hdr.idr_pic_flag != curr_pic->idr ||
      (slice_hdr.idr_pic_flag &&
       (slice_hdr.idr_pic_id != curr_pic->idr_pic_id ||
        // If we have two consecutive IDR slices, and the second one has
        // first_mb_in_slice == 0, treat it as a new picture.
        // Per spec, idr_pic_id should not be equal in this case (and we should
        // have hit the condition above instead, see spec 7.4.3 on idr_pic_id),
        // but some encoders neglect changing idr_pic_id for two consecutive
        // IDRs. Work around this by checking if the next slice contains the
        // zeroth macroblock, i.e. data that belongs to the next picture.
        // Do not perform this check for CENCv1 encrypted content as the
        // first_mb_in_slice field is not correctly populated in that case.
        (slice_hdr.first_mb_in_slice == 0 &&
         !slice_hdr.full_sample_encryption))))
    return true;

  if (!sps)
    return false;

  if (sps->pic_order_cnt_type == curr_pic->pic_order_cnt_type) {
    if (curr_pic->pic_order_cnt_type == 0) {
      if (slice_hdr.pic_order_cnt_lsb != curr_pic->pic_order_cnt_lsb ||
          slice_hdr.delta_pic_order_cnt_bottom !=
              curr_pic->delta_pic_order_cnt_bottom)
        return true;
    } else if (curr_pic->pic_order_cnt_type == 1) {
      if (slice_hdr.delta_pic_order_cnt0 != curr_pic->delta_pic_order_cnt0 ||
          slice_hdr.delta_pic_order_cnt1 != curr_pic->delta_pic_order_cnt1)
        return true;
    }
  }

  return false;
}

}  // namespace media
