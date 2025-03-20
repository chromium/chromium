// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GDBUS_FD_LIST_H_
#define REMOTING_HOST_LINUX_GDBUS_FD_LIST_H_

#include <gio/gio.h>
#include <glib.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/gvariant_type.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

class SparseFdList;

// A list of file descriptors that can be sent or received as part of a D-Bus
// method call.
class GDBusFdList {
 public:
  // A handle to a file descriptor stored in an file-descriptor list. Can be
  // converted to and from a GVariantRef<"h">.
  struct Handle {
    std::int32_t index;
  };

  // Movable, not copyable.
  GDBusFdList();
  ~GDBusFdList();
  GDBusFdList(GDBusFdList&&);
  GDBusFdList& operator=(GDBusFdList&&);

  // Takes ownership of the provided file descriptor, stores it in the list, and
  // returns the resulting handle.
  Handle Insert(base::ScopedFD fd);

  // Attempts to dup the provided file descriptor, leaving the caller with
  // ownership of the original. If dup succeeds, stores the new descriptor and
  // returns the resulting handle. Otherwise, returns the error message.
  base::expected<Handle, Loggable> InsertDup(int fd);

  // Get the file descriptor associated with the given handle, or nullopt if
  // there is no such file descriptor in the list. The list retains ownership of
  // the file descriptor.
  std::optional<int> Get(Handle handle);

  // Converts to a SparseFdList.
  SparseFdList MakeSparse() &&;

  // Creates a new GUnixFDList owning the contained file descriptors, leaving
  // this empty.
  ScopedGObject<GUnixFDList> IntoGUnixFDList() &&;

  // Creates a new GDBusFDList by stealing the file descriptors from a
  // GUnixFDList, leaving it empty.
  static GDBusFdList StealFromGUnixFDList(GUnixFDList* fd_list);

 private:
  std::vector<int> fds_;
};

// A list of file descriptors that may not be contiguous. Allows extracting
// ownership of individual file descriptors, but can not be sent over D-Bus.
class SparseFdList {
 public:
  using Handle = GDBusFdList::Handle;

  // Movable, not copyable.
  SparseFdList();
  ~SparseFdList();
  SparseFdList(SparseFdList&&);
  SparseFdList& operator=(SparseFdList&&);

  // Get the file descriptor associated with the given handle, or nullopt if
  // there is no such fd in the list. The list retains ownership of the file
  // descriptor.
  std::optional<int> Get(Handle handle);

  // Removes and returns the file descriptor associated with the given handle,
  // or a null ScopedFD if there is no such fd in the list. (Test the return
  // value with .is_valid() before use.) Ownership is transferred to the caller.
  base::ScopedFD Extract(Handle handle);

 private:
  explicit SparseFdList(std::vector<int> fds);
  std::vector<int> fds_;

  friend class GDBusFdList;
};

template <>
struct gvariant::Mapping<GDBusFdList::Handle> {
  static constexpr Type kType = "h";
  static GVariantRef<kType> From(GDBusFdList::Handle value) {
    return GVariantRef<kType>::TakeUnchecked(g_variant_new_handle(value.index));
  }
  static GDBusFdList::Handle Into(const GVariantRef<kType>& variant) {
    return GDBusFdList::Handle{g_variant_get_handle(variant.raw())};
  }
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GDBUS_FD_LIST_H_
