// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_attributes.h"

#include <string>
#include <type_traits>
#include <vector>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/mf_initializer.h"
#include "remoting/host/win/evaluate_3d_display_mode.h"
#include "remoting/host/win/evaluate_d3d.h"
#endif

namespace remoting {

namespace {

static constexpr char kSeparator[] = ",";

struct Attribute {
  const char* name;
  bool (*get_value_func)();
};

inline constexpr bool IsDebug() {
#if defined(NDEBUG)
  return false;
#else
  return true;
#endif
}

inline constexpr bool IsChromeBranded() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#elif BUILDFLAG(CHROMIUM_BRANDING)
  return false;
#else
#error Only Chrome and Chromium brands are supported.
#endif
}

inline constexpr bool IsChromiumBranded() {
  return !IsChromeBranded();
}

inline constexpr bool IsOfficialBuild() {
#if defined(OFFICIAL_BUILD)
  return true;
#else
  return false;
#endif
}

inline constexpr bool IsNonOfficialBuild() {
  return !IsOfficialBuild();
}

// By using std::size() macro in base/macros.h, it's illegal to have empty
// arrays.
//
// error: no matching function for call to 'ArraySizeHelper'
// note: candidate template ignored: substitution failure
// [with T = const remoting::StaticAttribute, N = 0]:
// zero-length arrays are not permitted in C++.
//
// So we need IsDebug() function, and "Debug-Build" Attribute.

static constexpr Attribute kAttributes[] = {
    {"Debug-Build", &IsDebug},
    {"ChromeBrand", &IsChromeBranded},
    {"ChromiumBrand", &IsChromiumBranded},
    {"OfficialBuild", &IsOfficialBuild},
    {"NonOfficialBuild", &IsNonOfficialBuild},
};

}  // namespace

static_assert(std::is_pod<Attribute>::value, "Attribute should be POD.");

std::string GetHostAttributes() {
  std::vector<std::string> result;
  for (const auto& attribute : kAttributes) {
    DCHECK_EQ(std::string(attribute.name).find(kSeparator), std::string::npos);
    if (attribute.get_value_func()) {
      result.push_back(attribute.name);
    }
  }
#if BUILDFLAG(IS_WIN)
  GetD3DCapabilities(&result);
  result.push_back("Win10+");

  // TODO(crbug.com/40752360): Remove this and/or the entire HostAttributes
  // class so we can remove //remoting/host:common from //media/gpu's visibility
  // list.
  if (media::InitializeMediaFoundation()) {
    result.push_back("HWEncoder");
  }
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  result.push_back("HWEncoder");
#endif

  return base::JoinString(result, kSeparator);
}

}  // namespace remoting
