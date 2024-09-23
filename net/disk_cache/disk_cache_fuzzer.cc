// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cinttypes>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/base/interval.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_fuzzer.pb.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_index.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

// To get a good idea of what a test case is doing, just run the libfuzzer
// target with LPM_DUMP_NATIVE_INPUT=1 prefixed. This will trigger all the
// prints below and will convey exactly what the test case is doing: use this
// instead of trying to print the protobuf as text.

// For code coverage:
// python ./tools/code_coverage/coverage.py disk_cache_lpm_fuzzer       -b
// out/coverage -o out/report       -c 'out/coverage/disk_cache_lpm_fuzzer
// -runs=0 -workers=24  corpus_disk_cache_simple'       -f net/disk_cache

void IOCallback(std::string io_type, int rv);

namespace {
const uint32_t kMaxSizeKB = 128;  // 128KB maximum.
const uint32_t kMaxSize = kMaxSizeKB * 1024;
const uint32_t kMaxEntrySize = kMaxSize * 2;
const uint32_t kNumStreams = 3;  // All caches seem to have 3 streams. TODO do
                                 // other specialized caches have this?
const uint64_t kFirstSavedTime =
    5;  // Totally random number chosen by dice roll. ;)
const uint32_t kMaxNumMillisToWait = 2019;
const int kMaxFdsSimpleCache = 10;

// Known colliding key values taken from SimpleCacheCreateCollision unittest.
const std::string kCollidingKey1 =
    "\xfb\x4e\x9c\x1d\x66\x71\xf7\x54\xa3\x11\xa0\x7e\x16\xa5\x68\xf6";
const std::string kCollidingKey2 =
    "\xbc\x60\x64\x92\xbc\xa0\x5c\x15\x17\x93\x29\x2d\xe4\x21\xbd\x03";

#define IOTYPES_APPLY(F) \
  F(WriteData)           \
  F(ReadData)            \
  F(WriteSparseData)     \
  F(ReadSparseData)      \
  F(DoomAllEntries)      \
  F(DoomEntriesSince)    \
  F(DoomEntriesBetween)  \
  F(GetAvailableRange)   \
  F(DoomKey)

enum class IOType {
#define ENUM_ENTRY(IO_TYPE) IO_TYPE,
  IOTYPES_APPLY(ENUM_ENTRY)
#undef ENUM_ENTRY
};

struct InitGlobals {
  InitGlobals() {
    base::CommandLine::Init(0, nullptr);

    print_comms_ = ::getenv("LPM_DUMP_NATIVE_INPUT");

    // TaskEnvironment requires TestTimeouts initialization to watch for
    // problematic long-running tasks.
    TestTimeouts::Initialize();

    // Mark this thread as an IO_THREAD with MOCK_TIME, and ensure that Now()
    // is driven from the same mock clock.
    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::IO,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);

    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    // Re-using this buffer for write operations may technically be against
    // IOBuffer rules but it shouldn't cause any actual problems.
    buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(
        static_cast<size_t>(kMaxEntrySize));
    CacheTestFillBuffer(buffer_->data(), kMaxEntrySize, false);

#define CREATE_IO_CALLBACK(IO_TYPE) \
  io_callbacks_.push_back(base::BindRepeating(&IOCallback, #IO_TYPE));
    IOTYPES_APPLY(CREATE_IO_CALLBACK)
#undef CREATE_IO_CALLBACK
  }

  // This allows us to mock time for all threads.
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  // Used as a pre-filled buffer for all writes.
  scoped_refptr<net::IOBuffer> buffer_;

  // Should we print debugging info?
  bool print_comms_;

  // List of IO callbacks. They do nothing (except maybe print) but are used by
  // all async entry operations.
  std::vector<base::RepeatingCallback<void(int)>> io_callbacks_;
};

InitGlobals* init_globals = new InitGlobals();
}  // namespace

class DiskCacheLPMFuzzer {
 public:
  DiskCacheLPMFuzzer() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    cache_path_ = temp_dir_.GetPath();
  }

  ~DiskCacheLPMFuzzer();

  void RunCommands(const disk_cache_fuzzer::FuzzCommands& commands);

 private:
  struct EntryInfo {
    EntryInfo() = default;

    EntryInfo(const EntryInfo&) = delete;
    EntryInfo& operator=(const EntryInfo&) = delete;

    // RAW_PTR_EXCLUSION: #addr-of
    RAW_PTR_EXCLUSION disk_cache::Entry* entry_ptr = nullptr;
    std::unique_ptr<TestEntryResultCompletionCallback> tcb;
  };
  void RunTaskForTest(base::OnceClosure closure);

  // Waits for an entry to be ready. Only should be called if there is a pending
  // callback for this entry; i.e. ei->tcb != nullptr.
  // Also takes the rv that the cache entry creation functions return, and does
  // not wait if rv.net_error != net::ERR_IO_PENDING (and would never have
  // called the callback).
  disk_cache::EntryResult WaitOnEntry(
      EntryInfo* ei,
      disk_cache::EntryResult result =
          disk_cache::EntryResult::MakeError(net::ERR_IO_PENDING));

  // Used as a callback for entry-opening backend calls. Will record the entry
  // in the map as usable and will release any entry-specific calls waiting for
  // the entry to be ready.
  void OpenCacheEntryCallback(uint64_t entry_id,
                              bool async,
                              bool set_is_sparse,
                              disk_cache::EntryResult result);

  // Waits for the entry to finish opening, in the async case. Then, if the
  // entry is successfully open (callback returns net::OK, or was already
  // successfully opened), check if the entry_ptr == nullptr. If so, the
  // entry has been closed.
  bool IsValidEntry(EntryInfo* ei);

  // Closes any non-nullptr entries in open_cache_entries_.
  void CloseAllRemainingEntries();

  void HandleSetMaxSize(const disk_cache_fuzzer::SetMaxSize&);
  void CreateBackend(
      disk_cache_fuzzer::FuzzCommands::CacheBackend cache_backend,
      uint32_t mask,
      net::CacheType type,
      bool simple_cache_wait_for_index);

