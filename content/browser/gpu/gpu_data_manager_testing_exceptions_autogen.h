// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_EXCEPTIONS_AUTOGEN_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_EXCEPTIONS_AUTOGEN_H_

namespace gpu {
const GpuControlList::Conditions kExceptionsForEntry5[1] = {
    {
        GpuControlList::kOsAny,  // os_type
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // os_version
        0x00,                                                      // vendor_id
        0,                                                // Devices size
        nullptr,                                          // Devices
        GpuControlList::kMultiGpuCategoryNone,            // multi_gpu_category
        GpuControlList::kMultiGpuStyleNone,               // multi_gpu_style
        nullptr,                                          // driver info
        &kGLStringsForGpuManagerTestingEntry5Exception0,  // GL strings
        nullptr,                                          // machine model info
        0,        // intel_gpu_series size
        nullptr,  // intel_gpu_series
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},                            // intel_gpu_generation
        &kMoreForEntry5_572251052Exception0,  // more data
    },
};

}  // namespace gpu

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_TESTING_EXCEPTIONS_AUTOGEN_H_
