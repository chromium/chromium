// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_VAR_H_
#define PPAPI_SHARED_IMPL_VAR_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class ArrayBufferVar;
class ArrayVar;
class DictionaryVar;
class ProxyObjectVar;
class ResourceVar;
class StringVar;
class V8ObjectVar;
class VarTracker;

// Var -------------------------------------------------------------------------

// Represents a non-POD var.
class PPAPI_SHARED_EXPORT Var : public base::RefCounted<Var> {
 public:
  Var(const Var&) = delete;
  Var& operator=(const Var&) = delete;

  // Returns a string representing the given var for logging purposes.
  static std::string PPVarToLogString(PP_Var var);

  virtual StringVar* AsStringVar();
  virtual ArrayBufferVar* AsArrayBufferVar();
  virtual V8ObjectVar* AsV8ObjectVar();
  virtual ProxyObjectVar* AsProxyObjectVar();
  virtual ArrayVar* AsArrayVar();
  virtual DictionaryVar* AsDictionaryVar();
  virtual ResourceVar* AsResourceVar();

  // Creates a PP_Var corresponding to this object. The return value will have
  // one reference addrefed on behalf of the caller.
  PP_Var GetPPVar();

  // Returns the type of this var.
  virtual PP_VarType GetType() const = 0;

  // Returns the ID corresponing to the string or object if it exists already,
  // or 0 if an ID hasn't been generated for this object (the plugin is holding
  // no refs).
  //
  // Contrast to GetOrCreateVarID which creates the ID and a ref on behalf of
  // the plugin.
  int32_t GetExistingVarID() const;

 protected:
  friend class base::RefCounted<Var>;
  friend class VarTracker;

  Var();
  virtual ~Var();

  // Returns the unique ID associated with this string or object, creating it
  // if necessary. The return value will be 0 if the string or object is
  // invalid.
  //
  // This function will take a reference to the var that will be passed to the
  // caller.
  int32_t GetOrCreateVarID();

  // Sets the internal object ID. This assumes that the ID hasn't been set
  // before. This is used in cases where the ID is generated externally.
  void AssignVarID(int32_t id);

  // Reset the assigned object ID.
  void ResetVarID() { var_id_ = 0; }

 private:
  // This will be 0 if no ID has been assigned (this happens lazily).
  int32_t var_id_;
};

// StringVar -------------------------------------------------------------------

// Represents a string-based Var.
//
// Returning a given string as a PP_Var:
//   return StringVar::StringToPPVar(my_string);
//
// Converting a PP_Var to a string:
//   StringVar* string = StringVar::FromPPVar(var);
//   if (!string)
//     return false;  // Not a string or an invalid var.
//   DoSomethingWithTheString(string->value());
class PPAPI_SHARED_EXPORT StringVar : public Var {
 public:
  explicit StringVar(const std::string& str);
  StringVar(const char* str, uint32_t len);

  StringVar(const StringVar&) = delete;
  StringVar& operator=(const StringVar&) = delete;

  ~StringVar() override;

  const std::string& value() const { return value_; }
  // Return a pointer to the internal string. This allows other objects to
  // temporarily store a weak pointer to our internal string. Use with care; the
  // pointer *will* become invalid if this StringVar is removed from the
  // tracker. (All of this applies to value(), but this one's even easier to use
  // dangerously).
  const std::string* ptr() const { return &value_; }

  // Var override.
  StringVar* AsStringVar() override;
  PP_VarType GetType() const override;

  // Helper function to create a PP_Var of type string that contains a copy of
  // the given string. The input data must be valid UTF-8 encoded text, if it
  // is not valid UTF-8, a NULL var will be returned.
  //
  // The return value will have a reference count of 1. Internally, this will
  // create a StringVar and return the reference to it in the var.
  static PP_Var StringToPPVar(const std::string& str);
  static PP_Var StringToPPVar(const char* str, uint32_t len);

  // Same as StringToPPVar but avoids a copy by destructively swapping the
  // given string into the newly created StringVar. The string must already be
  // valid UTF-8. After the call, *src will be empty.
  static PP_Var SwapValidatedUTF8StringIntoPPVar(std::string* src);

  // Helper function that converts a PP_Var to a string. This will return NULL
  // if the PP_Var is not of string type or the string is invalid.
  static StringVar* FromPPVar(PP_Var var);

 private:
  StringVar();  // Makes an empty string.

  std::string value_;
};

// ArrayBufferVar --------------------------------------------------------------

// Represents an array buffer Var.
//
// Note this is an abstract class. To create an appropriate concrete one, you
// need to use the VarTracker:
//   VarArrayBuffer* buf =
//       PpapiGlobals::Get()->GetVarTracker()->CreateArrayBuffer(size);
//
// Converting a PP_Var to an ArrayBufferVar:
//   ArrayBufferVar* array = ArrayBufferVar::FromPPVar(var);
//   if (!array)
//     return false;  // Not an ArrayBuffer or an invalid var.
//   DoSomethingWithTheBuffer(array);
class PPAPI_SHARED_EXPORT ArrayBufferVar : public Var {
 public:
  ArrayBufferVar();

  ArrayBufferVar(const ArrayBufferVar&) = delete;
  ArrayBufferVar& operator=(const ArrayBufferVar&) = delete;

  ~ArrayBufferVar() override;

  virtual void* Map() = 0;
  virtual void Unmap() = 0;
  virtual uint32_t ByteLength() = 0;

  // Creates a new shared memory region, and copies the data in the
  // ArrayBufferVar into it. On the plugin side, host_shm_handle_id will be set
  // to some value that is not -1. On the host side, plugin_shm_region will be
  // set to a valid UnsafeSharedMemoryRegion.
  //
  // Returns true if creating the shared memory (and copying) is successful,
  // false otherwise.
  virtual bool CopyToNewShmem(
      PP_Instance instance,
      int* host_shm_handle_id,
      base::UnsafeSharedMemoryRegion* plugin_shm_region) = 0;

  // Var override.
  ArrayBufferVar* AsArrayBufferVar() override;
  PP_VarType GetType() const override;

  // Helper function that converts a PP_Var to an ArrayBufferVar. This will
  // return NULL if the PP_Var is not of ArrayBuffer type.
  static ArrayBufferVar* FromPPVar(PP_Var var);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_VAR_H_