  // Places to keep our cache files.
  base::FilePath cache_path_;
  base::ScopedTempDir temp_dir_;

  // Pointers to our backend. Only one of block_impl_, simple_cache_impl_, and
  // mem_cache_ are active at one time.
  std::unique_ptr<disk_cache::Backend> cache_;
  raw_ptr<disk_cache::BackendImpl> block_impl_ = nullptr;
  std::unique_ptr<disk_cache::SimpleFileTracker> simple_file_tracker_;
  raw_ptr<disk_cache::SimpleBackendImpl> simple_cache_impl_ = nullptr;
  raw_ptr<disk_cache::MemBackendImpl> mem_cache_ = nullptr;

  // Maximum size of the cache, that we have currently set.
  uint32_t max_size_ = kMaxSize;

  // This "consistent hash table" keeys track of the keys we've added to the
  // backend so far. This should always be indexed by a "key_id" from a
  // protobuf.
  std::map<uint64_t, std::string> created_cache_entries_;
  // This "consistent hash table" keeps track of all opened entries we have from
  // the backend, and also contains some nullptr's where entries were already
  // closed. This should always be indexed by an "entry_id" from a protobuf.
  // When destructed, we close all entries that are still open in order to avoid
  // memory leaks.
  std::map<uint64_t, EntryInfo> open_cache_entries_;
  // This "consistent hash table" keeps track of all times we have saved, so
  // that we can call backend methods like DoomEntriesSince or
  // DoomEntriesBetween with sane timestamps. This should always be indexed by a
  // "time_id" from a protobuf.
  std::map<uint64_t, base::Time> saved_times_;
  // This "consistent hash table" keeps tack of all the iterators we have open
  // from the backend. This should always be indexed by a "it_id" from a
  // protobuf.
  std::map<uint64_t, std::unique_ptr<disk_cache::Backend::Iterator>>
      open_iterators_;

  // This maps keeps track of the sparsity of each entry, using their pointers.
  // TODO(mpdenton) remove if CreateEntry("Key0"); WriteData("Key0", index = 2,
  // ...); WriteSparseData("Key0", ...); is supposed to be valid.
  // Then we can just use CouldBeSparse before the WriteData.
  std::map<disk_cache::Entry*, bool> sparse_entry_tracker_;
};

#define MAYBE_PRINT               \
  if (init_globals->print_comms_) \
  std::cout

inline base::RepeatingCallback<void(int)> GetIOCallback(IOType iot) {
  return init_globals->io_callbacks_[static_cast<int>(iot)];
}

std::string ToKey(uint64_t key_num) {
  // Use one of the two colliding key values in 1% of executions.
  if (key_num % 100 == 99)
    return kCollidingKey1;
  if (key_num % 100 == 98)
    return kCollidingKey2;

  // Otherwise, use a value based on the key id and fuzzy padding.
  std::string padding(key_num & 0xFFFF, 'A');
  return "Key" + padding + base::NumberToString(key_num);
}

net::RequestPriority GetRequestPriority(
    disk_cache_fuzzer::RequestPriority lpm_pri) {
  CHECK(net::MINIMUM_PRIORITY <= static_cast<int>(lpm_pri) &&
        static_cast<int>(lpm_pri) <= net::MAXIMUM_PRIORITY);
  return static_cast<net::RequestPriority>(lpm_pri);
}

net::CacheType GetCacheTypeAndPrint(
    disk_cache_fuzzer::FuzzCommands::CacheType type,
    disk_cache_fuzzer::FuzzCommands::CacheBackend backend) {
  switch (type) {
    case disk_cache_fuzzer::FuzzCommands::APP_CACHE:
      MAYBE_PRINT << "Cache type = APP_CACHE." << std::endl;
      return net::CacheType::APP_CACHE;
    case disk_cache_fuzzer::FuzzCommands::REMOVED_MEDIA_CACHE:
      // Media cache no longer in use; handle as HTTP_CACHE
      MAYBE_PRINT << "Cache type = REMOVED_MEDIA_CACHE." << std::endl;
      return net::CacheType::DISK_CACHE;
    case disk_cache_fuzzer::FuzzCommands::SHADER_CACHE:
      MAYBE_PRINT << "Cache type = SHADER_CACHE." << std::endl;
      return net::CacheType::SHADER_CACHE;
    case disk_cache_fuzzer::FuzzCommands::PNACL_CACHE:
      // Simple cache won't handle PNACL_CACHE.
      if (backend == disk_cache_fuzzer::FuzzCommands::SIMPLE) {
        MAYBE_PRINT << "Cache type = DISK_CACHE." << std::endl;
        return net::CacheType::DISK_CACHE;
      }
      MAYBE_PRINT << "Cache type = PNACL_CACHE." << std::endl;
      return net::CacheType::PNACL_CACHE;
    case disk_cache_fuzzer::FuzzCommands::GENERATED_BYTE_CODE_CACHE:
      MAYBE_PRINT << "Cache type = GENERATED_BYTE_CODE_CACHE." << std::endl;
      return net::CacheType::GENERATED_BYTE_CODE_CACHE;
    case disk_cache_fuzzer::FuzzCommands::GENERATED_NATIVE_CODE_CACHE:
      MAYBE_PRINT << "Cache type = GENERATED_NATIVE_CODE_CACHE." << std::endl;
      return net::CacheType::GENERATED_NATIVE_CODE_CACHE;
    case disk_cache_fuzzer::FuzzCommands::DISK_CACHE:
      MAYBE_PRINT << "Cache type = DISK_CACHE." << std::endl;
      return net::CacheType::DISK_CACHE;
  }
}

void IOCallback(std::string io_type, int rv) {
  MAYBE_PRINT << " [Async IO (" << io_type << ") = " << rv << "]" << std::endl;
}

