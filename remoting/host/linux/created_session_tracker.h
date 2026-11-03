// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CREATED_SESSION_TRACKER_H_
#define REMOTING_HOST_LINUX_CREATED_SESSION_TRACKER_H_

#include <sys/types.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"

namespace remoting {

// Persists the systemd-logind sessions that Chrome Remote Desktop created via a
// RemoteUserDesktopSessionCreator, so that they can be terminated when the host
// service is stopped. This includes sessions created by an earlier instance of
// the daemon process, e.g. before a host upgrade or crash.
//
// The state file is stored in a directory that is cleared at boot. This
// matters because logind assigns session IDs from counters that start over at
// boot, so after a reboot the same ID values are assigned to new, unrelated
// sessions, and an entry that survived a reboot could refer to one of them.
// The IDs of non-audit sessions (e.g. "c2") can also be re-assigned within a
// boot if systemd-logind restarts, so consumers must check that the live
// session still belongs to the entry's UID and username before acting on an
// entry.
//
// All methods perform blocking file I/O, so this class must be used on a
// sequence that allows blocking. Add() and Remove() do an unlocked
// read-modify-write, so only one instance may modify a given state file at a
// time.
class CreatedSessionTracker {
 public:
  struct Entry {
    bool operator==(const Entry&) const = default;

    // The logind session ID, e.g. "3" or "c2".
    std::string session_id;

    // The canonical name of the user that owns the session.
    std::string username;

    // The UID of the user that owns the session.
    uid_t uid = 0;
  };

  // Returns the path of the state file used by the multi-process daemon.
  static base::FilePath GetDefaultFilePath();

  // Uses GetDefaultFilePath(). The state file and its parent directory must be
  // owned by root. The parent directory is created by the daemon process at
  // startup.
  CreatedSessionTracker();

  // `file_path`: The state file. Its parent directory must already exist.
  // `expected_owner_uid`: The UID that must own the state file and its parent
  //     directory.
  CreatedSessionTracker(base::FilePath file_path, uid_t expected_owner_uid);

  ~CreatedSessionTracker();

  CreatedSessionTracker(const CreatedSessionTracker&) = delete;
  CreatedSessionTracker& operator=(const CreatedSessionTracker&) = delete;

  // Returns all tracked entries, or an empty list if the state file or its
  // parent directory doesn't exist. Returns an error if the state file or its
  // parent directory fails the security checks of ReadSecureFile() (e.g. it is
  // a symlink, has the wrong owner, or is writable by group or others), or if
  // the state file is malformed.
  base::expected<std::vector<Entry>, Loggable> Load() const;

  // Adds `entry`, replacing any existing entry with the same session ID, and
  // writes the state file atomically with mode 0600. A malformed state file is
  // deleted, even if writing the new file then fails. Returns an error if the
  // parent directory is missing, the security checks fail, the file can't be
  // written, or the file would exceed its size limit, in which case a valid
  // existing state file is left unchanged.
  base::expected<void, Loggable> Add(const Entry& entry);

  // Removes the entries with the given session IDs. Unknown IDs are ignored.
  // Deletes the state file once it no longer contains any entries. A malformed
  // state file is deleted. Returns an error if the security checks fail or the
  // file can't be updated.
  base::expected<void, Loggable> Remove(
      base::span<const std::string> session_ids);

 private:
  // Like Load(), but deletes a malformed state file and treats it as empty, so
  // that callers can start over. Security and I/O errors are still returned.
  base::expected<std::vector<Entry>, Loggable> LoadForUpdate() const;

  // Writes `entries` to the state file, or deletes it if `entries` is empty.
  base::expected<void, Loggable> Save(const std::vector<Entry>& entries) const;

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath file_path_;
  const uid_t expected_owner_uid_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CREATED_SESSION_TRACKER_H_
