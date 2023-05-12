// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_TRACE_UTIL_H_
#define EXTENSIONS_RENDERER_TRACE_UTIL_H_

#include "base/trace_event/typed_macros.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/trace_util.h"

// Helper macro for adding renderer-side, extension-related trace events that
// capture 1) extension id (passed as an argument of the helper macro) and 2)
// render process id (implicitly retrieved under the covers via
// `content::RenderThread`).
#define TRACE_RENDERER_EXTENSION_EVENT(event_name, extension_id)              \
  TRACE_EVENT("extensions", event_name,                                       \
              perfetto::protos::pbzero::ChromeTrackEvent::kRenderProcessHost, \
              content::RenderThread::Get(),                                   \
              perfetto::protos::pbzero::ChromeTrackEvent::kChromeExtensionId, \
              ExtensionIdForTracing(extension_id))

#endif  // EXTENSIONS_RENDERER_TRACE_UTIL_H_
