// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/jsfs/js_fs_node.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include "nacl_io/jsfs/js_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/log.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/macros.h"

namespace nacl_io {

JsFsNode::JsFsNode(Filesystem* filesystem, int32_t fd)
    : Node(filesystem),
      ppapi_(filesystem->ppapi()),
      array_iface_(ppapi_->GetVarArrayInterface()),
      buffer_iface_(ppapi_->GetVarArrayBufferInterface()),
      var_iface_(ppapi_->GetVarInterface()),
      fd_(fd) {
}

void JsFsNode::Destroy() {
  // TODO(binji): implement
}

bool JsFsNode::SendRequestAndWait(ScopedVar* out_response,
                                  const char* format,
                                  ...) {
  va_list args;
  va_start(args, format);
  bool result = filesystem()->VSendRequestAndWait(out_response, format, args);
  va_end(args);
  return result;
}

int JsFsNode::ScanVar(PP_Var var, const char* format, ...) {
  va_list args;
  va_start(args, format);
  int result = filesystem()->VScanVar(var, format, args);
  va_end(args);
  return result;
}

bool JsFsNode::CanOpen(int open_flags) {
  struct stat statbuf;
  Error error = GetStat(&statbuf);
  if (error)
    return false;

  // GetStat cached the mode in stat_.st_mode. Forward to Node::CanOpen,
  // which will check this mode against open_flags.
  return Node::CanOpen(open_flags);
}

Error JsFsNode::GetStat(struct stat* stat) {
  AUTO_LOCK(node_lock_);

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%d", "cmd", "fstat", "fildes", fd_)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

#if defined(__native_client__)
#if defined(__GLIBC__)
const char* format = "%d%lld%d%d%d%d%lld%lld%lld%lld%lld%lld%lld";
#else
const char* format = "%d%lld%d%d%d%d%lld%lld%d%d%lld%lld%lld";
#endif
#else
#define FIELD(x)                               \
  if (sizeof(stat->x) == sizeof(int64_t))      \
    strcat(format, "%lld");                    \
  else if (sizeof(stat->x) == sizeof(int16_t)) \
    strcat(format, "%hd");                     \
  else                                         \
    strcat(format, "%d");

  // For host builds, we'll build up the format string at runtime.
  char format[100] = "%d";  // First field is "error".
  FIELD(st_ino);
  FIELD(st_mode);
  FIELD(st_nlink);
  FIELD(st_uid);
  FIELD(st_gid);
  FIELD(st_rdev);
  FIELD(st_size);
  FIELD(st_blksize);
  FIELD(st_blocks);
  FIELD(st_atime);
  FIELD(st_mtime);
  FIELD(st_ctime);

#undef FIELD
#endif

  int32_t error;
  int result = ScanVar(response.pp_var(),
                       format,
                       "error", &error,
                       "st_ino", &stat->st_ino,
                       "st_mode", &stat->st_mode,
                       "st_nlink", &stat->st_nlink,
                       "st_uid", &stat->st_uid,
                       "st_gid", &stat->st_gid,
                       "st_rdev", &stat->st_rdev,
                       "st_size", &stat->st_size,
                       "st_blksize", &stat->st_blksize,
                       "st_blocks", &stat->st_blocks,
                       "st_atime", &stat->st_atime,
                       "st_mtime", &stat->st_mtime,
                       "st_ctime", &stat->st_ctime);

  if (result >= 1 && error)
    return error;

  if (result != 13) {
    LOG_ERROR(
        "Expected \"st_*\" and \"error\" fields in response (should be 13 "
        "total, got %d).", result);
    return EINVAL;
  }

  stat->st_dev = filesystem()->dev();

  return 0;
}

Error JsFsNode::GetSize(off_t* out_size) {
  *out_size = 0;

  struct stat statbuf;
  Error error = GetStat(&statbuf);
  if (error)
    return error;

  *out_size = stat_.st_size;
  return 0;
}

Error JsFsNode::FSync() {
  AUTO_LOCK(node_lock_);

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%d", "cmd", "fsync", "fildes", fd_)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return filesystem()->ErrorFromResponse(response);
}

Error JsFsNode::FTruncate(off_t length) {
  AUTO_LOCK(node_lock_);

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response,
      "%s%d%lld", "cmd", "ftruncate", "fildes", fd_, "length", length)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  return filesystem()->ErrorFromResponse(response);
}

