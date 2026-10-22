// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/in_memory_file_registry.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "storage/common/file_system/file_system_types.h"

namespace storage {

namespace {

// Name used when nothing usable is left after sanitizing a caller supplied
// name. The name ends up as the last component of the file's `filesystem:`
// URL, so it must be a single non-empty path component.
constexpr char kFallbackName[] = "file";

constexpr char kRegisterResultHistogram[] =
    "Storage.InMemoryFileRegistry.RegisterResult";
constexpr char kFindResultHistogram[] =
    "Storage.InMemoryFileRegistry.FindResult";

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    // At most `InMemoryFileRegistry::kDefaultMaxTotalBytes`, typically much
    // less.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    // File contents are large allocations, freed without being touched.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    // Evicted files can no longer be read, so uploads of them fail.
    base::MemoryConsumerTraits::InformationRetention::kLossy,
    // Registered through AsyncMemoryConsumerRegistration.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous);

std::string SanitizeName(std::string_view name) {
  std::string sanitized(name);
  base::ReplaceChars(sanitized, "/\\", "_", &sanitized);
  // Dots are kept, since the page infers the file's type from its extension.
  // A name made up only of dots and whitespace is unusable though: "." and ".."
  // are dot segments that URL parsing removes, and on Windows base::FilePath
  // treats any such name containing ".." (e.g. "...") as a reference to the
  // parent directory, which IsolatedContext refuses to crack.
  if (sanitized.find_first_not_of(". \n\r\t") == std::string::npos) {
    return kFallbackName;
  }
  return sanitized;
}

}  // namespace

InMemoryFileRegistry::File::File(std::string contents,
                                 base::Time modification_time)
    : contents_(std::move(contents)), modification_time_(modification_time) {}

InMemoryFileRegistry::File::~File() = default;

InMemoryFileRegistry::Entry::Entry(scoped_refptr<File> file,
                                   uint64_t registration_order)
    : file(std::move(file)), registration_order(registration_order) {}

InMemoryFileRegistry::Entry::~Entry() = default;

InMemoryFileRegistry::ScopedFile::ScopedFile() = default;

InMemoryFileRegistry::ScopedFile::ScopedFile(ScopedFile&& other)
    : registry_(other.registry_),
      fs_handle_(std::move(other.fs_handle_)),
      register_name_(std::move(other.register_name_)),
      length_(other.length_),
      modification_time_(other.modification_time_) {
  other.registry_ = nullptr;
  other.length_ = 0;
  other.modification_time_ = base::Time();
}

InMemoryFileRegistry::ScopedFile& InMemoryFileRegistry::ScopedFile::operator=(
    ScopedFile&& other) {
  if (this == &other) {
    return *this;
  }
  Reset();
  registry_ = other.registry_;
  fs_handle_ = std::move(other.fs_handle_);
  register_name_ = std::move(other.register_name_);
  length_ = other.length_;
  modification_time_ = other.modification_time_;
  other.registry_ = nullptr;
  other.length_ = 0;
  other.modification_time_ = base::Time();
  return *this;
}

InMemoryFileRegistry::ScopedFile::~ScopedFile() {
  Reset();
}

void InMemoryFileRegistry::ScopedFile::Reset() {
  if (fs_handle_.is_valid()) {
    // Drop the contents first; `fs_handle_` releases this process' reference to
    // the isolated file system when it is cleared below.
    registry_->Unregister(fs_handle_.id());
    fs_handle_ = IsolatedContext::ScopedFSHandle();
  }
  registry_ = nullptr;
  register_name_.clear();
  length_ = 0;
  modification_time_ = base::Time();
}

// static
InMemoryFileRegistry* InMemoryFileRegistry::GetInstance() {
  static base::NoDestructor<InMemoryFileRegistry> instance(
      kDefaultMaxTotalBytes);
  return instance.get();
}

// static
std::unique_ptr<InMemoryFileRegistry> InMemoryFileRegistry::CreateForTesting(
    size_t max_total_bytes) {
  return base::WrapUnique(new InMemoryFileRegistry(max_total_bytes));
}

InMemoryFileRegistry::InMemoryFileRegistry(size_t max_total_bytes)
    : default_max_total_bytes_(max_total_bytes),
      max_total_bytes_(max_total_bytes),
      // Notifications arrive on the sequence that creates the registry; the
      // state they update is guarded by `lock_`. Unregistration is not checked
      // because the process-global instance is never destroyed.
      memory_consumer_registration_(
          "InMemoryFileRegistry",
          kMemoryConsumerTraits,
          this,
          base::AsyncMemoryConsumerRegistration::CheckUnregister::kDisabled) {}

InMemoryFileRegistry::~InMemoryFileRegistry() = default;

InMemoryFileRegistry::ScopedFile InMemoryFileRegistry::Register(
    std::string_view name,
    std::string contents,
    base::Time modification_time) {
  std::string register_name = SanitizeName(name);

  // The file has no path at all, so it is registered under a virtual path. The
  // resulting URL is `filesystem:<origin>/isolated/<fsid>/<register_name>`,
  // which cracks to an empty FileSystemURL::path(); the registry is therefore
  // keyed by file system id.
  std::string filesystem_id =
      IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
          kFileSystemTypeInMemory, register_name, base::FilePath());
  if (filesystem_id.empty()) {
    base::UmaHistogramEnumeration(
        kRegisterResultHistogram,
        RegisterResult::kFileSystemRegistrationFailed);
    return ScopedFile();
  }
  // The handle revokes the file system again unless the file is added to it.
  return AddFile(IsolatedContext::ScopedFSHandle(std::move(filesystem_id)),
                 std::move(register_name), std::move(contents),
                 modification_time);
}

