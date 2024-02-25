// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_MODULE_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_MODULE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "base/process/process.h"
#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/content_plugin_info.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "url/origin.h"

typedef void* NPIdentifier;

class GURL;

namespace base {
class FilePath;
}

namespace ppapi {
class CallbackTracker;
}  // namespace ppapi

namespace IPC {
struct ChannelHandle;
}

namespace blink {
class WebPluginContainer;
}  // namespace blink

namespace content {
class HostDispatcherWrapper;
class PepperPluginInstanceImpl;
class RendererPpapiHostImpl;
class RenderFrameImpl;
struct WebPluginInfo;

// Represents one plugin library loaded into one renderer. This library may
// have multiple instances.
//
// Note: to get from a PP_Instance to a PepperPluginInstance*, use the
// ResourceTracker.
class CONTENT_EXPORT PluginModule : public base::RefCounted<PluginModule> {
 public:
  typedef std::set<raw_ptr<PepperPluginInstanceImpl, SetExperimental>>
      PluginInstanceSet;

  // You must call one of the Init functions after the constructor to create a
  // module of the type you desire.
  //
  // The module lifetime delegate is a non-owning pointer that must outlive
  // all plugin modules. In practice it will be a global singleton that
  // tracks which modules are alive.
  PluginModule(const std::string& name,
               const std::string& version,
               const base::FilePath& path,
               const ppapi::PpapiPermissions& perms);

  PluginModule(const PluginModule&) = delete;
  PluginModule& operator=(const PluginModule&) = delete;

  // Sets the given class as being associated with this module. It will be
  // deleted when the module is destroyed. You can only set it once, subsequent
  // sets will assert.
  void SetRendererPpapiHost(std::unique_ptr<RendererPpapiHostImpl> host);

  // Initializes this module as an internal plugin with the given entrypoints.
  // This is used for "plugins" compiled into Chrome. Returns true on success.
  // False means that the plugin can not be used.
  bool InitAsInternalPlugin(const ContentPluginInfo::EntryPoints& entry_points);

  // Initializes this module using the given library path as the plugin.
  // Returns true on success. False means that the plugin can not be used.
  bool InitAsLibrary(const base::FilePath& path);

  // Initializes this module for the given out of process proxy. This takes
  // ownership of the given pointer, even in the failure case.
  void InitAsProxied(HostDispatcherWrapper* host_dispatcher_wrapper);

  // Creates a new module for an external plugin instance that will be using the
  // IPC proxy. We can't use the existing module, or new instances of the plugin
  // can't be created.
  scoped_refptr<PluginModule> CreateModuleForExternalPluginInstance();

  // Initializes the external plugin module for the out of process proxy.
  // InitAsProxied must be called before calling InitAsProxiedExternalPlugin.
  // Returns a result code indicating whether the proxy started successfully or
  // there was an error.
  PP_ExternalPluginResult InitAsProxiedExternalPlugin(
      PepperPluginInstanceImpl* instance);

  bool IsProxied() const;

  // Returns the peer process ID if the plugin is running out of process;
  // returns |base::kNullProcessId| otherwise.
  base::ProcessId GetPeerProcessId();

  // Returns the plugin child process ID if the plugin is running out of
  // process. Returns 0 otherwise. This is the ID that the browser process uses
  // to idetify the child process for the plugin. This isn't directly useful
  // from our process (the renderer) except in messages to the browser to
  // disambiguate plugins.
  int GetPluginChildId();

  static const PPB_Core* GetCore();

  // Returns whether an interface is supported. This method can be called from
  // the browser process and used for interface matching before plugin
  // registration.
  // NOTE: those custom interfaces provided by ContentRendererClient will not be
  // considered when called on the browser process.
  static bool SupportsInterface(const char* name);

  RendererPpapiHostImpl* renderer_ppapi_host() {
    return renderer_ppapi_host_.get();
  }

  // Returns the module handle. This may be used before Init() is called (the
  // proxy needs this information to set itself up properly).
  PP_Module pp_module() const { return pp_module_; }

  const std::string& name() const { return name_; }
  const std::string& version() const { return version_; }
  const base::FilePath& path() const { return path_; }
  const ppapi::PpapiPermissions& permissions() const { return permissions_; }

  PepperPluginInstanceImpl* CreateInstance(RenderFrameImpl* render_frame,
                                           blink::WebPluginContainer* container,
                                           const GURL& plugin_url);

  // Returns "some" plugin instance associated with this module. This is not
  // guaranteed to be any one in particular. This is normally used to execute
  // callbacks up to the browser layer that are not inherently per-instance,
  // but the helper lives only on the plugin instance so we need one of them.
  PepperPluginInstanceImpl* GetSomeInstance() const;

