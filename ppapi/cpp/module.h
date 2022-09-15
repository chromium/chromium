// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MODULE_H_
#define PPAPI_CPP_MODULE_H_

#include <map>
#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/cpp/core.h"


/// @file
/// This file defines a Module class.
namespace pp {

class Instance;

/// The Module class.  The browser calls CreateInstance() to create
/// an instance of your module on the web page.  The browser creates a new
/// instance for each <code>\<embed></code> tag with
/// <code>type="application/x-nacl"</code>
class Module {
 public:
  typedef std::map<PP_Instance, Instance*> InstanceMap;

  // You may not call any other PP functions from the constructor, put them
  // in Init instead. Various things will not be set up until the constructor
  // completes.
  Module();
  virtual ~Module();

  /// Get() returns the global instance of this module object, or NULL if the
  /// module is not initialized yet.
  ///
  /// @return The global instance of the module object.
  static Module* Get();

  /// Init() is automatically called after the object is created. This is where
  /// you can put functions that rely on other parts of the API, now that the
  /// module has been created.
  ///
  /// @return true if successful, otherwise false.
  virtual bool Init();

  /// The pp_module() function returns the internal module handle.
  ///
  /// @return A <code>PP_Module</code> internal module handle.
  PP_Module pp_module() const { return pp_module_; }

  /// The get_browser_interface() function returns the internal
  /// <code>get_browser_interface</code> pointer.
  ///
  /// @return A <code>PPB_GetInterface</code> internal pointer.
  // TODO(sehr): This should be removed once the NaCl browser plugin no longer
  // needs it.
  PPB_GetInterface get_browser_interface() const {
    return get_browser_interface_;
  }

  /// The core() function returns the core interface for doing basic
  /// global operations. The return value is guaranteed to be non-NULL once the
  /// module has successfully initialized and during the Init() call.
  ///
  /// It will be NULL before Init() has been called.
  ///
  /// @return The core interface for doing basic global operations.
  Core* core() { return core_; }

  /// GetPluginInterface() implements <code>GetInterface</code> for the browser
  /// to get module interfaces. If you need to provide your own implementations
  /// of new interfaces, use AddPluginInterface() which this function will use.
  ///
  /// @param[in] interface_name The module interface for the browser to get.
  const void* GetPluginInterface(const char* interface_name);

  /// GetBrowserInterface() returns interfaces which the browser implements
  /// (i.e. PPB interfaces).
  /// @param[in] interface_name The browser interface for the module to get.
  const void* GetBrowserInterface(const char* interface_name);

  /// InstanceForPPInstance() returns the object associated with this
  /// <code>PP_Instance</code>, or NULL if one is not found. This should only
  /// be called from the main thread! This instance object may be destroyed at
  /// any time on the main thread, so using it on other threads may cause a
  /// crash.
  ///
  /// @param[in] instance This <code>PP_Instance</code>.
  ///
  /// @return The object associated with this <code>PP_Instance</code>,
  /// or NULL if one is not found.
  Instance* InstanceForPPInstance(PP_Instance instance);

  /// AddPluginInterface() adds a handler for a provided interface name. When
  /// the browser requests that interface name, the provided
  /// <code>vtable</code> will be returned.
  ///
  /// In general, modules will not need to call this directly. Instead, the
  /// C++ wrappers for each interface will register themselves with this
  /// function.
  ///
  /// This function may be called more than once with the same interface name
  /// and vtable with no effect. However, it may not be used to register a
  /// different vtable for an already-registered interface. It will assert for
  /// a different registration for an already-registered interface in debug
  /// mode, and just ignore the registration in release mode.
  ///
  /// @param[in] interface_name The interface name that will receive a handler.
  /// @param[in,out] vtable The vtable to return for
  /// <code>interface_name</code>.
  void AddPluginInterface(const std::string& interface_name,
                          const void* vtable);

  // InternalInit() sets the browser interface and calls the regular Init()
  /// function that can be overridden by the base classes.
  ///
  /// @param[in] mod A <code>PP_Module</code>.
  /// @param[in] get_browser_interface The browser interface to set.
  ///
  /// @return true if successful, otherwise false.
  // TODO(brettw) make this private when I can figure out how to make the
  // initialize function a friend.
  bool InternalInit(PP_Module mod,
                    PPB_GetInterface get_browser_interface);

  /// The current_instances() function allows iteration over the
  /// current instances in the module.
  ///
  /// @return An <code>InstanceMap</code> of all instances in the module.
  const InstanceMap& current_instances() const { return current_instances_; }

 protected:
  /// CreateInstance() should be overridden to create your own module type.
  ///
  /// @param[in] instance A <code>PP_Instance</code>.
  ///
  /// @return The resulting instance.
  virtual Instance* CreateInstance(PP_Instance instance) = 0;

 private:
  friend PP_Bool Instance_DidCreate(PP_Instance pp_instance,
                                    uint32_t argc,
                                    const char* argn[],
                                    const char* argv[]);
  friend void Instance_DidDestroy(PP_Instance instance);

  // Unimplemented (disallow copy and assign).
  Module(const Module&);
  Module& operator=(const Module&);

  // Instance tracking.
  InstanceMap current_instances_;

  PP_Module pp_module_;
  PPB_GetInterface get_browser_interface_;

  Core* core_;

  // All additional interfaces this plugin can handle as registered by
  // AddPluginInterface.
  typedef std::map<std::string, const void*> InterfaceMap;
  InterfaceMap additional_interfaces_;
};

}  // namespace pp

#endif  // PPAPI_CPP_MODULE_H_