InMemoryFileRegistry::ScopedFile InMemoryFileRegistry::AddFile(
    IsolatedContext::ScopedFSHandle fs_handle,
    std::string register_name,
    std::string contents,
    base::Time modification_time) {
  const size_t size = contents.size();
  bool fits = false;
  {
    base::AutoLock locker(lock_);
    // `total_bytes_` exceeds `max_total_bytes_` after the memory limit is
    // lowered, until the files beyond it are evicted.
    if (total_bytes_ <= max_total_bytes_ &&
        size <= max_total_bytes_ - total_bytes_) {
      fits = true;
      // A file system registered by Register() holds no file yet:
      // IsolatedContext only hands out ids that no registered file system
      // uses, and a file's file system stays registered until the file is
      // unregistered.
      const bool inserted =
          files_
              .try_emplace(fs_handle.id(),
                           base::MakeRefCounted<File>(std::move(contents),
                                                      modification_time),
                           next_registration_order_++)
              .second;
      CHECK(inserted);
      total_bytes_ += size;
    }
  }
  if (!fits) {
    base::UmaHistogramEnumeration(kRegisterResultHistogram,
                                  RegisterResult::kExceedsMemoryLimit);
    return ScopedFile();
  }
  base::UmaHistogramEnumeration(kRegisterResultHistogram,
                                RegisterResult::kSuccess);

  ScopedFile file;
  file.registry_ = this;
  file.fs_handle_ = std::move(fs_handle);
  file.register_name_ = std::move(register_name);
  file.length_ = base::checked_cast<int64_t>(size);
  file.modification_time_ = modification_time;
  return file;
}

scoped_refptr<InMemoryFileRegistry::File> InMemoryFileRegistry::Find(
    const std::string& filesystem_id) const {
  scoped_refptr<File> file;
  FindResult result = FindResult::kNotFound;
  {
    base::AutoLock locker(lock_);
    auto it = files_.find(filesystem_id);
    if (it != files_.end()) {
      file = it->second.file;
      result = file ? FindResult::kFound : FindResult::kEvicted;
    }
  }
  base::UmaHistogramEnumeration(kFindResultHistogram, result);
  return file;
}

void InMemoryFileRegistry::OnUpdateMemoryLimit() {
  // Only new files are held to the updated limit here. Files already held
  // beyond it are evicted by OnReleaseMemory().
  base::AutoLock locker(lock_);
  max_total_bytes_ = memory_limit().Scale(default_max_total_bytes_);
}

void InMemoryFileRegistry::OnReleaseMemory() {
  base::AutoLock locker(lock_);
  if (total_bytes_ <= max_total_bytes_) {
    return;
  }
  // Evict the oldest files first.
  std::vector<Entry*> held_entries;
  for (auto& [filesystem_id, entry] : files_) {
    if (entry.file) {
      held_entries.push_back(&entry);
    }
  }
  std::ranges::sort(held_entries, {}, [](const Entry* entry) {
    return entry->registration_order;
  });
  for (Entry* entry : held_entries) {
    if (total_bytes_ <= max_total_bytes_) {
      break;
    }
    total_bytes_ -= entry->file->contents().size();
    entry->file = nullptr;
  }
}

void InMemoryFileRegistry::Unregister(const std::string& filesystem_id) {
  base::AutoLock locker(lock_);
  auto it = files_.find(filesystem_id);
  if (it == files_.end()) {
    return;
  }
  if (it->second.file) {
    total_bytes_ -= it->second.file->contents().size();
  }
  files_.erase(it);
}

}  // namespace storage
