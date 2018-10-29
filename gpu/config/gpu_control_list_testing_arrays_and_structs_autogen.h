// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_

#include "gpu/config/gpu_control_list_testing_data.h"

namespace gpu {
const int kFeatureListForGpuControlTestingEntry1[1] = {
    TEST_FEATURE_0,
};

const char* const kDisabledExtensionsForEntry1[2] = {
    "test_extension1", "test_extension2",
};

const uint32_t kCrBugsForGpuControlTestingEntry1[2] = {
    1024, 678,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry1[1] = {
    0x0640,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry1 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "1.6.18",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry2[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry3[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry4[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry5[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry6[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry6 = {
    nullptr,  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "2010.5.8",
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry7[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry7[2] = {
    0x1023, 0x0640,
};

const int kFeatureListForGpuControlTestingEntry8[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry9[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry9 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "3.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry10[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry10 = {
    GpuControlList::kGLTypeANGLE,  // gl_type
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical, "2.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry11[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry11 = {
    GpuControlList::kGLTypeGL,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.0",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry12[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry12 = {
    "NVIDIA", nullptr, nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry13[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry13 = {
    "X\\.Org.*", nullptr, nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry14[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry14 = {
    nullptr, ".*GeForce.*", nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry15[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry15 = {
    nullptr, "(?i).*software.*", nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry16[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry16 = {
    nullptr, nullptr, ".*GL_SUN_slice_accum", nullptr,
};

const int kFeatureListForGpuControlTestingEntry17[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry18[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry19[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry19 = {
    "NVIDIA.*",  // driver_vendor
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry20[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry20 = {
    nullptr,  // driver_vendor
    {GpuControlList::kEQ, GpuControlList::kVersionStyleLexical, "8.76",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry21[1] = {
    TEST_FEATURE_1,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry21 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.7",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry22[1] = {
    TEST_FEATURE_1,
};

const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry22Exception0 = {
        nullptr, ".*mesa.*", nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry23[1] = {
    TEST_FEATURE_1,
};

const GpuControlList::More kMoreForEntry23 = {
    GpuControlList::kGLTypeGL,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "3.5",
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry24[3] = {
    TEST_FEATURE_0, TEST_FEATURE_1, TEST_FEATURE_2,
};

const int kFeatureListForGpuControlTestingEntry25[2] = {
    TEST_FEATURE_1, TEST_FEATURE_2,
};

const int kFeatureListForGpuControlTestingEntry26[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry26[1] = {
    0x0640,
};

const int kFeatureListForGpuControlTestingEntry27[1] = {
    TEST_FEATURE_0,
};

const char* const kMachineModelNameForEntry27[4] = {
    "Nexus 4", "XT1032", "GT-.*", "SCH-.*",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry27 = {
    base::size(kMachineModelNameForEntry27),  // machine model name size
    kMachineModelNameForEntry27,              // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // machine model version
};

const int kFeatureListForGpuControlTestingEntry28[1] = {
    TEST_FEATURE_0,
};

const char* const kMachineModelNameForEntry28Exception0[1] = {
    "Nexus.*",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry28Exception0 = {
    base::size(
        kMachineModelNameForEntry28Exception0),  // machine model name size
    kMachineModelNameForEntry28Exception0,       // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // machine model version
};

const int kFeatureListForGpuControlTestingEntry29[1] = {
    TEST_FEATURE_0,
};

const char* const kMachineModelNameForEntry29[1] = {
    "MacBookPro",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry29 = {
    base::size(kMachineModelNameForEntry29),  // machine model name size
    kMachineModelNameForEntry29,              // machine model names
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical, "7.1",
     nullptr},  // machine model version
};

const int kFeatureListForGpuControlTestingEntry30[1] = {
    TEST_FEATURE_0,
};

const char* const kMachineModelNameForEntry30[1] = {
    "MacBookPro",
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry30 = {
    base::size(kMachineModelNameForEntry30),  // machine model name size
    kMachineModelNameForEntry30,              // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // machine model version
};

const GpuControlList::MachineModelInfo kMachineModelInfoForEntry30Exception0 = {
    0,        // machine model name size
    nullptr,  // machine model names
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical, "7.1",
     nullptr},  // machine model version
};

const int kFeatureListForGpuControlTestingEntry31[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry31[1] = {
    0x0166,
};

const int kFeatureListForGpuControlTestingEntry32[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry32[1] = {
    0x0640,
};

const int kFeatureListForGpuControlTestingEntry33[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry33[1] = {
    0x0166,
};

const int kFeatureListForGpuControlTestingEntry34[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry34[1] = {
    0x0166,
};

const int kFeatureListForGpuControlTestingEntry35[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry35[1] = {
    0x0166,
};

const int kFeatureListForGpuControlTestingEntry36[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry36[2] = {
    0x0166, 0x0168,
};

const int kFeatureListForGpuControlTestingEntry37[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry38[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry38[1] = {
    0x0640,
};

const int kFeatureListForGpuControlTestingEntry39[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry40[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry40 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.1",
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry41[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry42[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry43[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry44[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry45[1] = {
    TEST_FEATURE_0,
};

const uint32_t kDeviceIDsForGpuControlTestingEntry45Exception0[1] = {
    0x2a06,
};

const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry45Exception0 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "8.1",
         nullptr},  // driver_version
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         nullptr, nullptr},  // driver_date
};

const uint32_t kDeviceIDsForGpuControlTestingEntry45Exception1[1] = {
    0x2a02,
};

const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry45Exception1 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "9.1",
         nullptr},  // driver_version
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         nullptr, nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry46[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry47[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry48[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry48 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    true,       // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry49[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry50[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry50 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
     "8.17.12.6973", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry51[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry51 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "12",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const int kFeatureListForGpuControlTestingEntry52[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry52Exception0 = {
        nullptr, ".*mesa.*", nullptr, nullptr,
};

const int kFeatureListForGpuControlTestingEntry53[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry54[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry54 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "10.7",
     nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

const char* const kDisabledExtensionsForEntry55[2] = {
    "test_extension2", "test_extension1",
};

const char* const kDisabledExtensionsForEntry56[2] = {
    "test_extension3", "test_extension2",
};

const int kFeatureListForGpuControlTestingEntry57[1] = {
    TEST_FEATURE_1,
};

const GpuControlList::More kMoreForEntry57 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    false,      // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    0,          // test_group
};

const int kFeatureListForGpuControlTestingEntry58[1] = {
    TEST_FEATURE_0,
};

const int kFeatureListForGpuControlTestingEntry59[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::More kMoreForEntry59 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    1,          // test_group
};

const int kFeatureListForGpuControlTestingEntry60[1] = {
    TEST_FEATURE_1,
};

const GpuControlList::More kMoreForEntry60 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    true,       // direct_rendering
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // gpu_count
    2,          // test_group
};

const int kFeatureListForGpuControlTestingEntry61[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry61[2] = {
    GpuSeriesType::kIntelSkyLake, GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry62[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry62[1] = {
    GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry63[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry63[1] = {
    GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry64[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry64[1] = {
    GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry65[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry65[1] = {
    GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry66[1] = {
    TEST_FEATURE_0,
};

const GpuSeriesType kGpuSeriesForEntry66Exception0[1] = {
    GpuSeriesType::kIntelKabyLake,
};

const int kFeatureListForGpuControlTestingEntry67[1] = {
    TEST_FEATURE_0,
};

const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry67 = {
    nullptr,  // driver_vendor
    {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
     "8.15.10.2702", nullptr},  // driver_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical, nullptr,
     nullptr},  // driver_date
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
