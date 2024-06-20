// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/parsers/h266_parser.h"
#include "media/parsers/h266_poc.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H266POCTest : public testing::Test {
 public:
  H266POCTest() : sps_(), pps_(), vps_(), ph_(), slice_hdr_(), h266_poc_() {}

  H266POCTest(const H266POCTest&) = delete;
  H266POCTest& operator=(const H266POCTest&) = delete;

 protected:
  void ComputePOC() {
    poc_ = h266_poc_.ComputePicOrderCnt(&sps_, &pps_, &vps_, &ph_, slice_hdr_);
  }
  int32_t poc_;
  H266SPS sps_;
  H266PPS pps_;
  H266VPS vps_;
  H266PictureHeader ph_;
  H266SliceHeader slice_hdr_;
  H266POC h266_poc_;
};

TEST_F(H266POCTest, PicOrderCnt) {
  sps_.sps_log2_max_pic_order_cnt_lsb_minus4 = 7;
  vps_.vps_video_parameter_set_id = 0;
  vps_.vps_max_layers_minus1 = 0;
  vps_.vps_max_sublayers_minus1 = 0;
  vps_.vps_independent_layer_flag[0] = 1;
  vps_.vps_layer_id[0] = 0;
  vps_.general_layer_idx[0] = 0;
  vps_.vps_ols_ptl_idx[0] = 0;
  vps_.vps_ptl_max_tid[0] = 0;

  slice_hdr_.temporal_id = 0;
  slice_hdr_.nuh_layer_id = 0;

  // Initial I frame with POC 0.
  slice_hdr_.nal_unit_type = H266NALU::kIDRNoLeadingPicture;
  ph_.ph_pic_order_cnt_lsb = 0;
  ComputePOC();
  ASSERT_EQ(0, poc_);
  // P frame with POC lsb 4.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 4;
  ComputePOC();
  ASSERT_EQ(4, poc_);
  // B frame with POC lsb 2.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 2;
  ComputePOC();
  ASSERT_EQ(2, poc_);
  // B frame with POC lsb 1.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 1;
  ComputePOC();
  ASSERT_EQ(1, poc_);
  // B frame with POC lsb 3.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 3;
  ComputePOC();
  ASSERT_EQ(3, poc_);
  // P frame with POC lsb 8.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 8;
  ComputePOC();
  ASSERT_EQ(8, poc_);
  // B Ref frame with POC lsb 6.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 6;
  ComputePOC();
  ASSERT_EQ(6, poc_);
  // B frame with POC lsb 5.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 5;
  ComputePOC();
  ASSERT_EQ(5, poc_);
  // B frame with POC lsb 7.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 7;
  ComputePOC();
  ASSERT_EQ(7, poc_);
  // P frame with POC lsb 12.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 12;
  ComputePOC();
  ASSERT_EQ(12, poc_);
  // B frame with POC lsb 10.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 10;
  ComputePOC();
  ASSERT_EQ(10, poc_);
  // B frame with POC lsb 9.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 9;
  ComputePOC();
  ASSERT_EQ(9, poc_);
  // B frame with POC lsb 11.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 11;
  ComputePOC();
  ASSERT_EQ(11, poc_);
  // P frame with POC lsb 16.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 16;
  ComputePOC();
  ASSERT_EQ(16, poc_);
  // B frame with POC lsb 14.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 14;
  ComputePOC();
  ASSERT_EQ(14, poc_);
  // B frame with POC lsb 13.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 13;
  ComputePOC();
  ASSERT_EQ(13, poc_);
  // B frame with POC lsb 15.
  slice_hdr_.nal_unit_type = H266NALU::kSTSA;
  ph_.ph_pic_order_cnt_lsb = 15;
  ComputePOC();
  ASSERT_EQ(15, poc_);
}
TEST_F(H266POCTest, PicOrderCntInOrder) {
  sps_.sps_log2_max_pic_order_cnt_lsb_minus4 = 7;
  vps_.vps_video_parameter_set_id = 0;
  vps_.vps_max_layers_minus1 = 0;
  vps_.vps_max_sublayers_minus1 = 0;
  vps_.vps_independent_layer_flag[0] = 1;
  vps_.vps_layer_id[0] = 0;
  vps_.general_layer_idx[0] = 0;
  vps_.vps_ols_ptl_idx[0] = 0;
  vps_.vps_ptl_max_tid[0] = 0;

  slice_hdr_.temporal_id = 0;
  slice_hdr_.nuh_layer_id = 0;

  // Initial I frame with POC 0.
  slice_hdr_.nal_unit_type = H266NALU::kIDRWithRADL;
  ph_.ph_pic_order_cnt_lsb = 0;
  ComputePOC();
  ASSERT_EQ(0, poc_);
  // P frame with POC lsb 1.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 1;
  ComputePOC();
  ASSERT_EQ(1, poc_);
  // P frame with POC lsb 2.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 2;
  ComputePOC();
  ASSERT_EQ(2, poc_);
  // P frame with POC lsb 3.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 3;
  ComputePOC();
  ASSERT_EQ(3, poc_);
  // P frame with POC lsb 4.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 4;
  ComputePOC();
  ASSERT_EQ(4, poc_);
  // P frame with POC lsb 5.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 5;
  ComputePOC();
  ASSERT_EQ(5, poc_);
  // P frame with POC lsb 6.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 6;
  ComputePOC();
  ASSERT_EQ(6, poc_);
  // P frame with POC lsb 7.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 7;
  ComputePOC();
  ASSERT_EQ(7, poc_);
  // P frame with POC lsb 8.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 8;
  ComputePOC();
  ASSERT_EQ(8, poc_);
  // P frame with POC lsb 9.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 9;
  ComputePOC();
  ASSERT_EQ(9, poc_);
  // P frame with POC lsb 10.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 10;
  ComputePOC();
  ASSERT_EQ(10, poc_);
  // P frame with POC lsb 11.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 11;
  ComputePOC();
  ASSERT_EQ(11, poc_);
  // P frame with POC lsb 12.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 12;
  ComputePOC();
  ASSERT_EQ(12, poc_);
  // P frame with POC lsb 13.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 13;
  ComputePOC();
  ASSERT_EQ(13, poc_);
  // P frame with POC lsb 14.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 14;
  ComputePOC();
  ASSERT_EQ(14, poc_);
  // P frame with POC lsb 15.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 15;
  ComputePOC();
  ASSERT_EQ(15, poc_);
  // P frame with POC lsb 16.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 16;
  ComputePOC();
  ASSERT_EQ(16, poc_);
  // P frame with POC lsb 17.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 17;
  ComputePOC();
  ASSERT_EQ(17, poc_);
  // P frame with POC lsb 18.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 18;
  ComputePOC();
  ASSERT_EQ(18, poc_);
  // P frame with POC lsb 19.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 19;
  ComputePOC();
  ASSERT_EQ(19, poc_);
  // P frame with POC lsb 20.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 20;
  ComputePOC();
  ASSERT_EQ(20, poc_);
  // P frame with POC lsb 21.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 21;
  ComputePOC();
  ASSERT_EQ(21, poc_);
  // P frame with POC lsb 22.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 22;
  ComputePOC();
  ASSERT_EQ(22, poc_);
  // P frame with POC lsb 23.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 23;
  ComputePOC();
  ASSERT_EQ(23, poc_);
  // P frame with POC lsb 24.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 24;
  ComputePOC();
  ASSERT_EQ(24, poc_);
  // P frame with POC lsb 25.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 25;
  ComputePOC();
  ASSERT_EQ(25, poc_);
  // P frame with POC lsb 26.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 26;
  ComputePOC();
  ASSERT_EQ(26, poc_);
  // P frame with POC lsb 27.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 27;
  ComputePOC();
  ASSERT_EQ(27, poc_);
  // P frame with POC lsb 28.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 28;
  ComputePOC();
  ASSERT_EQ(28, poc_);
  // P frame with POC lsb 29.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 29;
  ComputePOC();
  ASSERT_EQ(29, poc_);
  // I frame with POC 0.
  slice_hdr_.nal_unit_type = H266NALU::kIDRWithRADL;
  ph_.ph_pic_order_cnt_lsb = 0;
  ComputePOC();
  ASSERT_EQ(0, poc_);
  // P frame with POC lsb 1.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 1;
  ComputePOC();
  ASSERT_EQ(1, poc_);
  // P frame with POC lsb 2.
  slice_hdr_.nal_unit_type = H266NALU::kTrail;
  ph_.ph_pic_order_cnt_lsb = 2;
  ComputePOC();
  ASSERT_EQ(2, poc_);
}

}  // namespace media
