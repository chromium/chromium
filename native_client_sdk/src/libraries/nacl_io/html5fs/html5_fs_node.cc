// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/html5fs/html5_fs_node.h"

#include <errno.h>
#include <fcntl.h>
#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_directory_entry.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_file_info.h>
#include <ppapi/c/ppb_file_io.h>
#include <string.h>
#include <vector>

#include "nacl_io/filesystem.h"
#include "nacl_io/getdents_helper.h"
#include "nacl_io/hash.h"
#include "nacl_io/html5fs/html5_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

struct OutputBuffer {
  void* data;
  int element_count;
};

void* GetOutputBuffer(void* user_data, uint32_t count, uint32_t size) {
  OutputBuffer* output = static_cast<OutputBuffer*>(user_data);
  output->element_count = count;
  if (count) {
    output->data = malloc(count * size);
    if (!output->data)
      output->element_count = 0;
  } else {
    output->data = NULL;
  }
  return output->data;
}

int32_t OpenFlagsToPPAPIOpenFlags(int open_flags) {
  int32_t ppapi_flags = 0;

  switch (open_flags & 3) {
    default:
    case O_RDONLY:
      ppapi_flags = PP_FILEOPENFLAG_READ;
      break;
    case O_WRONLY:
      ppapi_flags = PP_FILEOPENFLAG_WRITE;
      break;
    case O_RDWR:
      ppapi_flags = PP_FILEOPENFLAG_READ | PP_FILEOPENFLAG_WRITE;
      break;
  }

  if (open_flags & O_CREAT)
    ppapi_flags |= PP_FILEOPENFLAG_CREATE;
  if (open_flags & O_TRUNC)
    ppapi_flags |= PP_FILEOPENFLAG_TRUNCATE;
  if (open_flags & O_EXCL)
    ppapi_flags |= PP_FILEOPENFLAG_EXCLUSIVE;

  return ppapi_flags;
}

}  // namespace

Error Html5FsNode::FSync() {
  // Cannot call Flush on a directory; simply do nothing.
  if (IsaDir())
    return 0;

  int32_t result =
      file_io_iface_->Flush(fileio_resource_, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);
  return 0;
}

