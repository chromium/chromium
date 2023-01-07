// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "ipc/ipc_logging.h"
#include "ppapi/nacl_irt/irt_interfaces.h"
#include "ppapi/nacl_irt/irt_ppapi.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/nacl_irt/ppapi_dispatcher.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

static struct PP_StartFunctions g_pp_functions;

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* thread_functions) {
  // Initialize all classes that need to create threads that call back into
  // user code.
  ppapi::PPB_Audio_Shared::SetThreadFunctions(thread_functions);
}

int irt_ppapi_start(const struct PP_StartFunctions* funcs) {
  g_pp_functions = *funcs;

  base::SingleThreadTaskExecutor executor;
  ppapi::proxy::PluginGlobals plugin_globals(
      ppapi::GetIOThread()->task_runner());

  ppapi::PpapiDispatcher ppapi_dispatcher(
      ppapi::GetIOThread()->task_runner(), ppapi::GetShutdownEvent(),
      ppapi::GetBrowserIPCChannelHandle(),
      ppapi::GetRendererIPCChannelHandle());
  plugin_globals.SetPluginProxyDelegate(&ppapi_dispatcher);

  base::RunLoop().Run();

  return 0;
}

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  return g_pp_functions.PPP_InitializeModule(module_id, get_browser_interface);
}

void PPP_ShutdownModule(void) {
  g_pp_functions.PPP_ShutdownModule();
}

const void* PPP_GetInterface(const char* interface_name) {
  return g_pp_functions.PPP_GetInterface(interface_name);
}

const struct nacl_irt_ppapihook nacl_irt_ppapihook = {
  irt_ppapi_start,
  PpapiPluginRegisterThreadCreator,
};
