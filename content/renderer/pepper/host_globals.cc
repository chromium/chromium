// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_globals.h"

#include <limits>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using ppapi::CheckIdType;
using ppapi::MakeTypedId;
using ppapi::PPIdType;
using ppapi::ResourceTracker;
using blink::WebConsoleMessage;
using blink::WebLocalFrame;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

namespace {

typedef std::set<WebPluginContainer*> ContainerSet;

// Adds all WebPluginContainers associated with the given module to the set.
void GetAllContainersForModule(PluginModule* module, ContainerSet* containers) {
  const PluginModule::PluginInstanceSet& instances = module->GetAllInstances();
  for (auto i = instances.begin(); i != instances.end(); ++i) {
    WebPluginContainer* container = (*i)->container();
    // If "Delete" is called on an instance, the instance sets its container to
    // NULL, but the instance may actually outlive its container. Callers of
    // GetAllContainersForModule only want to know about valid containers.
    if (container)
      containers->insert(container);
  }
}

blink::mojom::ConsoleMessageLevel LogLevelToWebLogLevel(PP_LogLevel level) {
  switch (level) {
    case PP_LOGLEVEL_TIP:
      return blink::mojom::ConsoleMessageLevel::kVerbose;
    case PP_LOGLEVEL_LOG:
      return blink::mojom::ConsoleMessageLevel::kInfo;
    case PP_LOGLEVEL_WARNING:
      return blink::mojom::ConsoleMessageLevel::kWarning;
    case PP_LOGLEVEL_ERROR:
    default:
      return blink::mojom::ConsoleMessageLevel::kError;
  }
}

WebConsoleMessage MakeLogMessage(PP_LogLevel level,
                                 const std::string& source,
                                 const std::string& message) {
  std::string result = source;
  if (!result.empty())
    result.append(": ");
  result.append(message);
  return WebConsoleMessage(LogLevelToWebLogLevel(level),
                           WebString::FromUTF8(result));
}

}  // namespace

HostGlobals* HostGlobals::host_globals_ = nullptr;

HostGlobals::HostGlobals()
    : ppapi::PpapiGlobals(),
      resource_tracker_(ResourceTracker::SINGLE_THREADED) {
  DCHECK(!host_globals_);
  host_globals_ = this;
  // We do not support calls off of the main thread on the host side, and thus
  // do not lock.
  ppapi::ProxyLock::DisableLocking();
}

HostGlobals::~HostGlobals() {
  DCHECK(host_globals_ == this || !host_globals_);
  host_globals_ = nullptr;
}

ppapi::ResourceTracker* HostGlobals::GetResourceTracker() {
  return &resource_tracker_;
}

ppapi::VarTracker* HostGlobals::GetVarTracker() { return &host_var_tracker_; }

ppapi::CallbackTracker* HostGlobals::GetCallbackTrackerForInstance(
    PP_Instance instance) {
  auto found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return nullptr;
  return found->second->module()->GetCallbackTracker().get();
}

ppapi::thunk::PPB_Instance_API* HostGlobals::GetInstanceAPI(
    PP_Instance instance) {
  // The InstanceAPI is just implemented by the PluginInstance object.
  return GetInstance(instance);
}

ppapi::thunk::ResourceCreationAPI* HostGlobals::GetResourceCreationAPI(
    PP_Instance pp_instance) {
  PepperPluginInstanceImpl* instance = GetInstance(pp_instance);
  if (!instance)
    return nullptr;
  return &instance->resource_creation();
}

PP_Module HostGlobals::GetModuleForInstance(PP_Instance instance) {
  PepperPluginInstanceImpl* inst = GetInstance(instance);
  if (!inst)
    return 0;
  return inst->module()->pp_module();
}

void HostGlobals::LogWithSource(PP_Instance instance,
                                PP_LogLevel level,
                                const std::string& source,
                                const std::string& value) {
  PepperPluginInstanceImpl* instance_object =
      HostGlobals::Get()->GetInstance(instance);
  // It's possible to process this message in a nested run loop while
  // detaching a Document…
  // TODO(dcheng): Make it so this can't happen. https://crbug.com/561683
  if (instance_object &&
      instance_object->container()->GetDocument().GetFrame()) {
    instance_object->container()->GetDocument().GetFrame()->AddMessageToConsole(
        MakeLogMessage(level, source, value));
  } else {
    BroadcastLogWithSource(0, level, source, value);
  }
}

