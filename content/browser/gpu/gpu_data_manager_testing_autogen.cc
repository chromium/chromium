// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "content/browser/gpu/gpu_data_manager_testing_autogen.h"

#include <iterator>

#include "content/browser/gpu/gpu_data_manager_testing_arrays_and_structs_autogen.h"
#include "content/browser/gpu/gpu_data_manager_testing_exceptions_autogen.h"

namespace gpu {

const std::array<GpuControlList::Entry, 6> kGpuDataManagerTestingEntries = {{
    {
        1,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlocklisting.0",
        base::span(kFeatureListForGpuManagerTestingEntry1),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry1_572251052,  // more data
        },
        base::span<const GpuControlList::Conditions>(),  // exceptions
    },
    {
        2,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlocklisting.1",
        base::span(kFeatureListForGpuManagerTestingEntry2),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForGpuManagerTestingEntry2,  // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry2_572251052,  // more data
        },
        base::span<const GpuControlList::Conditions>(),  // exceptions
    },
    {
        3,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlocklistingWebGL.0",
        base::span(kFeatureListForGpuManagerTestingEntry3),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry3_572251052,  // more data
        },
        base::span<const GpuControlList::Conditions>(),  // exceptions
    },
    {
        4,  // id
        "GpuDataManagerImplPrivateTest.GpuSideBlocklistingWebGL.1",
        base::span(kFeatureListForGpuManagerTestingEntry4),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            &kGLStringsForGpuManagerTestingEntry4,  // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry4_572251052,  // more data
        },
        base::span<const GpuControlList::Conditions>(),  // exceptions
    },
    {
        5,  // id
        "GpuDataManagerImplPrivateTest.GpuSideException",
        base::span(kFeatureListForGpuManagerTestingEntry5),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry5_572251052,  // more data
        },
        base::span(kExceptionsForEntry5),  // exceptions
    },
    {
        6,  // id
        "GpuDataManagerImplPrivateTest.BlocklistAllFeatures",
        base::span(kFeatureListForGpuManagerTestingEntry6),  // features
        base::span<const char* const>(),  // DisabledExtensions
        base::span<const char* const>(),  // DisabledWebGLExtensions
        base::span<const uint32_t>(),     // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                  // intel_gpu_generation
            &kMoreForEntry6_572251052,  // more data
        },
        base::span<const GpuControlList::Conditions>(),  // exceptions
    },
}};
}  // namespace gpu
