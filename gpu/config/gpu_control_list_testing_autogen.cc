// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/gpu_control_list_testing_autogen.h"

#include <iterator>

#include "gpu/config/gpu_control_list_testing_arrays_and_structs_autogen.h"
#include "gpu/config/gpu_control_list_testing_exceptions_autogen.h"

namespace gpu {

const GpuControlList::Entry kGpuControlListTestingEntries[] = {
    {
        1,  // id
        "GpuControlListEntryTest.DetailedEntry",
        std::size(kFeatureListForGpuControlTestingEntry1),  // features size
        kFeatureListForGpuControlTestingEntry1,             // features
        std::size(kDisabledExtensionsForEntry1),  // DisabledExtensions size
        kDisabledExtensionsForEntry1,             // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        std::size(kCrBugsForGpuControlTestingEntry1),  // CrBugs size
        kCrBugsForGpuControlTestingEntry1,             // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "10.6.4",
             nullptr},                                      // os_version
            0x10de,                                         // vendor_id
            std::size(kDevicesForGpuControlTestingEntry1),  // Devices size
            kDevicesForGpuControlTestingEntry1,             // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry1,  // driver info
            nullptr,                                 // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry1_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        2,  // id
        "GpuControlListEntryTest.VendorOnAllOsEntry",
        std::size(kFeatureListForGpuControlTestingEntry2),  // features size
        kFeatureListForGpuControlTestingEntry2,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x10de,                                 // vendor_id
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry2_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        3,  // id
        "GpuControlListEntryTest.VendorOnLinuxEntry",
        std::size(kFeatureListForGpuControlTestingEntry3),  // features size
        kFeatureListForGpuControlTestingEntry3,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x10de,                                 // vendor_id
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry3_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        4,  // id
        "GpuControlListEntryTest.AllExceptNVidiaOnLinuxEntry",
        std::size(kFeatureListForGpuControlTestingEntry4),  // features size
        kFeatureListForGpuControlTestingEntry4,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry4_1440601243,  // more data
        },
        std::size(kExceptionsForEntry4),  // exceptions count
        kExceptionsForEntry4,             // exceptions
    },
    {
        5,  // id
        "GpuControlListEntryTest.AllExceptIntelOnLinuxEntry",
        std::size(kFeatureListForGpuControlTestingEntry5),  // features size
        kFeatureListForGpuControlTestingEntry5,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry5_1440601243,  // more data
        },
        std::size(kExceptionsForEntry5),  // exceptions count
        kExceptionsForEntry5,             // exceptions
    },
    {
        6,  // id
        "GpuControlListEntryTest.MultipleDevicesEntry",
        std::size(kFeatureListForGpuControlTestingEntry6),  // features size
        kFeatureListForGpuControlTestingEntry6,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                      // os_version
            0x10de,                                         // vendor_id
            std::size(kDevicesForGpuControlTestingEntry6),  // Devices size
            kDevicesForGpuControlTestingEntry6,             // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry6_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        7,  // id
        "GpuControlListEntryTest.ChromeOSEntry",
        std::size(kFeatureListForGpuControlTestingEntry7),  // features size
        kFeatureListForGpuControlTestingEntry7,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry7_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        8,  // id
        "GpuControlListEntryTest.GlVersionGLESEntry",
        std::size(kFeatureListForGpuControlTestingEntry8),  // features size
        kFeatureListForGpuControlTestingEntry8,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry8_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        9,  // id
        "GpuControlListEntryTest.GlVersionANGLEEntry",
        std::size(kFeatureListForGpuControlTestingEntry9),  // features size
        kFeatureListForGpuControlTestingEntry9,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                   // intel_gpu_generation
            &kMoreForEntry9_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        10,  // id
        "GpuControlListEntryTest.GlVersionGLEntry",
        std::size(kFeatureListForGpuControlTestingEntry10),  // features size
        kFeatureListForGpuControlTestingEntry10,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry10_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        11,  // id
        "GpuControlListEntryTest.GlVendorEqual",
        std::size(kFeatureListForGpuControlTestingEntry11),  // features size
        kFeatureListForGpuControlTestingEntry11,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry11,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry11_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        12,  // id
        "GpuControlListEntryTest.GlVendorWithDot",
        std::size(kFeatureListForGpuControlTestingEntry12),  // features size
        kFeatureListForGpuControlTestingEntry12,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry12,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry12_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        13,  // id
        "GpuControlListEntryTest.GlRendererContains",
        std::size(kFeatureListForGpuControlTestingEntry13),  // features size
        kFeatureListForGpuControlTestingEntry13,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry13,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry13_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        14,  // id
        "GpuControlListEntryTest.GlRendererCaseInsensitive",
        std::size(kFeatureListForGpuControlTestingEntry14),  // features size
        kFeatureListForGpuControlTestingEntry14,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry14,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry14_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        15,  // id
        "GpuControlListEntryTest.GlExtensionsEndWith",
        std::size(kFeatureListForGpuControlTestingEntry15),  // features size
        kFeatureListForGpuControlTestingEntry15,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry15,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry15_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        16,  // id
        "GpuControlListEntryTest.OptimusEntry",
        std::size(kFeatureListForGpuControlTestingEntry16),  // features size
        kFeatureListForGpuControlTestingEntry16,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleOptimus,  // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry16_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        17,  // id
        "GpuControlListEntryTest.AMDSwitchableEntry",
        std::size(kFeatureListForGpuControlTestingEntry17),  // features size
        kFeatureListForGpuControlTestingEntry17,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                    // os_version
            0x00,                                         // vendor_id
            0,                                            // Devices size
            nullptr,                                      // Devices
            GpuControlList::kMultiGpuCategoryNone,        // multi_gpu_category
            GpuControlList::kMultiGpuStyleAMDSwitchable,  // multi_gpu_style
            nullptr,                                      // driver info
            nullptr,                                      // GL strings
            nullptr,                                      // machine model info
            0,        // intel_gpu_series size
            nullptr,  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry17_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        18,  // id
        "GpuControlListEntryTest.DriverVendorBeginWith",
        std::size(kFeatureListForGpuControlTestingEntry18),  // features size
        kFeatureListForGpuControlTestingEntry18,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry18,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry18_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        19,  // id
        "GpuControlListEntryTest.LexicalDriverVersionEntry",
        std::size(kFeatureListForGpuControlTestingEntry19),  // features size
        kFeatureListForGpuControlTestingEntry19,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x1002,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry19,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry19_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        20,  // id
        "GpuControlListEntryTest.NeedsMoreInfoEntry",
        std::size(kFeatureListForGpuControlTestingEntry20),  // features size
        kFeatureListForGpuControlTestingEntry20,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry20,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry20_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        21,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForExceptionsEntry",
        std::size(kFeatureListForGpuControlTestingEntry21),  // features size
        kFeatureListForGpuControlTestingEntry21,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry21_1440601243,  // more data
        },
        std::size(kExceptionsForEntry21),  // exceptions count
        kExceptionsForEntry21,             // exceptions
    },
    {
        22,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForGlVersionEntry",
        std::size(kFeatureListForGpuControlTestingEntry22),  // features size
        kFeatureListForGpuControlTestingEntry22,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry22_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        23,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntry",
        std::size(kFeatureListForGpuControlTestingEntry23),  // features size
        kFeatureListForGpuControlTestingEntry23,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry23_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        24,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntryWithExceptions",
        std::size(kFeatureListForGpuControlTestingEntry24),  // features size
        kFeatureListForGpuControlTestingEntry24,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry24_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        25,  // id
        "GpuControlListEntryTest.SingleActiveGPU",
        std::size(kFeatureListForGpuControlTestingEntry25),  // features size
        kFeatureListForGpuControlTestingEntry25,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x10de,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry25),  // Devices size
            kDevicesForGpuControlTestingEntry25,             // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry25_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        26,  // id
        "GpuControlListEntryTest.MachineModelName",
        std::size(kFeatureListForGpuControlTestingEntry26),  // features size
        kFeatureListForGpuControlTestingEntry26,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
            &kMachineModelInfoForEntry26,           // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry26_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        27,  // id
        "GpuControlListEntryTest.MachineModelNameException",
        std::size(kFeatureListForGpuControlTestingEntry27),  // features size
        kFeatureListForGpuControlTestingEntry27,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry27_1440601243,  // more data
        },
        std::size(kExceptionsForEntry27),  // exceptions count
        kExceptionsForEntry27,             // exceptions
    },
    {
        28,  // id
        "GpuControlListEntryTest.MachineModelVersion",
        std::size(kFeatureListForGpuControlTestingEntry28),  // features size
        kFeatureListForGpuControlTestingEntry28,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
            &kMachineModelInfoForEntry28,           // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry28_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        29,  // id
        "GpuControlListEntryTest.MachineModelVersionException",
        std::size(kFeatureListForGpuControlTestingEntry29),  // features size
        kFeatureListForGpuControlTestingEntry29,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
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
            &kMachineModelInfoForEntry29,           // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry29_1440601243,  // more data
        },
        std::size(kExceptionsForEntry29),  // exceptions count
        kExceptionsForEntry29,             // exceptions
    },
    {
        30,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.Intel",
        std::size(kFeatureListForGpuControlTestingEntry30),  // features size
        kFeatureListForGpuControlTestingEntry30,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x8086,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry30),  // Devices size
            kDevicesForGpuControlTestingEntry30,             // Devices
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            0,                                     // intel_gpu_series size
            nullptr,                               // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry30_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        31,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.NVidia",
        std::size(kFeatureListForGpuControlTestingEntry31),  // features size
        kFeatureListForGpuControlTestingEntry31,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x10de,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry31),  // Devices size
            kDevicesForGpuControlTestingEntry31,             // Devices
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            0,                                     // intel_gpu_series size
            nullptr,                               // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry31_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        32,  // id
        "GpuControlListEntryDualGPUTest.CategorySecondary",
        std::size(kFeatureListForGpuControlTestingEntry32),  // features size
        kFeatureListForGpuControlTestingEntry32,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x8086,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry32),  // Devices size
            kDevicesForGpuControlTestingEntry32,             // Devices
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,        // intel_gpu_series size
            nullptr,  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry32_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        33,  // id
        "GpuControlListEntryDualGPUTest.CategoryPrimary",
        std::size(kFeatureListForGpuControlTestingEntry33),  // features size
        kFeatureListForGpuControlTestingEntry33,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x8086,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry33),  // Devices size
            kDevicesForGpuControlTestingEntry33,             // Devices
            GpuControlList::kMultiGpuCategoryPrimary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,        // multi_gpu_style
            nullptr,                                   // driver info
            nullptr,                                   // GL strings
            nullptr,                                   // machine model info
            0,                                         // intel_gpu_series size
            nullptr,                                   // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry33_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        34,  // id
        "GpuControlListEntryDualGPUTest.CategoryDefault",
        std::size(kFeatureListForGpuControlTestingEntry34),  // features size
        kFeatureListForGpuControlTestingEntry34,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x8086,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry34),  // Devices size
            kDevicesForGpuControlTestingEntry34,             // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry34_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        35,  // id
        "GpuControlListEntryDualGPUTest.ActiveSecondaryGPU",
        std::size(kFeatureListForGpuControlTestingEntry35),  // features size
        kFeatureListForGpuControlTestingEntry35,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x8086,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry35),  // Devices size
            kDevicesForGpuControlTestingEntry35,             // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry35_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        36,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActiveSecondaryGPU",
        std::size(kFeatureListForGpuControlTestingEntry36),  // features size
        kFeatureListForGpuControlTestingEntry36,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry36_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        37,  // id
        "GpuControlListEntryDualGPUTest.ActivePrimaryGPU",
        std::size(kFeatureListForGpuControlTestingEntry37),  // features size
        kFeatureListForGpuControlTestingEntry37,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x10de,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry37),  // Devices size
            kDevicesForGpuControlTestingEntry37,             // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry37_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        38,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActivePrimaryGPU",
        std::size(kFeatureListForGpuControlTestingEntry38),  // features size
        kFeatureListForGpuControlTestingEntry38,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry38_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        39,  // id
        "GpuControlListEntryTest.PixelShaderVersion",
        std::size(kFeatureListForGpuControlTestingEntry39),  // features size
        kFeatureListForGpuControlTestingEntry39,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry39_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        40,  // id
        "GpuControlListEntryTest.OsVersionZeroLT",
        std::size(kFeatureListForGpuControlTestingEntry40),  // features size
        kFeatureListForGpuControlTestingEntry40,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "4.2",
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry40_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        41,  // id
        "GpuControlListEntryTest.OsVersionZeroAny",
        std::size(kFeatureListForGpuControlTestingEntry41),  // features size
        kFeatureListForGpuControlTestingEntry41,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry41_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        42,  // id
        "GpuControlListEntryTest.OsComparisonAny",
        std::size(kFeatureListForGpuControlTestingEntry42),  // features size
        kFeatureListForGpuControlTestingEntry42,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry42_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        43,  // id
        "GpuControlListEntryTest.OsComparisonGE",
        std::size(kFeatureListForGpuControlTestingEntry43),  // features size
        kFeatureListForGpuControlTestingEntry43,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "6", nullptr},  // os_version
            0x00,                                                  // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry43_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        44,  // id
        "GpuControlListEntryTest.ExceptionWithoutVendorId",
        std::size(kFeatureListForGpuControlTestingEntry44),  // features size
        kFeatureListForGpuControlTestingEntry44,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry44_1440601243,  // more data
        },
        std::size(kExceptionsForEntry44),  // exceptions count
        kExceptionsForEntry44,             // exceptions
    },
    {
        45,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableDiscrete",
        std::size(kFeatureListForGpuControlTestingEntry45),  // features size
        kFeatureListForGpuControlTestingEntry45,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableDiscrete,  // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry45_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        46,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableIntegrated",
        std::size(kFeatureListForGpuControlTestingEntry46),  // features size
        kFeatureListForGpuControlTestingEntry46,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // Devices size
            nullptr,                                // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableIntegrated,  // multi_gpu_style
            nullptr,                                    // driver info
            nullptr,                                    // GL strings
            nullptr,                                    // machine model info
            0,                                          // intel_gpu_series size
            nullptr,                                    // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry46_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        47,  // id
        "GpuControlListEntryTest.InProcessGPU",
        std::size(kFeatureListForGpuControlTestingEntry47),  // features size
        kFeatureListForGpuControlTestingEntry47,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry47_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        48,  // id
        "GpuControlListEntryTest.SameGPUTwiceTest",
        std::size(kFeatureListForGpuControlTestingEntry48),  // features size
        kFeatureListForGpuControlTestingEntry48,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry48_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        49,  // id
        "GpuControlListEntryTest.NVidiaNumberingScheme",
        std::size(kFeatureListForGpuControlTestingEntry49),  // features size
        kFeatureListForGpuControlTestingEntry49,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry49,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry49_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        50,  // id
        "GpuControlListTest.NeedsMoreInfo",
        std::size(kFeatureListForGpuControlTestingEntry50),  // features size
        kFeatureListForGpuControlTestingEntry50,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry50,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry50_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        51,  // id
        "GpuControlListTest.NeedsMoreInfoForExceptions",
        std::size(kFeatureListForGpuControlTestingEntry51),  // features size
        kFeatureListForGpuControlTestingEntry51,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry51_1440601243,  // more data
        },
        std::size(kExceptionsForEntry51),  // exceptions count
        kExceptionsForEntry51,             // exceptions
    },
    {
        52,  // id
        "GpuControlListTest.IgnorableEntries.0",
        std::size(kFeatureListForGpuControlTestingEntry52),  // features size
        kFeatureListForGpuControlTestingEntry52,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry52_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        53,  // id
        "GpuControlListTest.IgnorableEntries.1",
        std::size(kFeatureListForGpuControlTestingEntry53),  // features size
        kFeatureListForGpuControlTestingEntry53,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry53,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry53_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        54,  // id
        "GpuControlListTest.DisabledExtensionTest.0",
        0,                                         // feature size
        nullptr,                                   // features
        std::size(kDisabledExtensionsForEntry54),  // DisabledExtensions size
        kDisabledExtensionsForEntry54,             // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry54_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        55,  // id
        "GpuControlListTest.DisabledExtensionTest.1",
        0,                                         // feature size
        nullptr,                                   // features
        std::size(kDisabledExtensionsForEntry55),  // DisabledExtensions size
        kDisabledExtensionsForEntry55,             // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry55_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        56,  // id
        "GpuControlListEntryTest.DirectRendering",
        std::size(kFeatureListForGpuControlTestingEntry56),  // features size
        kFeatureListForGpuControlTestingEntry56,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry56_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        57,  // id
        "GpuControlListTest.LinuxKernelVersion",
        std::size(kFeatureListForGpuControlTestingEntry57),  // features size
        kFeatureListForGpuControlTestingEntry57,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "3.19.1",
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry57_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        58,  // id
        "GpuControlListTest.TestGroup.0",
        std::size(kFeatureListForGpuControlTestingEntry58),  // features size
        kFeatureListForGpuControlTestingEntry58,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry58_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        59,  // id
        "GpuControlListTest.TestGroup.1",
        std::size(kFeatureListForGpuControlTestingEntry59),  // features size
        kFeatureListForGpuControlTestingEntry59,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry59_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        60,  // id
        "GpuControlListEntryTest.GpuSeries",
        std::size(kFeatureListForGpuControlTestingEntry60),  // features size
        kFeatureListForGpuControlTestingEntry60,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
            std::size(kIntelGpuSeriesForEntry60),   // intel_gpu_series size
            kIntelGpuSeriesForEntry60,              // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry60_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        61,  // id
        "GpuControlListEntryTest.GpuSeriesActive",
        std::size(kFeatureListForGpuControlTestingEntry61),  // features size
        kFeatureListForGpuControlTestingEntry61,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x00,                                     // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            std::size(kIntelGpuSeriesForEntry61),     // intel_gpu_series size
            kIntelGpuSeriesForEntry61,                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry61_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        62,  // id
        "GpuControlListEntryTest.GpuSeriesAny",
        std::size(kFeatureListForGpuControlTestingEntry62),  // features size
        kFeatureListForGpuControlTestingEntry62,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                             // os_version
            0x00,                                  // vendor_id
            0,                                     // Devices size
            nullptr,                               // Devices
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            std::size(kIntelGpuSeriesForEntry62),  // intel_gpu_series size
            kIntelGpuSeriesForEntry62,             // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry62_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        63,  // id
        "GpuControlListEntryTest.GpuSeriesPrimary",
        std::size(kFeatureListForGpuControlTestingEntry63),  // features size
        kFeatureListForGpuControlTestingEntry63,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                 // os_version
            0x00,                                      // vendor_id
            0,                                         // Devices size
            nullptr,                                   // Devices
            GpuControlList::kMultiGpuCategoryPrimary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,        // multi_gpu_style
            nullptr,                                   // driver info
            nullptr,                                   // GL strings
            nullptr,                                   // machine model info
            std::size(kIntelGpuSeriesForEntry63),      // intel_gpu_series size
            kIntelGpuSeriesForEntry63,                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry63_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        64,  // id
        "GpuControlListEntryTest.GpuSeriesSecondary",
        std::size(kFeatureListForGpuControlTestingEntry64),  // features size
        kFeatureListForGpuControlTestingEntry64,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                   // os_version
            0x00,                                        // vendor_id
            0,                                           // Devices size
            nullptr,                                     // Devices
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            std::size(kIntelGpuSeriesForEntry64),  // intel_gpu_series size
            kIntelGpuSeriesForEntry64,             // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry64_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        65,  // id
        "GpuControlListEntryTest.GpuSeriesInException",
        std::size(kFeatureListForGpuControlTestingEntry65),  // features size
        kFeatureListForGpuControlTestingEntry65,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry65_1440601243,  // more data
        },
        std::size(kExceptionsForEntry65),  // exceptions count
        kExceptionsForEntry65,             // exceptions
    },
    {
        66,  // id
        "GpuControlListEntryTest.MultipleDrivers",
        std::size(kFeatureListForGpuControlTestingEntry66),  // features size
        kFeatureListForGpuControlTestingEntry66,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry66,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry66_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        67,  // id
        "GpuControlListEntryTest.HardwareOverlay",
        std::size(kFeatureListForGpuControlTestingEntry67),  // features size
        kFeatureListForGpuControlTestingEntry67,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x8086,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry67_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        68,  // id
        "GpuControlListEntryTest.GpuGeneration",
        std::size(kFeatureListForGpuControlTestingEntry68),  // features size
        kFeatureListForGpuControlTestingEntry68,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
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
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "9",
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry68_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        69,  // id
        "GpuControlListEntryTest.GpuGenerationActive",
        std::size(kFeatureListForGpuControlTestingEntry69),  // features size
        kFeatureListForGpuControlTestingEntry69,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x00,                                     // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "9",
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry69_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        70,  // id
        "GpuControlListEntryTest.GpuGenerationAny",
        std::size(kFeatureListForGpuControlTestingEntry70),  // features size
        kFeatureListForGpuControlTestingEntry70,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                             // os_version
            0x00,                                  // vendor_id
            0,                                     // Devices size
            nullptr,                               // Devices
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            0,                                     // intel_gpu_series size
            nullptr,                               // intel_gpu_series
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "9",
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry70_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        71,  // id
        "GpuControlListEntryTest.GpuGenerationPrimary",
        std::size(kFeatureListForGpuControlTestingEntry71),  // features size
        kFeatureListForGpuControlTestingEntry71,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                 // os_version
            0x00,                                      // vendor_id
            0,                                         // Devices size
            nullptr,                                   // Devices
            GpuControlList::kMultiGpuCategoryPrimary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,        // multi_gpu_style
            nullptr,                                   // driver info
            nullptr,                                   // GL strings
            nullptr,                                   // machine model info
            0,                                         // intel_gpu_series size
            nullptr,                                   // intel_gpu_series
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "9",
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry71_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        72,  // id
        "GpuControlListEntryTest.GpuGenerationSecondary",
        std::size(kFeatureListForGpuControlTestingEntry72),  // features size
        kFeatureListForGpuControlTestingEntry72,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                   // os_version
            0x00,                                        // vendor_id
            0,                                           // Devices size
            nullptr,                                     // Devices
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,        // intel_gpu_series size
            nullptr,  // intel_gpu_series
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, "9",
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry72_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        73,  // id
        "GpuControlListEntryTest.SubpixelFontRendering",
        std::size(kFeatureListForGpuControlTestingEntry73),  // features size
        kFeatureListForGpuControlTestingEntry73,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry73_1440601243,  // more data
        },
        std::size(kExceptionsForEntry73),  // exceptions count
        kExceptionsForEntry73,             // exceptions
    },
    {
        74,  // id
        "GpuControlListEntryTest.SubpixelFontRenderingDontCare",
        std::size(kFeatureListForGpuControlTestingEntry74),  // features size
        kFeatureListForGpuControlTestingEntry74,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry74,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry74_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        75,  // id
        "GpuControlListEntryTest.IntelDriverVendorEntry",
        std::size(kFeatureListForGpuControlTestingEntry75),  // features size
        kFeatureListForGpuControlTestingEntry75,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry75,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry75_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        76,  // id
        "GpuControlListEntryTest.IntelDriverVersionEntry",
        std::size(kFeatureListForGpuControlTestingEntry76),  // features size
        kFeatureListForGpuControlTestingEntry76,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x8086,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry76,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry76_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        77,  // id
        "GpuControlListEntryTest.DeviceRevisionEntry",
        std::size(kFeatureListForGpuControlTestingEntry77),  // features size
        kFeatureListForGpuControlTestingEntry77,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                              // os_version
            0x1002,                                 // vendor_id
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
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry77_1440601243,  // more data
        },
        std::size(kExceptionsForEntry77),  // exceptions count
        kExceptionsForEntry77,             // exceptions
    },
    {
        78,  // id
        "GpuControlListEntryTest.DeviceRevisionUnspecifiedEntry",
        std::size(kFeatureListForGpuControlTestingEntry78),  // features size
        kFeatureListForGpuControlTestingEntry78,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                       // os_version
            0x1002,                                          // vendor_id
            std::size(kDevicesForGpuControlTestingEntry78),  // Devices size
            kDevicesForGpuControlTestingEntry78,             // Devices
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // intel_gpu_series size
            nullptr,                                // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry78_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        79,  // id
        "GpuControlListEntryTest.AnyDriverVersion",
        std::size(kFeatureListForGpuControlTestingEntry79),  // features size
        kFeatureListForGpuControlTestingEntry79,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryAny,     // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry79,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry79_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        80,  // id
        "GpuControlListEntryTest.ActiveDriverVersion",
        std::size(kFeatureListForGpuControlTestingEntry80),  // features size
        kFeatureListForGpuControlTestingEntry80,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                                // os_version
            0x10de,                                   // vendor_id
            0,                                        // Devices size
            nullptr,                                  // Devices
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry80,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // intel_gpu_series size
            nullptr,                                  // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry80_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        81,  // id
        "GpuControlListEntryTest.NativeAngleRenderer",
        std::size(kFeatureListForGpuControlTestingEntry81),  // features size
        kFeatureListForGpuControlTestingEntry81,             // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                               // os_version
            0x00,                                    // vendor_id
            0,                                       // Devices size
            nullptr,                                 // Devices
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry81,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // intel_gpu_series size
            nullptr,                                 // intel_gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                    // intel_gpu_generation
            &kMoreForEntry81_1440601243,  // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
};
const size_t kGpuControlListTestingEntryCount = 81;
}  // namespace gpu