  const PluginInstanceSet& GetAllInstances() const { return instances_; }

  // Calls the plugin's GetInterface and returns the given interface pointer,
  // which could be NULL.
  const void* GetPluginInterface(const char* name) const;

  // This module is associated with a set of instances. The PluginInstance
  // object declares its association with this module in its destructor and
  // releases us in its destructor.
  void InstanceCreated(PepperPluginInstanceImpl* instance);
  void InstanceDeleted(PepperPluginInstanceImpl* instance);

  scoped_refptr<ppapi::CallbackTracker> GetCallbackTracker();

  // Called when running out of process and the plugin crashed. This will
  // release relevant resources and update all affected instances.
  void PluginCrashed();

  bool is_in_destructor() const { return is_in_destructor_; }
  bool is_crashed() const { return is_crashed_; }

  // Reserves the given instance is unique within the plugin, checking for
  // collisions. See PPB_Proxy_Private for more information.
  //
  // The setter will set the callback which is set up when the proxy
  // initializes. The Reserve function will call the previously set callback if
  // it exists to validate the ID. If the callback has not been set (such as
  // for in-process plugins), the Reserve function will assume that the ID is
  // usable and will return true.
  void SetReserveInstanceIDCallback(PP_Bool (*reserve)(PP_Module, PP_Instance));
  bool ReserveInstanceID(PP_Instance instance);

  // Create a new HostDispatcher for proxying, hook it to the PluginModule,
  // and perform other common initialization.
  RendererPpapiHostImpl* CreateOutOfProcessModule(
      RenderFrameImpl* render_frame,
      const base::FilePath& path,
      ppapi::PpapiPermissions permissions,
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId plugin_pid,
      int plugin_child_id,
      bool is_external,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // In production we purposely leak the HostGlobals object but in unittest
  // code, this can interfere with subsequent tests. This deletes the
  // existing HostGlobals. A new one will be constructed when a PluginModule is
  // instantiated.
  static void ResetHostGlobalsForTest();

  // Attempts to create a PPAPI plugin for the given filepath. On success, it
  // will return the newly-created module.
  //
  // There are two reasons for failure. The first is that the plugin isn't
  // a PPAPI plugin. In this case, |*pepper_plugin_was_registered| will be set
  // to false and the caller may want to fall back on creating an NPAPI plugin.
  // the second is that the plugin failed to initialize. In this case,
  // |*pepper_plugin_was_registered| will be set to true and the caller should
  // not fall back on any other plugin types.
  static scoped_refptr<PluginModule> Create(
      RenderFrameImpl* render_frame,
      const WebPluginInfo& webplugin_info,
      const std::optional<url::Origin>& origin_lock,
      bool* pepper_plugin_was_registered,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  friend class base::RefCounted<PluginModule>;
  ~PluginModule();
  // Calls the InitializeModule entrypoint. The entrypoint must have been
  // set and the plugin must not be out of process (we don't maintain
  // entrypoints in that case).
  bool InitializeModule(const ContentPluginInfo::EntryPoints& entry_points);

  std::unique_ptr<RendererPpapiHostImpl> renderer_ppapi_host_;

  // Tracker for completion callbacks, used mainly to ensure that all callbacks
  // are properly aborted on module shutdown.
  scoped_refptr<ppapi::CallbackTracker> callback_tracker_;

  PP_Module pp_module_;

  // True when we're running in the destructor. This allows us to write some
  // assertions.
  bool is_in_destructor_;

  // True if the plugin is running out-of-process and has crashed.
  bool is_crashed_;

  // Manages the out of process proxy interface. The presence of this
  // pointer indicates that the plugin is running out of process and that the
  // entry_points_ aren't valid.
  std::unique_ptr<HostDispatcherWrapper> host_dispatcher_wrapper_;

  // Holds a reference to the base::NativeLibrary handle if this PluginModule
  // instance wraps functions loaded from a library.  Can be NULL.  If
  // |library_| is non-NULL, PluginModule will attempt to unload the library
  // during destruction.
  base::NativeLibrary library_;

  // Contains pointers to the entry points of the actual plugin implementation.
  // These will be NULL for out-of-process plugins, which is indicated by the
  // presence of the host_dispatcher_wrapper_ value.
  ContentPluginInfo::EntryPoints entry_points_;

  // The name, version, and file location of the module.
  const std::string name_;
  const std::string version_;
  const base::FilePath path_;

  ppapi::PpapiPermissions permissions_;

  // Non-owning pointers to all instances associated with this module. When
  // there are no more instances, this object should be deleted.
  PluginInstanceSet instances_;

  PP_Bool (*reserve_instance_id_)(PP_Module, PP_Instance);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_MODULE_H_
