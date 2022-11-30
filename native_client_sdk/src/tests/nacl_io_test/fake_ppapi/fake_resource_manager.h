// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_RESOURCE_MANAGER_H_
#define TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_RESOURCE_MANAGER_H_

#include <map>

#include <ppapi/c/pp_resource.h>

#include "sdk_util/atomicops.h"
#include "sdk_util/macros.h"
#include "sdk_util/simple_lock.h"

class FakeResource;
class FakeResourceTracker;

class FakeResourceManager {
 public:
  FakeResourceManager();

  FakeResourceManager(const FakeResourceManager&) = delete;
  FakeResourceManager& operator=(const FakeResourceManager&) = delete;

  ~FakeResourceManager();

  PP_Resource Create(FakeResource* resource,
                     const char* classname,
                     const char* file,
                     int line);
  void AddRef(PP_Resource handle);
  void Release(PP_Resource handle);
  template <typename T>
  T* Get(PP_Resource handle, bool not_found_ok=false);

 private:
  FakeResourceTracker* Get(PP_Resource handle, bool not_found_ok);

  typedef std::map<PP_Resource, FakeResourceTracker*> ResourceMap;
  PP_Resource next_handle_;
  ResourceMap resource_map_;
  sdk_util::SimpleLock lock_;  // Protects next_handle_ and resource_map_.
};

// FakeResourceTracker wraps a FakeResource to keep metadata about the
// resource, including its refcount, the type of resource, etc.
class FakeResourceTracker {
 public:
  FakeResourceTracker(FakeResource* resource,
                      const char* classname,
                      const char* file,
                      int line);

  FakeResourceTracker(const FakeResourceTracker&) = delete;
  FakeResourceTracker& operator=(const FakeResourceTracker&) = delete;

  ~FakeResourceTracker();

  void AddRef() { sdk_util::AtomicAddFetch(&ref_count_, 1); }
  void Release() { sdk_util::AtomicAddFetch(&ref_count_, -1); }
  int32_t ref_count() const { return ref_count_; }

  // Give up ownership of this resource. It is the responsibility of the caller
  // to delete this FakeResource.
  FakeResource* Pass() {
    FakeResource* result = resource_;
    resource_ = NULL;
    return result;
  }

  template <typename T>
  T* resource() {
    if (!CheckType(T::classname()))
      return NULL;

    return static_cast<T*>(resource_);
  }

  FakeResource* resource() { return resource_; }

  const char* classname() const { return classname_; }
  const char* file() const { return file_; }
  int line() const { return line_; }

 private:
  bool CheckType(const char* classname) const;

  FakeResource* resource_;  // Owned.
  const char* classname_;   // Weak reference.
  const char* file_;        // Weak reference.
  int line_;
  int32_t ref_count_;
};

class FakeResource {
 public:
  FakeResource() {}

  FakeResource(const FakeResource&) = delete;
  FakeResource& operator=(const FakeResource&) = delete;

  // Called when the resource is destroyed. For debugging purposes, this does
  // not happen until the resource manager is destroyed.
  virtual ~FakeResource() {}
  // Called when the last reference to the resource is released.
  virtual void Destroy() {}
};

template <typename T>
inline T* FakeResourceManager::Get(PP_Resource handle, bool not_found_ok) {
  FakeResourceTracker* tracker = Get(handle, not_found_ok);
  if (!tracker)
    return NULL;
  return tracker->resource<T>();
}

#define CREATE_RESOURCE(MANAGER, TYPE, RESOURCE) \
  (MANAGER)->Create((RESOURCE), #TYPE, __FILE__, __LINE__)

#endif  // TESTS_NACL_IO_TEST_FAKE_PPAPI_FAKE_RESOURCE_MANAGER_H_
