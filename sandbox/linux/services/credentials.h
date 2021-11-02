// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_CREDENTIALS_H_
#define SANDBOX_LINUX_SERVICES_CREDENTIALS_H_

#include "build/build_config.h"
// Link errors are tedious to track, raise a compile-time error instead.
#if defined(OS_ANDROID)
#error "Android is not supported."
#endif  // defined(OS_ANDROID).

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "sandbox/linux/system_headers/capability.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This class should be used to manipulate the current process' credentials.
// It is currently a stub used to manipulate POSIX.1e capabilities as
// implemented by the Linux kernel.
class SANDBOX_EXPORT Credentials {
 public:
  // For brevity, we only expose enums for the subset of capabilities we use.
  // This can be expanded as the need arises.
  enum class Capability {
    SYS_CHROOT,
    SYS_ADMIN,
  };

  // Checks that the set of RES-uids and the set of RES-gids have
  // one element each and returns that element in |resuid| and |resgid|
  // respectively. It's OK to pass NULL as one or both of the ids.
  static bool GetRESIds(uid_t* resuid, gid_t* resgid);

  // Sets gid and uid maps. See man newgidmap(1)/newuidmap(1) for details.
  static bool SetGidAndUidMaps(gid_t gid, uid_t uid);

  // Drops all capabilities in the effective, inheritable and permitted sets for
  // the current thread. For security reasons, since capabilities are
  // per-thread, the caller is responsible for ensuring it is single-threaded
  // when calling this API.
  // |proc_fd| must be a file descriptor to /proc/ and remains owned by
  // the caller.
  static bool DropAllCapabilities(int proc_fd) WARN_UNUSED_RESULT;
  // A similar API which assumes that it can open /proc/self/ by itself.
  static bool DropAllCapabilities() WARN_UNUSED_RESULT;
  // Sets the effective and permitted capability sets for the current thread to
  // the list of capabiltiies in |caps|. All other capability flags are cleared.
  static bool SetCapabilities(int proc_fd, const std::vector<Capability>& caps)
      WARN_UNUSED_RESULT;

  // Versions of the above functions which do not check that the process is
  // single-threaded. After calling these functions, capabilities of other
  // threads will not be changed. This is dangerous, do not use unless you know
  // what you are doing.
  static bool DropAllCapabilitiesOnCurrentThread() WARN_UNUSED_RESULT;
  static bool SetCapabilitiesOnCurrentThread(
      const std::vector<Capability>& caps) WARN_UNUSED_RESULT;

  // Returns true if the current thread has either the effective, permitted, or
  // inheritable flag set for the given capability.
  static bool HasCapability(Capability cap);

  // Returns true iff there is any capability in any of the capabilities sets
  // of the current thread.
  static bool HasAnyCapability();

  // Returns whether the kernel supports CLONE_NEWUSER and whether it would be
  // possible to immediately move to a new user namespace. There is no point
  // in using this method right before calling MoveToNewUserNS(), simply call
  // MoveToNewUserNS() immediately. This method is only useful to test the
  // ability to move to a user namespace ahead of time.
  static bool CanCreateProcessInNewUserNS();

  // Moves the current process to a new "user namespace" as supported by Linux
  // 3.8+ (CLONE_NEWUSER).
  // The uid map will be set-up so that the perceived uid and gid will not
  // change.
  // If this call succeeds, the current process will be granted a full set of
  // capabilities in the new namespace.
  // This will fail if the process is not mono-threaded.
  static bool MoveToNewUserNS() WARN_UNUSED_RESULT;

  // Removes the ability of the process to access the file system. File
  // descriptors which are already open prior to calling this API remain
  // available.
  // The implementation currently uses chroot(2) and requires CAP_SYS_CHROOT.
  // CAP_SYS_CHROOT can be acquired by using the MoveToNewUserNS() API.
  // |proc_fd| must be a file descriptor to /proc/ and must be the only open
  // directory file descriptor of the process.
  //
  // CRITICAL:
  //   - the caller must close |proc_fd| eventually or access to the file
  // system can be recovered.
  //   - DropAllCapabilities() must be called to prevent escapes.
  static bool DropFileSystemAccess(int proc_fd) WARN_UNUSED_RESULT;

  // This function returns true if the process can still access the filesystem.
  static bool HasFileSystemAccess();

  // Forks and drops capabilities in the child.
  static pid_t ForkAndDropCapabilitiesInChild();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Credentials);
};

}  // namespace sandbox.

#endif  // SANDBOX_LINUX_SERVICES_CREDENTIALS_H_
