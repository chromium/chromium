// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h266_poc.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "media/parsers/h266_parser.h"

namespace media {

H266POC::H266POC() {
  Reset();
}

H266POC::~H266POC() = default;

H266RefEntry::H266RefEntry(int type, int poc, int layer)
    : entry_type(type), pic_order_cnt(poc), nuh_layer_id(layer) {}

void H266POC::Reset() {
  ref_pic_order_cnt_msb_ = 0;
  ref_pic_order_cnt_lsb_ = 0;
}

// 8.3.1 Decoding process for picture order count. At present multi-layer
// encoded stream is excluded from decoder support, so each AU is expected
// to only contain one PU, and decoder will pass the first slice for the
// POC calculation.
int32_t H266POC::ComputePicOrderCnt(const H266SPS* sps,
                                    const H266PPS* pps,
                                    const H266VPS* vps,
                                    const H266PictureHeader* ph,
                                    const H266SliceHeader& slice_hdr) {
  DCHECK(sps && pps && vps && ph);

  int32_t pic_order_cnt = 0;
  int32_t max_pic_order_cnt_lsb =
      1 << (sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
  int32_t pic_order_cnt_msb;
  bool clvss_pic_flag = false, irap_pic = false, gdr_pic = false;
  int curr_layer_idx = vps->GetGeneralLayerIdx(slice_hdr.nuh_layer_id);
  if (curr_layer_idx < 0) {
    DVLOG(1) << "Current slice's layer index is invalid.";
    return -1;
  }
  // If vps marks layer dependencies, and certain PU in current AU
  // with nuh_layer_id that GeneralLayerIdx[nuh_layer_id] is in the list
  // ReferenceLayerIdx[curr_layer_idx], PicOrderCntVal should be equal to that
  // PU's PicOrderCntVal, and ph_pic_order_cnt_lsb shall be the same for all
  // slices in current AU.
  // TODO(crbugs.com/1417910): Allow multi-layer streams.
  if (vps->vps_max_layers_minus1 > 0) {
    DVLOG(1) << "Multi-layer stream is not supported.";
    return -1;
  }
  irap_pic = slice_hdr.nal_unit_type >= H266NALU::kIDRWithRADL &&
             slice_hdr.nal_unit_type <= H266NALU::kCRA;
  gdr_pic = slice_hdr.nal_unit_type == H266NALU::kGDR;
  clvss_pic_flag =
      (irap_pic || gdr_pic) && slice_hdr.no_output_before_recovery_flag;
  int32_t prev_pic_order_cnt_lsb = ref_pic_order_cnt_lsb_,
          prev_pic_oder_cnt_msb = ref_pic_order_cnt_msb_;

  // Calculate POC for current picture.
  if (ph->ph_poc_msb_cycle_present_flag) {
    pic_order_cnt_msb = ph->ph_poc_msb_cycle_val * max_pic_order_cnt_lsb;
  } else {
    if (clvss_pic_flag) {
      pic_order_cnt_msb = 0;
    } else {
      if (ph->ph_pic_order_cnt_lsb < prev_pic_order_cnt_lsb &&
          prev_pic_order_cnt_lsb - ph->ph_pic_order_cnt_lsb >=
              max_pic_order_cnt_lsb / 2) {
        pic_order_cnt_msb = prev_pic_oder_cnt_msb + max_pic_order_cnt_lsb;
      } else if (ph->ph_pic_order_cnt_lsb > prev_pic_order_cnt_lsb &&
                 (ph->ph_pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
                  max_pic_order_cnt_lsb / 2)) {
        pic_order_cnt_msb = prev_pic_oder_cnt_msb - max_pic_order_cnt_lsb;
      } else {
        pic_order_cnt_msb = prev_pic_oder_cnt_msb;
      }
    }
  }
  // 8.3.1: Decoding process for picture order count.
  if (!slice_hdr.temporal_id && !ph->ph_non_ref_pic_flag &&
      (slice_hdr.nal_unit_type != H266NALU::kRADL &&
       slice_hdr.nal_unit_type != H266NALU::kRASL)) {
    ref_pic_order_cnt_lsb_ = ph->ph_pic_order_cnt_lsb;
    ref_pic_order_cnt_msb_ = pic_order_cnt_msb;
  }
  pic_order_cnt = pic_order_cnt_msb + ph->ph_pic_order_cnt_lsb;
  return pic_order_cnt;
}

// 8.3.2: Decoding process for reference picture lists construction.
void H266POC::ComputeRefPicPocList(
    const H266SPS* sps,
    const H266PPS* pps,
    const H266VPS* vps,
    const H266PictureHeader* ph,
    const H266SliceHeader& slice_hdr,
    int current_poc,
    std::vector<H266RefEntry>& ref_pic_poc_list0,
    std::vector<H266RefEntry>& ref_pic_poc_list1) {
  DCHECK(sps && pps && vps && ph);
  DCHECK(current_poc >= 0);

  ref_pic_poc_list0.clear();
  ref_pic_poc_list1.clear();

  const H266RefPicLists* ref_pic_lists = nullptr;
  if ((slice_hdr.nal_unit_type == H266NALU::kIDRNoLeadingPicture ||
       slice_hdr.nal_unit_type == H266NALU::kIDRWithRADL) &&
      !sps->sps_idr_rpl_present_flag) {
    return;
  }

  if (pps->pps_rpl_info_in_ph_flag) {
    ref_pic_lists = &ph->ref_pic_lists;
  } else {
    ref_pic_lists = &slice_hdr.ref_pic_lists;
  }

  for (int i = 0; i < 2; i++) {
    const H266RefPicListStruct* ref_pic_list_struct = nullptr;
    if (ref_pic_lists->rpl_sps_flag[i]) {
      ref_pic_list_struct =
          &sps->ref_pic_list_struct[i][ref_pic_lists->rpl_idx[i]];
    } else {
      ref_pic_list_struct = &ref_pic_lists->rpl_ref_lists[i];
    }

    int poc_base = current_poc;
    int ltrp_entry_handled = 0;
    for (int j = 0; j < ref_pic_list_struct->num_ref_entries; j++) {
      // STRP
      if (!ref_pic_list_struct->inter_layer_ref_pic_flag[j] &&
          ref_pic_list_struct->st_ref_pic_flag[j]) {
        poc_base += ref_pic_list_struct->delta_poc_val_st[j];
        if (i == 0) {
          ref_pic_poc_list0.emplace_back(H266RefEntry::kSTRP, poc_base,
                                         slice_hdr.nuh_layer_id);
        } else {
          ref_pic_poc_list1.emplace_back(H266RefEntry::kSTRP, poc_base,
                                         slice_hdr.nuh_layer_id);
        }
      } else if (!ref_pic_list_struct->inter_layer_ref_pic_flag[j] &&
                 !ref_pic_list_struct->st_ref_pic_flag[j]) {  // LTRP
        int poc_lsb_lt = ref_pic_list_struct->ltrp_in_header_flag
                             ? ref_pic_lists->poc_lsb_lt[i][ltrp_entry_handled]
                             : ref_pic_list_struct->rpls_poc_lsb_lt[j];
        int full_poc_lt =
            current_poc -
            ref_pic_lists
                    ->unpacked_delta_poc_msb_cycle_lt[i][ltrp_entry_handled] *
                sps->max_pic_order_cnt_lsb -
            (current_poc & (sps->max_pic_order_cnt_lsb - 1)) + poc_lsb_lt;
        int poc_entry_val =
            ref_pic_lists
                    ->delta_poc_msb_cycle_present_flag[i][ltrp_entry_handled]
                ? full_poc_lt
                : poc_lsb_lt;
        if (i == 0) {
          ref_pic_poc_list0.emplace_back(H266RefEntry::kLTRP, poc_entry_val,
                                         slice_hdr.nuh_layer_id);
        } else {
          ref_pic_poc_list1.emplace_back(H266RefEntry::kLTRP, poc_entry_val,
                                         slice_hdr.nuh_layer_id);
        }
        ltrp_entry_handled++;
      } else {  // ILRP
        // For inter-layer reference, just keep a record in reference picture
        // list. The location in DPB for the dependent layer picture needs to be
        // checked during runtime by decoder.
        if (i == 0) {
          ref_pic_poc_list0.emplace_back(H266RefEntry::kILRP, current_poc,
                                         ref_pic_list_struct->ilrp_idx[j]);
        } else {
          ref_pic_poc_list1.emplace_back(H266RefEntry::kILRP, current_poc,
                                         ref_pic_list_struct->ilrp_idx[j]);
        }
      }
    }
  }
}

}  // namespace media
