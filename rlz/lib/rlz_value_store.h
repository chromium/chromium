// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_RLZ_VALUE_STORE_H_
#define RLZ_LIB_RLZ_VALUE_STORE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "rlz/lib/rlz_enums.h"

#if BUILDFLAG(IS_WIN)
#include "rlz/win/lib/lib_mutex.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#endif

namespace base {
class FilePath;
}

namespace rlz_lib {

// Abstracts away rlz's key value store. On windows, this usually writes to
// the registry. On mac, it writes to an NSDefaults object.
class RlzValueStore {
 public:
  virtual ~RlzValueStore() {}

  enum AccessType { kReadAccess, kWriteAccess };
  virtual bool HasAccess(AccessType type) = 0;

  // Ping times.
  virtual bool WritePingTime(Product product, int64_t time) = 0;
  virtual bool ReadPingTime(Product product, int64_t* time) = 0;
  virtual bool ClearPingTime(Product product) = 0;

  // Access point RLZs.
  virtual bool WriteAccessPointRlz(AccessPoint access_point,
                                   const char* new_rlz) = 0;
  virtual bool ReadAccessPointRlz(AccessPoint access_point,
                                  char* rlz,  // At most kMaxRlzLength + 1 bytes
                                  size_t rlz_size) = 0;
  virtual bool ClearAccessPointRlz(AccessPoint access_point) = 0;
  virtual bool UpdateExistingAccessPointRlz(const std::string& brand) = 0;

  // Product events.
  // Stores |event_rlz| for product |product| as product event.
  virtual bool AddProductEvent(Product product, const char* event_rlz) = 0;
  // Appends all events for |product| to |events|, in arbirtrary order.
  virtual bool ReadProductEvents(Product product,
                                 std::vector<std::string>* events) = 0;
  // Removes the stored event |event_rlz| for |product| if it exists.
  virtual bool ClearProductEvent(Product product, const char* event_rlz) = 0;
  // Removes all stored product events for |product|.
  virtual bool ClearAllProductEvents(Product product) = 0;

  // Stateful events.
  // Stores |event_rlz| for product |product| as stateful event.
  virtual bool AddStatefulEvent(Product product, const char* event_rlz) = 0;
  // Checks if |event_rlz| has been stored as stateful event for |product|.
  virtual bool IsStatefulEvent(Product product, const char* event_rlz) = 0;
  // Removes all stored stateful events for |product|.
  virtual bool ClearAllStatefulEvents(Product product) = 0;

  // Tells the value store to clean up unimportant internal data structures, for
  // example empty registry folders, that might remain after clearing other
  // data. Best-effort.
  virtual void CollectGarbage() = 0;
};

// All methods of RlzValueStore must stays consistent even when accessed from
// multiple threads in multiple processes. To enforce this through the type
// system, the only way to access the RlzValueStore is through a
// ScopedRlzValueStoreLock, which is a cross-process lock. It is active while
// it is in scope. If the class fails to acquire a lock, its GetStore() method
// returns NULL. If the lock fails to be acquired, it must not be taken
// recursively. That is, all user code should look like this:
//   ScopedRlzValueStoreLock lock;
//   RlzValueStore* store = lock.GetStore();
//   if (!store)
//     return some_error_code;
//   ...
class ScopedRlzValueStoreLock {
 public:
  ScopedRlzValueStoreLock();
  ~ScopedRlzValueStoreLock();

  // Returns a RlzValueStore protected by a cross-process lock, or NULL if the
  // lock can't be obtained. The lifetime of the returned object is limited to
  // the lifetime of this ScopedRlzValueStoreLock object.
  RlzValueStore* GetStore();

 private:
  std::unique_ptr<RlzValueStore> store_;
#if BUILDFLAG(IS_WIN)
  LibMutex lock_;
#elif BUILDFLAG(IS_APPLE)
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool autorelease_pool_;
#endif
};

#if BUILDFLAG(IS_POSIX)
namespace testing {
// Prefix |directory| to the path where the RLZ data file lives, for tests.
void SetRlzStoreDirectory(const base::FilePath& directory);

// Returns the path of the file used as data store.
std::string RlzStoreFilenameStr();
}  // namespace testing
#endif  // BUILDFLAG(IS_POSIX)

}  // namespace rlz_lib

#endif  // RLZ_LIB_RLZ_VALUE_STORE_H_
