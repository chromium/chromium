// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_renderer_instance_data.h"

namespace content {

PepperRendererInstanceData::PepperRendererInstanceData()
    : render_process_id(0),
      render_frame_id(0),
      is_potentially_secure_plugin_context(false) {
}

PepperRendererInstanceData::PepperRendererInstanceData(int render_process,
                                                       int render_frame,
                                                       const GURL& document,
                                                       const GURL& plugin,
                                                       bool secure)
    : render_process_id(render_process),
      render_frame_id(render_frame),
      document_url(document),
      plugin_url(plugin),
      is_potentially_secure_plugin_context(secure) {
}

PepperRendererInstanceData::~PepperRendererInstanceData() {
}

}  // namespace content
