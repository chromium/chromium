// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_H266_POC_H_
#define MEDIA_PARSERS_H266_POC_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "media/base/media_export.h"

namespace media {

struct H266SPS;
struct H266PPS;
struct H266VPS;
struct H266PictureHeader;
struct H266SliceHeader;

struct MEDIA_EXPORT H266RefEntry {
  enum {
    kSTRP = 0,  // short-term reference
    kLTRP = 1,  // long-term reference
    kILRP = 2,  // inter-layer reference
  };

  H266RefEntry(int type, int poc, int layer);

  int entry_type;
  int pic_order_cnt;
  // For STRP/LTRP this should be the same as current picture;
  // for ILRP this will be the direct reference layer that the
  // reference picture belongs to.
  int nuh_layer_id;
};

class MEDIA_EXPORT H266POC {
 public:
  H266POC();

  H266POC(const H266POC&) = delete;
  H266POC& operator=(const H266POC&) = delete;

  ~H266POC();

  // Returns the picture order count for a slice.
  // It is required |ph| is a valid picture header structure
  // for current slice:
  // If shdr.sh_picture_header_in_slice_header_flag is set,
  // |ph| should come from |slice_hdr|'s picture_header member;
  // otherwise it will be from the parsing result of a PH_NUT.
  int32_t ComputePicOrderCnt(const H266SPS* sps,
                             const H266PPS* pps,
                             const H266VPS* vps,
                             const H266PictureHeader* ph,
                             const H266SliceHeader& slice_hdr);

  // Returns the lists of POC for the reference pictures, the
  // list for RPL 0 and RPL1. an entry in those lists with value -1 indicates
  // "no reference picture".
  // |slice_hdr| should be the output from a successful slice header parsing.
  // TODO(crbugs.com/1417910): support calculating the RefPicScale[i][j]
  // when RPR is enabled.
  static void ComputeRefPicPocList(
      const H266SPS* sps,
      const H266PPS* pps,
      const H266VPS* vps,
      const H266PictureHeader* ph,
      const H266SliceHeader& slice_hdr,
      int current_poc,
      std::vector<H266RefEntry>& ref_pic_poc_list0,
      std::vector<H266RefEntry>& ref_pic_poc_list1);

  // Reset computation state.
  void Reset();

 private:
  int32_t ref_pic_order_cnt_msb_;
  int32_t ref_pic_order_cnt_lsb_;
};
}  // namespace media
#endif  // MEDIA_PARSERS_H266_POC_H_
