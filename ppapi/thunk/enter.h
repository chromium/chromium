// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_ENTER_H_
#define PPAPI_THUNK_ENTER_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

// Enter* helper objects: These objects wrap a call from the C PPAPI into
// the internal implementation. They make sure the lock is acquired and will
// automatically set up some stuff for you.
//
// You should always check whether the enter succeeded before using the object.
// If this fails, then the instance or resource ID supplied was invalid.
//
// The |report_error| arguments to the constructor should indicate if errors
// should be logged to the console. If the calling function expects that the
// input values are correct (the normal case), this should be set to true. In
// some case like |IsFoo(PP_Resource)| the caller is questioning whether their
// handle is this type, and we don't want to report an error if it's not.
//
// Resource member functions: EnterResource
//   Automatically interprets the given PP_Resource as a resource ID and sets
//   up the resource object for you.

namespace subtle {

// This helps us define our RAII Enter classes easily. To make an RAII class
// which locks the proxy lock on construction and unlocks on destruction,
// inherit from |LockOnEntry<true>| before all other base classes. This ensures
// that the lock is acquired before any other base class's constructor can run,
// and that the lock is only released after all other destructors have run.
// (This order of initialization is guaranteed by C++98/C++11 12.6.2.10).
//
// For cases where you don't want to lock, inherit from |LockOnEntry<false>|.
// This allows us to share more code between Enter* and Enter*NoLock classes.
template <bool lock_on_entry>
struct LockOnEntry;

template <>
struct LockOnEntry<false> {
#if (!NDEBUG)
  LockOnEntry() {
    // You must already hold the lock to use Enter*NoLock.
    ProxyLock::AssertAcquired();
  }
  ~LockOnEntry() {
    // You must not release the lock before leaving the scope of the
    // Enter*NoLock.
    ProxyLock::AssertAcquired();
  }
#endif
};

template <>
struct LockOnEntry<true> {
  LockOnEntry() {
    ppapi::ProxyLock::Acquire();
  }
  ~LockOnEntry() {
    ppapi::ProxyLock::Release();
  }
};

// Keep non-templatized since we need non-inline functions here.
class PPAPI_THUNK_EXPORT EnterBase {
 public:
  EnterBase();
  explicit EnterBase(PP_Resource resource);
  EnterBase(PP_Instance instance, SingletonResourceID resource_id);
  EnterBase(PP_Resource resource, const PP_CompletionCallback& callback);
  EnterBase(PP_Instance instance, SingletonResourceID resource_id,
            const PP_CompletionCallback& callback);
  virtual ~EnterBase();

  // Sets the result for calls that use a completion callback. It handles making
  // sure that "Required" callbacks are scheduled to run asynchronously and
  // "Blocking" callbacks cause the caller to block. (Interface implementations,
  // therefore, should not do any special casing based on the type of the
  // callback.)
  //
  // Returns the "retval()". This is to support the typical usage of
  //   return enter.SetResult(...);
  // without having to write a separate "return enter.retval();" line.
  int32_t SetResult(int32_t result);

  // Use this value as the return value for the function.
  int32_t retval() const { return retval_; }

  // All failure conditions cause retval_ to be set to an appropriate error
  // code.
  bool succeeded() const { return retval_ == PP_OK; }
  bool failed() const { return !succeeded(); }

  const scoped_refptr<TrackedCallback>& callback() { return callback_; }

 protected:
  // Helper function to return a Resource from a PP_Resource. Having this
  // code be in the non-templatized base keeps us from having to instantiate
  // it in every template.
  static Resource* GetResource(PP_Resource resource);

  // Helper function to return a Resource from a PP_Instance and singleton
  // resource identifier.
  static Resource* GetSingletonResource(PP_Instance instance,
                                        SingletonResourceID resource_id);

  void ClearCallback();

  // Does error handling associated with entering a resource. The resource_base
  // is the result of looking up the given pp_resource. The object is the
  // result of converting the base to the desired object (converted to a void*
  // so this function doesn't have to be templatized). The reason for passing
  // both resource_base and object is that we can differentiate "bad resource
  // ID" from "valid resource ID not of the correct type."
  //
  // This will set retval_ = PP_ERROR_BADRESOURCE if the object is invalid, and
  // if report_error is set, log a message to the programmer.
  void SetStateForResourceError(PP_Resource pp_resource,
                                Resource* resource_base,
                                void* object,
                                bool report_error);

  // Same as SetStateForResourceError except for function API.
  void SetStateForFunctionError(PP_Instance pp_instance,
                                void* object,
                                bool report_error);

  // For Enter objects that need a resource, we'll store a pointer to the
  // Resource object so that we don't need to look it up more than once. For
  // Enter objects with no resource, this will be null.
  Resource* resource_ = nullptr;

 private:
  bool CallbackIsValid() const;

  // Checks whether the callback is valid (i.e., if it is either non-blocking,
  // or blocking and we're on a background thread). If the callback is invalid,
  // this will set retval_ = PP_ERROR_BLOCKS_MAIN_THREAD, and if report_error is
  // set, it will log a message to the programmer.
  void SetStateForCallbackError(bool report_error);

  // Holds the callback. For Enter objects that aren't given a callback, this
  // will be null.
  scoped_refptr<TrackedCallback> callback_;