/*
 * Consistent hashing inspired map for fuzzer state.
 * If we stored open cache entries in a hash table mapping cache_entry_id ->
 * disk_cache::Entry*, then it would be highly unlikely that any subsequent
 * "CloseEntry" or "WriteData" etc. command would come up with an ID that would
 * correspond to a valid entry in the hash table. The optimal solution is for
 * libfuzzer to generate CloseEntry commands with an ID that matches the ID of a
 * previous OpenEntry command. But libfuzzer is stateless and should stay that
 * way.
 *
 * On the other hand, if we stored entries in a vector, and on a CloseEntry
 * command we took the entry at CloseEntry.id % (size of entries vector), we
 * would always generate correct CloseEntries. This is good, but all
 * dumb/general minimization techniques stop working, because deleting a single
 * OpenEntry command changes the indexes of every entry in the vector from then
 * on.
 *
 * So, we use something that's more stable for minimization: consistent hashing.
 * Basically, when we see a CloseEntry.id, we take the entry in the table that
 * has the next highest id (wrapping when there is no higher entry).
 *
 * This makes us resilient to deleting irrelevant OpenEntry commands. But, if we
 * delete from the table on CloseEntry commands, we still screw up all the
 * indexes during minimization. We'll get around this by not deleting entries
 * after CloseEntry commands, but that will result in a slightly less efficient
 * fuzzer, as if there are many closed entries in the table, many of the *Entry
 * commands will be useless. It seems like a decent balance between generating
 * useful fuzz commands and effective minimization.
 */
template <typename T>
typename std::map<uint64_t, T>::iterator GetNextValue(
    typename std::map<uint64_t, T>* entries,
    uint64_t val) {
  auto iter = entries->lower_bound(val);
  if (iter != entries->end())
    return iter;
  // Wrap to 0
  iter = entries->lower_bound(0);
  if (iter != entries->end())
    return iter;

  return entries->end();
}

void DiskCacheLPMFuzzer::RunTaskForTest(base::OnceClosure closure) {
  if (!block_impl_) {
    std::move(closure).Run();
    return;
  }

  net::TestCompletionCallback cb;
  int rv = block_impl_->RunTaskForTest(std::move(closure), cb.callback());
  CHECK_EQ(cb.GetResult(rv), net::OK);
}

// Resets the cb in the map so that WriteData and other calls that work on an
// entry don't wait for its result.
void DiskCacheLPMFuzzer::OpenCacheEntryCallback(
    uint64_t entry_id,
    bool async,
    bool set_is_sparse,
    disk_cache::EntryResult result) {
  // TODO(mpdenton) if this fails should we delete the entry entirely?
  // Would need to mark it for deletion and delete it later, as
  // IsValidEntry might be waiting for it.
  EntryInfo* ei = &open_cache_entries_[entry_id];

  if (async) {
    int rv = result.net_error();
    ei->entry_ptr = result.ReleaseEntry();
    // We are responsible for setting things up.
    if (set_is_sparse && ei->entry_ptr) {
      sparse_entry_tracker_[ei->entry_ptr] = true;
    }
    if (ei->entry_ptr) {
      MAYBE_PRINT << " [Async opening of cache entry for \""
                  << ei->entry_ptr->GetKey() << "\" callback (rv = " << rv
                  << ")]" << std::endl;
    }
    // Unblock any subsequent ops waiting for this --- they don't care about
    // the actual return value, but use something distinctive for debugging.
    ei->tcb->callback().Run(
        disk_cache::EntryResult::MakeError(net::ERR_FILE_VIRUS_INFECTED));
  } else {
    // The operation code will pull the result out of the completion callback,
    // so hand it to it.
    ei->tcb->callback().Run(std::move(result));
  }
}

disk_cache::EntryResult DiskCacheLPMFuzzer::WaitOnEntry(
    EntryInfo* ei,
    disk_cache::EntryResult result) {
  CHECK(ei->tcb);
  result = ei->tcb->GetResult(std::move(result));

  // Reset the callback so nobody accidentally waits on a callback that never
  // comes.
  ei->tcb.reset();
  return result;
}

bool DiskCacheLPMFuzzer::IsValidEntry(EntryInfo* ei) {
  if (ei->tcb) {
    // If we have a callback, we are the first to access this async-created
    // entry. Wait for it, and then delete it so nobody waits on it again.
    WaitOnEntry(ei);
  }
  // entry_ptr will be nullptr if the entry has been closed.
  return ei->entry_ptr != nullptr;
}

/*
 * Async implementation:
 1. RunUntilIdle at the top of the loop to handle any callbacks we've been
 posted from the backend thread.
 2. Only the entry creation functions have important callbacks. The good thing
 is backend destruction will cancel these operations. The entry creation
 functions simply need to keep the entry_ptr* alive until the callback is
 posted, and then need to make sure the entry_ptr is added to the map in order
 to Close it in the destructor.
    As for iterators, it's unclear whether closing an iterator will cancel
 callbacks.

 Problem: WriteData (and similar) calls will fail on the entry_id until the
 callback happens. So, I should probably delay these calls or otherwise will
 have very unreliable test cases. These are the options:
 1. Queue up WriteData (etc.) calls in some map, such that when the OpenEntry
 callback runs, the WriteData calls will all run.
 2. Just sit there and wait for the entry to be ready.

 #2 is probably best as it doesn't prevent any interesting cases and is much
 simpler.
 */

