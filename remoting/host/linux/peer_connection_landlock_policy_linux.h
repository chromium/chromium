// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PEER_CONNECTION_LANDLOCK_POLICY_LINUX_H_
#define REMOTING_HOST_LINUX_PEER_CONNECTION_LANDLOCK_POLICY_LINUX_H_

#include <vector>

#include "base/files/file_path.h"

namespace remoting {

enum class PeerConnectionLandlockResult {
  // The Landlock policy has been applied.
  kApplied,
  // Landlock is not supported by the kernel, or is disabled.
  kNotSupported,
  // Landlock is supported, but the policy could not be applied.
  kFailed,
};

// Returns the Landlock ABI version supported by the kernel, or a value <= 0 if
// Landlock is not supported or disabled.
int GetLandlockAbiVersion();

// Restricts the calling thread, and any threads it creates afterwards, with a
// Landlock policy for the peer connection process. The policy only allows
// read-only access to the files that the peer connection process needs (system
// configuration and libraries, and the process' own /proc/self entries), plus
// read-write access to /dev/null, and denies all other filesystem access,
// including creating or writing files. Since all peer connection processes run
// as the same user, this prevents a peer connection process from accessing the
// /proc entries of other peer connection processes. In addition, Landlock
// denies ptrace access checks (e.g. for /proc/<pid>/mem) from a restricted
// process to processes outside of its Landlock domain. On Landlock ABI 6 and
// newer, sending signals to, and connecting to abstract UNIX sockets of,
// processes outside of the Landlock domain are denied as well.
//
// To restrict the whole process, this must be called while the process is
// single-threaded.
//
// `extra_read_only_paths` are additional files or directories that may be
// read, e.g. the installation directory.
PeerConnectionLandlockResult ApplyPeerConnectionLandlockPolicy(
    const std::vector<base::FilePath>& extra_read_only_paths);

// Returns the path that the policy allows reading for /etc/resolv.conf, given
// the resolved path of /etc/resolv.conf: its directory, or only the file
// itself if the directory is too broad.
base::FilePath GetResolvConfPathToAllowForTesting(
    const base::FilePath& resolv_conf);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PEER_CONNECTION_LANDLOCK_POLICY_LINUX_H_
