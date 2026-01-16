// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_HISTOGRAMS_EXTENSION_H_
#define CONTENT_RENDERER_WEB_UI_HISTOGRAMS_EXTENSION_H_

#include "v8/include/v8-forward.h"

namespace content {

// Installs UMA histograms recording functions on the `chrome.histograms`
// object. For a list of supported functions, see
// //tools/typescript/definitions/chrome_histograms.d.ts.
void InstallWebUIHistogramsExtension(v8::Isolate* isolate,
                                     v8::Local<v8::Context> context,
                                     v8::Local<v8::Object> chrome);

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_HISTOGRAMS_EXTENSION_H_