Error JsFsNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  AUTO_LOCK(node_lock_);

  *out_bytes = 0;

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%d%u%lld",
                          "cmd", "pread",
                          "fildes", fd_,
                          "nbyte", count,
                          "offset", attr.offs)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  int32_t error;

  PP_Var buf_var;
  int result =
      ScanVar(response.pp_var(), "%d%p", "error", &error, "buf", &buf_var);
  ScopedVar scoped_buf_var(ppapi_, buf_var);

  if (result >= 1 && error)
    return error;

  if (result != 2) {
    LOG_ERROR("Expected \"error\" and \"buf\" fields in response.");
    return EINVAL;
  }

  if (buf_var.type != PP_VARTYPE_ARRAY_BUFFER) {
    LOG_ERROR("Expected \"buf\" to be an ArrayBuffer.");
    return EINVAL;
  }

  uint32_t src_buf_len;
  if (!buffer_iface_->ByteLength(buf_var, &src_buf_len)) {
    LOG_ERROR("Unable to get byteLength of \"buf\".");
    return EINVAL;
  }

  if (src_buf_len > count)
    src_buf_len = count;

  void* src_buf = buffer_iface_->Map(buf_var);
  if (src_buf == NULL) {
    LOG_ERROR("Unable to map \"buf\".");
    return EINVAL;
  }

  memcpy(buf, src_buf, src_buf_len);
  *out_bytes = src_buf_len;

  buffer_iface_->Unmap(buf_var);

  return 0;
}

Error JsFsNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  AUTO_LOCK(node_lock_);

  *out_bytes = 0;

  PP_Var buf_var = buffer_iface_->Create(count);
  ScopedVar scoped_buf_var(ppapi_, buf_var);

  if (buf_var.type != PP_VARTYPE_ARRAY_BUFFER) {
    LOG_ERROR("Unable to create \"buf\" var.");
    return EINVAL;
  }

  void* dst_buf = buffer_iface_->Map(buf_var);
  if (dst_buf == NULL) {
    LOG_ERROR("Unable to map \"buf\".");
    return EINVAL;
  }

  memcpy(dst_buf, buf, count);

  buffer_iface_->Unmap(buf_var);

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%d%p%u%lld",
                          "cmd", "pwrite",
                          "fildes", fd_,
                          "buf", &buf_var,
                          "nbyte", count,
                          "offset", attr.offs)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  int error;
  uint32_t nwrote;
  int result =
      ScanVar(response.pp_var(), "%d%u", "error", &error, "nwrote", &nwrote);

  if (result >= 1 && error)
    return error;

  if (result != 2) {
    LOG_ERROR("Expected \"error\" and \"nwrote\" fields in response.");
    return EINVAL;
  }

  *out_bytes = nwrote;
  return 0;
}

Error JsFsNode::GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes) {
  AUTO_LOCK(node_lock_);

  *out_bytes = 0;

  // Round to the nearest sizeof(dirent) and ask for that.
  size_t first = offs / sizeof(dirent);
  size_t last = (offs + count + sizeof(dirent) - 1) / sizeof(dirent);

  ScopedVar response(ppapi_);
  if (!SendRequestAndWait(&response, "%s%d%u%u",
                          "cmd", "getdents",
                          "fildes", fd_,
                          "offs", first,
                          "count", last - first)) {
    LOG_ERROR("Failed to send request.");
    return EINVAL;
  }

  int error;
  PP_Var dirents_var;
  int result = ScanVar(
      response.pp_var(), "%d%p", "error", &error, "dirents", &dirents_var);

  ScopedVar scoped_dirents_var(ppapi_, dirents_var);

  if (result >= 1 && error)
    return error;

  if (result != 2) {
    LOG_ERROR("Expected \"error\" and \"dirents\" fields in response.");
    return EINVAL;
  }

  if (dirents_var.type != PP_VARTYPE_ARRAY) {
    LOG_ERROR("Expected \"dirents\" to be an Array.");
    return EINVAL;
  }

  uint32_t dirents_len = array_iface_->GetLength(dirents_var);
  uint32_t dirents_byte_len = dirents_len * sizeof(dirent);

  // Allocate enough full dirents to copy from. This makes it easier if, for
  // some reason, we are reading unaligned dirents.
  dirent* dirents = static_cast<dirent*>(malloc(dirents_byte_len));

  for (uint32_t i = 0; i < dirents_len; ++i) {
    PP_Var dirent_var = array_iface_->Get(dirents_var, i);
    PP_Var d_name_var;
    result = ScanVar(dirent_var,
                     "%lld%p",
                     "d_ino", &dirents[i].d_ino,
                     "d_name", &d_name_var);
    ScopedVar scoped_dirent_var(ppapi_, dirent_var);
    ScopedVar scoped_d_name_var(ppapi_, d_name_var);

    if (result != 2) {
      LOG_ERROR("Expected dirent[%d] to have \"d_ino\" and \"d_name\".", i);
      free(dirents);
      return EINVAL;
    }

    uint32_t d_name_len;
    const char* d_name = var_iface_->VarToUtf8(d_name_var, &d_name_len);

    dirents[i].d_reclen = sizeof(dirent);
    strncpy(dirents[i].d_name, d_name, sizeof(dirents[i].d_name));
  }

  size_t dirents_offs = offs - first * sizeof(dirent);
  if (dirents_offs + count > dirents_byte_len)
    count = dirents_byte_len - dirents_offs;

  memcpy(pdir, reinterpret_cast<const char*>(dirents) + dirents_offs, count);
  *out_bytes = count;

  free(dirents);
  return 0;
}

}  // namespace nacl_io