void DiskCacheLPMFuzzer::RunCommands(
    const disk_cache_fuzzer::FuzzCommands& commands) {
  // Skip too long command sequences, they are counterproductive for fuzzing.
  // The number was chosen empirically using the existing fuzzing corpus.
  if (commands.fuzz_commands_size() > 129)
    return;

  uint32_t mask =
      commands.has_set_mask() ? (commands.set_mask() ? 0x1 : 0xf) : 0;
  net::CacheType type =
      GetCacheTypeAndPrint(commands.cache_type(), commands.cache_backend());
  CreateBackend(commands.cache_backend(), mask, type,
                commands.simple_cache_wait_for_index());
  MAYBE_PRINT << "CreateBackend()" << std::endl;

  if (commands.has_set_max_size()) {
    HandleSetMaxSize(commands.set_max_size());
  }

  {
    base::Time curr_time = base::Time::Now();
    saved_times_[kFirstSavedTime] = curr_time;
    // MAYBE_PRINT << "Saved initial time " << curr_time << std::endl;
  }

  for (const disk_cache_fuzzer::FuzzCommand& command :
       commands.fuzz_commands()) {
    // Handle any callbacks that other threads may have posted to us in the
    // meantime, so any successful async OpenEntry's (etc.) add their
    // entry_ptr's to the map.
    init_globals->task_environment_->RunUntilIdle();

    switch (command.fuzz_command_oneof_case()) {
      case disk_cache_fuzzer::FuzzCommand::kSetMaxSize: {
        HandleSetMaxSize(command.set_max_size());
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kCreateEntry: {
        if (!cache_)
          continue;

        const disk_cache_fuzzer::CreateEntry& ce = command.create_entry();
        uint64_t key_id = ce.key_id();
        uint64_t entry_id = ce.entry_id();
        net::RequestPriority pri = GetRequestPriority(ce.pri());
        bool async = ce.async();
        bool is_sparse = ce.is_sparse();

        if (open_cache_entries_.find(entry_id) != open_cache_entries_.end())
          continue;  // Don't overwrite a currently open cache entry.

        std::string key_str = ToKey(key_id);
        created_cache_entries_[key_id] = key_str;

        EntryInfo* entry_info = &open_cache_entries_[entry_id];

        entry_info->tcb = std::make_unique<TestEntryResultCompletionCallback>();
        disk_cache::EntryResultCallback cb =
            base::BindOnce(&DiskCacheLPMFuzzer::OpenCacheEntryCallback,
                           base::Unretained(this), entry_id, async, is_sparse);

        MAYBE_PRINT << "CreateEntry(\"" << key_str
                    << "\", set_is_sparse = " << is_sparse
                    << ") = " << std::flush;
        disk_cache::EntryResult result =
            cache_->CreateEntry(key_str, pri, std::move(cb));
        if (!async || result.net_error() != net::ERR_IO_PENDING) {
          result = WaitOnEntry(entry_info, std::move(result));
          int rv = result.net_error();

          // Ensure we mark sparsity, save entry if the callback never ran.
          if (rv == net::OK) {
            entry_info->entry_ptr = result.ReleaseEntry();
            sparse_entry_tracker_[entry_info->entry_ptr] = is_sparse;
          }
          MAYBE_PRINT << rv << std::endl;
        } else {
          MAYBE_PRINT << "net::ERR_IO_PENDING (async)" << std::endl;
        }
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kOpenEntry: {
        if (!cache_)
          continue;

        const disk_cache_fuzzer::OpenEntry& oe = command.open_entry();
        uint64_t key_id = oe.key_id();
        uint64_t entry_id = oe.entry_id();
        net::RequestPriority pri = GetRequestPriority(oe.pri());
        bool async = oe.async();

        if (created_cache_entries_.empty())
          continue;

        if (open_cache_entries_.find(entry_id) != open_cache_entries_.end())
          continue;  // Don't overwrite a currently open cache entry.

        EntryInfo* entry_info = &open_cache_entries_[entry_id];

        entry_info->tcb = std::make_unique<TestEntryResultCompletionCallback>();
        disk_cache::EntryResultCallback cb =
            base::BindOnce(&DiskCacheLPMFuzzer::OpenCacheEntryCallback,
                           base::Unretained(this), entry_id, async, false);

        auto key_it = GetNextValue(&created_cache_entries_, key_id);
        MAYBE_PRINT << "OpenEntry(\"" << key_it->second
                    << "\") = " << std::flush;
        disk_cache::EntryResult result =
            cache_->OpenEntry(key_it->second, pri, std::move(cb));
        if (!async || result.net_error() != net::ERR_IO_PENDING) {
          result = WaitOnEntry(entry_info, std::move(result));
          int rv = result.net_error();
          if (rv == net::OK)
            entry_info->entry_ptr = result.ReleaseEntry();
          MAYBE_PRINT << rv << std::endl;
        } else {
          MAYBE_PRINT << "net::ERR_IO_PENDING (async)" << std::endl;
        }
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kOpenOrCreateEntry: {
        if (!cache_)
          continue;

        const disk_cache_fuzzer::OpenOrCreateEntry& ooce =
            command.open_or_create_entry();
        uint64_t key_id = ooce.key_id();
        uint64_t entry_id = ooce.entry_id();
        net::RequestPriority pri = GetRequestPriority(ooce.pri());
        bool async = ooce.async();
        bool is_sparse = ooce.is_sparse();

        if (open_cache_entries_.find(entry_id) != open_cache_entries_.end())
          continue;  // Don't overwrite a currently open cache entry.

        std::string key_str;
        // If our proto tells us to create a new entry, create a new entry, just
        // with OpenOrCreateEntry.
        if (ooce.create_new()) {
          // Use a possibly new key.
          key_str = ToKey(key_id);
          created_cache_entries_[key_id] = key_str;
        } else {
          if (created_cache_entries_.empty())
            continue;
          auto key_it = GetNextValue(&created_cache_entries_, key_id);
          key_str = key_it->second;
        }

        // Setup for callbacks.

        EntryInfo* entry_info = &open_cache_entries_[entry_id];

        entry_info->tcb = std::make_unique<TestEntryResultCompletionCallback>();
        disk_cache::EntryResultCallback cb =
            base::BindOnce(&DiskCacheLPMFuzzer::OpenCacheEntryCallback,
                           base::Unretained(this), entry_id, async, is_sparse);

        // Will only be set as sparse if it is created and not opened.
        MAYBE_PRINT << "OpenOrCreateEntry(\"" << key_str
                    << "\", set_is_sparse = " << is_sparse
                    << ") = " << std::flush;
        disk_cache::EntryResult result =
            cache_->OpenOrCreateEntry(key_str, pri, std::move(cb));
        if (!async || result.net_error() != net::ERR_IO_PENDING) {
          result = WaitOnEntry(entry_info, std::move(result));
          int rv = result.net_error();
          bool opened = result.opened();
          entry_info->entry_ptr = result.ReleaseEntry();
          // Ensure we mark sparsity, even if the callback never ran.
          if (rv == net::OK && !opened)
            sparse_entry_tracker_[entry_info->entry_ptr] = is_sparse;
          MAYBE_PRINT << rv << ", opened = " << opened << std::endl;
        } else {
          MAYBE_PRINT << "net::ERR_IO_PENDING (async)" << std::endl;
        }
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kCloseEntry: {
        if (open_cache_entries_.empty())
          continue;

        auto entry_it = GetNextValue(&open_cache_entries_,
                                     command.close_entry().entry_id());
        if (!IsValidEntry(&entry_it->second))
          continue;

        MAYBE_PRINT << "CloseEntry(\"" << entry_it->second.entry_ptr->GetKey()
                    << "\")" << std::endl;
        entry_it->second.entry_ptr->Close();

        // Set the entry_ptr to nullptr to ensure no one uses it anymore.
        entry_it->second.entry_ptr = nullptr;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDoomEntry: {
        if (open_cache_entries_.empty())
          continue;

        auto entry_it =
            GetNextValue(&open_cache_entries_, command.doom_entry().entry_id());
        if (!IsValidEntry(&entry_it->second))
          continue;

        MAYBE_PRINT << "DoomEntry(\"" << entry_it->second.entry_ptr->GetKey()
                    << "\")" << std::endl;
        entry_it->second.entry_ptr->Doom();
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kWriteData: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::WriteData& wd = command.write_data();
        auto entry_it = GetNextValue(&open_cache_entries_, wd.entry_id());
        if (!IsValidEntry(&entry_it->second))
          continue;

        int index = 0;  // if it's sparse, these non-sparse aware streams must
                        // read from stream 0 according to the spec.
                        // Implementations might have weaker constraints.
        if (!sparse_entry_tracker_[entry_it->second.entry_ptr])
          index = wd.index() % kNumStreams;
        uint32_t offset = wd.offset() % kMaxEntrySize;
        size_t size = wd.size() % kMaxEntrySize;
        bool async = wd.async();

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::WriteData);

        MAYBE_PRINT << "WriteData(\"" << entry_it->second.entry_ptr->GetKey()
                    << "\", index = " << index << ", offset = " << offset
                    << ", size = " << size << ", truncate = " << wd.truncate()
                    << ")" << std::flush;
        int rv = entry_it->second.entry_ptr->WriteData(
            index, offset, init_globals->buffer_.get(), size, std::move(cb),
            wd.truncate());
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kReadData: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::ReadData& wd = command.read_data();
        auto entry_it = GetNextValue(&open_cache_entries_, wd.entry_id());
        if (!IsValidEntry(&entry_it->second))
          continue;

        int index = 0;  // if it's sparse, these non-sparse aware streams must
                        // read from stream 0 according to the spec.
                        // Implementations might weaker constraints?
        if (!sparse_entry_tracker_[entry_it->second.entry_ptr])
          index = wd.index() % kNumStreams;
        uint32_t offset = wd.offset() % kMaxEntrySize;
        size_t size = wd.size() % kMaxEntrySize;
        bool async = wd.async();
        auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::ReadData);

        MAYBE_PRINT << "ReadData(\"" << entry_it->second.entry_ptr->GetKey()
                    << "\", index = " << index << ", offset = " << offset
                    << ", size = " << size << ")" << std::flush;
        int rv = entry_it->second.entry_ptr->ReadData(
            index, offset, buffer.get(), size, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kWriteSparseData: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::WriteSparseData& wsd =
            command.write_sparse_data();
        auto entry_it = GetNextValue(&open_cache_entries_, wsd.entry_id());
        if (!IsValidEntry(&entry_it->second) ||
            !sparse_entry_tracker_[entry_it->second.entry_ptr])
          continue;

        uint64_t offset = wsd.offset();
        if (wsd.cap_offset())
          offset %= kMaxEntrySize;
        size_t size = wsd.size() % kMaxEntrySize;
        bool async = wsd.async();

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::WriteSparseData);
        MAYBE_PRINT << "WriteSparseData(\""
                    << entry_it->second.entry_ptr->GetKey()
                    << "\", offset = " << offset << ", size = " << size << ")"
                    << std::flush;
        int rv = entry_it->second.entry_ptr->WriteSparseData(
            offset, init_globals->buffer_.get(), size, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kReadSparseData: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::ReadSparseData& rsd =
            command.read_sparse_data();
        auto entry_it = GetNextValue(&open_cache_entries_, rsd.entry_id());
        if (!IsValidEntry(&entry_it->second) ||
            !sparse_entry_tracker_[entry_it->second.entry_ptr])
          continue;

        uint64_t offset = rsd.offset();
        if (rsd.cap_offset())
          offset %= kMaxEntrySize;
        size_t size = rsd.size() % kMaxEntrySize;
        bool async = rsd.async();
        auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(size);

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::ReadSparseData);

        MAYBE_PRINT << "ReadSparseData(\""
                    << entry_it->second.entry_ptr->GetKey()
                    << "\", offset = " << offset << ", size = " << size << ")"
                    << std::flush;
        int rv = entry_it->second.entry_ptr->ReadSparseData(
            offset, buffer.get(), size, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDoomAllEntries: {
        if (!cache_)
          continue;
        bool async = command.doom_all_entries().async();

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::DoomAllEntries);
        MAYBE_PRINT << "DoomAllEntries()" << std::flush;
        int rv = cache_->DoomAllEntries(std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kFlushQueueForTest: {
        // Blockfile-cache specific method.
        if (!block_impl_)
          return;

        net::TestCompletionCallback cb;
        MAYBE_PRINT << "FlushQueueForTest()" << std::endl;
        int rv = block_impl_->FlushQueueForTest(cb.callback());
        CHECK_EQ(cb.GetResult(rv), net::OK);
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kCreateIterator: {
        if (!cache_)
          continue;
        uint64_t it_id = command.create_iterator().it_id();
        MAYBE_PRINT << "CreateIterator(), id = " << it_id << std::endl;
        open_iterators_[it_id] = cache_->CreateIterator();
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kIteratorOpenNextEntry: {
        const disk_cache_fuzzer::IteratorOpenNextEntry& ione =
            command.iterator_open_next_entry();

        uint64_t it_id = ione.it_id();
        uint64_t entry_id = ione.entry_id();
        bool async = ione.async();

        if (open_iterators_.empty())
          continue;

        if (open_cache_entries_.find(entry_id) != open_cache_entries_.end())
          continue;  // Don't overwrite a currently
                     // open cache entry.

        auto iterator_it = GetNextValue(&open_iterators_, it_id);

        EntryInfo* entry_info = &open_cache_entries_[entry_id];

        entry_info->tcb = std::make_unique<TestEntryResultCompletionCallback>();
        disk_cache::EntryResultCallback cb =
            base::BindOnce(&DiskCacheLPMFuzzer::OpenCacheEntryCallback,
                           base::Unretained(this), entry_id, async, false);

        MAYBE_PRINT << "Iterator(" << ione.it_id()
                    << ").OpenNextEntry() = " << std::flush;
        disk_cache::EntryResult result =
            iterator_it->second->OpenNextEntry(std::move(cb));
        if (!async || result.net_error() != net::ERR_IO_PENDING) {
          result = WaitOnEntry(entry_info, std::move(result));
          int rv = result.net_error();
          entry_info->entry_ptr = result.ReleaseEntry();
          // Print return value, and key if applicable.
          if (!entry_info->entry_ptr) {
            MAYBE_PRINT << rv << std::endl;
          } else {
            MAYBE_PRINT << rv << ", key = " << entry_info->entry_ptr->GetKey()
                        << std::endl;
          }
        } else {
          MAYBE_PRINT << "net::ERR_IO_PENDING (async)" << std::endl;
        }
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kFastForwardBy: {
        base::TimeDelta to_wait =
            base::Milliseconds(command.fast_forward_by().capped_num_millis() %
                               kMaxNumMillisToWait);
        MAYBE_PRINT << "FastForwardBy(" << to_wait << ")" << std::endl;
        init_globals->task_environment_->FastForwardBy(to_wait);

        base::Time curr_time = base::Time::Now();
        saved_times_[command.fast_forward_by().time_id()] = curr_time;
        // MAYBE_PRINT << "Saved time " << curr_time << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDoomEntriesSince: {
        if (!cache_)
          continue;
        // App cache does not keep track of LRU timestamps so this method cannot
        // be used.
        if (type == net::APP_CACHE)
          continue;
        if (saved_times_.empty())
          continue;

        const disk_cache_fuzzer::DoomEntriesSince& des =
            command.doom_entries_since();
        auto time_it = GetNextValue(&saved_times_, des.time_id());
        bool async = des.async();

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::DoomEntriesSince);

        MAYBE_PRINT << "DoomEntriesSince(" << time_it->second << ")"
                    << std::flush;
        int rv = cache_->DoomEntriesSince(time_it->second, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDoomEntriesBetween: {
        if (!cache_)
          continue;
        // App cache does not keep track of LRU timestamps so this method cannot
        // be used.
        if (type == net::APP_CACHE)
          continue;
        if (saved_times_.empty())
          continue;

        const disk_cache_fuzzer::DoomEntriesBetween& deb =
            command.doom_entries_between();
        auto time_it1 = GetNextValue(&saved_times_, deb.time_id1());
        auto time_it2 = GetNextValue(&saved_times_, deb.time_id2());
        base::Time time1 = time_it1->second;
        base::Time time2 = time_it2->second;
        if (time1 > time2)
          std::swap(time1, time2);
        bool async = deb.async();

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::DoomEntriesBetween);

        MAYBE_PRINT << "DoomEntriesBetween(" << time1 << ", " << time2 << ")"
                    << std::flush;
        int rv = cache_->DoomEntriesBetween(time1, time2, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kOnExternalCacheHit: {
        if (!cache_)
          continue;
        if (created_cache_entries_.empty())
          continue;

        uint64_t key_id = command.on_external_cache_hit().key_id();

        auto key_it = GetNextValue(&created_cache_entries_, key_id);
        MAYBE_PRINT << "OnExternalCacheHit(\"" << key_it->second << "\")"
                    << std::endl;
        cache_->OnExternalCacheHit(key_it->second);
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kTrimForTest: {
        // Blockfile-cache specific method.
        if (!block_impl_ || type != net::DISK_CACHE)
          return;

        MAYBE_PRINT << "TrimForTest()" << std::endl;

        RunTaskForTest(base::BindOnce(&disk_cache::BackendImpl::TrimForTest,
                                      base::Unretained(block_impl_),
                                      command.trim_for_test().empty()));
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kTrimDeletedListForTest: {
        // Blockfile-cache specific method.
        if (!block_impl_ || type != net::DISK_CACHE)
          return;

        MAYBE_PRINT << "TrimDeletedListForTest()" << std::endl;

        RunTaskForTest(
            base::BindOnce(&disk_cache::BackendImpl::TrimDeletedListForTest,
                           base::Unretained(block_impl_),
                           command.trim_deleted_list_for_test().empty()));
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kGetAvailableRange: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::GetAvailableRange& gar =
            command.get_available_range();
        auto entry_it = GetNextValue(&open_cache_entries_, gar.entry_id());
        if (!IsValidEntry(&entry_it->second) ||
            !sparse_entry_tracker_[entry_it->second.entry_ptr])
          continue;

        disk_cache::Entry* entry = entry_it->second.entry_ptr;
        uint32_t offset = gar.offset() % kMaxEntrySize;
        uint32_t len = gar.len() % kMaxEntrySize;
        bool async = gar.async();

        auto result_checker = base::BindRepeating(
            [](net::CompletionOnceCallback callback, uint32_t offset,
               uint32_t len, const disk_cache::RangeResult& result) {
              std::move(callback).Run(result.net_error);

              if (result.net_error <= 0)
                return;

              // Make sure that the result is contained in what was
              // requested. It doesn't have to be the same even if there was
              // an exact corresponding write, since representation of ranges
              // may be imprecise, and here we don't know that there was.

              // No overflow thanks to % kMaxEntrySize.
              net::Interval<uint32_t> requested(offset, offset + len);

              uint32_t range_start, range_end;
              base::CheckedNumeric<uint64_t> range_start64(result.start);
              CHECK(range_start64.AssignIfValid(&range_start));
              base::CheckedNumeric<uint64_t> range_end64 =
                  range_start + result.available_len;
              CHECK(range_end64.AssignIfValid(&range_end));
              net::Interval<uint32_t> gotten(range_start, range_end);

              CHECK(requested.Contains(gotten));
            },
            GetIOCallback(IOType::GetAvailableRange), offset, len);

        TestRangeResultCompletionCallback tcb;
        disk_cache::RangeResultCallback cb =
            !async ? tcb.callback() : result_checker;

        MAYBE_PRINT << "GetAvailableRange(\"" << entry->GetKey() << "\", "
                    << offset << ", " << len << ")" << std::flush;
        disk_cache::RangeResult result =
            entry->GetAvailableRange(offset, len, std::move(cb));

        if (result.net_error != net::ERR_IO_PENDING) {
          // Run the checker callback ourselves.
          result_checker.Run(result);
        } else if (!async) {
          // In this case the callback will be run by the backend, so we don't
          // need to do it manually.
          result = tcb.GetResult(result);
        }

        // Finally, take care of printing.
        if (async && result.net_error == net::ERR_IO_PENDING) {
          MAYBE_PRINT << " = net::ERR_IO_PENDING (async)" << std::endl;
        } else {
          MAYBE_PRINT << " = " << result.net_error
                      << ", start = " << result.start
                      << ", available_len = " << result.available_len;
          if (result.net_error < 0) {
            MAYBE_PRINT << ", error to string: "
                        << net::ErrorToShortString(result.net_error)
                        << std::endl;
          } else {
            MAYBE_PRINT << std::endl;
          }
        }
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kCancelSparseIo: {
        if (open_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::CancelSparseIO& csio =
            command.cancel_sparse_io();
        auto entry_it = GetNextValue(&open_cache_entries_, csio.entry_id());
        if (!IsValidEntry(&entry_it->second))
          continue;

        MAYBE_PRINT << "CancelSparseIO(\""
                    << entry_it->second.entry_ptr->GetKey() << "\")"
                    << std::endl;
        entry_it->second.entry_ptr->CancelSparseIO();
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDoomKey: {
        if (!cache_)
          continue;
        if (created_cache_entries_.empty())
          continue;

        const disk_cache_fuzzer::DoomKey& dk = command.doom_key();
        uint64_t key_id = dk.key_id();
        net::RequestPriority pri = GetRequestPriority(dk.pri());
        bool async = dk.async();

        auto key_it = GetNextValue(&created_cache_entries_, key_id);

        net::TestCompletionCallback tcb;
        net::CompletionOnceCallback cb =
            !async ? tcb.callback() : GetIOCallback(IOType::DoomKey);

        MAYBE_PRINT << "DoomKey(\"" << key_it->second << "\")" << std::flush;
        int rv = cache_->DoomEntry(key_it->second, pri, std::move(cb));
        if (!async)
          rv = tcb.GetResult(rv);
        MAYBE_PRINT << " = " << rv << std::endl;

        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kDestructBackend: {
        // Block_impl_ will leak if we destruct the backend without closing
        // previous entries.
        // TODO(mpdenton) consider creating a separate fuzz target that allows
        // closing the |block_impl_| and ignore leaks.
        if (block_impl_ || !cache_)
          continue;

        const disk_cache_fuzzer::DestructBackend& db =
            command.destruct_backend();
        // Only sometimes actually destruct the backend.
        if (!db.actually_destruct1() || !db.actually_destruct2())
          continue;

        MAYBE_PRINT << "~Backend(). Backend destruction." << std::endl;
        cache_.reset();
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::kAddRealDelay: {
        if (!command.add_real_delay().actually_delay())
          continue;

        MAYBE_PRINT << "AddRealDelay(1ms)" << std::endl;
        base::PlatformThread::Sleep(base::Milliseconds(1));
        break;
      }
      case disk_cache_fuzzer::FuzzCommand::FUZZ_COMMAND_ONEOF_NOT_SET: {
        continue;
      }
    }
  }
}

void DiskCacheLPMFuzzer::HandleSetMaxSize(
    const disk_cache_fuzzer::SetMaxSize& sms) {
  if (!cache_)
    return;

  max_size_ = sms.size();
  max_size_ %= kMaxSizeKB;
  max_size_ *= 1024;
  MAYBE_PRINT << "SetMaxSize(" << max_size_ << ")" << std::endl;
  if (simple_cache_impl_)
    CHECK_EQ(true, simple_cache_impl_->SetMaxSize(max_size_));

  if (block_impl_)
    CHECK_EQ(true, block_impl_->SetMaxSize(max_size_));

  if (mem_cache_)
    CHECK_EQ(true, mem_cache_->SetMaxSize(max_size_));
}

void DiskCacheLPMFuzzer::CreateBackend(
    disk_cache_fuzzer::FuzzCommands::CacheBackend cache_backend,
    uint32_t mask,
    net::CacheType type,
    bool simple_cache_wait_for_index) {
  if (cache_backend == disk_cache_fuzzer::FuzzCommands::IN_MEMORY) {
    MAYBE_PRINT << "Using in-memory cache." << std::endl;
    auto cache = std::make_unique<disk_cache::MemBackendImpl>(nullptr);
    mem_cache_ = cache.get();
    cache_ = std::move(cache);
    CHECK(cache_);
  } else if (cache_backend == disk_cache_fuzzer::FuzzCommands::SIMPLE) {
    MAYBE_PRINT << "Using simple cache." << std::endl;
    net::TestCompletionCallback cb;
    // We limit ourselves to 64 fds since OS X by default gives us 256.
    // (Chrome raises the number on startup, but the fuzzer doesn't).
    if (!simple_file_tracker_)
      simple_file_tracker_ =
          std::make_unique<disk_cache::SimpleFileTracker>(kMaxFdsSimpleCache);
    auto simple_backend = std::make_unique<disk_cache::SimpleBackendImpl>(
        /*file_operations=*/nullptr, cache_path_,
        /*cleanup_tracker=*/nullptr, simple_file_tracker_.get(), max_size_,
        type, /*net_log=*/nullptr);
    simple_backend->Init(cb.callback());
    CHECK_EQ(cb.WaitForResult(), net::OK);
    simple_cache_impl_ = simple_backend.get();
    cache_ = std::move(simple_backend);

    if (simple_cache_wait_for_index) {
      MAYBE_PRINT << "Waiting for simple cache index to be ready..."
                  << std::endl;
      net::TestCompletionCallback wait_for_index_cb;
      simple_cache_impl_->index()->ExecuteWhenReady(
          wait_for_index_cb.callback());
      int rv = wait_for_index_cb.WaitForResult();
      CHECK_EQ(rv, net::OK);
    }
  } else {
    MAYBE_PRINT << "Using blockfile cache";
    std::unique_ptr<disk_cache::BackendImpl> cache;
    if (mask) {
      MAYBE_PRINT << ", mask = " << mask << std::endl;
      cache = std::make_unique<disk_cache::BackendImpl>(
          cache_path_, mask,
          /* runner = */ nullptr, type,
          /* net_log = */ nullptr);
    } else {
      MAYBE_PRINT << "." << std::endl;
      cache = std::make_unique<disk_cache::BackendImpl>(
          cache_path_,
          /* cleanup_tracker = */ nullptr,
          /* runner = */ nullptr, type,
          /* net_log = */ nullptr);
    }
    block_impl_ = cache.get();
    cache_ = std::move(cache);
    CHECK(cache_);
    // TODO(mpdenton) kNoRandom or not? It does a lot of waiting for IO. May be
    // good for avoiding leaks but tests a less realistic cache.
    // block_impl_->SetFlags(disk_cache::kNoRandom);

    // TODO(mpdenton) should I always wait here?
    net::TestCompletionCallback cb;
    block_impl_->Init(cb.callback());
    CHECK_EQ(cb.WaitForResult(), net::OK);
  }
}

void DiskCacheLPMFuzzer::CloseAllRemainingEntries() {
  for (auto& entry_info : open_cache_entries_) {
    disk_cache::Entry** entry_ptr = &entry_info.second.entry_ptr;
    if (!*entry_ptr)
      continue;
    MAYBE_PRINT << "Destructor CloseEntry(\"" << (*entry_ptr)->GetKey() << "\")"
                << std::endl;
    (*entry_ptr)->Close();
    *entry_ptr = nullptr;
  }
}

DiskCacheLPMFuzzer::~DiskCacheLPMFuzzer() {
  // |block_impl_| leaks a lot more if we don't close entries before destructing
  // the backend.
  if (block_impl_) {
    // TODO(mpdenton) Consider creating a fuzz target that does not wait for
    // blockfile, and also does not detect leaks.

    // Because the blockfile backend will leak any entries closed after its
    // destruction, we need to wait for any remaining backend callbacks to
    // finish. Otherwise, there will always be a race between handling callbacks
    // with RunUntilIdle() and actually closing all of the remaining entries.
    // And, closing entries after destructing the backend will not work and
    // cause leaks.
    for (auto& entry_it : open_cache_entries_) {
      if (entry_it.second.tcb) {
        WaitOnEntry(&entry_it.second);
      }
    }

    // Destroy any open iterators before destructing the backend so we don't
    // cause leaks. TODO(mpdenton) should maybe be documented?
    // Also *must* happen after waiting for all OpenNextEntry callbacks to
    // finish, because destructing the iterators may cause those callbacks to be
    // cancelled, which will cause WaitOnEntry() to spin forever waiting.
    // TODO(mpdenton) should also be documented?
    open_iterators_.clear();
    // Just in case, finish any callbacks.
    init_globals->task_environment_->RunUntilIdle();
    // Close all entries that haven't been closed yet.
    CloseAllRemainingEntries();
    // Destroy the backend.
    cache_.reset();
  } else {
    // Here we won't bother with waiting for our OpenEntry* callbacks.
    cache_.reset();
    // Finish any callbacks that came in before backend destruction.
    init_globals->task_environment_->RunUntilIdle();
    // Close all entries that haven't been closed yet.
    CloseAllRemainingEntries();
  }

  // Make sure any tasks triggered by the CloseEntry's have run.
  init_globals->task_environment_->RunUntilIdle();
  if (simple_cache_impl_)
    CHECK(simple_file_tracker_->IsEmptyForTesting());
  base::RunLoop().RunUntilIdle();

  DeleteCache(cache_path_);
}

DEFINE_BINARY_PROTO_FUZZER(const disk_cache_fuzzer::FuzzCommands& commands) {
  {
    DiskCacheLPMFuzzer disk_cache_fuzzer_instance;
    disk_cache_fuzzer_instance.RunCommands(commands);
  }
  MAYBE_PRINT << "-----------------------" << std::endl;
}
//
