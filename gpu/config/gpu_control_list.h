// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_H_

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/values.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"

namespace gpu {
struct GpuControlListData;
struct GPUInfo;

class GPU_EXPORT GpuControlList {
 public:
  typedef std::unordered_map<int, std::string> FeatureMap;

  enum OsType {
    kOsLinux,
    kOsMacosx,
    kOsWin,
    kOsChromeOS,
    kOsAndroid,
    kOsFuchsia,
    kOsAny
  };

  enum OsFilter {
    // In loading, ignore all entries that belong to other OS.
    kCurrentOsOnly,
    // In loading, keep all entries. This is for testing only.
    kAllOs
  };

  enum NumericOp {
    kBetween,  // <= * <=
    kEQ,  // =
    kLT,  // <
    kLE,  // <=
    kGT,  // >
    kGE,  // >=
    kAny,
    kUnknown  // Indicates the data is invalid.
  };

  enum MultiGpuStyle {
    kMultiGpuStyleOptimus,
    kMultiGpuStyleAMDSwitchable,
    kMultiGpuStyleAMDSwitchableIntegrated,
    kMultiGpuStyleAMDSwitchableDiscrete,
    kMultiGpuStyleNone
  };

  enum MultiGpuCategory {
    // This entry applies if this is the primary GPU on the system.
    kMultiGpuCategoryPrimary,
    // This entry applies if this is a secondary GPU on the system.
    kMultiGpuCategorySecondary,
    // This entry applies if this is the active GPU on the system.
    kMultiGpuCategoryActive,
    // This entry applies if this is any of the GPUs on the system.
    kMultiGpuCategoryAny,
    kMultiGpuCategoryNone
  };

  enum GLType {
    kGLTypeGL,     // This is default on MacOSX, Linux, ChromeOS
    kGLTypeGLES,   // This is default on Android
    kGLTypeANGLE,  // This is default on Windows
    kGLTypeNone
  };

  enum VersionStyle {
    kVersionStyleNumerical,
    kVersionStyleLexical,
    kVersionStyleUnknown
  };

  enum SupportedOrNot {
    kSupported,
    kUnsupported,
    kDontCare,
  };

  struct GPU_EXPORT Version {
    NumericOp op;
    VersionStyle style;
    const char* value1;
    const char* value2;

    bool IsSpecified() const { return op != kUnknown; }

    bool Contains(const std::string& version_string, char splitter) const;

    bool Contains(const std::string& version_string) const {
      return Contains(version_string, '.');
    }

    // Compare two version strings.
    // Return 1 if version > version_ref,
    //        0 if version = version_ref,
    //       -1 if version < version_ref.
    // Note that we only compare as many segments as both versions contain.
    // For example: Compare("10.3.1", "10.3") returns 0,
    //              Compare("10.3", "10.3.1") returns 0.
    // If "version_style" is Lexical, the first segment is compared
    // numerically, all other segments are compared lexically.
    // Lexical is used for AMD Linux driver versions only.
    static int Compare(const std::vector<std::string>& version,
                       const std::vector<std::string>& version_ref,
                       VersionStyle version_style);
  };

  struct GPU_EXPORT DriverInfo {
    const char* driver_vendor;
    Version driver_version;

