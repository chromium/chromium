// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/peer_connection_landlock_policy_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/linux/system_headers/linux_landlock.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"

namespace remoting {

namespace {

// Definitions from <linux/landlock.h> that are newer than the ones in
// //sandbox/linux/system_headers/linux_landlock.h.

// Landlock ABI 5.
constexpr uint64_t kLandlockAccessFsIoctlDev = 1ULL << 15;
// Landlock ABI 6.
constexpr uint64_t kLandlockScopeAbstractUnixSocket = 1ULL << 0;
constexpr uint64_t kLandlockScopeSignal = 1ULL << 1;

// struct landlock_ruleset_attr, including the fields added in ABI 4 and 6. The
// kernel accepts this struct as long as the fields it does not know about are
// zero.
struct LandlockRulesetAttr {
  uint64_t handled_access_fs;
  uint64_t handled_access_net;
  uint64_t scoped;
};

// Filesystem access rights of Landlock ABI 1.
constexpr uint64_t kAccessFsAbi1 =
    LANDLOCK_ACCESS_FS_EXECUTE | LANDLOCK_ACCESS_FS_WRITE_FILE |
    LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR |
    LANDLOCK_ACCESS_FS_REMOVE_DIR | LANDLOCK_ACCESS_FS_REMOVE_FILE |
    LANDLOCK_ACCESS_FS_MAKE_CHAR | LANDLOCK_ACCESS_FS_MAKE_DIR |
    LANDLOCK_ACCESS_FS_MAKE_REG | LANDLOCK_ACCESS_FS_MAKE_SOCK |
    LANDLOCK_ACCESS_FS_MAKE_FIFO | LANDLOCK_ACCESS_FS_MAKE_BLOCK |
    LANDLOCK_ACCESS_FS_MAKE_SYM;

// Access rights that only apply to directories.
constexpr uint64_t kDirectoryOnlyAccess =
    LANDLOCK_ACCESS_FS_READ_DIR | LANDLOCK_ACCESS_FS_REMOVE_DIR |
    LANDLOCK_ACCESS_FS_REMOVE_FILE | LANDLOCK_ACCESS_FS_MAKE_CHAR |
    LANDLOCK_ACCESS_FS_MAKE_DIR | LANDLOCK_ACCESS_FS_MAKE_REG |
    LANDLOCK_ACCESS_FS_MAKE_SOCK | LANDLOCK_ACCESS_FS_MAKE_FIFO |
    LANDLOCK_ACCESS_FS_MAKE_BLOCK | LANDLOCK_ACCESS_FS_MAKE_SYM |
    LANDLOCK_ACCESS_FS_REFER;

constexpr uint64_t kReadOnlyAccess =
    LANDLOCK_ACCESS_FS_READ_FILE | LANDLOCK_ACCESS_FS_READ_DIR;

// Directories and files that the peer connection process may read.
constexpr const char* kReadOnlyPaths[] = {
    // The process' own /proc entries, e.g. /proc/self/status, /proc/self/maps,
    // and /proc/self/fd/*, which base uses to reopen shared memory regions.
    // This resolves to /proc/<pid> when the rule is created, so the /proc
    // entries of other processes are not accessible.
    "/proc/self",
    // CPU and memory information, used by base and WebRTC, e.g. for CPU feature
    // and core count detection.
    "/proc/cpuinfo",
    "/proc/meminfo",
    "/sys/devices/system/cpu",
    // System configuration, e.g. DNS configuration (hosts, resolv.conf,
    // nsswitch.conf, gai.conf) and the time zone.
    "/etc",
    // System libraries (e.g. NSS modules loaded by getaddrinfo()) and data
    // (e.g. zoneinfo).
    "/usr",
    "/lib",
    "/lib64",
    "/lib32",
    "/dev/urandom",
};

// Files that the peer connection process may read and write.
constexpr const char* kReadWritePaths[] = {
    "/dev/null",
};

// Directories that are too broad to allow, if /etc/resolv.conf points into
// them. In that case, only the resolved file itself is allowed.
constexpr const char* kBroadDirectories[] = {"/", "/run", "/var/run"};

// Directories that must never be allowed, nor any directory under them. If
// /etc/resolv.conf points into them, only the resolved file itself is allowed.
constexpr const char* kDisallowedDirectories[] = {
    "/proc", "/sys", "/dev", "/tmp", "/var/tmp", "/run/user",
};

// Returns the path to allow for reading /etc/resolv.conf, given its resolved
// path. /etc/resolv.conf is often a symlink into /run (e.g. systemd-resolved
// or NetworkManager). The directory it points to is allowed, so that the file
// can be atomically replaced, unless that directory is too broad.
base::FilePath GetResolvConfPathToAllow(const base::FilePath& resolv_conf) {
  const base::FilePath dir = resolv_conf.DirName();
  for (const char* broad_dir : kBroadDirectories) {
    if (dir == base::FilePath(broad_dir)) {
      return resolv_conf;
    }
  }
  for (const char* disallowed_dir : kDisallowedDirectories) {
    const base::FilePath disallowed_path(disallowed_dir);
    if (dir == disallowed_path || disallowed_path.IsParent(dir)) {
      return resolv_conf;
    }
  }
  return dir;
}

// Returns the path to allow for reading /etc/resolv.conf, or nullopt if it
// can't be resolved.
std::optional<base::FilePath> GetResolvConfPathToAllow() {
  base::FilePath resolv_conf;
  if (!base::NormalizeFilePath(base::FilePath("/etc/resolv.conf"),
                               &resolv_conf)) {
    return std::nullopt;
  }
  return GetResolvConfPathToAllow(resolv_conf);
}

// Adds a rule allowing `access` to `path` (a file, or a directory and its
// contents). Paths that don't exist or can't be accessed are skipped. Returns
// false on other errors.
bool AddPathRule(int ruleset_fd, const base::FilePath& path, uint64_t access) {
  base::ScopedFD path_fd(
      HANDLE_EINTR(open(path.value().c_str(), O_PATH | O_CLOEXEC)));
  if (!path_fd.is_valid()) {
    if (errno == ENOENT || errno == ENOTDIR || errno == EACCES) {
      VPLOG(1) << "Skipping Landlock rule for " << path;
      return true;
    }
    PLOG(ERROR) << "Failed to open " << path;
    return false;
  }
  struct stat stat_buf;
  if (fstat(path_fd.get(), &stat_buf) != 0) {
    PLOG(ERROR) << "Failed to stat " << path;
    return false;
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    access &= ~kDirectoryOnlyAccess;
  }
  struct landlock_path_beneath_attr path_beneath = {
      .allowed_access = access,
      .parent_fd = static_cast<uint32_t>(path_fd.get()),
  };
  if (syscall(__NR_landlock_add_rule, ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
              &path_beneath, 0) != 0) {
    PLOG(ERROR) << "landlock_add_rule() failed for " << path;
    return false;
  }
  // Landlock ties rules to inodes (it holds a reference to the inode, but not
  // to the dentry). procfs creates a new inode whenever it instantiates a
  // dentry (e.g. proc_pid_instantiate() and proc_get_inode() call
  // new_inode()), so if the unused dentry were reclaimed, a later lookup would
  // get a new inode that the rule does not cover. Keep the O_PATH file
  // descriptor, which holds a reference to the dentry, open for the lifetime of
  // the process, so that lookups keep finding the same dentry and inode.
  if (base::FilePath("/proc").IsParent(path)) {
    std::ignore = path_fd.release();
  }
  return true;
}

}  // namespace

base::FilePath GetResolvConfPathToAllowForTesting(
    const base::FilePath& resolv_conf) {
  return GetResolvConfPathToAllow(resolv_conf);
}

int GetLandlockAbiVersion() {
  return static_cast<int>(syscall(__NR_landlock_create_ruleset, nullptr, 0,
                                  LANDLOCK_CREATE_RULESET_VERSION));
}

PeerConnectionLandlockResult ApplyPeerConnectionLandlockPolicy(
    const std::vector<base::FilePath>& extra_read_only_paths) {
  const int abi_version = GetLandlockAbiVersion();
  if (abi_version <= 0) {
    VPLOG(1) << "Landlock is not supported";
    return PeerConnectionLandlockResult::kNotSupported;
  }

  uint64_t handled_access_fs = kAccessFsAbi1;
  if (abi_version >= 2) {
    handled_access_fs |= LANDLOCK_ACCESS_FS_REFER;
  }
  if (abi_version >= 3) {
    handled_access_fs |= LANDLOCK_ACCESS_FS_TRUNCATE;
  }
  if (abi_version >= 5) {
    handled_access_fs |= kLandlockAccessFsIoctlDev;
  }
  uint64_t scoped = 0;
  if (abi_version >= 6) {
    scoped = kLandlockScopeAbstractUnixSocket | kLandlockScopeSignal;
  }

  const LandlockRulesetAttr ruleset_attr = {
      .handled_access_fs = handled_access_fs,
      .handled_access_net = 0,
      .scoped = scoped,
  };
  base::ScopedFD ruleset_fd(static_cast<int>(syscall(
      __NR_landlock_create_ruleset, &ruleset_attr, sizeof(ruleset_attr), 0)));
  if (!ruleset_fd.is_valid()) {
    PLOG(ERROR) << "landlock_create_ruleset() failed";
    return PeerConnectionLandlockResult::kFailed;
  }

  const uint64_t read_only_access = kReadOnlyAccess & handled_access_fs;
  const uint64_t read_write_access =
      (kReadOnlyAccess | LANDLOCK_ACCESS_FS_WRITE_FILE |
       LANDLOCK_ACCESS_FS_TRUNCATE) &
      handled_access_fs;

  std::vector<base::FilePath> read_only_paths = extra_read_only_paths;
  for (const char* path : kReadOnlyPaths) {
    read_only_paths.emplace_back(path);
  }
  if (std::optional<base::FilePath> resolv_conf = GetResolvConfPathToAllow()) {
    read_only_paths.push_back(*resolv_conf);
  }

  for (const base::FilePath& path : read_only_paths) {
    if (!AddPathRule(ruleset_fd.get(), path, read_only_access)) {
      return PeerConnectionLandlockResult::kFailed;
    }
  }
  for (const char* path : kReadWritePaths) {
    if (!AddPathRule(ruleset_fd.get(), base::FilePath(path),
                     read_write_access)) {
      return PeerConnectionLandlockResult::kFailed;
    }
  }

  // Landlock requires no_new_privs (or CAP_SYS_ADMIN).
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
    PLOG(ERROR) << "prctl(PR_SET_NO_NEW_PRIVS) failed";
    return PeerConnectionLandlockResult::kFailed;
  }
  if (syscall(__NR_landlock_restrict_self, ruleset_fd.get(), 0) != 0) {
    PLOG(ERROR) << "landlock_restrict_self() failed";
    return PeerConnectionLandlockResult::kFailed;
  }
  VLOG(1) << "Landlock policy applied (ABI " << abi_version << ").";
  return PeerConnectionLandlockResult::kApplied;
}

}  // namespace remoting
