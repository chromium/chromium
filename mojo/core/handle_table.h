// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_HANDLE_TABLE_H_
#define MOJO_CORE_HANDLE_TABLE_H_

#include <stdint.h>

#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "mojo/core/dispatcher.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace core {

class MOJO_SYSTEM_IMPL_EXPORT HandleTable
    : public base::trace_event::MemoryDumpProvider {
 public:
  HandleTable();

  HandleTable(const HandleTable&) = delete;
  HandleTable& operator=(const HandleTable&) = delete;

  ~HandleTable() override;

  // HandleTable is thread-hostile. All access should be gated by GetLock().
  base::Lock& GetLock();

  MojoHandle AddDispatcher(scoped_refptr<Dispatcher> dispatcher);

  // Inserts multiple dispatchers received from message transit, populating
  // |handles| with their newly allocated handles. Returns |true| on success.
  bool AddDispatchersFromTransit(
      const std::vector<Dispatcher::DispatcherInTransit>& dispatchers,
      MojoHandle* handles);

  scoped_refptr<Dispatcher> GetDispatcher(MojoHandle handle);
  MojoResult GetAndRemoveDispatcher(MojoHandle,
                                    scoped_refptr<Dispatcher>* dispatcher);

  // Marks handles as busy and populates |dispatchers|. Returns MOJO_RESULT_BUSY
  // if any of the handles are already in transit; MOJO_RESULT_INVALID_ARGUMENT
  // if any of the handles are invalid; or MOJO_RESULT_OK if successful.
  MojoResult BeginTransit(
      const MojoHandle* handles,
      size_t num_handles,
      std::vector<Dispatcher::DispatcherInTransit>* dispatchers);

  void CompleteTransitAndClose(
      const std::vector<Dispatcher::DispatcherInTransit>& dispatchers);
  void CancelTransit(
      const std::vector<Dispatcher::DispatcherInTransit>& dispatchers);

  void GetActiveHandlesForTest(std::vector<MojoHandle>* handles);

 private:
  FRIEND_TEST_ALL_PREFIXES(HandleTableTest, OnMemoryDump);

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  struct Entry {
    explicit Entry(scoped_refptr<Dispatcher> dispatcher);
    ~Entry();
    Entry(const Entry& entry);
    Entry& operator=(const Entry&) = delete;

    const scoped_refptr<Dispatcher> dispatcher;
    bool busy = false;
  };

  // A helper class for storing dispatchers that caches the last fetched
  // dispatcher. This is an optimization for the common case that the same
  // dispatcher is fetched repeatedly. Please see https://crbug.com/1295449 for
  // more details.
  class EntriesAccessor {
   public:
    EntriesAccessor();
    ~EntriesAccessor();

    // Returns whether an Entry was inserted.
    bool Add(MojoHandle handle, Entry entry);

    // Returns nullptr if a dispatcher is not found.
    const scoped_refptr<Dispatcher>* GetDispatcher(MojoHandle handle);

    // Returns nullptr if an entry is not found.
    Entry* GetMutable(MojoHandle handle);

    // See `Remove` below.
    enum RemovalCondition { kRemoveOnlyIfBusy, kRemoveOnlyIfNotBusy };

    // Returns whether an entry was found, and if found, `MOJO_RESULT_BUSY` if
    // `Entry.busy` is true and `MOJO_RESULT_OK` if `Entry.busy` is false. If an
    // entry is not found, `MOJO_RESULT_NOT_FOUND` is returned.
    //
    // If an entry is found, and if `removal_condition` matches `Entry.busy`, it
    // is removed from storage and -- if `dispatcher` is not nullptr -- the
    // corresponding dispatcher is returned in `dispatcher`. Otherwise,
    // `dispatcher` is left unchanged.
    MojoResult Remove(MojoHandle handle,
                      RemovalCondition removal_condition,
                      scoped_refptr<Dispatcher>* dispatcher);

    const std::unordered_map<MojoHandle, Entry>& GetUnderlyingMap() const;

   private:
    std::unordered_map<MojoHandle, Entry> handles_;
    scoped_refptr<Dispatcher> last_read_dispatcher_;
    MojoHandle last_read_handle_ = MOJO_HANDLE_INVALID;
  };

  EntriesAccessor entries_;

  base::Lock lock_;

  uintptr_t next_available_handle_ = 1;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_HANDLE_TABLE_H_
