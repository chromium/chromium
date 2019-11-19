// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_constants.h"

#include "build/branding_buildflags.h"

namespace content {

const base::FilePath::CharType kAppCacheDirname[] =
    FILE_PATH_LITERAL("Application Cache");
const base::FilePath::CharType kPepperDataDirname[] =
    FILE_PATH_LITERAL("Pepper Data");

const char kBrowserPluginMimeType[] = "application/browser-plugin";

const char kFlashPluginName[] = "Shockwave Flash";
const char kFlashPluginSwfMimeType[] = "application/x-shockwave-flash";
const char kFlashPluginSwfExtension[] = "swf";
const char kFlashPluginSwfDescription[] = "Shockwave Flash";
const char kFlashPluginSplMimeType[] = "application/futuresplash";
const char kFlashPluginSplExtension[] = "spl";
const char kFlashPluginSplDescription[] = "FutureSplash Player";

const size_t kMaxTitleChars = 4 * 1024;
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
const char kCorsExemptRequestedWithHeaderName[] = "X-Requested-With";

}  // namespace content
