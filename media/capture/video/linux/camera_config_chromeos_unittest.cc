// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "media/capture/video/linux/camera_config_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const char kConfigFileContent[] =
    "camera0.lens_facing=1\ncamera0.sensor_orientation=90\ncamera0.module0.usb_"
    "vid_pid=04f2:b53a\n";
}

TEST(CameraConfigChromeOSTest, ParseSuccessfully) {
  base::FilePath conf_path;
  ASSERT_TRUE(base::CreateTemporaryFile(&conf_path));
  base::WriteFile(conf_path, kConfigFileContent, sizeof(kConfigFileContent));

  CameraConfigChromeOS camera_config(conf_path.value());
  EXPECT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT,
            camera_config.GetCameraFacing(std::string("/dev/video2"),
                                          std::string("04f2:b53a")));
  EXPECT_EQ(90, camera_config.GetOrientation(std::string("/dev/video2"),
                                             std::string("04f2:b53a")));
}

TEST(CameraConfigChromeOSTest, ConfigFileNotExist) {
  CameraConfigChromeOS camera_config(std::string("file_not_exist"));
  EXPECT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
            camera_config.GetCameraFacing(std::string("/dev/video2"),
                                          std::string("04f2:b53a")));
  EXPECT_EQ(0, camera_config.GetOrientation(std::string("/dev/video2"),
                                            std::string("04f2:b53a")));
}

}  // namespace media
