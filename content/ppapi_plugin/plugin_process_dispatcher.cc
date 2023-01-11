// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/plugin_process_dispatcher.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/child/child_process.h"

namespace content {
namespace {

// How long we wait before releasing the plugin process.
const int kPluginReleaseTimeSeconds = 30;

}  // namespace

PluginProcessDispatcher::PluginProcessDispatcher(
    PP_GetInterface_Func get_interface,
    const ppapi::PpapiPermissions& permissions,
    bool incognito)
    : ppapi::proxy::PluginDispatcher(get_interface,
                                     permissions,
                                     incognito) {
}

PluginProcessDispatcher::~PluginProcessDispatcher() {
  // Don't free the process right away. This timer allows the child process
  // to be re-used if the user rapidly goes to a new page that requires this
  // plugin. This is the case for common plugins where they may be used on a
  // source and destination page of a navigation. We don't want to tear down
  // and re-start processes each time in these cases.
  process_ref_.ReleaseWithDelay(base::Seconds(kPluginReleaseTimeSeconds));
}

}  // namespace content
