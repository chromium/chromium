// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_H_

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_span.h"
#include "base/values.h"
#include "gpu/config/gpu_info.h"
#include "gpu/gpu_export.h"

namespace gpu {
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
    kOsIOS,
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
    // This entry applies if this is the NPU on the system.
    kMultiGpuCategoryNpu,
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

  enum VersionSchema {
    // All digits are meaningful when distinguishing versions.
    kVersionSchemaCommon,
    // The version format of Intel graphics driver is AA.BB.CCC.DDDD.
    // DDDD(old schema) or CCC.DDDD(new schema) is the build number.
    // That is, indicates the actual driver number.
    kVersionSchemaIntelDriver,
    // The version format of Nvidia drivers is XX.XX.XXXA.AAAA where the X's
    // can be any digits, and the A's are the actual version.  The workaround
    // list specifies them as AAA.AA to match how Nvidia publishes them.
    kVersionSchemaNvidiaDriver,
  };

  enum SupportedOrNot {
    kSupported,
    kUnsupported,
    kDontCare,
  };

  struct GPU_EXPORT Version {
    NumericOp op;
    VersionStyle style;
    VersionSchema schema;
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

    bool Contains(const std::vector<GPUInfo::GPUDevice>& gpus) const;
  };

  struct GPU_EXPORT GLStrings {
    const char* gl_vendor;
    const char* gl_renderer;
    const char* gl_extensions;
    const char* gl_version;

    bool Contains(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT MachineModelInfo {
    base::span<const char* const> machine_model_names;
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

  struct GPU_EXPORT Device {
    uint32_t device_id;
    uint32_t revision = 0u;
  };

  struct GPU_EXPORT IntelConditions {
    base::span<const IntelGpuSeriesType> intel_gpu_series_list;
    Version intel_gpu_generation;

    bool Contains(const std::vector<GPUInfo::GPUDevice>& candidates,
                  const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT Conditions {
    OsType os_type;
    Version os_version;
    uint32_t vendor_id;
    base::span<const Device> devices;
    MultiGpuCategory multi_gpu_category;
    MultiGpuStyle multi_gpu_style;
    // RAW_PTR_EXCLUSION: since these pointers only ever point to other
    // globals, and `Conditions` itself is used to construct globals, using
    // raw_ptr would add additional (unnecessary) complexity with
    // `NoDestructor`.
    RAW_PTR_EXCLUSION const DriverInfo* driver_info;
    RAW_PTR_EXCLUSION const GLStrings* gl_strings;
    RAW_PTR_EXCLUSION const MachineModelInfo* machine_model_info;
    RAW_PTR_EXCLUSION const IntelConditions* intel_conditions;
    RAW_PTR_EXCLUSION const More* more;

    Conditions(OsType os_type,
               Version os_version,
               uint32_t vendor_id,
               base::span<const Device> devices,
               MultiGpuCategory multi_gpu_category,
               MultiGpuStyle multi_gpu_style,
               const DriverInfo* driver_info,
               const GLStrings* gl_strings,
               const MachineModelInfo* machine_model_info,
               const IntelConditions* intel_conditions,
               const More* more);
    Conditions(const Conditions& other);

    bool Contains(OsType os_type,
                  const std::string& os_version,
                  const GPUInfo& gpu_info) const;

    // Determines whether we needs more gpu info to make the blocklisting
    // decision.  It should only be checked if Contains() returns true.
    bool NeedsMoreInfo(const GPUInfo& gpu_info) const;
  };

  struct GPU_EXPORT Entry {
    uint32_t id;
    const char* description;
    base::span<const int> features;
    base::span<const char* const> disabled_extensions;
    base::span<const char* const> disabled_webgl_extensions;
    base::span<const uint32_t> cr_bugs;
    Conditions conditions;
    base::span<const Conditions> exceptions;

    bool Contains(OsType os_type,
                  const std::string& os_version,
                  const GPUInfo& gpu_info) const;

    bool AppliesToTestGroup(uint32_t target_test_group) const;

    // Determines whether we needs more gpu info to make the blocklisting
    // decision.  It should only be checked if Contains() returns true.
    bool NeedsMoreInfo(const GPUInfo& gpu_info, bool consider_exceptions) const;

    base::Value::List GetFeatureNames(const FeatureMap& feature_map) const;

    // Logs a control list match for this rule in the list identified by
    // |control_list_logging_name|.
    void LogControlListMatch(
        const std::string& control_list_logging_name) const;
  };

  explicit GpuControlList(base::span<const GpuControlList::Entry> data);
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
  void GetReasons(base::Value::List& problem_list,
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

  // These always point to built-in arrays of constants, so raw_ptr doesn't
  // add any protection but costs some overhead.
  base::raw_span<const Entry> entries_;

  // This records all the entries that are applicable to the current user
  // machine.  It is updated everytime MakeDecision() is called and is used
  // later by GetDecisionEntries().
  std::vector<uint32_t> active_entries_;

  uint32_t max_entry_id_ = 0;

  bool needs_more_info_ = false;

  // The features a GpuControlList recognizes and handles.
  FeatureMap feature_map_;

  bool control_list_logging_enabled_ = false;
  std::string control_list_logging_name_;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_H_
