// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_

#include "content/child/scoped_child_process_reference.h"
#include "ppapi/proxy/plugin_dispatcher.h"

namespace content {

// Wrapper around a PluginDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class PluginProcessDispatcher : public ppapi::proxy::PluginDispatcher {
 public:
  PluginProcessDispatcher(PP_GetInterface_Func get_interface,
                          const ppapi::PpapiPermissions& permissions,
                          bool incognito);

  PluginProcessDispatcher(const PluginProcessDispatcher&) = delete;
  PluginProcessDispatcher& operator=(const PluginProcessDispatcher&) = delete;

  ~PluginProcessDispatcher() override;

 private:
  ScopedChildProcessReference process_ref_;
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PLUGIN_PROCESS_DISPATCHER_H_
