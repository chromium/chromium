// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_REGISTRY_H_
#define STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_REGISTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "storage/browser/file_system/isolated_context.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace storage {

// Process-global registry of files whose contents live only in browser process
// memory and are never written to disk.
//
// Each registered file gets its own isolated file system (see IsolatedContext)
// of type `kFileSystemTypeInMemory`, so it can be handed to a renderer as a
// `filesystem:` URL. InMemoryFileSystemBackend serves reads of those URLs
// straight out of this registry.
//
// The registry holds at most `kDefaultMaxTotalBytes` of file contents, scaled
// by the memory limit that the memory coordinator assigns to it. Registering a
// file that does not fit fails, and under memory pressure the oldest files are
// evicted: they stay registered, but can no longer be opened.
//
// Files are registered on the sequence that stages them (typically the UI
// thread) and looked up on the sequence that serves file system operations
// (the IO thread), so all access is guarded by a lock.
class COMPONENT_EXPORT(STORAGE_BROWSER) InMemoryFileRegistry
    : public base::MemoryConsumer {
 public:
  // Contents and metadata of a registered file. It is immutable, so it can be
  // shared across sequences: readers hold a reference for as long as they need
  // the bytes, even if the file is unregistered or evicted in the meantime.
  class COMPONENT_EXPORT(STORAGE_BROWSER) File
      : public base::RefCountedThreadSafe<File> {
   public:
    File(std::string contents, base::Time modification_time);

    File(const File&) = delete;
    File& operator=(const File&) = delete;

    const std::string& contents() const { return contents_; }
    base::Time modification_time() const { return modification_time_; }

   private:
    friend class base::RefCountedThreadSafe<File>;

    ~File();

    const std::string contents_;
    const base::Time modification_time_;
  };

  // Outcome of Register(). These values are persisted to logs. Entries should
  // not be renumbered and numeric values should never be reused.
  // LINT.IfChange(InMemoryFileRegistryRegisterResult)
  enum class RegisterResult {
    kSuccess = 0,
    kExceedsMemoryLimit = 1,
    kFileSystemRegistrationFailed = 2,
    kMaxValue = kFileSystemRegistrationFailed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InMemoryFileRegistryRegisterResult)

  // Outcome of Find(). These values are persisted to logs. Entries should not
  // be renumbered and numeric values should never be reused.
  // LINT.IfChange(InMemoryFileRegistryFindResult)
  enum class FindResult {
    kFound = 0,
    kNotFound = 1,
    kEvicted = 2,
    kMaxValue = kEvicted,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/storage/enums.xml:InMemoryFileRegistryFindResult)

  // The most file contents, in bytes, that the registry holds at the default
  // (100%) memory limit.
  static constexpr size_t kDefaultMaxTotalBytes = 100 * 1024 * 1024;

  // Keeps a file registered for as long as it is alive.
  //
  // Destroying the handle revokes this process' isolated file system reference
  // and drops the registry's reference to the contents, after which the file
  // can no longer be opened. Reads that are already in flight are unaffected,
  // because the file stream reader holds its own reference to the bytes.
  class COMPONENT_EXPORT(STORAGE_BROWSER) ScopedFile {
   public:
    ScopedFile();

    ScopedFile(const ScopedFile&) = delete;
    ScopedFile& operator=(const ScopedFile&) = delete;

    ScopedFile(ScopedFile&&);
    ScopedFile& operator=(ScopedFile&&);
    ~ScopedFile();

    bool is_valid() const { return fs_handle_.is_valid(); }

    // Id of the isolated file system that holds this file. Empty if invalid.
    const std::string& filesystem_id() const { return fs_handle_.id(); }

    // The name the file is registered under. This is the last component of its
    // `filesystem:` URL, and therefore the file name a page sees.
    const std::string& register_name() const { return register_name_; }

    int64_t length() const { return length_; }
    base::Time modification_time() const { return modification_time_; }

   private:
    friend class InMemoryFileRegistry;

    void Reset();

    raw_ptr<InMemoryFileRegistry> registry_ = nullptr;
    IsolatedContext::ScopedFSHandle fs_handle_;
    std::string register_name_;
    int64_t length_ = 0;
    base::Time modification_time_;
  };

  // The instance is lazily created per browser process.
  static InMemoryFileRegistry* GetInstance();

  // Creates a registry separate from the process-global one, which holds at
  // most `max_total_bytes` of file contents at the default memory limit. The
  // handles it returns must be destroyed before it is.
  static std::unique_ptr<InMemoryFileRegistry> CreateForTesting(
      size_t max_total_bytes);

  InMemoryFileRegistry(const InMemoryFileRegistry&) = delete;
  InMemoryFileRegistry& operator=(const InMemoryFileRegistry&) = delete;

  ~InMemoryFileRegistry() override;

  // Registers `contents` as a file named `name` and returns a handle keeping it
  // alive. `name` is reduced to a single path component; if nothing usable is
  // left it is replaced by "file". Returns an invalid handle if the file could
  // not be registered, e.g. because holding it would exceed the memory limit.
  ScopedFile Register(std::string_view name,
                      std::string contents,
                      base::Time modification_time);

  // Returns the file registered under `filesystem_id`, or null if there is
  // none or it was evicted. Safe to call from any sequence.
  scoped_refptr<File> Find(const std::string& filesystem_id) const;

 private:
  friend class base::NoDestructor<InMemoryFileRegistry>;
  friend class InMemoryFileRegistryTest;
  friend class ScopedFile;

  struct Entry {
    Entry(scoped_refptr<File> file, uint64_t registration_order);
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    ~Entry();

    // Null once the file is evicted. The entry is kept until the file is
    // unregistered, so that Find() can tell evicted files from unknown ones.
    scoped_refptr<File> file;
    // Files registered earlier have lower values, and are evicted first.
    uint64_t registration_order;
  };

  explicit InMemoryFileRegistry(size_t max_total_bytes);

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;

  // Adds `contents` as the file of the isolated file system `fs_handle` refers
  // to, which was registered for it under `register_name` and must not hold a
  // file yet. Returns an invalid handle if holding the file would exceed the
  // memory limit.
  ScopedFile AddFile(IsolatedContext::ScopedFSHandle fs_handle,
                     std::string register_name,
                     std::string contents,
                     base::Time modification_time);

  void Unregister(const std::string& filesystem_id);

  // Cap on `total_bytes_` at the default memory limit.
  const size_t default_max_total_bytes_;

  mutable base::Lock lock_;
  std::map<std::string, Entry> files_ GUARDED_BY(lock_);
  // Combined size of the contents of all files that are not evicted.
  size_t total_bytes_ GUARDED_BY(lock_) = 0;
  // `default_max_total_bytes_` scaled by the current memory limit.
  size_t max_total_bytes_ GUARDED_BY(lock_);
  uint64_t next_registration_order_ GUARDED_BY(lock_) = 0;

  base::AsyncMemoryConsumerRegistration memory_consumer_registration_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_IN_MEMORY_FILE_REGISTRY_H_
