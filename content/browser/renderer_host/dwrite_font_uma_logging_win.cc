// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace content {

void LogLoaderType(DirectWriteFontLoaderType loader_type) {
  UMA_HISTOGRAM_ENUMERATION("DirectWrite.Fonts.Proxy.LoaderType", loader_type);
}

void LogLastResortFontCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100("DirectWrite.Fonts.Proxy.LastResortFontCount",
                           count);
}

void LogLastResortFontFileCount(size_t count) {
  UMA_HISTOGRAM_COUNTS_100("DirectWrite.Fonts.Proxy.LastResortFontFileCount",
                           count);
}

void LogMessageFilterError(MessageFilterError error) {
  UMA_HISTOGRAM_ENUMERATION("DirectWrite.Fonts.Proxy.MessageFilterError",
                            error);
}

}  // namespace content
