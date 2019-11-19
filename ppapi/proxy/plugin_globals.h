// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_GLOBALS_H_
#define PPAPI_PROXY_PLUGIN_GLOBALS_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_local_storage.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/callback_tracker.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace base {
class Thread;
}
namespace IPC {
class Sender;
}

struct PP_BrowserFont_Trusted_Description;

namespace ppapi {

struct Preferences;

namespace proxy {

class MessageLoopResource;
class PluginMessageFilter;
class PluginProxyDelegate;
class ResourceReplyThreadRegistrar;
class UDPSocketFilter;

class PPAPI_PROXY_EXPORT PluginGlobals : public PpapiGlobals {
 public:
  explicit PluginGlobals(const scoped_refptr<base::TaskRunner>& task_runner);
  PluginGlobals(PpapiGlobals::PerThreadForTest,
                const scoped_refptr<base::TaskRunner>& task_runner);
  ~PluginGlobals() override;

  // Getter for the global singleton. Generally, you should use
  // PpapiGlobals::Get() when possible. Use this only when you need some
  // plugin-specific functionality.
  inline static PluginGlobals* Get() {
    // Explicitly crash if this is the wrong process type, we want to get
    // crash reports.
    CHECK(PpapiGlobals::Get()->IsPluginGlobals());
    return static_cast<PluginGlobals*>(PpapiGlobals::Get());
  }

  // PpapiGlobals implementation.
  ResourceTracker* GetResourceTracker() override;
  VarTracker* GetVarTracker() override;
  CallbackTracker* GetCallbackTrackerForInstance(PP_Instance instance) override;
  thunk::PPB_Instance_API* GetInstanceAPI(PP_Instance instance) override;
  thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) override;
  PP_Module GetModuleForInstance(PP_Instance instance) override;
  std::string GetCmdLine() override;
  void PreCacheFontForFlash(const void* logfontw) override;
  void LogWithSource(PP_Instance instance,
                     PP_LogLevel level,
                     const std::string& source,
                     const std::string& value) override;
  void BroadcastLogWithSource(PP_Module module,
                              PP_LogLevel level,
                              const std::string& source,
                              const std::string& value) override;
  MessageLoopShared* GetCurrentMessageLoop() override;
  base::TaskRunner* GetFileTaskRunner() override;

  // Returns the channel for sending to the browser.
  IPC::Sender* GetBrowserSender();

  base::TaskRunner* ipc_task_runner() { return ipc_task_runner_.get(); }

  // Returns the language code of the current UI language.
  std::string GetUILanguage();

  // Sets the active url which is reported by breakpad.
  void SetActiveURL(const std::string& url);

  PP_Resource CreateBrowserFont(
      Connection connection,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& desc,
      const Preferences& prefs);

  // Getters for the plugin-specific versions.
  PluginResourceTracker* plugin_resource_tracker() {
    return &plugin_resource_tracker_;
  }
  PluginVarTracker* plugin_var_tracker() {
    return &plugin_var_tracker_;
  }

  // The embedder should call SetPluginProxyDelegate during startup.
  void SetPluginProxyDelegate(PluginProxyDelegate* d);
  // The embedder may choose to call ResetPluginProxyDelegate during shutdown.
  // After that point, it's unsafe to call most members of PluginGlobals,
  // and GetBrowserSender will return NULL.
  void ResetPluginProxyDelegate();

  // Returns the TLS slot that holds the message loop TLS.
  //
  // If we end up needing more TLS storage for more stuff, we should probably
  // have a struct in here for the different items.
  base::ThreadLocalStorage::Slot* msg_loop_slot() {
    return msg_loop_slot_.get();
  }

  // Sets the message loop slot, takes ownership of the given heap-alloated
  // pointer.
  void set_msg_loop_slot(base::ThreadLocalStorage::Slot* slot) {
    msg_loop_slot_.reset(slot);
  }

  // Return the special Resource that represents the MessageLoop for the main
  // thread. This Resource is not associated with any instance, and lives as
  // long as the plugin.
  MessageLoopResource* loop_for_main_thread();

  // The embedder should call this function when the name of the plugin module
  // is known. This will be used for error logging.
  void set_plugin_name(const std::string& name) { plugin_name_ = name; }

  // The embedder should call this function when the command line is known.
  void set_command_line(const std::string& c) { command_line_ = c; }

  ResourceReplyThreadRegistrar* resource_reply_thread_registrar() {
    return resource_reply_thread_registrar_.get();
  }

  UDPSocketFilter* udp_socket_filter() const {
    return udp_socket_filter_.get();
  }
  // Add any necessary ResourceMessageFilters to the PluginMessageFilter so
  // that they can receive and handle appropriate messages on the IO thread.
  void RegisterResourceMessageFilters(
      ppapi::proxy::PluginMessageFilter* plugin_filter);

 private:
  class BrowserSender;

  // PpapiGlobals overrides.
  bool IsPluginGlobals() const override;

  static PluginGlobals* plugin_globals_;

  PluginProxyDelegate* plugin_proxy_delegate_;
  PluginResourceTracker plugin_resource_tracker_;
  PluginVarTracker plugin_var_tracker_;
  scoped_refptr<CallbackTracker> callback_tracker_;

  std::unique_ptr<base::ThreadLocalStorage::Slot> msg_loop_slot_;
  // Note that loop_for_main_thread's constructor sets msg_loop_slot_, so it
  // must be initialized after msg_loop_slot_ (hence the order here).
  scoped_refptr<MessageLoopResource> loop_for_main_thread_;

  // Name of the plugin used for error logging. This will be empty until
  // set_plugin_name is called.
  std::string plugin_name_;

  // Command line for the plugin. This will be empty until set_command_line is
  // called.
  std::string command_line_;

  std::unique_ptr<BrowserSender> browser_sender_;

  scoped_refptr<base::TaskRunner> ipc_task_runner_;

  // Thread for performing potentially blocking file operations. It's created
  // lazily, since it might not be needed.
  std::unique_ptr<base::Thread> file_thread_;

  scoped_refptr<ResourceReplyThreadRegistrar> resource_reply_thread_registrar_;

  scoped_refptr<UDPSocketFilter> udp_socket_filter_;

  // Member variables should appear before the WeakPtrFactory, see weak_ptr.h.
  base::WeakPtrFactory<PluginGlobals> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginGlobals);
};

}  // namespace proxy
}  // namespace ppapi

#endif   // PPAPI_PROXY_PLUGIN_GLOBALS_H_
