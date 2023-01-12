// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace network {
class TransferableDirectory;
}
namespace network::mojom {
class TransferableDirectoryDataView;
}
namespace mojo {
template <>
struct UnionTraits<network::mojom::TransferableDirectoryDataView,
                   network::TransferableDirectory>;
}

namespace network {

// Represents a directory that, on some platforms, can be sent as a handle
// across process boundaries to be later mounted to a process-local path.
//
// Platforms which use the same filesystem across process boundaries can simply
// use the path as-is, with no handle marshaling steps required.
//
// Example usage:
//  SENDER:
//   TransferableDirectory dir = base::FilePath("/dir/to/share");
//   if (dir.IsOpenForTransferRequired())
//     dir.OpenForTransfer();
//   mojo_client->UseDirectory(std::move(dir));
//
//  RECEIVER:
//   AutoClosureRunner dismounter;
//   if (dir.NeedsMount())
//     dismounter = AutoClosureRunner(dir.Mount());
//
//   base::File file(dir.path().Append("shared_file.txt"));
//   ...use |file| normally...
class COMPONENT_EXPORT(NETWORK_CPP_BASE) TransferableDirectory {
 public:
  TransferableDirectory();
  explicit TransferableDirectory(const base::FilePath& path);
  TransferableDirectory(TransferableDirectory&& other);
  ~TransferableDirectory();

  void operator=(TransferableDirectory&& other);
  void operator=(const base::FilePath& path);

  const base::FilePath& path() const;
  void ClearPath();

  static bool IsOpenForTransferRequired();

  // Gets a directory handle for |path_| to be sent over Mojo.
  // Call may block, so should be called on an IO thread.
  // Should be prefaced with a check against IsOpenForTransferRequired().
  void OpenForTransfer();

  // Returns true if |this| contains a handle that needs to be mounted before
  // the path can be accessed.
  bool NeedsMount() const;

  // Mounts |handle_| to a location on the filesystem.
  // Should be prefaced with a check against NeedsMount().
  // Returns a task that, when run, will dismount the directory.
  // The task should not be run while I/O is occurring to the directory from
  // any thread.
  // Deleting the task should not unmount the directory, so that during
  // shutdown the unmounting can be skipped with no adverse effects.
  [[nodiscard]] base::OnceClosure Mount();

 private:
  friend struct mojo::UnionTraits<network::mojom::TransferableDirectoryDataView,
                                  network::TransferableDirectory>;

  // Handle manipulation calls, hidden from callers and only surfaced to
  // UnionTraits.
  explicit TransferableDirectory(mojo::PlatformHandle handle);
  bool HasValidHandle() const { return handle_.is_valid(); }
  mojo::PlatformHandle TakeHandle();

  // The path to the source directory for the producer, or the mounted location
  // for the consumer.  Initially empty on the consumer unitil Mount() is
  // called.
  base::FilePath path_;

  // Contains a handle to the directory, populated by calling OpenForTransfer()
  // just prior to sending over Mojo.  On the directory consumer, calling
  // Mount() will mount |handle_| at |path_|, and |handle_| will be cleared.
  mojo::PlatformHandle handle_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRANSFERABLE_DIRECTORY_H_
