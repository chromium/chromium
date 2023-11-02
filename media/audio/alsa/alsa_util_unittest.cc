// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alsa/alsa_util.h"
#include "media/audio/alsa/mock_alsa_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace alsa_util {

namespace {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

}  // namespace

TEST(AlsaUtilTest, FreeHwParams) {
  InSequence seq;
  media::MockAlsaWrapper mock_alsa_wrapper;
  snd_pcm_hw_params_t* params_ptr = (snd_pcm_hw_params_t*)malloc(1);
  EXPECT_CALL(mock_alsa_wrapper, PcmOpen(_, _, _, _)).WillOnce(Return(0));
  EXPECT_CALL(mock_alsa_wrapper, PcmSetParams(_, _, _, _, _, _, _))
      .WillOnce(Return(-1));
  EXPECT_CALL(mock_alsa_wrapper, StrError(_)).WillOnce(Return("error"));
  EXPECT_CALL(mock_alsa_wrapper, PcmHwParamsMalloc(_))
      .WillOnce(Invoke([params_ptr](snd_pcm_hw_params_t** params) {
        *params = params_ptr;
        return 0;
      }));
  EXPECT_CALL(mock_alsa_wrapper, PcmHwParamsAny(_, _)).WillOnce(Return(-1));
  EXPECT_CALL(mock_alsa_wrapper, StrError(_)).WillOnce(Return("error"));
  EXPECT_CALL(mock_alsa_wrapper, PcmHwParamsFree(params_ptr));
  EXPECT_CALL(mock_alsa_wrapper, PcmName(_)).WillOnce(Return("default"));
  EXPECT_CALL(mock_alsa_wrapper, PcmClose(_)).WillOnce(Return(0));
  snd_pcm_t* handle = OpenCaptureDevice(&mock_alsa_wrapper, "default", 2, 48000,
                                        SND_PCM_FORMAT_S16, 40000, 10000);
  EXPECT_EQ(handle, nullptr);
  free(params_ptr);
}

}  // namespace alsa_util
