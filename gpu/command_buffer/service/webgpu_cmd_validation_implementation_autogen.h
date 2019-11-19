// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_webgpu_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_

static const PowerPreference valid_power_preference_table[] = {
    PowerPreference::kHighPerformance,
    PowerPreference::kLowPower,
};

Validators::Validators()
    : power_preference(valid_power_preference_table,
                       base::size(valid_power_preference_table)) {}

#endif  // GPU_COMMAND_BUFFER_SERVICE_WEBGPU_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
