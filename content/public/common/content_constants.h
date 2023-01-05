// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Content application.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_

#include <stddef.h>         // For size_t

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// The name of the directory under BrowserContext::GetPath where the AppCache is
// put.
CONTENT_EXPORT extern const base::FilePath::CharType kAppCacheDirname[];
// The name of the directory under BrowserContext::GetPath where Pepper plugin
// data is put.
CONTENT_EXPORT extern const base::FilePath::CharType kPepperDataDirname[];

// The MIME type used for the browser plugin.
CONTENT_EXPORT extern const char kBrowserPluginMimeType[];

// The maximum number of characters in the URL that we're willing to accept
// in the browser process. It is set low enough to avoid damage to the browser
// but high enough that a web site can abuse location.hash for a little storage.
// We have different values for "max accepted" and "max displayed" because
// a data: URI may be legitimately massive, but the full URI would kill all
// known operating systems if you dropped it into a UI control.
CONTENT_EXPORT extern const size_t kMaxURLChars;
CONTENT_EXPORT extern const size_t kMaxURLDisplayChars;

extern const char kStatsFilename[];
extern const int kStatsMaxThreads;
extern const int kStatsMaxCounters;

// Most sequence numbers are used by a renderer when responding to a browser
// request for histogram data.  This reserved number is used when a renderer
// sends an unprovoked update, such as after a page has been loaded.  Using
// this reserved constant avoids any chance of confusion with a response having
// a browser-supplied sequence number.
CONTENT_EXPORT extern const int kHistogramSynchronizerReservedSequenceNumber;

// How long to keep a detachable resource load alive before aborting it.
CONTENT_EXPORT extern const int kDefaultDetachableCancelDelayMs;

// Defines a HTTP header name that is set internally, and some code places
// in content need to know the name to manage the header stored in
// network::ResourceRequest::cors_exempt_headers.
CONTENT_EXPORT extern const char kCorsExemptPurposeHeaderName[];
// This should just be a constant string, but there is evidence of malware
// overwriting the value of the constant so try to confirm by constructing
// it at run time.
CONTENT_EXPORT std::string GetCorsExemptRequestedWithHeaderName();

// This is a value never returned as the unique id of any child processes of
// any kind, including the values returned by RenderProcessHost::GetID().
static constexpr int kInvalidChildProcessUniqueId = -1;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// The OOM score adj constants
// The highest and lowest assigned OOM score adjustment (oom_score_adj) for
// renderers and extensions used by the OomPriority Manager.
CONTENT_EXPORT extern const int kLowestRendererOomScore;
CONTENT_EXPORT extern const int kHighestRendererOomScore;

CONTENT_EXPORT extern const int kZygoteOomScore;
CONTENT_EXPORT extern const int kMiscOomScore;
CONTENT_EXPORT extern const int kPluginOomScore;

#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_CONSTANTS_H_
