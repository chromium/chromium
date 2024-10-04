// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry1 = {
    TEST_FEATURE_0,
};

static const std::array<const char* const, 2> kDisabledExtensionsForEntry1 = {
    "test_extension1",
    "test_extension2",
};

static const std::array<uint32_t, 2> kCrBugsForGpuControlTestingEntry1 = {
    1024,
    678,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry1 = {{
        {0x0640, 0x0},
    }};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry1 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "1.6.18",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry2 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry3 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry4 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry5 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry6 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 2>
    kDevicesForGpuControlTestingEntry6 = {{
        {0x1023, 0x0},
        {0x0640, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry7 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry8 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry8_1440601243 = {
    GpuControlList::kGLTypeGLES,  // gl_type
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "3.0", nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry9 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry9_1440601243 = {
    GpuControlList::kGLTypeANGLE,  // gl_type
    {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "2.0", nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry10 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry10_1440601243 = {
    GpuControlList::kGLTypeGL,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "4.0", nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry11 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry11 = {
    "NVIDIA",
    nullptr,
    nullptr,
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry12 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry12 = {
    "X\\.Org.*",
    nullptr,
    nullptr,
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry13 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry13 = {
    nullptr,
    ".*GeForce.*",
    nullptr,
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry14 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry14 = {
    nullptr,
    "(?i).*software.*",
    nullptr,
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry15 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry15 = {
    nullptr,
    nullptr,
    ".*GL_SUN_slice_accum",
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry16 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry17 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry18 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry18 =
    {
        "NVIDIA.*",  // driver_vendor
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry19 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry19 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kEQ, GpuControlList::kVersionStyleLexical,
         GpuControlList::kVersionSchemaCommon, "8.76",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry20 = {
    TEST_FEATURE_1,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry20 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "24.20.100.7000",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry21 = {
    TEST_FEATURE_1,
};

static const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry21Exception0 = {
        nullptr,
        ".*mesa.*",
        nullptr,
        nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry22 = {
    TEST_FEATURE_1,
};

static const GpuControlList::More kMoreForEntry22_1440601243 = {
    GpuControlList::kGLTypeGL,  // gl_type
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "3.5", nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 3> kFeatureListForGpuControlTestingEntry23 = {
    TEST_FEATURE_0,
    TEST_FEATURE_1,
    TEST_FEATURE_2,
};

static const std::array<int, 2> kFeatureListForGpuControlTestingEntry24 = {
    TEST_FEATURE_1,
    TEST_FEATURE_2,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry25 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry25 = {{
        {0x0640, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry26 = {
    TEST_FEATURE_0,
};

static const std::array<const char* const, 4> kMachineModelNameForEntry26 = {{
    "Nexus 4",
    "XT1032",
    "GT-.*",
    "SCH-.*",
}};

static const GpuControlList::MachineModelInfo kMachineModelInfoForEntry26 = {
    base::span(kMachineModelNameForEntry26),  // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // machine model version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry27 = {
    TEST_FEATURE_0,
};

static const std::array<const char* const, 1>
    kMachineModelNameForEntry27Exception0 = {{
        "Nexus.*",
    }};

static const GpuControlList::MachineModelInfo
    kMachineModelInfoForEntry27Exception0 = {
        base::span(
            kMachineModelNameForEntry27Exception0),  // machine model names
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // machine model version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry28 = {
    TEST_FEATURE_0,
};

static const std::array<const char* const, 1> kMachineModelNameForEntry28 = {{
    "MacBookPro",
}};

static const GpuControlList::MachineModelInfo kMachineModelInfoForEntry28 = {
    base::span(kMachineModelNameForEntry28),  // machine model names
    {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "7.1",
     nullptr},  // machine model version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry29 = {
    TEST_FEATURE_0,
};

static const std::array<const char* const, 1> kMachineModelNameForEntry29 = {{
    "MacBookPro",
}};

static const GpuControlList::MachineModelInfo kMachineModelInfoForEntry29 = {
    base::span(kMachineModelNameForEntry29),  // machine model names
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // machine model version
};

static const GpuControlList::MachineModelInfo
    kMachineModelInfoForEntry29Exception0 = {
        base::span<const char* const>(),  // machine model names
        {GpuControlList::kGT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "7.1",
         nullptr},  // machine model version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry30 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry30 = {{
        {0x0166, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry31 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry31 = {{
        {0x0640, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry32 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry32 = {{
        {0x0166, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry33 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry33 = {{
        {0x0166, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry34 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry34 = {{
        {0x0166, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry35 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 2>
    kDevicesForGpuControlTestingEntry35 = {{
        {0x0166, 0x0},
        {0x0168, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry36 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry37 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry37 = {{
        {0x0640, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry38 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry39 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry39_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "4.1",
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry40 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry41 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry42 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry43 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry44 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry44Exception0 = {{
        {0x2a06, 0x0},
    }};

static const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry44Exception0 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "8.1",
         nullptr},  // driver_version
};

static const std::array<GpuControlList::Device, 1>
    kDevicesForGpuControlTestingEntry44Exception1 = {{
        {0x2a02, 0x0},
    }};

static const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry44Exception1 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9.1",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry45 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry46 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry47 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry47_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    true,       // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry48 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry49 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry49 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "8.17.12.6973",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry50 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry50 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "12",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry51 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry51Exception0 = {
        nullptr,
        ".*mesa.*",
        nullptr,
        nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry52 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry53 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry53 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "10.7",
         nullptr},  // driver_version
};

static const std::array<const char* const, 2> kDisabledExtensionsForEntry54 = {
    "test_extension2",
    "test_extension1",
};

static const std::array<const char* const, 2> kDisabledExtensionsForEntry55 = {
    "test_extension3",
    "test_extension2",
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry56 = {
    TEST_FEATURE_1,
};

static const GpuControlList::More kMoreForEntry56_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, "2",
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    0,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry57 = {
    TEST_FEATURE_0,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry58 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry58_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    1,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry59 = {
    TEST_FEATURE_1,
};

static const GpuControlList::More kMoreForEntry59_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,  // hardware_overlay
    2,                          // test_group
    GpuControlList::kDontCare,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry60 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 2> kIntelGpuSeriesForEntry60 = {{
    IntelGpuSeriesType::kSkylake,
    IntelGpuSeriesType::kKabylake,
}};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry60_1440601243 = {
        base::span(kIntelGpuSeriesForEntry60),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry61 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 1> kIntelGpuSeriesForEntry61 = {{
    IntelGpuSeriesType::kKabylake,
}};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry61_1440601243 = {
        base::span(kIntelGpuSeriesForEntry61),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry62 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 1> kIntelGpuSeriesForEntry62 = {{
    IntelGpuSeriesType::kKabylake,
}};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry62_1440601243 = {
        base::span(kIntelGpuSeriesForEntry62),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry63 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 1> kIntelGpuSeriesForEntry63 = {{
    IntelGpuSeriesType::kKabylake,
}};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry63_1440601243 = {
        base::span(kIntelGpuSeriesForEntry63),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry64 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 1> kIntelGpuSeriesForEntry64 = {{
    IntelGpuSeriesType::kKabylake,
}};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry64_1440601243 = {
        base::span(kIntelGpuSeriesForEntry64),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry65 = {
    TEST_FEATURE_0,
};

static const std::array<IntelGpuSeriesType, 1>
    kIntelGpuSeriesForEntry65Exception0 = {{
        IntelGpuSeriesType::kKabylake,
    }};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry65_1440601243Exception0 = {
        base::span(
            kIntelGpuSeriesForEntry65Exception0),  // intel_gpu_series_list
        {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, nullptr,
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry66 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry66 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "8.15.10.2702",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry67 = {
    TEST_FEATURE_0,
};

static const GpuControlList::More kMoreForEntry67_1440601243 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kUnsupported,  // hardware_overlay
    0,                             // test_group
    GpuControlList::kDontCare,     // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry68 = {
    TEST_FEATURE_0,
};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry68_1440601243 = {
        base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9",
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry69 = {
    TEST_FEATURE_0,
};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry69_1440601243 = {
        base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9",
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry70 = {
    TEST_FEATURE_0,
};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry70_1440601243 = {
        base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9",
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry71 = {
    TEST_FEATURE_0,
};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry71_1440601243 = {
        base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9",
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry72 = {
    TEST_FEATURE_0,
};

static const GpuControlList::IntelConditions
    kIntelConditionsForEntry72_1440601243 = {
        base::span<const IntelGpuSeriesType>(),  // intel_gpu_series_list
        {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "9",
         nullptr},  // intel_gpu_generation
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry73 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry73Exception0 = {
        nullptr,
        "Mali.*",
        nullptr,
        nullptr,
};

static const GpuControlList::More kMoreForEntry73_1440601243Exception0 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,     // hardware_overlay
    0,                             // test_group
    GpuControlList::kUnsupported,  // subpixel_font_rendering
};

static const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry73Exception1 = {
        nullptr,
        "DontCare",
        nullptr,
        nullptr,
};

static const GpuControlList::GLStrings
    kGLStringsForGpuControlTestingEntry73Exception2 = {
        nullptr,
        "Supported",
        nullptr,
        nullptr,
};

static const GpuControlList::More kMoreForEntry73_1440601243Exception2 = {
    GpuControlList::kGLTypeNone,  // gl_type
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gl_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // pixel_shader_version
    false,      // in_process_gpu
    0,          // gl_reset_notification_strategy
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr,
     nullptr},  // direct_rendering_version
    {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
     GpuControlList::kVersionSchemaCommon, nullptr, nullptr},  // gpu_count
    GpuControlList::kDontCare,   // hardware_overlay
    0,                           // test_group
    GpuControlList::kSupported,  // subpixel_font_rendering
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry74 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry74 = {
    nullptr,
    "Mali.*",
    nullptr,
    nullptr,
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry75 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry75 =
    {
        "Intel.*",  // driver_vendor
        {GpuControlList::kBetween, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaIntelDriver, "24.20.100.6000",
         "26.20.100.7000"},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry76 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry76 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaIntelDriver, "24.20.100.7000",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry77 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 2>
    kDevicesForGpuControlTestingEntry77Exception0 = {{
        {0x15D8, 0x93},
        {0x15DD, 0x86},
    }};

static const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry77Exception0 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "26.20.15023.6032",
         nullptr},  // driver_version
};

static const std::array<GpuControlList::Device, 2>
    kDevicesForGpuControlTestingEntry77Exception1 = {{
        {0x15D8, 0xE1},
        {0x15D8, 0xE2},
    }};

static const GpuControlList::DriverInfo
    kDriverInfoForGpuControlTestingEntry77Exception1 = {
        nullptr,  // driver_vendor
        {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "26.20.12055.1000",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry78 = {
    TEST_FEATURE_0,
};

static const std::array<GpuControlList::Device, 2>
    kDevicesForGpuControlTestingEntry78 = {{
        {0x15D8, 0x0},
        {0x15DD, 0x0},
    }};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry79 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry79 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "24.21.13.9826",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry80 = {
    TEST_FEATURE_0,
};

static const GpuControlList::DriverInfo kDriverInfoForGpuControlTestingEntry80 =
    {
        nullptr,  // driver_vendor
        {GpuControlList::kLE, GpuControlList::kVersionStyleNumerical,
         GpuControlList::kVersionSchemaCommon, "24.21.13.9826",
         nullptr},  // driver_version
};

static const std::array<int, 1> kFeatureListForGpuControlTestingEntry81 = {
    TEST_FEATURE_0,
};

static const GpuControlList::GLStrings kGLStringsForGpuControlTestingEntry81 = {
    nullptr,
    "ANGLE \\(Samsung Xclipse 920\\) on Vulkan 1.1.179",
    nullptr,
    nullptr,
};

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_TESTING_ARRAYS_AND_STRUCTS_AUTOGEN_H_
