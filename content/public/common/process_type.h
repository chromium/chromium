// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_
#define CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// Defines the different process types.
// NOTE: Do not remove or reorder the elements in this enum, and only add new
// items at the end, right before PROCESS_TYPE_MAX. We depend on these specific
// values in histograms.
enum ProcessType {
  PROCESS_TYPE_UNKNOWN = 1,
  PROCESS_TYPE_BROWSER,
  PROCESS_TYPE_RENDERER,
  PROCESS_TYPE_PLUGIN_DEPRECATED,
  PROCESS_TYPE_WORKER_DEPRECATED,
  PROCESS_TYPE_UTILITY,
  PROCESS_TYPE_ZYGOTE,
  PROCESS_TYPE_SANDBOX_HELPER,
  PROCESS_TYPE_GPU,
  PROCESS_TYPE_PPAPI_PLUGIN,
  PROCESS_TYPE_PPAPI_BROKER,
  // Custom process types used by the embedder should start from here.
  PROCESS_TYPE_CONTENT_END,
  // If any embedder has more than 10 custom process types, update this.
  // We can switch to getting it from ContentClient, but that seems like
  // overkill at this time.
  PROCESS_TYPE_MAX = PROCESS_TYPE_CONTENT_END + 10,
};

// Returns an English name of the process type, should only be used for non
// user-visible strings or debugging pages.
CONTENT_EXPORT std::string GetProcessTypeNameInEnglish(int type);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PROCESS_TYPE_H_
