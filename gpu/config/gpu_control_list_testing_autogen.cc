// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/gpu_control_list_testing_autogen.h"

#include "gpu/config/gpu_control_list_testing_arrays_and_structs_autogen.h"
#include "gpu/config/gpu_control_list_testing_exceptions_autogen.h"

namespace gpu {

const GpuControlList::Entry kGpuControlListTestingEntries[] = {
    {
        1,  // id
        "GpuControlListEntryTest.DetailedEntry",
        base::size(kFeatureListForGpuControlTestingEntry1),  // features size
        kFeatureListForGpuControlTestingEntry1,              // features
        base::size(kDisabledExtensionsForEntry1),  // DisabledExtensions size
        kDisabledExtensionsForEntry1,              // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        base::size(kCrBugsForGpuControlTestingEntry1),  // CrBugs size
        kCrBugsForGpuControlTestingEntry1,              // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
             "10.6.4", nullptr},                               // os_version
            0x10de,                                            // vendor_id
            base::size(kDeviceIDsForGpuControlTestingEntry1),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry1,              // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry1,  // driver info
            nullptr,                                 // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        2,  // id
        "GpuControlListEntryTest.VendorOnAllOsEntry",
        base::size(kFeatureListForGpuControlTestingEntry2),  // features size
        kFeatureListForGpuControlTestingEntry2,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        3,  // id
        "GpuControlListEntryTest.VendorOnLinuxEntry",
        base::size(kFeatureListForGpuControlTestingEntry3),  // features size
        kFeatureListForGpuControlTestingEntry3,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x10de,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        4,  // id
        "GpuControlListEntryTest.AllExceptNVidiaOnLinuxEntry",
        base::size(kFeatureListForGpuControlTestingEntry4),  // features size
        kFeatureListForGpuControlTestingEntry4,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry4),  // exceptions count
        kExceptionsForEntry4,              // exceptions
    },
    {
        5,  // id
        "GpuControlListEntryTest.AllExceptIntelOnLinuxEntry",
        base::size(kFeatureListForGpuControlTestingEntry5),  // features size
        kFeatureListForGpuControlTestingEntry5,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry5),  // exceptions count
        kExceptionsForEntry5,              // exceptions
    },
    {
        6,  // id
        "GpuControlListEntryTest.DateOnWindowsEntry",
        base::size(kFeatureListForGpuControlTestingEntry6),  // features size
        kFeatureListForGpuControlTestingEntry6,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry6,  // driver info
            nullptr,                                 // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        7,  // id
        "GpuControlListEntryTest.MultipleDevicesEntry",
        base::size(kFeatureListForGpuControlTestingEntry7),  // features size
        kFeatureListForGpuControlTestingEntry7,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                                // os_version
            0x10de,                                            // vendor_id
            base::size(kDeviceIDsForGpuControlTestingEntry7),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry7,              // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        8,  // id
        "GpuControlListEntryTest.ChromeOSEntry",
        base::size(kFeatureListForGpuControlTestingEntry8),  // features size
        kFeatureListForGpuControlTestingEntry8,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsChromeOS,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        9,  // id
        "GpuControlListEntryTest.GlVersionGLESEntry",
        base::size(kFeatureListForGpuControlTestingEntry9),  // features size
        kFeatureListForGpuControlTestingEntry9,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry9,                        // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        10,  // id
        "GpuControlListEntryTest.GlVersionANGLEEntry",
        base::size(kFeatureListForGpuControlTestingEntry10),  // features size
        kFeatureListForGpuControlTestingEntry10,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry10,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        11,  // id
        "GpuControlListEntryTest.GlVersionGLEntry",
        base::size(kFeatureListForGpuControlTestingEntry11),  // features size
        kFeatureListForGpuControlTestingEntry11,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry11,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        12,  // id
        "GpuControlListEntryTest.GlVendorEqual",
        base::size(kFeatureListForGpuControlTestingEntry12),  // features size
        kFeatureListForGpuControlTestingEntry12,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry12,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        13,  // id
        "GpuControlListEntryTest.GlVendorWithDot",
        base::size(kFeatureListForGpuControlTestingEntry13),  // features size
        kFeatureListForGpuControlTestingEntry13,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry13,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        14,  // id
        "GpuControlListEntryTest.GlRendererContains",
        base::size(kFeatureListForGpuControlTestingEntry14),  // features size
        kFeatureListForGpuControlTestingEntry14,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry14,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        15,  // id
        "GpuControlListEntryTest.GlRendererCaseInsensitive",
        base::size(kFeatureListForGpuControlTestingEntry15),  // features size
        kFeatureListForGpuControlTestingEntry15,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry15,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        16,  // id
        "GpuControlListEntryTest.GlExtensionsEndWith",
        base::size(kFeatureListForGpuControlTestingEntry16),  // features size
        kFeatureListForGpuControlTestingEntry16,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                      // os_version
            0x00,                                    // vendor_id
            0,                                       // DeviceIDs size
            nullptr,                                 // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
            nullptr,                                 // driver info
            &kGLStringsForGpuControlTestingEntry16,  // GL strings
            nullptr,                                 // machine model info
            0,                                       // gpu_series size
            nullptr,                                 // gpu_series
            nullptr,                                 // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        17,  // id
        "GpuControlListEntryTest.OptimusEntry",
        base::size(kFeatureListForGpuControlTestingEntry17),  // features size
        kFeatureListForGpuControlTestingEntry17,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleOptimus,  // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        18,  // id
        "GpuControlListEntryTest.AMDSwitchableEntry",
        base::size(kFeatureListForGpuControlTestingEntry18),  // features size
        kFeatureListForGpuControlTestingEntry18,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                           // os_version
            0x00,                                         // vendor_id
            0,                                            // DeviceIDs size
            nullptr,                                      // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,        // multi_gpu_category
            GpuControlList::kMultiGpuStyleAMDSwitchable,  // multi_gpu_style
            nullptr,                                      // driver info
            nullptr,                                      // GL strings
            nullptr,                                      // machine model info
            0,                                            // gpu_series size
            nullptr,                                      // gpu_series
            nullptr,                                      // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        19,  // id
        "GpuControlListEntryTest.DriverVendorBeginWith",
        base::size(kFeatureListForGpuControlTestingEntry19),  // features size
        kFeatureListForGpuControlTestingEntry19,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x00,                                     // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry19,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        20,  // id
        "GpuControlListEntryTest.LexicalDriverVersionEntry",
        base::size(kFeatureListForGpuControlTestingEntry20),  // features size
        kFeatureListForGpuControlTestingEntry20,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x1002,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry20,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        21,  // id
        "GpuControlListEntryTest.NeedsMoreInfoEntry",
        base::size(kFeatureListForGpuControlTestingEntry21),  // features size
        kFeatureListForGpuControlTestingEntry21,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry21,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        22,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForExceptionsEntry",
        base::size(kFeatureListForGpuControlTestingEntry22),  // features size
        kFeatureListForGpuControlTestingEntry22,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry22),  // exceptions count
        kExceptionsForEntry22,              // exceptions
    },
    {
        23,  // id
        "GpuControlListEntryTest.NeedsMoreInfoForGlVersionEntry",
        base::size(kFeatureListForGpuControlTestingEntry23),  // features size
        kFeatureListForGpuControlTestingEntry23,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry23,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        24,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntry",
        base::size(kFeatureListForGpuControlTestingEntry24),  // features size
        kFeatureListForGpuControlTestingEntry24,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        25,  // id
        "GpuControlListEntryTest.FeatureTypeAllEntryWithExceptions",
        base::size(kFeatureListForGpuControlTestingEntry25),  // features size
        kFeatureListForGpuControlTestingEntry25,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        26,  // id
        "GpuControlListEntryTest.SingleActiveGPU",
        base::size(kFeatureListForGpuControlTestingEntry26),  // features size
        kFeatureListForGpuControlTestingEntry26,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x10de,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry26),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry26,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,     // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        27,  // id
        "GpuControlListEntryTest.MachineModelName",
        base::size(kFeatureListForGpuControlTestingEntry27),  // features size
        kFeatureListForGpuControlTestingEntry27,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            &kMachineModelInfoForEntry27,           // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        28,  // id
        "GpuControlListEntryTest.MachineModelNameException",
        base::size(kFeatureListForGpuControlTestingEntry28),  // features size
        kFeatureListForGpuControlTestingEntry28,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry28),  // exceptions count
        kExceptionsForEntry28,              // exceptions
    },
    {
        29,  // id
        "GpuControlListEntryTest.MachineModelVersion",
        base::size(kFeatureListForGpuControlTestingEntry29),  // features size
        kFeatureListForGpuControlTestingEntry29,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            &kMachineModelInfoForEntry29,           // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        30,  // id
        "GpuControlListEntryTest.MachineModelVersionException",
        base::size(kFeatureListForGpuControlTestingEntry30),  // features size
        kFeatureListForGpuControlTestingEntry30,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            &kMachineModelInfoForEntry30,           // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry30),  // exceptions count
        kExceptionsForEntry30,              // exceptions
    },
    {
        31,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.Intel",
        base::size(kFeatureListForGpuControlTestingEntry31),  // features size
        kFeatureListForGpuControlTestingEntry31,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x8086,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry31),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry31,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,        // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        32,  // id
        "GpuControlListEntryDualGPUTest.CategoryAny.NVidia",
        base::size(kFeatureListForGpuControlTestingEntry32),  // features size
        kFeatureListForGpuControlTestingEntry32,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x10de,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry32),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry32,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,        // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        33,  // id
        "GpuControlListEntryDualGPUTest.CategorySecondary",
        base::size(kFeatureListForGpuControlTestingEntry33),  // features size
        kFeatureListForGpuControlTestingEntry33,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x8086,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry33),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry33,       // DeviceIDs
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        34,  // id
        "GpuControlListEntryDualGPUTest.CategoryPrimary",
        base::size(kFeatureListForGpuControlTestingEntry34),  // features size
        kFeatureListForGpuControlTestingEntry34,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x8086,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry34),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry34,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryPrimary,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        35,  // id
        "GpuControlListEntryDualGPUTest.CategoryDefault",
        base::size(kFeatureListForGpuControlTestingEntry35),  // features size
        kFeatureListForGpuControlTestingEntry35,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x8086,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry35),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry35,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,       // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        36,  // id
        "GpuControlListEntryDualGPUTest.ActiveSecondaryGPU",
        base::size(kFeatureListForGpuControlTestingEntry36),  // features size
        kFeatureListForGpuControlTestingEntry36,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x8086,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry36),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry36,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,     // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        37,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActiveSecondaryGPU",
        base::size(kFeatureListForGpuControlTestingEntry37),  // features size
        kFeatureListForGpuControlTestingEntry37,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        38,  // id
        "GpuControlListEntryDualGPUTest.ActivePrimaryGPU",
        base::size(kFeatureListForGpuControlTestingEntry38),  // features size
        kFeatureListForGpuControlTestingEntry38,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},  // os_version
            0x10de,              // vendor_id
            base::size(
                kDeviceIDsForGpuControlTestingEntry38),  // DeviceIDs size
            kDeviceIDsForGpuControlTestingEntry38,       // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,     // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            0,                                           // gpu_series size
            nullptr,                                     // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        39,  // id
        "GpuControlListEntryDualGPUTest.VendorOnlyActivePrimaryGPU",
        base::size(kFeatureListForGpuControlTestingEntry39),  // features size
        kFeatureListForGpuControlTestingEntry39,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        40,  // id
        "GpuControlListEntryTest.PixelShaderVersion",
        base::size(kFeatureListForGpuControlTestingEntry40),  // features size
        kFeatureListForGpuControlTestingEntry40,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry40,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        41,  // id
        "GpuControlListEntryTest.OsVersionZeroLT",
        base::size(kFeatureListForGpuControlTestingEntry41),  // features size
        kFeatureListForGpuControlTestingEntry41,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical, "4.2",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        42,  // id
        "GpuControlListEntryTest.OsVersionZeroAny",
        base::size(kFeatureListForGpuControlTestingEntry42),  // features size
        kFeatureListForGpuControlTestingEntry42,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAndroid,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        43,  // id
        "GpuControlListEntryTest.OsComparisonAny",
        base::size(kFeatureListForGpuControlTestingEntry43),  // features size
        kFeatureListForGpuControlTestingEntry43,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        44,  // id
        "GpuControlListEntryTest.OsComparisonGE",
        base::size(kFeatureListForGpuControlTestingEntry44),  // features size
        kFeatureListForGpuControlTestingEntry44,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical, "6",
             nullptr},                              // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        45,  // id
        "GpuControlListEntryTest.ExceptionWithoutVendorId",
        base::size(kFeatureListForGpuControlTestingEntry45),  // features size
        kFeatureListForGpuControlTestingEntry45,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry45),  // exceptions count
        kExceptionsForEntry45,              // exceptions
    },
    {
        46,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableDiscrete",
        base::size(kFeatureListForGpuControlTestingEntry46),  // features size
        kFeatureListForGpuControlTestingEntry46,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableDiscrete,  // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        47,  // id
        "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableIntegrated",
        base::size(kFeatureListForGpuControlTestingEntry47),  // features size
        kFeatureListForGpuControlTestingEntry47,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::
                kMultiGpuStyleAMDSwitchableIntegrated,  // multi_gpu_style
            nullptr,                                    // driver info
            nullptr,                                    // GL strings
            nullptr,                                    // machine model info
            0,                                          // gpu_series size
            nullptr,                                    // gpu_series
            nullptr,                                    // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        48,  // id
        "GpuControlListEntryTest.InProcessGPU",
        base::size(kFeatureListForGpuControlTestingEntry48),  // features size
        kFeatureListForGpuControlTestingEntry48,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry48,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        49,  // id
        "GpuControlListEntryTest.SameGPUTwiceTest",
        base::size(kFeatureListForGpuControlTestingEntry49),  // features size
        kFeatureListForGpuControlTestingEntry49,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        50,  // id
        "GpuControlListEntryTest.NVidiaNumberingScheme",
        base::size(kFeatureListForGpuControlTestingEntry50),  // features size
        kFeatureListForGpuControlTestingEntry50,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry50,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        51,  // id
        "GpuControlListTest.NeedsMoreInfo",
        base::size(kFeatureListForGpuControlTestingEntry51),  // features size
        kFeatureListForGpuControlTestingEntry51,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x10de,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry51,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        52,  // id
        "GpuControlListTest.NeedsMoreInfoForExceptions",
        base::size(kFeatureListForGpuControlTestingEntry52),  // features size
        kFeatureListForGpuControlTestingEntry52,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry52),  // exceptions count
        kExceptionsForEntry52,              // exceptions
    },
    {
        53,  // id
        "GpuControlListTest.IgnorableEntries.0",
        base::size(kFeatureListForGpuControlTestingEntry53),  // features size
        kFeatureListForGpuControlTestingEntry53,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        54,  // id
        "GpuControlListTest.IgnorableEntries.1",
        base::size(kFeatureListForGpuControlTestingEntry54),  // features size
        kFeatureListForGpuControlTestingEntry54,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry54,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        55,  // id
        "GpuControlListTest.DisabledExtensionTest.0",
        0,                                          // feature size
        nullptr,                                    // features
        base::size(kDisabledExtensionsForEntry55),  // DisabledExtensions size
        kDisabledExtensionsForEntry55,              // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        56,  // id
        "GpuControlListTest.DisabledExtensionTest.1",
        0,                                          // feature size
        nullptr,                                    // features
        base::size(kDisabledExtensionsForEntry56),  // DisabledExtensions size
        kDisabledExtensionsForEntry56,              // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsWin,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        57,  // id
        "GpuControlListEntryTest.DirectRendering",
        base::size(kFeatureListForGpuControlTestingEntry57),  // features size
        kFeatureListForGpuControlTestingEntry57,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry57,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        58,  // id
        "GpuControlListTest.LinuxKernelVersion",
        base::size(kFeatureListForGpuControlTestingEntry58),  // features size
        kFeatureListForGpuControlTestingEntry58,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsLinux,  // os_type
            {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
             "3.19.1", nullptr},                    // os_version
            0x8086,                                 // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        59,  // id
        "GpuControlListTest.TestGroup.0",
        base::size(kFeatureListForGpuControlTestingEntry59),  // features size
        kFeatureListForGpuControlTestingEntry59,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry59,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        60,  // id
        "GpuControlListTest.TestGroup.1",
        base::size(kFeatureListForGpuControlTestingEntry60),  // features size
        kFeatureListForGpuControlTestingEntry60,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            &kMoreForEntry60,                       // more data
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        61,  // id
        "GpuControlListEntryTest.GpuSeries",
        base::size(kFeatureListForGpuControlTestingEntry61),  // features size
        kFeatureListForGpuControlTestingEntry61,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            base::size(kGpuSeriesForEntry61),       // gpu_series size
            kGpuSeriesForEntry61,                   // gpu_series
            nullptr,                                // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        62,  // id
        "GpuControlListEntryTest.GpuSeriesActive",
        base::size(kFeatureListForGpuControlTestingEntry62),  // features size
        kFeatureListForGpuControlTestingEntry62,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x00,                                     // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryActive,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            nullptr,                                  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            base::size(kGpuSeriesForEntry62),         // gpu_series size
            kGpuSeriesForEntry62,                     // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        63,  // id
        "GpuControlListEntryTest.GpuSeriesAny",
        base::size(kFeatureListForGpuControlTestingEntry63),  // features size
        kFeatureListForGpuControlTestingEntry63,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x00,                                  // vendor_id
            0,                                     // DeviceIDs size
            nullptr,                               // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            base::size(kGpuSeriesForEntry63),      // gpu_series size
            kGpuSeriesForEntry63,                  // gpu_series
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        64,  // id
        "GpuControlListEntryTest.GpuSeriesPrimary",
        base::size(kFeatureListForGpuControlTestingEntry64),  // features size
        kFeatureListForGpuControlTestingEntry64,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                        // os_version
            0x00,                                      // vendor_id
            0,                                         // DeviceIDs size
            nullptr,                                   // DeviceIDs
            GpuControlList::kMultiGpuCategoryPrimary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,        // multi_gpu_style
            nullptr,                                   // driver info
            nullptr,                                   // GL strings
            nullptr,                                   // machine model info
            base::size(kGpuSeriesForEntry64),          // gpu_series size
            kGpuSeriesForEntry64,                      // gpu_series
            nullptr,                                   // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        65,  // id
        "GpuControlListEntryTest.GpuSeriesSecondary",
        base::size(kFeatureListForGpuControlTestingEntry65),  // features size
        kFeatureListForGpuControlTestingEntry65,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                          // os_version
            0x00,                                        // vendor_id
            0,                                           // DeviceIDs size
            nullptr,                                     // DeviceIDs
            GpuControlList::kMultiGpuCategorySecondary,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,          // multi_gpu_style
            nullptr,                                     // driver info
            nullptr,                                     // GL strings
            nullptr,                                     // machine model info
            base::size(kGpuSeriesForEntry65),            // gpu_series size
            kGpuSeriesForEntry65,                        // gpu_series
            nullptr,                                     // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
    {
        66,  // id
        "GpuControlListEntryTest.GpuSeriesInException",
        base::size(kFeatureListForGpuControlTestingEntry66),  // features size
        kFeatureListForGpuControlTestingEntry66,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                     // os_version
            0x00,                                   // vendor_id
            0,                                      // DeviceIDs size
            nullptr,                                // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
            nullptr,                                // driver info
            nullptr,                                // GL strings
            nullptr,                                // machine model info
            0,                                      // gpu_series size
            nullptr,                                // gpu_series
            nullptr,                                // more conditions
        },
        base::size(kExceptionsForEntry66),  // exceptions count
        kExceptionsForEntry66,              // exceptions
    },
    {
        67,  // id
        "GpuControlListEntryTest.MultipleDrivers",
        base::size(kFeatureListForGpuControlTestingEntry67),  // features size
        kFeatureListForGpuControlTestingEntry67,              // features
        0,        // DisabledExtensions size
        nullptr,  // DisabledExtensions
        0,        // DisabledWebGLExtensions size
        nullptr,  // DisabledWebGLExtensions
        0,        // CrBugs size
        nullptr,  // CrBugs
        {
            GpuControlList::kOsAny,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                       // os_version
            0x8086,                                   // vendor_id
            0,                                        // DeviceIDs size
            nullptr,                                  // DeviceIDs
            GpuControlList::kMultiGpuCategoryNone,    // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,       // multi_gpu_style
            &kDriverInfoForGpuControlTestingEntry67,  // driver info
            nullptr,                                  // GL strings
            nullptr,                                  // machine model info
            0,                                        // gpu_series size
            nullptr,                                  // gpu_series
            nullptr,                                  // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    },
};
const size_t kGpuControlListTestingEntryCount = 67;
}  // namespace gpu
