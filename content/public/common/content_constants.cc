// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_constants.h"

#include <vector>

#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"

namespace content {

const char kBrowserPluginMimeType[] = "application/browser-plugin";

const size_t kMaxURLDisplayChars = 32 * 1024;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kStatsFilename[] = "ChromeStats2";
#else
const char kStatsFilename[] = "ChromiumStats2";
#endif

const int kStatsMaxThreads = 32;
const int kStatsMaxCounters = 3000;

const int kHistogramSynchronizerReservedSequenceNumber = 0;

// TODO(jkarlin): The value is high to reduce the chance of the detachable
// request timing out, forcing a blocked second request to open a new connection
// and start over. Reduce this value once we have a better idea of what it
// should be and once we stop blocking multiple simultaneous requests for the
// same resource (see bugs 46104 and 31014).
const int kDefaultDetachableCancelDelayMs = 30000;

const char kCorsExemptPurposeHeaderName[] = "Purpose";

std::string GetCorsExemptRequestedWithHeaderName() {
  std::vector<std::string> pieces;
  pieces.push_back("X");
  pieces.push_back("Requested");
  pieces.push_back("With");
  return base::JoinString(pieces, "-");
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const int kLowestRendererOomScore = 300;
const int kHighestRendererOomScore = 1000;

// The minimum amount to bump a score by.  This is large enough that
// even if it's translated into the old values, it will still go up
// by at least one.
static const int kOomScoreBump = 100;

// Browsers and zygotes should still be killable, but killed last.
const int kZygoteOomScore = 0;
// For "miscellaneous" things, we want them after renderers, but before plugins.
const int kMiscOomScore = kLowestRendererOomScore - kOomScoreBump;
// We want plugins to die after the renderers.
const int kPluginOomScore = kMiscOomScore - kOomScoreBump;

static_assert(kMiscOomScore > 0, "kMiscOomScore should be greater than 0");
static_assert(kPluginOomScore > 0, "kPluginOomScore should be greater than 0");
#endif

#if BUILDFLAG(IS_ANDROID)
const int kAndroidMinimumTabletWidthDp = 600;
#endif

}  // namespace content
