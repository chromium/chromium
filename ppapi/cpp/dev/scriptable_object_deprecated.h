// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_SCRIPTABLE_OBJECT_DEPRECATED_H_
#define PPAPI_CPP_DEV_SCRIPTABLE_OBJECT_DEPRECATED_H_

#include <vector>

struct PPP_Class_Deprecated;

namespace pp {
class Var;
class VarPrivate;
}

namespace pp {

namespace deprecated {

// This class allows you to implement objects accessible by JavaScript. Derive
// from this class and override the virtual functions you support. pp::Var has
// a constructor that takes a pointer to a ScriptableObject for when you want
// to convert your custom object to a var.
//
// Please see the PPB_Core C interface for more information on how to implement
// these functions. These functions are the backend implementation for the
// functions in PPB_Var, which contains further information.
//
// Please see:
//   http://code.google.com/p/ppapi/wiki/InterfacingWithJavaScript
// for a general overview of interfacing with JavaScript.
class ScriptableObject {
 public:
  ScriptableObject() {}
  virtual ~ScriptableObject() {}

  // The default implementation returns false.
  virtual bool HasProperty(const Var& name, Var* exception);

  // The default implementation returns false.
  virtual bool HasMethod(const Var& name, Var* exception);

  // The default implementation sets an exception that the property doesn't
  // exist.
  virtual Var GetProperty(const Var& name, Var* exception);

  // The default implementation returns no properties.
  virtual void GetAllPropertyNames(std::vector<Var>* properties,
                                   Var* exception);

  // The default implementation sets an exception that the property can not be
  // set.
  virtual void SetProperty(const Var& name,
                           const Var& value,
                           Var* exception);

  // The default implementation sets an exception that the method does not
  // exist.
  virtual void RemoveProperty(const Var& name,
                              Var* exception);

  // TODO(brettw) need native array access here.

  // method_name is guaranteed to be either a string or an integer.
  //
  // The default implementation sets an exception that the method does not
  // exist.
  virtual Var Call(const Var& method_name,
                   const std::vector<Var>& args,
                   Var* exception);

  // The default implementation sets an exception that the method does not
  // exist.
  virtual Var Construct(const std::vector<Var>& args,
                        Var* exception);

 private:
  friend class ::pp::Var;
  friend class ::pp::VarPrivate;
  static const PPP_Class_Deprecated* GetClass();

  // Unimplemented, copy and assignment is not allowed.
  ScriptableObject(const ScriptableObject& other);
  ScriptableObject& operator=(const ScriptableObject& other);
};

}  // namespace deprecated

}  // namespace pp

#endif  // PPAPI_CPP_DEV_SCRIPTABLE_OBJECT_DEPRECATED_H_

