// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
#define PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_

#include <stack>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace ppapi {

class CallbackTracker;
class MessageLoopShared;
class ResourceTracker;
class VarTracker;

namespace thunk {
class PPB_Instance_API;
class ResourceCreationAPI;
}

// Abstract base class
class PPAPI_SHARED_EXPORT PpapiGlobals {
 public:
  // Must be created on the main thread.
  PpapiGlobals();

  // This constructor is to be used only for making a PpapiGlobal for testing
  // purposes. This avoids setting the global static ppapi_globals_. For unit
  // tests that use this feature, the "test" PpapiGlobals should be constructed
  // using this method. See SetPpapiGlobalsOnThreadForTest for more information.
  struct PerThreadForTest {};
  explicit PpapiGlobals(PerThreadForTest);

  PpapiGlobals(const PpapiGlobals&) = delete;
  PpapiGlobals& operator=(const PpapiGlobals&) = delete;

  virtual ~PpapiGlobals();

  // Getter for the global singleton.
  static PpapiGlobals* Get();

  // This allows us to set a given PpapiGlobals object as the PpapiGlobals for
  // a given thread. After setting the PpapiGlobals for a thread, Get() will
  // return that PpapiGlobals when Get() is called on that thread. Other threads
  // are unaffected. This allows us to have tests which use >1 PpapiGlobals in
  // the same process, e.g. for having 1 thread emulate the "host" and 1 thread
  // emulate the "plugin".
  //
  // PpapiGlobals object must have been constructed using the "PerThreadForTest"
  // parameter.
  static void SetPpapiGlobalsOnThreadForTest(PpapiGlobals* ptr);

  // Retrieves the corresponding tracker.
  virtual ResourceTracker* GetResourceTracker() = 0;
  virtual VarTracker* GetVarTracker() = 0;
  virtual CallbackTracker* GetCallbackTrackerForInstance(
      PP_Instance instance) = 0;

  // Logs the given string to the JS console. If "source" is empty, the name of
  // the current module will be used, if it can be determined.
  virtual void LogWithSource(PP_Instance instance,
                             PP_LogLevel level,
                             const std::string& source,
                             const std::string& value) = 0;

  // Like LogWithSource but broadcasts the log to all instances of the given
  // module. The module may be 0 to specify that all consoles possibly
  // associated with the calling code should be notified. This allows us to
  // log errors for things like bad resource IDs where we may not have an
  // associated instance.
  //
  // Note that in the plugin process, the module parameter is ignored since
  // there is only one possible one.
  virtual void BroadcastLogWithSource(PP_Module module,
                                      PP_LogLevel level,
                                      const std::string& source,
                                      const std::string& value) = 0;

  // Returns the given API object associated with the given instance, or NULL
  // if the instance is invalid.
  virtual thunk::PPB_Instance_API* GetInstanceAPI(PP_Instance instance) = 0;
  virtual thunk::ResourceCreationAPI* GetResourceCreationAPI(
      PP_Instance instance) = 0;

  // Returns the PP_Module associated with the given PP_Instance, or 0 on
  // failure.
  virtual PP_Module GetModuleForInstance(PP_Instance instance) = 0;

  // Returns the base::SingleThreadTaskRunner for the main thread. This is set
  // in the constructor, so PpapiGlobals must be created on the main thread.
  base::SingleThreadTaskRunner* GetMainThreadMessageLoop();

  // Manage the MessageLoop
  void RunMsgLoop();
  void QuitMsgLoop();
  // In tests, the PpapiGlobals object persists across tests but the MLP pointer
  // it hangs on will go stale and the next PPAPI test will crash because of
  // thread checks. This resets the pointer to be the current MLP object.
  void ResetMainThreadMessageLoopForTesting();

  // Return the MessageLoopShared of the current thread, if any. This will
  // always return NULL on the host side, where PPB_MessageLoop is not
  // supported.
  virtual MessageLoopShared* GetCurrentMessageLoop() = 0;

  // Returns a task runner for file operations that may block.
  // TODO(bbudge) Move this to PluginGlobals when we no longer support
  // in-process plugins.
  virtual base::TaskRunner* GetFileTaskRunner() = 0;

  virtual bool IsHostGlobals() const;
  virtual bool IsPluginGlobals() const;

 private:
  // Return the thread-local pointer which is used only for unit testing. It
  // should always be NULL when running in production. It allows separate
  // threads to have distinct "globals".
  static PpapiGlobals* GetThreadLocalPointer();

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::stack<base::OnceClosure> message_loop_quit_closures_;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPAPI_GLOBALS_H_
