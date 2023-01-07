// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_
#define CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_

#include "ppapi/buildflags/buildflags.h"
#include "url/gurl.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace content {

// This struct contains data which is associated with a particular plugin
// instance and is related to the renderer in which the plugin instance lives.
// This data is transferred to the browser process from the renderer when the
// instance is created and is stored in the BrowserPpapiHost.
struct PepperRendererInstanceData {
  PepperRendererInstanceData();
  PepperRendererInstanceData(int render_process,
                             int render_frame_id,
                             const GURL& document,
                             const GURL& plugin,
                             bool secure);
  ~PepperRendererInstanceData();
  int render_process_id;
  int render_frame_id;
  GURL document_url;
  GURL plugin_url;
  // Whether the plugin context is secure. That is, it is served from a secure
  // origin and it is embedded within a hierarchy of secure frames. This value
  // comes from the renderer so should not be trusted. It is used for metrics.
  bool is_potentially_secure_plugin_context;
};

}  // namespace content

#endif  // CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_
