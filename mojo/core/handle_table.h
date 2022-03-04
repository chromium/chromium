// Copyright 2014 The Chromium Authors. All rights reserved.
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

  scoped_refptr<Dispatcher> GetDispatcher(MojoHandle handle) const;
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
    Entry();
    explicit Entry(scoped_refptr<Dispatcher> dispatcher);
    Entry(const Entry& other);
    ~Entry();

    scoped_refptr<Dispatcher> dispatcher;
    bool busy = false;
  };

  using HandleMap = std::unordered_map<MojoHandle, Entry>;

  HandleMap handles_;
  base::Lock lock_;

  uint64_t next_available_handle_ = 1;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_HANDLE_TABLE_H_
