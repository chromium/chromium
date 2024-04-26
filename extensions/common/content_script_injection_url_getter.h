// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_
#define EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_

#include "extensions/common/frame_context_data.h"
#include "extensions/common/script_constants.h"
#include "url/gurl.h"

namespace extensions {

// A helper for deciding which URL to use for deciding whether to inject a
// content script - it finds the effective document URL by (depending on content
// script options) possibly looking at the parent-or-opener document instead,
// looking at the precursor origin of data: documents, etc.
//
// TODO(crbug.com/40753677): Content script injection assumes that
// about:blank inherits origin from the parent.  This can return the incorrect
// result, e.g.  if a parent frame navigates a grandchild frame to about:blank.
class ContentScriptInjectionUrlGetter {
 public:
  // Only static methods.
  ContentScriptInjectionUrlGetter() = delete;

  static GURL Get(const FrameContextData& context_data,
                  const GURL& document_url,
                  MatchOriginAsFallbackBehavior match_origin_as_fallback,
                  bool allow_inaccessible_parents);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CONTENT_SCRIPT_INJECTION_URL_GETTER_H_
