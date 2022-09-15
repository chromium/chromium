// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_
#define PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_var.h"

namespace ppapi {
namespace proxy {

// Encapsulates the rules for serializing and deserializing vars to and from
// the local process. The renderer and the plugin process each have separate
// bookkeeping rules.
class VarSerializationRules
    : public base::RefCountedThreadSafe<VarSerializationRules> {
 public:
  // Caller-owned calls --------------------------------------------------------
  //
  // A caller-owned call is when doing a function call with a "normal" input
  // argument. The caller has a reference to the var, and the caller is
  // responsible for freeing that reference.

  // Prepares the given var for sending to the remote process. For object vars,
  // the returned var will contain the id valid for the host process.
  // Otherwise, the returned var is valid in the local process.
  virtual PP_Var SendCallerOwned(const PP_Var& var) = 0;

  // When receiving a caller-owned variable, normally we don't have to do
  // anything. However, in the case of strings, we need to deserialize the
  // string from IPC, call the function, and then destroy the temporary string.
  // These two functions handle that process.
  //
  // BeginReceiveCallerOwned takes a var from IPC and returns a new var
  // representing the input in the local process.
  //
  // EndReceiveCallerOwned releases the reference count in the Var tracker for
  // the object or string that was added to the tracker. (Note, if the recipient
  // took a reference to the Var, it will remain in the tracker after
  // EndReceiveCallerOwned).
  virtual PP_Var BeginReceiveCallerOwned(const PP_Var& var) = 0;
  virtual void EndReceiveCallerOwned(const PP_Var& var) = 0;

  // Passing refs -------------------------------------------------------------
  //
  // A pass-ref transfer is when ownership of a reference is passed from
  // one side to the other. Normally, this happens via return values and
  // output arguments, as for exceptions. The code generating the value
  // (the function returning it in the case of a return value) will AddRef
  // the var on behalf of the consumer of the value. Responsibility for
  // Release is on the consumer (the caller of the function in the case of a
  // return value).

  // Creates a var in the context of the local process from the given
  // deserialized var. The input var should be the result of calling
  // SendPassRef in the remote process. The return value is the var valid in
  // the host process for object vars. Otherwise, the return value is a var
  // which is valid in the local process.
  virtual PP_Var ReceivePassRef(const PP_Var& var) = 0;

  // Prepares a var to be sent to the remote side. One local reference will
  // be passed to the remote side. Call Begin* before doing the send and End*
  // after doing the send
  //
  // For object vars, the return value from BeginSendPassRef will be the var
  // valid for the host process. Otherwise, it is a var that is valid in the
  // local process. This same var must be passed to EndSendPassRef.
  virtual PP_Var BeginSendPassRef(const PP_Var& var) = 0;
  virtual void EndSendPassRef(const PP_Var& var) = 0;

  // ---------------------------------------------------------------------------

  virtual void ReleaseObjectRef(const PP_Var& var) = 0;

 protected:
  VarSerializationRules() {}
  virtual ~VarSerializationRules() {}

 private:
  friend class base::RefCountedThreadSafe<VarSerializationRules>;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VAR_SERIALIZATION_RULES_H_
