// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_V8OBJECT_VAR_H_
#define CONTENT_RENDERER_PEPPER_V8OBJECT_VAR_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/shared_impl/var.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace content {
class PepperPluginInstanceImpl;
}  // namespace content

namespace ppapi {

// V8ObjectVar -----------------------------------------------------------------

// Represents a JavaScript object Var. By itself, this represents random
// v8 objects that a given plugin (identified by the resource's module) wants to
// reference. If two different modules reference the same v8 object (like the
// "window" object), then there will be different V8ObjectVar's (and hence
// PP_Var IDs) for each module. This allows us to track all references owned by
// a given module and free them when the plugin exits independently of other
// plugins that may be running at the same time.
class CONTENT_EXPORT V8ObjectVar : public Var {
 public:
  V8ObjectVar(PP_Instance instance, v8::Local<v8::Object> v8_object);

  V8ObjectVar(const V8ObjectVar&) = delete;
  V8ObjectVar& operator=(const V8ObjectVar&) = delete;

  // Var overrides.
  V8ObjectVar* AsV8ObjectVar() override;
  PP_VarType GetType() const override;

  // Returns the underlying v8 object corresponding to this V8ObjectVar. This
  // should only be used on the stack.
  v8::Local<v8::Object> GetHandle() const;

  // Notification that the instance was deleted, the internal reference will be
  // zeroed out.
  void InstanceDeleted();

  // Possibly NULL if the object has outlived its instance.
  content::PepperPluginInstanceImpl* instance() const { return instance_; }

  // Helper function that converts a PP_Var to an object. This will return NULL
  // if the PP_Var is not of object type or the object is invalid.
  static scoped_refptr<V8ObjectVar> FromPPVar(PP_Var var);

 private:
  ~V8ObjectVar() override;

  raw_ptr<content::PepperPluginInstanceImpl> instance_;

  v8::Persistent<v8::Object> v8_object_;
};

}  // ppapi

#endif  // CONTENT_RENDERER_PEPPER_V8OBJECT_VAR_H_