Error Html5FsNode::GetDents(size_t offs,
                            struct dirent* pdir,
                            size_t size,
                            int* out_bytes) {
  *out_bytes = 0;

  // If this is not a directory, fail
  if (!IsaDir())
    return ENOTDIR;

  // TODO(binji): Better handling of ino numbers.
  const ino_t kCurDirIno = -1;
  const ino_t kParentDirIno = -2;
  GetDentsHelper helper(kCurDirIno, kParentDirIno);

  OutputBuffer output_buf = {NULL, 0};
  PP_ArrayOutput output = {&GetOutputBuffer, &output_buf};
  int32_t result = file_ref_iface_->ReadDirectoryEntries(
      fileref_resource_, output, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  PP_DirectoryEntry* entries = static_cast<PP_DirectoryEntry*>(output_buf.data);

  for (int i = 0; i < output_buf.element_count; ++i) {
    PP_Var file_name_var = file_ref_iface_->GetName(entries[i].file_ref);

    // Release the file reference.
    filesystem_->ppapi()->ReleaseResource(entries[i].file_ref);

    if (file_name_var.type != PP_VARTYPE_STRING)
      continue;

    uint32_t file_name_length;
    const char* file_name =
        var_iface_->VarToUtf8(file_name_var, &file_name_length);

    if (file_name) {
      file_name_length =
          std::min(static_cast<size_t>(file_name_length),
                   MEMBER_SIZE(dirent, d_name) - 1);  // -1 for NULL.

      // The INO is based on the running hash of fully qualified path, so
      // a childs INO must be the parent directories hash, plus '/', plus
      // the filename.
      ino_t child_ino =
          HashPathSegment(stat_.st_ino, file_name, file_name_length);

      helper.AddDirent(child_ino, file_name, file_name_length);
    }

    var_iface_->Release(file_name_var);
  }

  // Release the output buffer.
  free(output_buf.data);

  return helper.GetDents(offs, pdir, size, out_bytes);
}

Error Html5FsNode::GetStat(struct stat* stat) {
  AUTO_LOCK(node_lock_);

  PP_FileInfo info;
  int32_t result =
      file_ref_iface_->Query(fileref_resource_, &info, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  // Fill in known info here.
  memcpy(stat, &stat_, sizeof(stat_));

  // Fill in the additional info from ppapi.
  switch (info.type) {
    case PP_FILETYPE_REGULAR:
      stat->st_mode |= S_IFREG;
      break;
    case PP_FILETYPE_DIRECTORY:
      stat->st_mode |= S_IFDIR;
      break;
    case PP_FILETYPE_OTHER:
    default:
      break;
  }
  stat->st_size = static_cast<off_t>(info.size);
  stat->st_atime = info.last_access_time;
  stat->st_mtime = info.last_modified_time;
  stat->st_ctime = info.creation_time;

  return 0;
}

Error Html5FsNode::Read(const HandleAttr& attr,
                        void* buf,
                        size_t count,
                        int* out_bytes) {
  *out_bytes = 0;

  if (IsaDir())
    return EISDIR;

  int32_t result = file_io_iface_->Read(
      fileio_resource_, attr.offs, static_cast<char*>(buf),
      static_cast<int32_t>(count), PP_BlockUntilComplete());
  if (result < 0)
    return PPERROR_TO_ERRNO(result);

  *out_bytes = result;
  return 0;
}

Error Html5FsNode::FTruncate(off_t size) {
  if (IsaDir())
    return EISDIR;

  int32_t result = file_io_iface_->SetLength(fileio_resource_, size,
                                             PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);
  return 0;
}

Error Html5FsNode::Write(const HandleAttr& attr,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = 0;

  if (IsaDir())
    return EISDIR;

  int32_t result = file_io_iface_->Write(
      fileio_resource_, attr.offs, static_cast<const char*>(buf),
      static_cast<int32_t>(count), PP_BlockUntilComplete());
  if (result < 0)
    return PPERROR_TO_ERRNO(result);

  *out_bytes = result;
  return 0;
}

int Html5FsNode::GetType() {
  return fileio_resource_ ? S_IFREG : S_IFDIR;
}

Error Html5FsNode::GetSize(off_t* out_size) {
  *out_size = 0;

  if (IsaDir())
    return 0;

  AUTO_LOCK(node_lock_);

  PP_FileInfo info;
  int32_t result =
      file_io_iface_->Query(fileio_resource_, &info, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  *out_size = info.size;
  return 0;
}

Html5FsNode::Html5FsNode(Filesystem* filesystem, PP_Resource fileref_resource)
    : Node(filesystem),
      fileref_resource_(fileref_resource),
      fileio_resource_(0) {}

Error Html5FsNode::Init(int open_flags) {
  Error error = Node::Init(open_flags);
  if (error)
    return error;

  file_io_iface_ = filesystem_->ppapi()->GetFileIoInterface();
  file_ref_iface_ = filesystem_->ppapi()->GetFileRefInterface();
  var_iface_ = filesystem_->ppapi()->GetVarInterface();

  if (!(file_io_iface_ && file_ref_iface_ && var_iface_)) {
    LOG_ERROR("Got NULL interface(s): %s%s%s", file_ref_iface_ ? "" : "FileRef",
              file_io_iface_ ? "" : "FileIo ", var_iface_ ? "" : "Var ");
    return EIO;
  }

  // Set all files and directories to RWX.
  SetMode(S_IWALL | S_IRALL | S_IXALL);

  // First query the FileRef to see if it is a file or directory.
  PP_FileInfo file_info;
  int32_t query_result = file_ref_iface_->Query(fileref_resource_, &file_info,
                                                PP_BlockUntilComplete());
  // If this is a directory, do not get a FileIO.
  if (query_result == PP_OK && file_info.type == PP_FILETYPE_DIRECTORY) {
    return 0;
  }

  fileio_resource_ =
      file_io_iface_->Create(filesystem_->ppapi()->GetInstance());
  if (!fileio_resource_) {
    LOG_ERROR("Couldn't create FileIo resource.");
    return EIO;
  }

  int32_t open_result = file_io_iface_->Open(
      fileio_resource_, fileref_resource_,
      OpenFlagsToPPAPIOpenFlags(open_flags), PP_BlockUntilComplete());
  if (open_result != PP_OK)
    return PPERROR_TO_ERRNO(open_result);
  return 0;
}

void Html5FsNode::Destroy() {
  FSync();

  if (fileio_resource_) {
    file_io_iface_->Close(fileio_resource_);
    filesystem_->ppapi()->ReleaseResource(fileio_resource_);
  }

  filesystem_->ppapi()->ReleaseResource(fileref_resource_);
  fileio_resource_ = 0;
  fileref_resource_ = 0;
  Node::Destroy();
}

Error Html5FsNode::Fchmod(mode_t mode) {
  // html5fs does not support any kinds of permissions or mode bits.
  return 0;
}

}  // namespace nacl_io
