// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSCALL_BROKER_BROKER_FILE_PERMISSION_H_
#define SANDBOX_LINUX_SYSCALL_BROKER_BROKER_FILE_PERMISSION_H_

#include <bitset>
#include <string>

#include "sandbox/sandbox_export.h"

namespace sandbox {
namespace syscall_broker {

// Recursive: allow everything under |path| (must be a dir).
enum class RecursionOption { kNonRecursive = 0, kRecursive };
// Temporary: file will be unlink'd after opening.
enum class PersistenceOption { kPermanent = 0, kTemporaryOnly };
enum class ReadPermission { kBlockRead = 0, kAllowRead };
enum class WritePermission { kBlockWrite = 0, kAllowWrite };
enum class CreatePermission { kBlockCreate = 0, kAllowCreate };
// Allow stat() for the path and all intermediate dirs.
enum class StatWithIntermediatesPermission {
  kBlockStatWithIntermediates = 0,
  kAllowStatWithIntermediates
};
enum class InotifyAddWatchWithIntermediatesPermission {
  kBlockInotifyAddWatchWithIntermediates = 0,
  kAllowInotifyAddWatchWithIntermediates
};

// BrokerFilePermission defines a path for allowlisting.
// Pick the correct static factory method to create a permission.
// CheckOpen and CheckAccess are async signal safe.
// Construction and Destruction are not async signal safe.
// |path| is the path to be allowlisted.
class SANDBOX_EXPORT BrokerFilePermission {
 public:
  // Movable and copyable.
  BrokerFilePermission(BrokerFilePermission&&);
  BrokerFilePermission& operator=(BrokerFilePermission&&);
  BrokerFilePermission(const BrokerFilePermission&);
  BrokerFilePermission& operator=(const BrokerFilePermission&);

  ~BrokerFilePermission();

