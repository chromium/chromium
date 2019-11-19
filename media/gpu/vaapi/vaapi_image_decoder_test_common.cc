// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder_test_common.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"

namespace media {

VaapiImageDecoderTestCommon::VaapiImageDecoderTestCommon(
    std::unique_ptr<VaapiImageDecoder> decoder)
    : decoder_(std::move(decoder)) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line && cmd_line->HasSwitch("test_data_path"))
    test_data_path_ = cmd_line->GetSwitchValueASCII("test_data_path");
}

VaapiImageDecoderTestCommon::~VaapiImageDecoderTestCommon() = default;

void VaapiImageDecoderTestCommon::SetUp() {
  ASSERT_TRUE(decoder_->Initialize(
      base::BindRepeating([]() { LOG(FATAL) << "Oh noes! Decoder failed"; })));
}

base::FilePath VaapiImageDecoderTestCommon::FindTestDataFilePath(
    const std::string& file_name) const {
  const base::FilePath file_path = base::FilePath(file_name);
  if (base::PathExists(file_path))
    return file_path;
  if (!test_data_path_.empty())
    return base::FilePath(test_data_path_).Append(file_path);
  return GetTestDataFilePath(file_name);
}

}  // namespace media
