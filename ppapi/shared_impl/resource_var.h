// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_VAR_H_
#define PPAPI_SHARED_IMPL_RESOURCE_VAR_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/var.h"

namespace IPC {
class Message;
}

namespace ppapi {

// Represents a resource Var.
class PPAPI_SHARED_EXPORT ResourceVar : public Var {
 public:
  ResourceVar(const ResourceVar&) = delete;
  ResourceVar& operator=(const ResourceVar&) = delete;

  // Gets the resource ID associated with this var.
  // This is 0 if a resource is still pending (only possible on the host side).
  // NOTE: This can return a PP_Resource with a reference count of 0 on the
  // plugin side. It should be AddRef'd if the resource is passed to the user.
  virtual PP_Resource GetPPResource() const = 0;

  // Gets the pending resource host ID in the renderer.
  virtual int GetPendingRendererHostId() const;

  // Gets the pending resource host ID in the browser.
  virtual int GetPendingBrowserHostId() const;

  // Gets the message for creating a plugin-side resource. Returns NULL if the
  // message is empty (which is always true on the plugin side).
  virtual const IPC::Message* GetCreationMessage() const;

  // Determines whether this is a pending resource.
  // This is true if, on the host side, the there is a creation_message and no
  // PP_Resource.
  virtual bool IsPending() const = 0;

  // Var override.
  ResourceVar* AsResourceVar() override;
  PP_VarType GetType() const override;

  // Helper function that converts a PP_Var to a ResourceVar. This will
  // return NULL if the PP_Var is not of Resource type.
  static ResourceVar* FromPPVar(PP_Var var);

 protected:
  ResourceVar();

  ~ResourceVar() override;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_VAR_H_
