// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_control_list.h"

#include <utility>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "gpu/config/gpu_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace gpu {
namespace {

// Break a version string into segments.  Return true if each segment is
// a valid number, and not all segment is 0.
bool ProcessVersionString(const std::string& version_string,
                          char splitter,
                          std::vector<std::string>* version) {
  DCHECK(version);
  *version = base::SplitString(
      version_string, std::string(1, splitter),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (version->size() == 0)
    return false;
  // If the splitter is '-', we assume it's a date with format "mm-dd-yyyy";
  // we split it into the order of "yyyy", "mm", "dd".
  if (splitter == '-') {
    std::string year = version->back();
    for (size_t i = version->size() - 1; i > 0; --i) {
      (*version)[i] = (*version)[i - 1];
    }
    (*version)[0] = year;
  }
  bool all_zero = true;
  for (size_t i = 0; i < version->size(); ++i) {
    unsigned num = 0;
    if (!base::StringToUint((*version)[i], &num)) {
      version->resize(i);
      break;
    }
    if (num)
      all_zero = false;
  }
  return !all_zero;
}

// Compare two number strings using numerical ordering.
// Return  0 if number = number_ref,
//         1 if number > number_ref,
//        -1 if number < number_ref.
int CompareNumericalNumberStrings(
    const std::string& number, const std::string& number_ref) {
  unsigned value1 = 0;
  unsigned value2 = 0;
  bool valid = base::StringToUint(number, &value1);
  DCHECK(valid);
  valid = base::StringToUint(number_ref, &value2);
  DCHECK(valid);
  if (value1 == value2)
    return 0;
  if (value1 > value2)
    return 1;
  return -1;
}

// Compare two number strings using lexical ordering.
// Return  0 if number = number_ref,
//         1 if number > number_ref,
//        -1 if number < number_ref.
// We only compare as many digits as number_ref contains.
// If number_ref is xxx, it's considered as xxx*
// For example: CompareLexicalNumberStrings("121", "12") returns 0,
//              CompareLexicalNumberStrings("12", "121") returns -1.
int CompareLexicalNumberStrings(
    const std::string& number, const std::string& number_ref) {
  for (size_t i = 0; i < number_ref.length(); ++i) {
    unsigned value1 = 0;
    if (i < number.length())
      value1 = number[i] - '0';
    unsigned value2 = number_ref[i] - '0';
    if (value1 > value2)
      return 1;
    if (value1 < value2)
      return -1;
  }
  return 0;
}

// A mismatch is identified only if both |input| and |pattern| are not empty.
bool StringMismatch(const std::string& input, const std::string& pattern) {
  if (input.empty() || pattern.empty())
    return false;
  static crash_reporter::CrashKeyString<128> crash_key(
      "StringMismatch::pattern");
  crash_reporter::ScopedCrashKeyString scoped_crash_key(&crash_key, pattern);
  return !RE2::FullMatch(input, pattern);
}

bool StringMismatch(const std::string& input, const char* pattern) {
  if (!pattern)
    return false;
  std::string pattern_string(pattern);
  return StringMismatch(input, pattern_string);
}

bool ProcessANGLEGLRenderer(const std::string& gl_renderer,
                            std::string* vendor,
                            std::string* renderer,
                            std::string* version) {
  DCHECK(vendor);
  DCHECK(renderer);
  DCHECK(version);
  return RE2::FullMatch(gl_renderer, "ANGLE \\((.*), (.*), (.*)\\)", vendor,
                        renderer, version);
}

}  // namespace

bool GpuControlList::Version::Contains(const std::string& version_string,
                                       char splitter) const {
  if (op == kUnknown)
    return false;
  if (op == kAny)
    return true;
  std::vector<std::string> version;
  if (!ProcessVersionString(version_string, splitter, &version))
    return false;
  std::vector<std::string> ref_version1, ref_version2;
  bool valid = ProcessVersionString(value1, '.', &ref_version1);
  DCHECK(valid);
  if (op == kBetween) {
    valid = ProcessVersionString(value2, '.', &ref_version2);
    DCHECK(valid);
  }
  if (schema == kVersionSchemaIntelDriver) {
    // Intel graphics driver version schema should only be specified on Windows.
    // https://www.intel.com/content/www/us/en/support/articles/000005654/graphics-drivers.html
    // If either of the two versions doesn't match the Intel driver version
    // schema, they should not be compared.
    if (version.size() != 4 || ref_version1.size() != 4)
      return false;
    if (op == kBetween && ref_version2.size() != 4) {
      return false;
    }
    for (size_t ii = 0; ii < 2; ++ii) {
      version.erase(version.begin());
      ref_version1.erase(ref_version1.begin());
      if (op == kBetween)
        ref_version2.erase(ref_version2.begin());
    }
  } else if (schema == kVersionSchemaNvidiaDriver) {
    // The driver version we get from the os is "XX.XX.XXXA.BBCC", while the
    // workaround is of the form "ABB.CC".  Drop the first two stanzas from the
    // detected version, erase all but the last character of the third, and move
    // "B" to the previous stanza.
    if (version.size() != 4)
      return false;
    // Remember that the detected version might not have leading zeros, so we
    // have to be a bit careful.  [2] is of the form "001A", where A > 0, so we
    // just care that there's at least one digit.  However, if there's less than
    // that, the splitter stops anyway on that stanza, and the check for four
    // stanzas will fail instead.
    version.erase(version.begin(), version.begin() + 2);
    version[0].erase(0, version[0].length() - 1);
    // The last stanza may be missing leading zeros, so handle them.
    if (version[1].length() < 3) {
      // Two or more removed leading zeros, so BB are both zero.
      version[0] += "00";
    } else if (version[1].length() < 4) {
      // One removed leading zero.  BB is 0[1-9].
      version[0] += "0" + version[1].substr(0, 1);
      version[1].erase(0, 1);
    } else {
      // No leading zeros.
      version[0] += version[1].substr(0, 2);
      version[1].erase(0, 2);
    }
  }
  int relation = Version::Compare(version, ref_version1, style);
  switch (op) {
    case kEQ:
      return (relation == 0);
    case kLT:
      return (relation < 0);
    case kLE:
      return (relation <= 0);
    case kGT:
      return (relation > 0);
    case kGE:
      return (relation >= 0);
    case kBetween:
      if (relation < 0)
        return false;
      return Version::Compare(version, ref_version2, style) <= 0;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// static
int GpuControlList::Version::Compare(
    const std::vector<std::string>& version,
    const std::vector<std::string>& version_ref,
    VersionStyle version_style) {
  DCHECK(version.size() > 0 && version_ref.size() > 0);
  DCHECK(version_style != kVersionStyleUnknown);
  for (size_t i = 0; i < version_ref.size(); ++i) {
    if (i >= version.size())
      return 0;
    int ret = 0;
    // We assume both versions are checked by ProcessVersionString().
    if (i > 0 && version_style == kVersionStyleLexical)
      ret = CompareLexicalNumberStrings(version[i], version_ref[i]);
    else
      ret = CompareNumericalNumberStrings(version[i], version_ref[i]);
    if (ret != 0)
      return ret;
  }
  return 0;
}

bool GpuControlList::More::GLVersionInfoMismatch(
    const std::string& gl_version_string) const {
  if (gl_version_string.empty())
    return false;
  if (!gl_version.IsSpecified() && gl_type == kGLTypeNone)
    return false;
  std::vector<std::string> segments = base::SplitString(
      gl_version_string, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::string number;
  GLType target_gl_type = kGLTypeNone;
  if (segments.size() > 2 &&
      segments[0] == "OpenGL" && segments[1] == "ES") {
    bool full_match = RE2::FullMatch(segments[2], "([\\d.]+).*", &number);
    DCHECK(full_match);

    target_gl_type = kGLTypeGLES;
    if (segments.size() > 3 &&
        base::StartsWith(segments[3], "(ANGLE",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      target_gl_type = kGLTypeANGLE;
    }
  } else {
    number = segments[0];
    target_gl_type = kGLTypeGL;
  }

  GLType entry_gl_type = gl_type;
  if (entry_gl_type == kGLTypeNone && gl_version.IsSpecified()) {
    entry_gl_type = GetDefaultGLType();
  }
  if (entry_gl_type != kGLTypeNone && entry_gl_type != target_gl_type) {
    return true;
  }
  if (gl_version.IsSpecified() && !gl_version.Contains(number)) {
    return true;
  }
  return false;
}

// static
GpuControlList::GLType GpuControlList::More::GetDefaultGLType() {
#if BUILDFLAG(IS_CHROMEOS)
  return kGLTypeGL;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_OPENBSD)
  return kGLTypeGL;
#elif BUILDFLAG(IS_MAC)
  return kGLTypeGL;
#elif BUILDFLAG(IS_WIN)
  return kGLTypeANGLE;
#elif BUILDFLAG(IS_ANDROID)
  return kGLTypeGLES;
#else
  return kGLTypeNone;
#endif
}

void GpuControlList::Entry::LogControlListMatch(
    const std::string& control_list_logging_name) const {
  static const char kControlListMatchMessage[] =
      "Control list match for rule #%u in %s.";
  VLOG(1) << base::StringPrintf(kControlListMatchMessage, id,
                                control_list_logging_name.c_str());
}

bool GpuControlList::DriverInfo::Contains(
    const std::vector<GPUInfo::GPUDevice>& gpus) const {
  for (auto& gpu : gpus) {
    if (StringMismatch(gpu.driver_vendor, driver_vendor))
      continue;

    if (driver_version.IsSpecified() && !gpu.driver_version.empty() &&
        !driver_version.Contains(gpu.driver_version)) {
      continue;
    }
    return true;
  }
  return false;
}

bool GpuControlList::GLStrings::Contains(const GPUInfo& gpu_info) const {
  if (StringMismatch(gpu_info.gl_extensions, gl_extensions)) {
    return false;
  }

  std::string vendor;
  std::string renderer;
  std::string version;
  bool is_angle_gl = ProcessANGLEGLRenderer(gpu_info.gl_renderer, &vendor,
                                            &renderer, &version);
  if (StringMismatch(is_angle_gl ? vendor : gpu_info.gl_vendor, gl_vendor)) {
    return false;
  }
  if (StringMismatch(is_angle_gl ? renderer : gpu_info.gl_renderer,
                     gl_renderer)) {
    return false;
  }
  if (StringMismatch(is_angle_gl ? version : gpu_info.gl_version, gl_version)) {
    return false;
  }
  return true;
}

bool GpuControlList::MachineModelInfo::Contains(const GPUInfo& gpu_info) const {
  if (machine_model_names.size() > 0) {
    if (gpu_info.machine_model_name.empty())
      return false;
    bool found_match = false;
    for (auto* const machine_model_name : machine_model_names) {
      if (RE2::FullMatch(gpu_info.machine_model_name, machine_model_name)) {
        found_match = true;
        break;
      }
    }
    if (!found_match)
      return false;
  }
  if (machine_model_version.IsSpecified() &&
      (gpu_info.machine_model_version.empty() ||
       !machine_model_version.Contains(gpu_info.machine_model_version))) {
    return false;
  }
  return true;
}

bool GpuControlList::More::Contains(const GPUInfo& gpu_info) const {
  std::string vendor, renderer, version;
  bool is_angle_gl = ProcessANGLEGLRenderer(gpu_info.gl_renderer, &vendor,
                                            &renderer, &version);
  if (GLVersionInfoMismatch(is_angle_gl ? version : gpu_info.gl_version)) {
    return false;
  }

  if (gl_reset_notification_strategy != 0 &&
      gl_reset_notification_strategy !=
          gpu_info.gl_reset_notification_strategy) {
    return false;
  }
  if (gpu_count.IsSpecified()) {
    if (!gpu_count.Contains(std::to_string(gpu_info.GpuCount()))) {
      return false;
    }
  }
  if (direct_rendering_version.IsSpecified() &&
      !direct_rendering_version.Contains(gpu_info.direct_rendering_version)) {
    return false;
  }
  if (in_process_gpu && !gpu_info.in_process_gpu) {
    return false;
  }
  if (pixel_shader_version.IsSpecified() &&
      !pixel_shader_version.Contains(gpu_info.pixel_shader_version)) {
    return false;
  }
  switch (hardware_overlay) {
    case kDontCare:
      break;
    case kSupported:
#if BUILDFLAG(IS_WIN)
      if (!gpu_info.overlay_info.supports_overlays)
        return false;
#endif  // BUILDFLAG(IS_WIN)
      break;
    case kUnsupported:
#if BUILDFLAG(IS_WIN)
      if (gpu_info.overlay_info.supports_overlays)
        return false;
#endif  // BUILDFLAG(IS_WIN)
      break;
  }
  if ((subpixel_font_rendering == kUnsupported &&
       gpu_info.subpixel_font_rendering) ||
      (subpixel_font_rendering == kSupported &&
       !gpu_info.subpixel_font_rendering)) {
    return false;
  }
  return true;
}

bool GpuControlList::IntelConditions::Contains(
    const std::vector<GPUInfo::GPUDevice>& candidates,
    const GPUInfo& gpu_info) const {
  if (intel_gpu_series_list.size() > 0) {
    DCHECK(!intel_gpu_generation.IsSpecified());
    for (auto& candidate : candidates) {
      IntelGpuSeriesType candidate_series =
          GetIntelGpuSeriesType(candidate.vendor_id, candidate.device_id);
      if (candidate_series == IntelGpuSeriesType::kUnknown) {
        continue;
      }
      for (auto intel_gpu_series : intel_gpu_series_list) {
        if (candidate_series == intel_gpu_series) {
          return true;
        }
      }
    }
  } else {
    DCHECK(intel_gpu_generation.IsSpecified());
    for (auto& candidate : candidates) {
      std::string candidate_generation =
          GetIntelGpuGeneration(candidate.vendor_id, candidate.device_id);
      if (candidate_generation.empty()) {
        continue;
      }
      if (intel_gpu_generation.Contains(candidate_generation)) {
        return true;
      }
    }
  }
  return false;
}

GpuControlList::Conditions::Conditions(
    OsType os_type,
    Version os_version,
    uint32_t vendor_id,
    base::span<const Device> devices,
    MultiGpuCategory multi_gpu_category,
    MultiGpuStyle multi_gpu_style,
    const DriverInfo* driver_info,
    const GLStrings* gl_strings,
    const MachineModelInfo* machine_model_info,
    const IntelConditions* intel_conditions,
    const More* more)
    : os_type(os_type),
      os_version(os_version),
      vendor_id(vendor_id),
      devices(devices),
      multi_gpu_category(multi_gpu_category),
      multi_gpu_style(multi_gpu_style),
      driver_info(driver_info),
      gl_strings(gl_strings),
      machine_model_info(machine_model_info),
      intel_conditions(intel_conditions),
      more(more) {}

GpuControlList::Conditions::Conditions(const Conditions& other) = default;

bool GpuControlList::Conditions::Contains(OsType target_os_type,
                                          const std::string& target_os_version,
                                          const GPUInfo& gpu_info) const {
  DCHECK(target_os_type != kOsAny);
  if (os_type != kOsAny) {
    if (os_type != target_os_type)
      return false;
    if (os_version.IsSpecified() && !os_version.Contains(target_os_version))
      return false;
  }

  std::vector<GPUInfo::GPUDevice> candidates;
  switch (multi_gpu_category) {
    case kMultiGpuCategoryPrimary:
      candidates.push_back(gpu_info.gpu);
      break;
    case kMultiGpuCategorySecondary:
      candidates = gpu_info.secondary_gpus;
      break;
    case kMultiGpuCategoryNpu:
      candidates = gpu_info.npus;
      break;
    case kMultiGpuCategoryAny:
      candidates = gpu_info.secondary_gpus;
      candidates.push_back(gpu_info.gpu);
      break;
    case kMultiGpuCategoryActive:
    case kMultiGpuCategoryNone:
      // If gpu category is not specified, default to the active gpu.
      if (gpu_info.gpu.active || gpu_info.secondary_gpus.empty())
        candidates.push_back(gpu_info.gpu);
      for (auto& gpu : gpu_info.secondary_gpus) {
        if (gpu.active)
          candidates.push_back(gpu);
      }
      if (candidates.empty())
        candidates.push_back(gpu_info.gpu);
  }

  if (vendor_id != 0 || intel_conditions) {
    bool found = false;
    if (intel_conditions) {
      found = intel_conditions->Contains(candidates, gpu_info);
    } else {
      if (devices.size() == 0) {
        for (auto& candidate : candidates) {
          if (vendor_id == candidate.vendor_id) {
            found = true;
            break;
          }
        }
      } else {
        for (size_t ii = 0; !found && ii < devices.size(); ++ii) {
          uint32_t device_id = devices[ii].device_id;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
          uint32_t revision = devices[ii].revision;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
          for (auto& candidate : candidates) {
            if (vendor_id != candidate.vendor_id ||
                device_id != candidate.device_id)
              continue;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
            if (revision && revision != candidate.revision)
              continue;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
            found = true;
            break;
          }
        }
      }
    }
    if (!found)
      return false;
  }
  switch (multi_gpu_style) {
    case kMultiGpuStyleOptimus:
      if (!gpu_info.optimus)
        return false;
      break;
    case kMultiGpuStyleAMDSwitchable:
      if (!gpu_info.amd_switchable)
        return false;
      break;
    case kMultiGpuStyleAMDSwitchableDiscrete:
      if (!gpu_info.amd_switchable)
        return false;
      // The discrete GPU is always the primary GPU.
      // This is guaranteed by GpuInfoCollector.
      if (!gpu_info.gpu.active)
        return false;
      break;
    case kMultiGpuStyleAMDSwitchableIntegrated:
      if (!gpu_info.amd_switchable)
        return false;
      // Assume the integrated GPU is the first in the secondary GPU list.
      if (gpu_info.secondary_gpus.size() == 0 ||
          !gpu_info.secondary_gpus[0].active)
        return false;
      break;
    case kMultiGpuStyleNone:
      break;
  }

  if (driver_info) {
    // We don't have a reliable way to check driver version without
    // also checking for vendor.
    DCHECK(vendor_id != 0 || candidates.size() < 2);

    // Remove candidate GPUs made by different vendors.
    auto behind_last =
        std::remove_if(candidates.begin(), candidates.end(),
                       [vid = vendor_id](const GPUInfo::GPUDevice& gpu) {
                         return (vid && vid != gpu.vendor_id);
                       });
    candidates.erase(behind_last, candidates.end());

    if (!driver_info->Contains(candidates))
      return false;
  }
  if (gl_strings && !gl_strings->Contains(gpu_info)) {
    return false;
  }
  if (machine_model_info && !machine_model_info->Contains(gpu_info)) {
    return false;
  }
  if (more && !more->Contains(gpu_info)) {
    return false;
  }
  return true;
}

bool GpuControlList::Entry::Contains(OsType target_os_type,
                                     const std::string& target_os_version,
                                     const GPUInfo& gpu_info) const {
  static crash_reporter::CrashKeyString<8> crash_key(
      "GpuControlList::Entry::id");
  crash_reporter::ScopedCrashKeyString scoped_crash_key(
      &crash_key, base::StringPrintf("%d", id));
  if (!conditions.Contains(target_os_type, target_os_version, gpu_info)) {
    return false;
  }
  for (const auto& exception : exceptions) {
    if (exception.Contains(target_os_type, target_os_version, gpu_info) &&
        !exception.NeedsMoreInfo(gpu_info)) {
      return false;
    }
  }
  return true;
}

bool GpuControlList::Entry::AppliesToTestGroup(
    uint32_t target_test_group) const {
  // If an entry specifies non-zero test group, then the entry only applies
  // if that test group is enabled (as specified in |target_test_group|).
  if (conditions.more && conditions.more->test_group)
    return conditions.more->test_group == target_test_group;
  return true;
}

bool GpuControlList::Conditions::NeedsMoreInfo(const GPUInfo& gpu_info) const {
  // We only check for missing info that might be collected with a gl context.
  // If certain info is missing due to some error, say, we fail to collect
  // vendor_id/device_id, then even if we launch GPU process and create a gl
  // context, we won't gather such missing info, so we still return false.
  const GPUInfo::GPUDevice& active_gpu = gpu_info.active_gpu();
  if (driver_info) {
    if (driver_info->driver_vendor && active_gpu.driver_vendor.empty()) {
      return true;
    }
    if (driver_info->driver_version.IsSpecified() &&
        active_gpu.driver_version.empty()) {
      return true;
    }
  }
  if (((more && more->gl_version.IsSpecified()) ||
       (gl_strings && gl_strings->gl_version)) &&
      gpu_info.gl_version.empty()) {
    return true;
  }
  if (gl_strings && gl_strings->gl_vendor && gpu_info.gl_vendor.empty()) {
    return true;
  }
  if (gl_strings && gl_strings->gl_renderer && gpu_info.gl_renderer.empty()) {
    return true;
  }
  if (more && more->pixel_shader_version.IsSpecified() &&
      gpu_info.pixel_shader_version.empty()) {
    return true;
  }
  return false;
}

bool GpuControlList::Entry::NeedsMoreInfo(const GPUInfo& gpu_info,
                                          bool consider_exceptions) const {
  if (conditions.NeedsMoreInfo(gpu_info))
    return true;
  if (consider_exceptions) {
    for (const auto& exception : exceptions) {
      if (exception.NeedsMoreInfo(gpu_info)) {
        return true;
      }
    }
  }
  return false;
}

base::Value::List GpuControlList::Entry::GetFeatureNames(
    const FeatureMap& feature_map) const {
  base::Value::List feature_names;
  for (auto feature : features) {
    auto iter = feature_map.find(feature);
    CHECK(iter != feature_map.end(), base::NotFatalUntil::M130);
    feature_names.Append(iter->second);
  }
  for (auto* const extension : disabled_extensions) {
    std::string name = base::StringPrintf("disable(%s)", extension);
    feature_names.Append(name);
  }
  return feature_names;
}

GpuControlList::GpuControlList(base::span<const Entry> data) : entries_(data) {
  DCHECK(!entries_.empty());
  // Assume the newly last added entry has the largest ID.
  max_entry_id_ = entries_.back().id;
}

GpuControlList::~GpuControlList() = default;

std::set<int32_t> GpuControlList::MakeDecision(GpuControlList::OsType os,
                                               const std::string& os_version,
                                               const GPUInfo& gpu_info) {
  return MakeDecision(os, os_version, gpu_info, 0);
}

std::set<int32_t> GpuControlList::MakeDecision(GpuControlList::OsType os,
                                               const std::string& os_version,
                                               const GPUInfo& gpu_info,
                                               uint32_t target_test_group) {
  active_entries_.clear();
  std::set<int> features;

  needs_more_info_ = false;
  // Has all features permanently in the list without any possibility of
  // removal in the future (subset of "features" set).
  std::set<int32_t> permanent_features;
  // Has all features absent from "features" set that could potentially be
  // included later with more information.
  std::set<int32_t> potential_features;

  if (os == kOsAny)
    os = GetOsType();
  std::string processed_os_version = os_version;
  if (processed_os_version.empty())
    processed_os_version = base::SysInfo::OperatingSystemVersion();
  // Get rid of the non numbers because later processing expects a valid
  // version string in the format of "a.b.c".
  size_t pos = processed_os_version.find_first_not_of("0123456789.");
  if (pos != std::string::npos)
    processed_os_version = processed_os_version.substr(0, pos);

  for (size_t ii = 0; ii < entries_.size(); ++ii) {
    const Entry& entry = entries_[ii];
    DCHECK_NE(0u, entry.id);
    if (!entry.AppliesToTestGroup(target_test_group))
      continue;
    if (entry.Contains(os, processed_os_version, gpu_info)) {
      bool needs_more_info_main = entry.NeedsMoreInfo(gpu_info, false);
      bool needs_more_info_exception = entry.NeedsMoreInfo(gpu_info, true);

      if (control_list_logging_enabled_)
        entry.LogControlListMatch(control_list_logging_name_);
      // Only look at main entry info when deciding what to add to "features"
      // set. If we don't have enough info for an exception, it's safer if we
      // just ignore the exception and assume the exception doesn't apply.
      for (auto feature : entry.features) {
        if (needs_more_info_main) {
          if (!features.count(feature))
            potential_features.insert(feature);
        } else {
          features.insert(feature);
          potential_features.erase(feature);
          if (!needs_more_info_exception)
            permanent_features.insert(feature);
        }
      }

      if (!needs_more_info_main)
        active_entries_.push_back(base::checked_cast<uint32_t>(ii));
    }
  }

  needs_more_info_ = permanent_features.size() < features.size() ||
                     !potential_features.empty();
  return features;
}

const std::vector<uint32_t>& GpuControlList::GetActiveEntries() const {
  return active_entries_;
}

std::vector<uint32_t> GpuControlList::GetEntryIDsFromIndices(
    const std::vector<uint32_t>& entry_indices) const {
  std::vector<uint32_t> ids;
  for (auto index : entry_indices) {
    ids.push_back(entries_[index].id);
  }
  return ids;
}

std::vector<std::string> GpuControlList::GetDisabledExtensions() {
  std::set<std::string> disabled_extensions;
  for (auto index : active_entries_) {
    const Entry& entry = entries_[index];
    for (auto* const extension : entry.disabled_extensions) {
      disabled_extensions.insert(extension);
    }
  }
  return std::vector<std::string>(disabled_extensions.begin(),
                                  disabled_extensions.end());
}

std::vector<std::string> GpuControlList::GetDisabledWebGLExtensions() {
  std::set<std::string> disabled_webgl_extensions;
  for (auto index : active_entries_) {
    const Entry& entry = entries_[index];
    for (auto* const extension : entry.disabled_webgl_extensions) {
      disabled_webgl_extensions.insert(extension);
    }
  }
  return std::vector<std::string>(disabled_webgl_extensions.begin(),
                                  disabled_webgl_extensions.end());
}

void GpuControlList::GetReasons(base::Value::List& problem_list,
                                const std::string& tag,
                                const std::vector<uint32_t>& entries) const {
  for (auto index : entries) {
    const Entry& entry = entries_[index];
    base::Value::Dict problem;

    problem.Set("description", entry.description);

    base::Value::List cr_bugs;
    for (auto cr_bug : entry.cr_bugs) {
      cr_bugs.Append(base::Int64ToValue(static_cast<int64_t>(cr_bug)));
    }
    problem.Set("crBugs", std::move(cr_bugs));

    base::Value::List features = entry.GetFeatureNames(feature_map_);
    problem.Set("affectedGpuSettings", std::move(features));

    DCHECK(tag == "workarounds" || tag == "disabledFeatures");
    problem.Set("tag", tag);

    problem_list.Append(std::move(problem));
  }
}

size_t GpuControlList::num_entries() const {
  return entries_.size();
}

uint32_t GpuControlList::max_entry_id() const {
  return max_entry_id_;
}

// static
GpuControlList::OsType GpuControlList::GetOsType() {
#if BUILDFLAG(IS_CHROMEOS)
  return kOsChromeOS;
#elif BUILDFLAG(IS_WIN)
  return kOsWin;
#elif BUILDFLAG(IS_ANDROID)
  return kOsAndroid;
#elif BUILDFLAG(IS_FUCHSIA)
  return kOsFuchsia;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_OPENBSD)
  return kOsLinux;
#elif BUILDFLAG(IS_MAC)
  return kOsMacosx;
#elif BUILDFLAG(IS_IOS)
  return kOsIOS;
#else
  return kOsAny;
#endif
}

void GpuControlList::AddSupportedFeature(
    const std::string& feature_name, int feature_id) {
  feature_map_[feature_id] = feature_name;
}

// static
bool GpuControlList::AreEntryIndicesValid(
    const std::vector<uint32_t>& entry_indices,
    size_t total_entries) {
  for (auto index : entry_indices) {
    if (index >= total_entries)
      return false;
  }
  return true;
}

}  // namespace gpu