void HostGlobals::BroadcastLogWithSource(PP_Module pp_module,
                                         PP_LogLevel level,
                                         const std::string& source,
                                         const std::string& value) {
  // Get the unique containers associated with the broadcast. This prevents us
  // from sending the same message to the same console when there are two
  // instances on the page.
  ContainerSet containers;
  PluginModule* module = GetModule(pp_module);
  if (module) {
    GetAllContainersForModule(module, &containers);
  } else {
    // Unknown module, get containers for all modules.
    for (ModuleMap::const_iterator i = module_map_.begin();
         i != module_map_.end();
         ++i) {
      GetAllContainersForModule(i->second, &containers);
    }
  }

  WebConsoleMessage message = MakeLogMessage(level, source, value);
  for (auto i = containers.begin(); i != containers.end(); ++i) {
    WebLocalFrame* frame = (*i)->GetDocument().GetFrame();
    if (frame)
      frame->AddMessageToConsole(message);
  }
}

base::TaskRunner* HostGlobals::GetFileTaskRunner() {
  if (!file_task_runner_)
    file_task_runner_ = base::ThreadPool::CreateTaskRunner({base::MayBlock()});
  return file_task_runner_.get();
}

ppapi::MessageLoopShared* HostGlobals::GetCurrentMessageLoop() {
  return nullptr;
}

PP_Module HostGlobals::AddModule(PluginModule* module) {
#ifndef NDEBUG
  // Make sure we're not adding one more than once.
  for (ModuleMap::const_iterator i = module_map_.begin();
       i != module_map_.end();
       ++i)
    DCHECK(i->second != module);
#endif

  // See AddInstance.
  PP_Module new_module;
  do {
    new_module = MakeTypedId(static_cast<PP_Module>(base::RandUint64()),
                             ppapi::PP_ID_TYPE_MODULE);
  } while (!new_module || module_map_.find(new_module) != module_map_.end());
  module_map_[new_module] = module;
  return new_module;
}

void HostGlobals::ModuleDeleted(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, ppapi::PP_ID_TYPE_MODULE))
      << module << " is not a PP_Module.";
  auto found = module_map_.find(module);
  if (found == module_map_.end()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  module_map_.erase(found);
}

PluginModule* HostGlobals::GetModule(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, ppapi::PP_ID_TYPE_MODULE))
      << module << " is not a PP_Module.";
  auto found = module_map_.find(module);
  if (found == module_map_.end())
    return nullptr;
  return found->second;
}

PP_Instance HostGlobals::AddInstance(PepperPluginInstanceImpl* instance) {
  DCHECK(instance_map_.find(instance->pp_instance()) == instance_map_.end());

  // Use a random number for the instance ID. This helps prevent some
  // accidents. See also AddModule below.
  //
  // Need to make sure the random number isn't a duplicate or 0.
  PP_Instance new_instance;
  do {
    new_instance = MakeTypedId(static_cast<PP_Instance>(base::RandUint64()),
                               ppapi::PP_ID_TYPE_INSTANCE);
  } while (!new_instance ||
           instance_map_.find(new_instance) != instance_map_.end() ||
           !instance->module()->ReserveInstanceID(new_instance));

  instance_map_[new_instance] = instance;

  resource_tracker_.DidCreateInstance(new_instance);
  return new_instance;
}

void HostGlobals::InstanceDeleted(PP_Instance instance) {
  resource_tracker_.DidDeleteInstance(instance);
  host_var_tracker_.DidDeleteInstance(instance);
  instance_map_.erase(instance);
}

void HostGlobals::InstanceCrashed(PP_Instance instance) {
  resource_tracker_.DidDeleteInstance(instance);
  host_var_tracker_.DidDeleteInstance(instance);
}

PepperPluginInstanceImpl* HostGlobals::GetInstance(PP_Instance instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, ppapi::PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  auto found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return nullptr;
  return found->second;
}

bool HostGlobals::IsHostGlobals() const { return true; }

}  // namespace content
