// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_VAR_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_VAR_PRIVATE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/cpp/var.h"

namespace pp {

class InstanceHandle;

namespace deprecated {
class ScriptableObject;
}

// VarPrivate is a version of Var that exposes the private scripting API.
// It's designed to be mostly interchangeable with Var since most callers will
// be dealing with Vars from various places.
class VarPrivate : public Var {
 public:
  VarPrivate() : Var() {}
  VarPrivate(Null) : Var(Null()) {}
  VarPrivate(bool b) : Var(b) {}
  VarPrivate(int32_t i) : Var(i) {}
  VarPrivate(double d) : Var(d) {}
  VarPrivate(const char* utf8_str) : Var(utf8_str) {}
  VarPrivate(const std::string& utf8_str) : Var(utf8_str) {}
  VarPrivate(PassRef, PP_Var var) : Var(PassRef(), var) {}
  VarPrivate(DontManage, PP_Var var) : Var(DontManage(), var) {}
  VarPrivate(const InstanceHandle& instance,
             deprecated::ScriptableObject* object);
  VarPrivate(const Var& other) : Var(other) {}

  virtual ~VarPrivate() {}

  // This assumes the object is of type object. If it's not, it will assert in
  // debug mode. If it is not an object or not a ScriptableObject type, returns
  // NULL.
  deprecated::ScriptableObject* AsScriptableObject() const;

  bool HasProperty(const Var& name, Var* exception = NULL) const;
  bool HasMethod(const Var& name, Var* exception = NULL) const;
  VarPrivate GetProperty(const Var& name, Var* exception = NULL) const;
  void GetAllPropertyNames(std::vector<Var>* properties,
                           Var* exception = NULL) const;
  void SetProperty(const Var& name, const Var& value, Var* exception = NULL);
  void RemoveProperty(const Var& name, Var* exception = NULL);
  VarPrivate Call(const Var& method_name, uint32_t argc, Var* argv,
           Var* exception = NULL);
  VarPrivate Construct(uint32_t argc, Var* argv, Var* exception = NULL) const;

  // Convenience functions for calling functions with small # of args.
  VarPrivate Call(const Var& method_name, Var* exception = NULL);
  VarPrivate Call(const Var& method_name, const Var& arg1,
                  Var* exception = NULL);
  VarPrivate Call(const Var& method_name, const Var& arg1, const Var& arg2,
                  Var* exception = NULL);
  VarPrivate Call(const Var& method_name, const Var& arg1, const Var& arg2,
                  const Var& arg3, Var* exception = NULL);
  VarPrivate Call(const Var& method_name, const Var& arg1, const Var& arg2,
                  const Var& arg3, const Var& arg4, Var* exception = NULL);

  // For use when calling the raw C PPAPI when using the C++ Var as a possibly
  // NULL exception. This will handle getting the address of the internal value
  // out if it's non-NULL and fixing up the reference count.
  //
  // Danger: this will only work for things with exception semantics, i.e. that
  // the value will not be changed if it's a non-undefined exception. Otherwise,
  // this class will mess up the refcounting.
  //
  // This is a bit subtle:
  // - If NULL is passed, we return NULL from get() and do nothing.
  //
  // - If a undefined value is passed, we return the address of a undefined var
  //   from get and have the output value take ownership of that var.
  //
  // - If a non-undefined value is passed, we return the address of that var
  //   from get, and nothing else should change.
  //
  // Example:
  //   void FooBar(a, b, Var* exception = NULL) {
  //     foo_interface->Bar(a, b, VarPrivate::OutException(exception).get());
  //   }
  class OutException {
   public:
    OutException(Var* v)
        : output_(v),
          originally_had_exception_(v && !v->is_undefined()) {
      if (output_) {
        temp_ = output_->pp_var();
      } else {
        temp_.padding = 0;
        temp_.type = PP_VARTYPE_UNDEFINED;
      }
    }
    ~OutException() {
      if (output_ && !originally_had_exception_)
        *output_ = Var(PassRef(), temp_);
    }

    PP_Var* get() {
      if (output_)
        return &temp_;
      return NULL;
    }

   private:
    Var* output_;
    bool originally_had_exception_;
    PP_Var temp_;
  };

 private:
  // Prevent an arbitrary pointer argument from being implicitly converted to
  // a bool at Var construction. If somebody makes such a mistake, they will
  // get a compilation error.
  VarPrivate(void* non_scriptable_object_pointer);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_VAR_PRIVATE_H_