    bool Contains(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT GLStrings {
    const char* gl_vendor;
    const char* gl_renderer;
    const char* gl_extensions;
    const char* gl_version;

    bool Contains(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT MachineModelInfo {
    size_t machine_model_name_size;
    const char* const* machine_model_names;
    Version machine_model_version;

    bool Contains(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT More {
    // These are just part of Entry fields that are less common.
    // Putting them to a separate struct to save Entry data size.
    GLType gl_type;
    Version gl_version;
    Version pixel_shader_version;
    bool in_process_gpu;
    uint32_t gl_reset_notification_strategy;
    Version direct_rendering_version;
    Version gpu_count;
    SupportedOrNot hardware_overlay;

    uint32_t test_group;

    SupportedOrNot subpixel_font_rendering;

    // Return true if GL_VERSION string does not fit the entry info
    // on GL type and GL version.
    bool GLVersionInfoMismatch(const std::string& gl_version_string) const;

    bool Contains(const GPUInfo& gpu_info) const;

    // Return the default GL type, depending on the OS.
    // See GLType declaration.
    static GLType GetDefaultGLType();
  };

  struct GPU_EXPORT Conditions {
    OsType os_type;
    Version os_version;
    uint32_t vendor_id;
    size_t device_id_size;
    const uint32_t* device_ids;
    MultiGpuCategory multi_gpu_category;
    MultiGpuStyle multi_gpu_style;
    const DriverInfo* driver_info;
    const GLStrings* gl_strings;
    const MachineModelInfo* machine_model_info;
    size_t gpu_series_list_size;
    const GpuSeriesType* gpu_series_list;
    Version intel_gpu_generation;
    const More* more;

    bool Contains(OsType os_type,
                  const std::string& os_version,
                  const GPUInfo& gpu_info) const;

    // Determines whether we needs more gpu info to make the blacklisting
    // decision.  It should only be checked if Contains() returns true.
    bool NeedsMoreInfo(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT Entry {
    uint32_t id;
    const char* description;
    size_t feature_size;
    const int* features;
    size_t disabled_extension_size;
    const char* const* disabled_extensions;
    size_t disabled_webgl_extension_size;
    const char* const* disabled_webgl_extensions;
    size_t cr_bug_size;
    const uint32_t* cr_bugs;
    Conditions conditions;
    size_t exception_size;
    const Conditions* exceptions;

    bool Contains(OsType os_type,
                  const std::string& os_version,
                  const GPUInfo& gpu_info) const;

    bool AppliesToTestGroup(uint32_t target_test_group) const;

    // Determines whether we needs more gpu info to make the blacklisting
    // decision.  It should only be checked if Contains() returns true.
    bool NeedsMoreInfo(const GPUInfo& gpu_info, bool consider_exceptions) const;

    void GetFeatureNames(base::ListValue* feature_names,
                         const FeatureMap& feature_map) const;

    // Logs a control list match for this rule in the list identified by
    // |control_list_logging_name|.
    void LogControlListMatch(
        const std::string& control_list_logging_name) const;
  };

  explicit GpuControlList(const GpuControlListData& data);
  virtual ~GpuControlList();

  // Collects system information and combines them with gpu_info and control
  // list information to decide which entries are applied to the current
  // system and returns the union of features specified in each entry.
  // If os is kOsAny, use the current OS; if os_version is empty, use the
  // current OS version.
  std::set<int32_t> MakeDecision(OsType os,
                                 const std::string& os_version,
                                 const GPUInfo& gpu_info);
  // Same as the above function, but instead of using the entries with no
  // "test_group" specified or "test_group" = 0, using the entries with
  // "test_group" = |target_test_group|.
  std::set<int32_t> MakeDecision(OsType os,
                                 const std::string& os_version,
                                 const GPUInfo& gpu_info,
                                 uint32_t target_test_group);

  // Return the active entry indices from the last MakeDecision() call.
  const std::vector<uint32_t>& GetActiveEntries() const;
  // Return corresponding entry IDs from entry indices.
  std::vector<uint32_t> GetEntryIDsFromIndices(
      const std::vector<uint32_t>& entry_indices) const;

  // Collects all disabled extensions.
  std::vector<std::string> GetDisabledExtensions();
  // Collects all disabled WebGL extensions.
  std::vector<std::string> GetDisabledWebGLExtensions();

  // Returns the description and bugs from active entries provided.
  // Each problems has:
  // {
  //    "description": "Your GPU is too old",
  //    "crBugs": [1234],
  // }
  // The use case is we compute the entries from GPU process and send them to
  // browser process, and call GetReasons() in browser process.
  void GetReasons(base::ListValue* problem_list,
                  const std::string& tag,
                  const std::vector<uint32_t>& entries) const;

  // Return the largest entry id.  This is used for histogramming.
  uint32_t max_entry_id() const;

  // Check if we need more gpu info to make the decisions.
  // This is computed from the last MakeDecision() call.
  // If yes, we should create a gl context and do a full gpu info collection.
  bool needs_more_info() const { return needs_more_info_; }

  // Returns the number of entries.  This is only for tests.
  size_t num_entries() const;

  // Register a feature to FeatureMap.
  void AddSupportedFeature(const std::string& feature_name, int feature_id);

  // Enables logging of control list decisions.
  void EnableControlListLogging(const std::string& control_list_logging_name) {
    control_list_logging_enabled_ = true;
    control_list_logging_name_ = control_list_logging_name;
  }

 protected:
  // Return false if an entry index goes beyond |total_entries|.
  static bool AreEntryIndicesValid(const std::vector<uint32_t>& entry_indices,
                                   size_t total_entries);

 private:
  friend class GpuControlListEntryTest;
  friend class VersionInfoTest;

  // Gets the current OS type.
  static OsType GetOsType();

  size_t entry_count_;
  const Entry* entries_;
  // This records all the entries that are appliable to the current user
  // machine.  It is updated everytime MakeDecision() is called and is used
  // later by GetDecisionEntries().
  std::vector<uint32_t> active_entries_;

  uint32_t max_entry_id_;

  bool needs_more_info_;

  // The features a GpuControlList recognizes and handles.
  FeatureMap feature_map_;

  bool control_list_logging_enabled_;
  std::string control_list_logging_name_;
};

struct GPU_EXPORT GpuControlListData {
  size_t entry_count;
  const GpuControlList::Entry* entries;

  GpuControlListData() : entry_count(0u), entries(nullptr) {}

  GpuControlListData(size_t a_entry_count,
                     const GpuControlList::Entry* a_entries)
      : entry_count(a_entry_count), entries(a_entries) {}
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_H_
