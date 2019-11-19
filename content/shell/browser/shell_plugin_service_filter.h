// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_
#define CONTENT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/plugin_service_filter.h"

namespace content {

class ShellPluginServiceFilter : public PluginServiceFilter {
 public:
  ShellPluginServiceFilter();
  ~ShellPluginServiceFilter() override;

  // PluginServiceFilter implementation.
  bool IsPluginAvailable(int render_process_id,
                         int render_frame_id,
                         const GURL& url,
                         const url::Origin& main_frame_origin,
                         WebPluginInfo* plugin) override;

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override;

 private:

  DISALLOW_COPY_AND_ASSIGN(ShellPluginServiceFilter);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_PLUGIN_SERVICE_FILTER_H_