  int32_t retval_ = PP_OK;
};

}  // namespace subtle

// EnterResource ---------------------------------------------------------------

template<typename ResourceT, bool lock_on_entry = true>
class EnterResource
    : public subtle::LockOnEntry<lock_on_entry>,  // Must be first; see above.
      public subtle::EnterBase {
 public:
  EnterResource(PP_Resource resource, bool report_error)
      : EnterBase(resource) {
    Init(resource, report_error);
  }
  EnterResource(PP_Resource resource, const PP_CompletionCallback& callback,
                bool report_error)
      : EnterBase(resource, callback) {
    Init(resource, report_error);
  }

  EnterResource(const EnterResource&) = delete;
  EnterResource& operator=(const EnterResource&) = delete;

  ~EnterResource() {}

  ResourceT* object() { return object_; }
  Resource* resource() { return resource_; }

 private:
  void Init(PP_Resource resource, bool report_error) {
    if (resource_)
      object_ = resource_->GetAs<ResourceT>();
    else
      object_ = nullptr;
    // Validate the resource (note, if both are wrong, we will return
    // PP_ERROR_BADRESOURCE; last in wins).
    SetStateForResourceError(resource, resource_, object_, report_error);
  }

  ResourceT* object_;
};

// ----------------------------------------------------------------------------

// Like EnterResource but assumes the lock is already held.
template<typename ResourceT>
class EnterResourceNoLock : public EnterResource<ResourceT, false> {
 public:
  EnterResourceNoLock(PP_Resource resource, bool report_error)
      : EnterResource<ResourceT, false>(resource, report_error) {
  }
  EnterResourceNoLock(PP_Resource resource,
                      const PP_CompletionCallback& callback,
                      bool report_error)
      : EnterResource<ResourceT, false>(resource, callback, report_error) {
  }
};

// EnterInstance ---------------------------------------------------------------

class PPAPI_THUNK_EXPORT EnterInstance
    : public subtle::LockOnEntry<true>,  // Must be first; see above.
      public subtle::EnterBase {
 public:
  explicit EnterInstance(PP_Instance instance);
  EnterInstance(PP_Instance instance,
                const PP_CompletionCallback& callback);
  ~EnterInstance();

  bool succeeded() const { return !!functions_; }
  bool failed() const { return !functions_; }

  PPB_Instance_API* functions() const { return functions_; }

 private:
  PPB_Instance_API* functions_;
};

class PPAPI_THUNK_EXPORT EnterInstanceNoLock
    : public subtle::LockOnEntry<false>,  // Must be first; see above.
      public subtle::EnterBase {
 public:
  explicit EnterInstanceNoLock(PP_Instance instance);
  EnterInstanceNoLock(PP_Instance instance,
                      const PP_CompletionCallback& callback);
  ~EnterInstanceNoLock();

  PPB_Instance_API* functions() { return functions_; }

 private:
  PPB_Instance_API* functions_;
};

// EnterInstanceAPI ------------------------------------------------------------

template<typename ApiT, bool lock_on_entry = true>
class EnterInstanceAPI
    : public subtle::LockOnEntry<lock_on_entry>,  // Must be first; see above
      public subtle::EnterBase {
 public:
  explicit EnterInstanceAPI(PP_Instance instance)
      : EnterBase(instance, ApiT::kSingletonResourceID) {
    if (resource_)
      functions_ = resource_->GetAs<ApiT>();
    SetStateForFunctionError(instance, functions_, true);
  }
  EnterInstanceAPI(PP_Instance instance, const PP_CompletionCallback& callback)
      : EnterBase(instance, ApiT::kSingletonResourceID, callback) {
    if (resource_)
      functions_ = resource_->GetAs<ApiT>();
    SetStateForFunctionError(instance, functions_, true);
  }
  ~EnterInstanceAPI() {}

  bool succeeded() const { return !!functions_; }
  bool failed() const { return !functions_; }

  ApiT* functions() const { return functions_; }

 private:
  ApiT* functions_ = nullptr;
};

template<typename ApiT>
class EnterInstanceAPINoLock : public EnterInstanceAPI<ApiT, false> {
 public:
  explicit EnterInstanceAPINoLock(PP_Instance instance)
      : EnterInstanceAPI<ApiT, false>(instance) {
  }
};

// EnterResourceCreation -------------------------------------------------------

class PPAPI_THUNK_EXPORT EnterResourceCreation
    : public subtle::LockOnEntry<true>,  // Must be first; see above.
      public subtle::EnterBase {
 public:
  explicit EnterResourceCreation(PP_Instance instance);
  ~EnterResourceCreation();

  ResourceCreationAPI* functions() { return functions_; }

 private:
  ResourceCreationAPI* functions_;
};

class PPAPI_THUNK_EXPORT EnterResourceCreationNoLock
    : public subtle::LockOnEntry<false>,  // Must be first; see above.
      public subtle::EnterBase {
 public:
  explicit EnterResourceCreationNoLock(PP_Instance instance);
  ~EnterResourceCreationNoLock();

  ResourceCreationAPI* functions() { return functions_; }

 private:
  ResourceCreationAPI* functions_;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_ENTER_H_
