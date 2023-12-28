// Copyright 2022 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: log/absl_vlog_is_on.h
// -----------------------------------------------------------------------------
//
// This header defines the `ABSL_VLOG_IS_ON()` macro that controls the
// variable-verbosity conditional logging.
//
// It's used by `VLOG` in log.h, or it can also be used directly like this:
//
//   if (ABSL_VLOG_IS_ON(2)) {
//     foo_server.RecomputeStatisticsExpensive();
//     LOG(INFO) << foo_server.LastStatisticsAsString();
//   }
//
// Each source file has an effective verbosity level that's a non-negative
// integer computed from the `--vmodule` and `--v` flags.
// `ABSL_VLOG_IS_ON(n)` is true, and `VLOG(n)` logs, if that effective verbosity
// level is greater than or equal to `n`.
//
// `--vmodule` takes a comma-delimited list of key=value pairs.  Each key is a
// pattern matched against filenames, and the values give the effective severity
// level applied to matching files.  '?' and '*' characters in patterns are
// interpreted as single-character and zero-or-more-character wildcards.
// Patterns including a slash character are matched against full pathnames,
// while those without are matched against basenames only.  One suffix (i.e. the
// last . and everything after it) is stripped from each filename prior to
// matching, as is the special suffix "-inl".
//
// Files are matched against globs in `--vmodule` in order, and the first match
// determines the verbosity level.
//
// Files which do not match any pattern in `--vmodule` use the value of `--v` as
// their effective verbosity level.  The default is 0.
//
// SetVLOGLevel helper function is provided to do limited dynamic control over
// V-logging by appending to `--vmodule`. Because these go at the beginning of
// the list, they take priority over any globs previously added.
//
// Resetting --vmodule will override all previous modifications to `--vmodule`,
// including via SetVLOGLevel.

#ifndef ABSL_LOG_ABSL_VLOG_IS_ON_H_
#define ABSL_LOG_ABSL_VLOG_IS_ON_H_

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/log/internal/vlog_config.h"  // IWYU pragma: export
#include "absl/strings/string_view.h"

// IWYU pragma: private, include "absl/log/log.h"

// This is expanded at the callsite to allow the compiler to optimize
// always-false cases out of the build.
// An ABSL_MAX_VLOG_VERBOSITY of 2 means that VLOG(3) and above should never
// log.
#ifdef ABSL_MAX_VLOG_VERBOSITY
#define ABSL_LOG_INTERNAL_MAX_LOG_VERBOSITY_CHECK(x) \
  ((x) <= ABSL_MAX_VLOG_VERBOSITY)&&
#else
#define ABSL_LOG_INTERNAL_MAX_LOG_VERBOSITY_CHECK(x)
#endif

// Each ABSL_VLOG_IS_ON call site gets its own VLogSite that registers with the
// global linked list of sites to asynchronously update its verbosity level on
// changes to --v or --vmodule. The verbosity can also be set by manually
// calling SetVLOGLevel.
//
// ABSL_VLOG_IS_ON is not async signal safe, but it is guaranteed not to
// allocate new memory.
#define ABSL_VLOG_IS_ON(verbose_level)                                     \
  (ABSL_LOG_INTERNAL_MAX_LOG_VERBOSITY_CHECK(verbose_level)[]()            \
       ->::absl::log_internal::VLogSite *                                  \
   {                                                                       \
     ABSL_CONST_INIT static ::absl::log_internal::VLogSite site(__FILE__); \
     return &site;                                                         \
   }()                                                                     \
       ->IsEnabled(verbose_level))

namespace absl {
ABSL_NAMESPACE_BEGIN

// Sets the global `(ABSL_)VLOG(_IS_ON)` level to `log_level`.  This level is
// applied to any sites whose filename doesn't match any `module_pattern`.
// Returns the prior value.
inline int SetGlobalVLogLevel(int log_level) {
  return absl::log_internal::UpdateGlobalVLogLevel(log_level);
}

// Sets `(ABSL_)VLOG(_IS_ON)` level for `module_pattern` to `log_level`.
// This lets us dynamically control what is normally set by the --vmodule flag.
// Returns the level that previously applied to module_pattern.
// Calling this with `log_level` of kUseFlag will have all sites for that
// pattern use the value of --v.
inline int SetVLogLevel(absl::string_view module_pattern, int log_level) {
  return absl::log_internal::PrependVModule(module_pattern, log_level);
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_LOG_ABSL_VLOG_IS_ON_H_