  static BrokerFilePermission ReadOnly(const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kAllowRead, WritePermission::kBlockWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission ReadOnlyRecursive(const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kRecursive, PersistenceOption::kPermanent,
        ReadPermission::kAllowRead, WritePermission::kBlockWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission WriteOnly(const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kBlockRead, WritePermission::kAllowWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission ReadWrite(const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kAllowRead, WritePermission::kAllowWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission ReadWriteCreate(const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kAllowRead, WritePermission::kAllowWrite,
        CreatePermission::kAllowCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission ReadWriteCreateRecursive(
      const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kRecursive, PersistenceOption::kPermanent,
        ReadPermission::kAllowRead, WritePermission::kAllowWrite,
        CreatePermission::kAllowCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  // Temporary files must always be newly created and do not confer rights to
  // use pre-existing files of the same name.
  static BrokerFilePermission ReadWriteCreateTemporary(
      const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kTemporaryOnly,
        ReadPermission::kAllowRead, WritePermission::kAllowWrite,
        CreatePermission::kAllowCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission ReadWriteCreateTemporaryRecursive(
      const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kRecursive, PersistenceOption::kTemporaryOnly,
        ReadPermission::kAllowRead, WritePermission::kAllowWrite,
        CreatePermission::kAllowCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission StatOnlyWithIntermediateDirs(
      const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kBlockRead, WritePermission::kBlockWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kAllowStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kBlockInotifyAddWatchWithIntermediates);
  }

  static BrokerFilePermission InotifyAddWatchWithIntermediateDirs(
      const std::string& path) {
    return BrokerFilePermission(
        path, RecursionOption::kNonRecursive, PersistenceOption::kPermanent,
        ReadPermission::kBlockRead, WritePermission::kBlockWrite,
        CreatePermission::kBlockCreate,
        StatWithIntermediatesPermission::kBlockStatWithIntermediates,
        InotifyAddWatchWithIntermediatesPermission::
            kAllowInotifyAddWatchWithIntermediates);
  }

  // Returns true if |requested_filename| is allowed to be accessed
  // by this permission as per access(2).
  // If |file_to_access| is not NULL, it is set to point to either
  // the |requested_filename| in the case of a recursive match,
  // or a pointer to the matched path in the allowlist if an absolute
  // match.
  // |mode| is per mode argument of access(2).
  // Async signal safe if |file_to_access| is NULL
  bool CheckAccess(const char* requested_filename,
                   int mode,
                   const char** file_to_access) const;

  // Returns true if |requested_filename| is allowed to be opened
  // by this permission.
  // If |file_to_open| is not NULL it is set to point to either
  // the |requested_filename| in the case of a recursive match,
  // or a pointer the matched path in the allowlist if an absolute
  // match.
  // If not NULL, |unlink_after_open| is set to point to true if the
  // caller is required to unlink the path after opening.
  // Async signal safe if |file_to_open| is NULL.
  bool CheckOpen(const char* requested_filename,
                 int flags,
                 const char** file_to_open,
                 bool* unlink_after_open) const;

  // Returns true if |requested_filename| is allowed to be stat'd
  // by this permission as per stat(2). Differs from CheckAccess()
  // in that if create permission is granted to a file, we permit
  // stat() on all of its leading components.
  // If |file_to_access| is not NULL, it is set to point to either
  // the |requested_filename| in the case of a recursive match,
  // or a pointer to the matched path in the allowlist if an absolute
  // match.
  // Async signal safe if |file_to_access| is NULL
  bool CheckStatWithIntermediates(const char* requested_filename,
                                  const char** file_to_access) const;

  // Returns true if |requested_filename| is allowed by this permission to be
  // added to an inotify instance's watch list by inotify_add_watch(2), with the
  // specific |mask|. Differs from CheckAccess() in that if inotify_add_watch
  // permission is granted to a file, we permit inotify_add_watch() on all of
  // its leading components.
  //
  // If |file_to_inotify_add_watch| is not NULL, it is set to point to either
  // the |requested_filename| in the case of a recursive match, or a pointer to
  // the matched path in the allowlist if an absolute match.
  //
  // Async signal safe if |file_to_inotify_add_watch| is NULL
  bool CheckInotifyAddWatchWithIntermediates(
      const char* requested_filename,
      uint32_t mask,
      const char** file_to_inotify_add_watch) const;

 private:
  friend class BrokerFilePermissionTester;

  enum BitPositions {
    kRecursiveBitPos = 0,
    kTemporaryOnlyBitPos,
    kAllowReadBitPos,
    kAllowWriteBitPos,
    kAllowCreateBitPos,
    kAllowStatWithIntermediatesBitPos,
    kAllowInotifyAddWatchWithIntermediates,

    kMaxValueBitPos = kAllowInotifyAddWatchWithIntermediates
  };

  // NOTE: Validates the permission and dies if invalid!
  BrokerFilePermission(std::string path,
                       RecursionOption recurse_opt,
                       PersistenceOption persist_opt,
                       ReadPermission read_perm,
                       WritePermission write_perm,
                       CreatePermission create_perm,
                       StatWithIntermediatesPermission stat_perm,
                       InotifyAddWatchWithIntermediatesPermission inotify_perm);

  // Allows construction from the raw bitset.
  BrokerFilePermission(std::string path, uint64_t flags);

  const std::string& path() const { return path_; }

  // Returns a serialiazable version of |flags_|.
  uint64_t flags() const { return flags_.to_ullong(); }

  bool recursive() const { return flags_.test(kRecursiveBitPos); }

  bool temporary_only() const { return flags_.test(kTemporaryOnlyBitPos); }

  bool allow_read() const { return flags_.test(kAllowReadBitPos); }

  bool allow_write() const { return flags_.test(kAllowWriteBitPos); }

  bool allow_create() const { return flags_.test(kAllowCreateBitPos); }

  bool allow_stat_with_intermediates() const {
    return flags_.test(kAllowStatWithIntermediatesBitPos);
  }

  bool allow_inotify_add_watch_with_intermediates() const {
    return flags_.test(kAllowInotifyAddWatchWithIntermediates);
  }

  // ValidatePath checks |path| and returns true if these conditions are met
  // * Greater than 0 length
  // * Is an absolute path
  // * No trailing slash
  // * No /../ path traversal
  static bool ValidatePath(const char* path);

  // MatchPath returns true if |requested_filename| is covered by this instance
  bool MatchPath(const char* requested_filename) const;

  // Helper routine for CheckAccess() and CheckStat(). Must be safe to call
  // from an async signal context.
  bool CheckAccessInternal(const char* requested_filename,
                           int mode,
                           const char** file_to_access) const;

  // Helper routine for CheckStatWithIntermediates() and
  // CheckInotifyAddWatchWithIntermediates() to return true if one of the
  // following is true:
  // 1. |requested_filename| matches a leading directory of |path_|.
  // 2. |can_match_full_path| is true and |path_| == |requested_filename|.
  bool CheckIntermediates(const char* requested_filename,
                          bool can_match_full_path) const;

  // Used in by BrokerFilePermissionTester for tests.
  static const char* GetErrorMessageForTests();

  void DieOnInvalidPermission();

  // These are not const as std::vector requires copy-assignment and this class
  // is stored in vectors. All methods are marked const so the compiler will
  // still enforce no changes outside of the constructor.
  std::string path_;
  std::bitset<kMaxValueBitPos + 1> flags_;
};

}  // namespace syscall_broker
}  // namespace sandbox

#endif  //  SANDBOX_LINUX_SYSCALL_BROKER_BROKER_FILE_PERMISSION_H_
