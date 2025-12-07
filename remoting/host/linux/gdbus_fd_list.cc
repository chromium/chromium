// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gdbus_fd_list.h"

#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

namespace {

constexpr int kInvalidFd = -1;

void CloseFds(base::span<int> fds) {
  for (int fd : fds) {
    if (fd == kInvalidFd) {
      continue;
    }

    int result = close(fd);

    if (result == -1) {
      if (errno == EBADF) {
        LOG(ERROR) << "Attempting to close non-existent fd " << fd;
      } else {
        // Any other error still results in the fd getting closed.
        PLOG(WARNING) << "Error while closing fd " << fd;
      }
    }
  }
}

}  // namespace

GDBusFdList::GDBusFdList() = default;

GDBusFdList::~GDBusFdList() {
  CloseFds(fds_);
}

GDBusFdList::GDBusFdList(GDBusFdList&& other) {
  fds_.swap(other.fds_);
}

GDBusFdList& GDBusFdList::operator=(GDBusFdList&& other) {
  if (this != &other) {
    CloseFds(fds_);
    fds_.clear();
    fds_.swap(other.fds_);
  }
  return *this;
}

GDBusFdList::Handle GDBusFdList::Insert(base::ScopedFD fd) {
  std::size_t next_index = fds_.size();
  // GUnixFDList requires all file descriptors to be close-on-exec.
  fcntl(fd.get(), F_SETFD, FD_CLOEXEC);
  fds_.push_back(fd.release());
  return Handle{static_cast<int32_t>(next_index)};
}

base::expected<GDBusFdList::Handle, Loggable> GDBusFdList::InsertDup(int fd) {
  int new_fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);

  if (new_fd == -1) {
    return base::unexpected(
        Loggable(FROM_HERE, base::StrCat({"Error duplicating file descriptor: ",
                                          base::safe_strerror(errno)})));
  }

  return base::ok(Insert(base::ScopedFD(new_fd)));
}

std::optional<int> GDBusFdList::Get(Handle handle) {
  if (handle.index < 0 ||
      static_cast<std::size_t>(handle.index) >= fds_.size()) {
    return std::nullopt;
  }

  return fds_[handle.index];
}

SparseFdList GDBusFdList::MakeSparse() && {
  return SparseFdList(std::move(fds_));
}

ScopedGObject<GUnixFDList> GDBusFdList::IntoGUnixFDList() && {
  std::vector<int> fds(std::move(fds_));
  return TakeGObject(g_unix_fd_list_new_from_array(fds.data(), fds.size()));
}

// static
GDBusFdList GDBusFdList::StealFromGUnixFDList(GUnixFDList* fd_list) {
  gint length;
  gint* fds = g_unix_fd_list_steal_fds(fd_list, &length);
  GDBusFdList result;
  // SAFETY: g_unix_fd_list_steal_fds() is guaranteed to return a non-null array
  // with |length| elements.
  base::span<gint> fds_span =
      UNSAFE_BUFFERS(base::span(fds, base::checked_cast<size_t>(length)));
  result.fds_.insert(result.fds_.end(), fds_span.begin(), fds_span.end());
  g_free(fds);
  return result;
}

SparseFdList::SparseFdList() = default;

SparseFdList::~SparseFdList() {
  CloseFds(fds_);
}

SparseFdList::SparseFdList(SparseFdList&&) = default;

SparseFdList& SparseFdList::operator=(SparseFdList&& other) {
  if (this != &other) {
    CloseFds(fds_);
    fds_ = std::move(other.fds_);
  }
  return *this;
}

std::optional<int> SparseFdList::Get(Handle handle) {
  if (handle.index < 0 ||
      static_cast<std::size_t>(handle.index) >= fds_.size()) {
    return std::nullopt;
  }

  int fd = fds_[handle.index];

  if (fd == kInvalidFd) {
    return std::nullopt;
  }

  return fd;
}

base::ScopedFD SparseFdList::Extract(Handle handle) {
  auto fd = Get(handle);

  if (fd.has_value()) {
    fds_[handle.index] = kInvalidFd;
    return base::ScopedFD(fd.value());
  } else {
    return {};
  }
}

SparseFdList::SparseFdList(std::vector<int> fds) : fds_(std::move(fds)) {}

}  // namespace remoting
