// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "media/base/media_util.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/gpu/windows/d3d11_video_device_format_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Combine;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Values;

namespace media {

class FormatSupportCheckerUnittest : public ::testing::Test {};

TEST_F(FormatSupportCheckerUnittest, NullDeviceDoesntCrash) {
  FormatSupportChecker checker(nullptr);
  // Init and Check should both fail, but not crash.
  EXPECT_FALSE(checker.Initialize());
  EXPECT_FALSE(checker.CheckOutputFormatSupport(DXGI_FORMAT_NV12));
}

TEST_F(FormatSupportCheckerUnittest, CheckInitializationCantCast) {
  auto device = MakeComPtr<NiceMock<D3D11DeviceMock>>();
  auto vdevice = MakeComPtr<NiceMock<D3D11VideoDeviceMock>>();
  auto enumerator = MakeComPtr<NiceMock<D3D11VideoProcessorEnumeratorMock>>();

  ON_CALL(*device.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
      .WillByDefault(SetComPointeeAndReturnOk<1>(vdevice.Get()));

  EXPECT_CALL(*vdevice.Get(), CreateVideoProcessorEnumerator(_, _))
      .WillOnce(SetComPointeeAndReturnOk<1>(enumerator.Get()));

  EXPECT_CALL(*device.Get(), CheckFormatSupport(_, _)).WillOnce(Return(S_OK));

  EXPECT_CALL(*enumerator.Get(), CheckVideoProcessorFormat(_, _))
      .WillOnce(Return(S_OK));

  FormatSupportChecker checker(device);
  EXPECT_TRUE(checker.Initialize());
}

TEST_F(FormatSupportCheckerUnittest, CheckFormatSupportWorks) {
  auto device = MakeComPtr<NiceMock<D3D11DeviceMock>>();
  auto vdevice = MakeComPtr<NiceMock<D3D11VideoDeviceMock>>();
  auto enumerator = MakeComPtr<NiceMock<D3D11VideoProcessorEnumeratorMock>>();

  ON_CALL(*device.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
      .WillByDefault(SetComPointeeAndReturnOk<1>(vdevice.Get()));

  EXPECT_CALL(*vdevice.Get(), CreateVideoProcessorEnumerator(_, _))
      .WillOnce(SetComPointeeAndReturnOk<1>(enumerator.Get()));

  EXPECT_CALL(*device.Get(), CheckFormatSupport(_, _)).WillOnce(Return(S_OK));

  EXPECT_CALL(*enumerator.Get(), CheckVideoProcessorFormat(_, _))
      .WillOnce(Return(S_OK));

  FormatSupportChecker checker(device);
  EXPECT_TRUE(checker.Initialize());

  UINT enumerator_outcome = D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT;
  UINT device_outcome = D3D11_FORMAT_SUPPORT_VIDEO_PROCESSOR_OUTPUT;
  EXPECT_CALL(*enumerator.Get(), CheckVideoProcessorFormat(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(enumerator_outcome), Return(S_OK)));
  EXPECT_CALL(*device.Get(), CheckFormatSupport(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(device_outcome), Return(S_OK)));

  EXPECT_TRUE(checker.CheckOutputFormatSupport(DXGI_FORMAT_NV12));
}

TEST_F(FormatSupportCheckerUnittest, CheckFormatSupportRequiresBoth) {
  auto device = MakeComPtr<NiceMock<D3D11DeviceMock>>();
  auto vdevice = MakeComPtr<NiceMock<D3D11VideoDeviceMock>>();
  auto enumerator = MakeComPtr<NiceMock<D3D11VideoProcessorEnumeratorMock>>();

  ON_CALL(*device.Get(), QueryInterface(IID_ID3D11VideoDevice, _))
      .WillByDefault(SetComPointeeAndReturnOk<1>(vdevice.Get()));

  EXPECT_CALL(*vdevice.Get(), CreateVideoProcessorEnumerator(_, _))
      .WillOnce(SetComPointeeAndReturnOk<1>(enumerator.Get()));

  EXPECT_CALL(*device.Get(), CheckFormatSupport(_, _)).WillOnce(Return(S_OK));

  EXPECT_CALL(*enumerator.Get(), CheckVideoProcessorFormat(_, _))
      .WillOnce(Return(S_OK));

  FormatSupportChecker checker(device);
  EXPECT_TRUE(checker.Initialize());

  UINT enumerator_outcome = D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT;
  UINT device_outcome = 0;
  EXPECT_CALL(*enumerator.Get(), CheckVideoProcessorFormat(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(enumerator_outcome), Return(S_OK)));
  EXPECT_CALL(*device.Get(), CheckFormatSupport(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(device_outcome), Return(S_OK)));

  EXPECT_FALSE(checker.CheckOutputFormatSupport(DXGI_FORMAT_NV12));
}

}  // namespace media
