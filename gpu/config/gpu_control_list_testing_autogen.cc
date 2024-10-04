// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
//    gpu/config/process_json.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/config/gpu_control_list_testing_autogen.h"

#include <array>
#include <iterator>

#include "gpu/config/gpu_control_list_testing_data.h"

namespace gpu {

const std::array<GpuControlList::Entry, 81>& GetGpuControlListTestingEntries() {
#include "gpu/config/gpu_control_list_testing_arrays_and_structs_autogen.h"
#include "gpu/config/gpu_control_list_testing_exceptions_autogen.h"

  static const std::array<GpuControlList::Entry, 81>
      kGpuControlListTestingEntries = {{
          {
              1,  // id
              "GpuControlListEntryTest.DetailedEntry",
              base::span(kFeatureListForGpuControlTestingEntry1),  // features
              base::span(kDisabledExtensionsForEntry1),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span(kCrBugsForGpuControlTestingEntry1),  // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kEQ, GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, "10.6.4",
                   nullptr},                                       // os_version
                  0x10de,                                          // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry1),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry1,  // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              2,  // id
              "GpuControlListEntryTest.VendorOnAllOsEntry",
              base::span(kFeatureListForGpuControlTestingEntry2),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              3,  // id
              "GpuControlListEntryTest.VendorOnLinuxEntry",
              base::span(kFeatureListForGpuControlTestingEntry3),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              4,  // id
              "GpuControlListEntryTest.AllExceptNVidiaOnLinuxEntry",
              base::span(kFeatureListForGpuControlTestingEntry4),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry4),  // exceptions
          },
          {
              5,  // id
              "GpuControlListEntryTest.AllExceptIntelOnLinuxEntry",
              base::span(kFeatureListForGpuControlTestingEntry5),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry5),  // exceptions
          },
          {
              6,  // id
              "GpuControlListEntryTest.MultipleDevicesEntry",
              base::span(kFeatureListForGpuControlTestingEntry6),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                       // os_version
                  0x10de,                                          // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry6),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              7,  // id
              "GpuControlListEntryTest.ChromeOSEntry",
              base::span(kFeatureListForGpuControlTestingEntry7),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsChromeOS,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              8,  // id
              "GpuControlListEntryTest.GlVersionGLESEntry",
              base::span(kFeatureListForGpuControlTestingEntry8),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry8_1440601243,             // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              9,  // id
              "GpuControlListEntryTest.GlVersionANGLEEntry",
              base::span(kFeatureListForGpuControlTestingEntry9),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry9_1440601243,             // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              10,  // id
              "GpuControlListEntryTest.GlVersionGLEntry",
              base::span(kFeatureListForGpuControlTestingEntry10),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry10_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              11,  // id
              "GpuControlListEntryTest.GlVendorEqual",
              base::span(kFeatureListForGpuControlTestingEntry11),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry11,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              12,  // id
              "GpuControlListEntryTest.GlVendorWithDot",
              base::span(kFeatureListForGpuControlTestingEntry12),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry12,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              13,  // id
              "GpuControlListEntryTest.GlRendererContains",
              base::span(kFeatureListForGpuControlTestingEntry13),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry13,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              14,  // id
              "GpuControlListEntryTest.GlRendererCaseInsensitive",
              base::span(kFeatureListForGpuControlTestingEntry14),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry14,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              15,  // id
              "GpuControlListEntryTest.GlExtensionsEndWith",
              base::span(kFeatureListForGpuControlTestingEntry15),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry15,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              16,  // id
              "GpuControlListEntryTest.OptimusEntry",
              base::span(kFeatureListForGpuControlTestingEntry16),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleOptimus,  // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              17,  // id
              "GpuControlListEntryTest.AMDSwitchableEntry",
              base::span(kFeatureListForGpuControlTestingEntry17),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::
                      kMultiGpuStyleAMDSwitchable,  // multi_gpu_style
                  nullptr,                          // driver info
                  nullptr,                          // GL strings
                  nullptr,                          // machine model info
                  nullptr,                          // Intel conditions
                  nullptr,                          // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              18,  // id
              "GpuControlListEntryTest.DriverVendorBeginWith",
              base::span(kFeatureListForGpuControlTestingEntry18),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry18,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              19,  // id
              "GpuControlListEntryTest.LexicalDriverVersionEntry",
              base::span(kFeatureListForGpuControlTestingEntry19),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x1002,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry19,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              20,  // id
              "GpuControlListEntryTest.NeedsMoreInfoEntry",
              base::span(kFeatureListForGpuControlTestingEntry20),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry20,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              21,  // id
              "GpuControlListEntryTest.NeedsMoreInfoForExceptionsEntry",
              base::span(kFeatureListForGpuControlTestingEntry21),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry21),  // exceptions
          },
          {
              22,  // id
              "GpuControlListEntryTest.NeedsMoreInfoForGlVersionEntry",
              base::span(kFeatureListForGpuControlTestingEntry22),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry22_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              23,  // id
              "GpuControlListEntryTest.FeatureTypeAllEntry",
              base::span(kFeatureListForGpuControlTestingEntry23),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              24,  // id
              "GpuControlListEntryTest.FeatureTypeAllEntryWithExceptions",
              base::span(kFeatureListForGpuControlTestingEntry24),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              25,  // id
              "GpuControlListEntryTest.SingleActiveGPU",
              base::span(kFeatureListForGpuControlTestingEntry25),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x10de,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry25),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              26,  // id
              "GpuControlListEntryTest.MachineModelName",
              base::span(kFeatureListForGpuControlTestingEntry26),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAndroid,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  &kMachineModelInfoForEntry26,           // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              27,  // id
              "GpuControlListEntryTest.MachineModelNameException",
              base::span(kFeatureListForGpuControlTestingEntry27),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry27),  // exceptions
          },
          {
              28,  // id
              "GpuControlListEntryTest.MachineModelVersion",
              base::span(kFeatureListForGpuControlTestingEntry28),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  &kMachineModelInfoForEntry28,           // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              29,  // id
              "GpuControlListEntryTest.MachineModelVersionException",
              base::span(kFeatureListForGpuControlTestingEntry29),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  &kMachineModelInfoForEntry29,           // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry29),  // exceptions
          },
          {
              30,  // id
              "GpuControlListEntryDualGPUTest.CategoryAny.Intel",
              base::span(kFeatureListForGpuControlTestingEntry30),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x8086,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry30),  // Devices
                  GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
                  nullptr,                               // driver info
                  nullptr,                               // GL strings
                  nullptr,                               // machine model info
                  nullptr,                               // Intel conditions
                  nullptr,                               // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              31,  // id
              "GpuControlListEntryDualGPUTest.CategoryAny.NVidia",
              base::span(kFeatureListForGpuControlTestingEntry31),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x10de,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry31),  // Devices
                  GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
                  nullptr,                               // driver info
                  nullptr,                               // GL strings
                  nullptr,                               // machine model info
                  nullptr,                               // Intel conditions
                  nullptr,                               // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              32,  // id
              "GpuControlListEntryDualGPUTest.CategorySecondary",
              base::span(kFeatureListForGpuControlTestingEntry32),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x8086,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry32),  // Devices
                  GpuControlList::
                      kMultiGpuCategorySecondary,      // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              33,  // id
              "GpuControlListEntryDualGPUTest.CategoryPrimary",
              base::span(kFeatureListForGpuControlTestingEntry33),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x8086,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry33),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryPrimary,        // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              34,  // id
              "GpuControlListEntryDualGPUTest.CategoryDefault",
              base::span(kFeatureListForGpuControlTestingEntry34),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x8086,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry34),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              35,  // id
              "GpuControlListEntryDualGPUTest.ActiveSecondaryGPU",
              base::span(kFeatureListForGpuControlTestingEntry35),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x8086,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry35),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              36,  // id
              "GpuControlListEntryDualGPUTest.VendorOnlyActiveSecondaryGPU",
              base::span(kFeatureListForGpuControlTestingEntry36),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              37,  // id
              "GpuControlListEntryDualGPUTest.ActivePrimaryGPU",
              base::span(kFeatureListForGpuControlTestingEntry37),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x10de,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry37),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              38,  // id
              "GpuControlListEntryDualGPUTest.VendorOnlyActivePrimaryGPU",
              base::span(kFeatureListForGpuControlTestingEntry38),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  nullptr,                             // driver info
                  nullptr,                             // GL strings
                  nullptr,                             // machine model info
                  nullptr,                             // Intel conditions
                  nullptr,                             // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              39,  // id
              "GpuControlListEntryTest.PixelShaderVersion",
              base::span(kFeatureListForGpuControlTestingEntry39),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry39_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              40,  // id
              "GpuControlListEntryTest.OsVersionZeroLT",
              base::span(kFeatureListForGpuControlTestingEntry40),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAndroid,  // os_type
                  {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, "4.2",
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              41,  // id
              "GpuControlListEntryTest.OsVersionZeroAny",
              base::span(kFeatureListForGpuControlTestingEntry41),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAndroid,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              42,  // id
              "GpuControlListEntryTest.OsComparisonAny",
              base::span(kFeatureListForGpuControlTestingEntry42),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              43,  // id
              "GpuControlListEntryTest.OsComparisonGE",
              base::span(kFeatureListForGpuControlTestingEntry43),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kGE, GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, "6",
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              44,  // id
              "GpuControlListEntryTest.ExceptionWithoutVendorId",
              base::span(kFeatureListForGpuControlTestingEntry44),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry44),  // exceptions
          },
          {
              45,  // id
              "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableDiscrete",
              base::span(kFeatureListForGpuControlTestingEntry45),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::
                      kMultiGpuStyleAMDSwitchableDiscrete,  // multi_gpu_style
                  nullptr,                                  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              46,  // id
              "GpuControlListEntryTest.MultiGpuStyleAMDSwitchableIntegrated",
              base::span(kFeatureListForGpuControlTestingEntry46),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::
                      kMultiGpuStyleAMDSwitchableIntegrated,  // multi_gpu_style
                  nullptr,                                    // driver info
                  nullptr,                                    // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              47,  // id
              "GpuControlListEntryTest.InProcessGPU",
              base::span(kFeatureListForGpuControlTestingEntry47),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry47_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              48,  // id
              "GpuControlListEntryTest.SameGPUTwiceTest",
              base::span(kFeatureListForGpuControlTestingEntry48),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              49,  // id
              "GpuControlListEntryTest.NVidiaNumberingScheme",
              base::span(kFeatureListForGpuControlTestingEntry49),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry49,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              50,  // id
              "GpuControlListTest.NeedsMoreInfo",
              base::span(kFeatureListForGpuControlTestingEntry50),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry50,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              51,  // id
              "GpuControlListTest.NeedsMoreInfoForExceptions",
              base::span(kFeatureListForGpuControlTestingEntry51),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry51),  // exceptions
          },
          {
              52,  // id
              "GpuControlListTest.IgnorableEntries.0",
              base::span(kFeatureListForGpuControlTestingEntry52),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              53,  // id
              "GpuControlListTest.IgnorableEntries.1",
              base::span(kFeatureListForGpuControlTestingEntry53),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry53,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              54,  // id
              "GpuControlListTest.DisabledExtensionTest.0",
              base::span<const int>(),                    // features
              base::span(kDisabledExtensionsForEntry54),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              55,  // id
              "GpuControlListTest.DisabledExtensionTest.1",
              base::span<const int>(),                    // features
              base::span(kDisabledExtensionsForEntry55),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              56,  // id
              "GpuControlListEntryTest.DirectRendering",
              base::span(kFeatureListForGpuControlTestingEntry56),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry56_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              57,  // id
              "GpuControlListTest.LinuxKernelVersion",
              base::span(kFeatureListForGpuControlTestingEntry57),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsLinux,  // os_type
                  {GpuControlList::kLT, GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, "3.19.1",
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              58,  // id
              "GpuControlListTest.TestGroup.0",
              base::span(kFeatureListForGpuControlTestingEntry58),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry58_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              59,  // id
              "GpuControlListTest.TestGroup.1",
              base::span(kFeatureListForGpuControlTestingEntry59),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry59_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              60,  // id
              "GpuControlListEntryTest.GpuSeries",
              base::span(kFeatureListForGpuControlTestingEntry60),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry60_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              61,  // id
              "GpuControlListEntryTest.GpuSeriesActive",
              base::span(kFeatureListForGpuControlTestingEntry61),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,             // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry61_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              62,  // id
              "GpuControlListEntryTest.GpuSeriesAny",
              base::span(kFeatureListForGpuControlTestingEntry62),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryAny,    // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry62_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              63,  // id
              "GpuControlListEntryTest.GpuSeriesPrimary",
              base::span(kFeatureListForGpuControlTestingEntry63),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryPrimary,            // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry63_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              64,  // id
              "GpuControlListEntryTest.GpuSeriesSecondary",
              base::span(kFeatureListForGpuControlTestingEntry64),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategorySecondary,          // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry64_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              65,  // id
              "GpuControlListEntryTest.GpuSeriesInException",
              base::span(kFeatureListForGpuControlTestingEntry65),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry65),  // exceptions
          },
          {
              66,  // id
              "GpuControlListEntryTest.MultipleDrivers",
              base::span(kFeatureListForGpuControlTestingEntry66),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry66,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              67,  // id
              "GpuControlListEntryTest.HardwareOverlay",
              base::span(kFeatureListForGpuControlTestingEntry67),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  &kMoreForEntry67_1440601243,            // more data
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              68,  // id
              "GpuControlListEntryTest.GpuGeneration",
              base::span(kFeatureListForGpuControlTestingEntry68),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry68_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              69,  // id
              "GpuControlListEntryTest.GpuGenerationActive",
              base::span(kFeatureListForGpuControlTestingEntry69),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,             // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry69_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              70,  // id
              "GpuControlListEntryTest.GpuGenerationAny",
              base::span(kFeatureListForGpuControlTestingEntry70),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryAny,    // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry70_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              71,  // id
              "GpuControlListEntryTest.GpuGenerationPrimary",
              base::span(kFeatureListForGpuControlTestingEntry71),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryPrimary,            // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry71_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              72,  // id
              "GpuControlListEntryTest.GpuGenerationSecondary",
              base::span(kFeatureListForGpuControlTestingEntry72),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategorySecondary,          // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  nullptr,                                 // GL strings
                  nullptr,                                 // machine model info
                  &kIntelConditionsForEntry72_1440601243,  // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              73,  // id
              "GpuControlListEntryTest.SubpixelFontRendering",
              base::span(kFeatureListForGpuControlTestingEntry73),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsChromeOS,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry73),  // exceptions
          },
          {
              74,  // id
              "GpuControlListEntryTest.SubpixelFontRenderingDontCare",
              base::span(kFeatureListForGpuControlTestingEntry74),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsChromeOS,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry74,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              75,  // id
              "GpuControlListEntryTest.IntelDriverVendorEntry",
              base::span(kFeatureListForGpuControlTestingEntry75),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry75,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              76,  // id
              "GpuControlListEntryTest.IntelDriverVersionEntry",
              base::span(kFeatureListForGpuControlTestingEntry76),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x8086,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry76,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              77,  // id
              "GpuControlListEntryTest.DeviceRevisionEntry",
              base::span(kFeatureListForGpuControlTestingEntry77),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x1002,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span(kExceptionsForEntry77),  // exceptions
          },
          {
              78,  // id
              "GpuControlListEntryTest.DeviceRevisionUnspecifiedEntry",
              base::span(kFeatureListForGpuControlTestingEntry78),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsWin,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},  // os_version
                  0x1002,     // vendor_id
                  base::span(kDevicesForGpuControlTestingEntry78),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,     // multi_gpu_style
                  nullptr,                                // driver info
                  nullptr,                                // GL strings
                  nullptr,                                // machine model info
                  nullptr,                                // Intel conditions
                  nullptr,                                // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              79,  // id
              "GpuControlListEntryTest.AnyDriverVersion",
              base::span(kFeatureListForGpuControlTestingEntry79),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry79,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              80,  // id
              "GpuControlListEntryTest.ActiveDriverVersion",
              base::span(kFeatureListForGpuControlTestingEntry80),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsMacosx,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x10de,                                      // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::
                      kMultiGpuCategoryActive,         // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,  // multi_gpu_style
                  &kDriverInfoForGpuControlTestingEntry80,  // driver info
                  nullptr,                                  // GL strings
                  nullptr,  // machine model info
                  nullptr,  // Intel conditions
                  nullptr,  // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
          {
              81,  // id
              "GpuControlListEntryTest.NativeAngleRenderer",
              base::span(kFeatureListForGpuControlTestingEntry81),  // features
              base::span<const char* const>(),  // DisabledExtensions
              base::span<const char* const>(),  // DisabledWebGLExtensions
              base::span<const uint32_t>(),     // CrBugs
              {
                  GpuControlList::kOsAny,  // os_type
                  {GpuControlList::kUnknown,
                   GpuControlList::kVersionStyleNumerical,
                   GpuControlList::kVersionSchemaCommon, nullptr,
                   nullptr},                                   // os_version
                  0x00,                                        // vendor_id
                  base::span<const GpuControlList::Device>(),  // Devices
                  GpuControlList::kMultiGpuCategoryNone,   // multi_gpu_category
                  GpuControlList::kMultiGpuStyleNone,      // multi_gpu_style
                  nullptr,                                 // driver info
                  &kGLStringsForGpuControlTestingEntry81,  // GL strings
                  nullptr,                                 // machine model info
                  nullptr,                                 // Intel conditions
                  nullptr,                                 // more conditions
              },
              base::span<const GpuControlList::Conditions>(),  // exceptions
          },
      }};
  return kGpuControlListTestingEntries;
}
}  // namespace gpu
