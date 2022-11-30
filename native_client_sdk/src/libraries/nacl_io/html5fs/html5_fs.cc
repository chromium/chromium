// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/html5fs/html5_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

#include "nacl_io/hash.h"
#include "nacl_io/html5fs/html5_fs_node.h"
#include "sdk_util/auto_lock.h"

namespace nacl_io {

namespace {

#if defined(WIN32)
int64_t strtoull(const char* nptr, char** endptr, int base) {
  return _strtoui64(nptr, endptr, base);
}
#endif

}  // namespace

// For HTML5, the INO should be the one used by the system, however PPAPI
// does not provide access to the real INO.  Instead, since HTML5 does not
// suport links, we assume that files are unique based on path to the base
// of the mount.
void Html5Fs::OnNodeCreated(Node* node) {
  node->stat_.st_dev = dev_;
}

void Html5Fs::OnNodeDestroyed(Node* node) {}

Error Html5Fs::OpenWithMode(const Path& path,
                            int open_flags,
                            mode_t mode,
                            ScopedNode* out_node) {
  out_node->reset(NULL);
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  PP_Resource fileref = file_ref_iface_->Create(
      filesystem_resource_, GetFullPath(path).Join().c_str());
  if (!fileref)
    return ENOENT;

  ScopedNode node(new Html5FsNode(this, fileref));
  error = node->Init(open_flags);

  // Set the INO based on the path
  node->stat_.st_ino = HashPath(path);

  if (error)
    return error;

  *out_node = node;
  return 0;
}

Path Html5Fs::GetFullPath(const Path& path) {
  if (prefix_.empty())
    return path;

  Path rel_path(path);
  rel_path.MakeRelative();
  Path full_path(prefix_);
  full_path.Append(rel_path);
  return full_path;
}

Error Html5Fs::Unlink(const Path& path) {
  return RemoveInternal(path, REMOVE_FILE);
}

Error Html5Fs::Mkdir(const Path& path, int permissions) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  // FileRef returns PP_ERROR_NOACCESS which is translated to EACCES if you
  // try to create the root directory. EEXIST is a better errno here.
  if (path.IsRoot())
    return EEXIST;

  ScopedResource fileref_resource(
      ppapi(), file_ref_iface_->Create(filesystem_resource_,
                                       GetFullPath(path).Join().c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  int32_t result = file_ref_iface_->MakeDirectory(
      fileref_resource.pp_resource(), PP_FALSE, PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  return 0;
}

Error Html5Fs::Rmdir(const Path& path) {
  return RemoveInternal(path, REMOVE_DIR);
}

Error Html5Fs::Remove(const Path& path) {
  return RemoveInternal(path, REMOVE_ALL);
}

Error Html5Fs::RemoveInternal(const Path& path, int remove_type) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  ScopedResource fileref_resource(
      ppapi(), file_ref_iface_->Create(filesystem_resource_,
                                       GetFullPath(path).Join().c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  // Check file type
  if (remove_type != REMOVE_ALL) {
    PP_FileInfo file_info;
    int32_t query_result = file_ref_iface_->Query(
        fileref_resource.pp_resource(), &file_info, PP_BlockUntilComplete());
    if (query_result != PP_OK) {
      if (query_result == PP_ERROR_FILENOTFOUND) {
        return ENOENT;
      }
      LOG_ERROR("Error querying file type");
      return EINVAL;
    }
    switch (file_info.type) {
      case PP_FILETYPE_DIRECTORY:
        if (!(remove_type & REMOVE_DIR))
          return EISDIR;
        break;
      case PP_FILETYPE_REGULAR:
        if (!(remove_type & REMOVE_FILE))
          return ENOTDIR;
        break;
      default:
        LOG_ERROR("Invalid file type: %d", file_info.type);
        return EINVAL;
    }
  }

  int32_t result = file_ref_iface_->Delete(fileref_resource.pp_resource(),
                                           PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  return 0;
}

Error Html5Fs::Rename(const Path& path, const Path& newpath) {
  Error error = BlockUntilFilesystemOpen();
  if (error)
    return error;

  std::string oldpath_full = GetFullPath(path).Join();
  ScopedResource fileref_resource(
      ppapi(),
      file_ref_iface_->Create(filesystem_resource_, oldpath_full.c_str()));
  if (!fileref_resource.pp_resource())
    return ENOENT;

  std::string newpath_full = GetFullPath(newpath).Join();
  ScopedResource new_fileref_resource(
      ppapi(),
      file_ref_iface_->Create(filesystem_resource_, newpath_full.c_str()));
  if (!new_fileref_resource.pp_resource())
    return ENOENT;

  int32_t result = file_ref_iface_->Rename(fileref_resource.pp_resource(),
                                           new_fileref_resource.pp_resource(),
                                           PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  return 0;
}

Html5Fs::Html5Fs()
    : filesystem_iface_(NULL),
      file_ref_iface_(NULL),
      file_io_iface_(NULL),
      filesystem_resource_(0),
      filesystem_open_has_result_(false),
      filesystem_open_error_(0) {}

Error Html5Fs::Init(const FsInitArgs& args) {
  pthread_cond_init(&filesystem_open_cond_, NULL);

  Error error = Filesystem::Init(args);
  if (error)
    return error;

  if (!args.ppapi) {
    LOG_ERROR("ppapi is NULL.");
    return ENOSYS;
  }

  core_iface_ = ppapi()->GetCoreInterface();
  filesystem_iface_ = ppapi()->GetFileSystemInterface();
  file_io_iface_ = ppapi()->GetFileIoInterface();
  file_ref_iface_ = ppapi()->GetFileRefInterface();

  if (!(core_iface_ && filesystem_iface_ && file_io_iface_ &&
        file_ref_iface_)) {
    LOG_ERROR("Got NULL interface(s): %s%s%s%s", core_iface_ ? "" : "Core ",
              filesystem_iface_ ? "" : "FileSystem ",
              file_ref_iface_ ? "" : "FileRef",
              file_io_iface_ ? "" : "FileIo ");
    return ENOSYS;
  }

  // Parse filesystem args.
  PP_FileSystemType filesystem_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
  int64_t expected_size = 0;
  for (StringMap_t::const_iterator iter = args.string_map.begin();
       iter != args.string_map.end(); ++iter) {
    if (iter->first == "type") {
      if (iter->second == "PERSISTENT") {
        filesystem_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
      } else if (iter->second == "TEMPORARY") {
        filesystem_type = PP_FILESYSTEMTYPE_LOCALTEMPORARY;
      } else if (iter->second.empty()) {
        filesystem_type = PP_FILESYSTEMTYPE_LOCALPERSISTENT;
      } else {
        LOG_ERROR("Unknown filesystem type: '%s'", iter->second.c_str());
        return EINVAL;
      }
    } else if (iter->first == "expected_size") {
      expected_size = strtoull(iter->second.c_str(), NULL, 10);
    } else if (iter->first == "filesystem_resource") {
      PP_Resource resource = strtoull(iter->second.c_str(), NULL, 10);
      if (!filesystem_iface_->IsFileSystem(resource))
        return EINVAL;

      filesystem_resource_ = resource;
      ppapi_->AddRefResource(filesystem_resource_);
    } else if (iter->first == "SOURCE") {
      prefix_ = iter->second;
    } else {
      LOG_ERROR("Invalid mount param: %s", iter->first.c_str());
      return EINVAL;
    }
  }

  if (filesystem_resource_ != 0) {
    filesystem_open_has_result_ = true;
    filesystem_open_error_ = PP_OK;
    return 0;
  }

  // Initialize filesystem.
  filesystem_resource_ =
      filesystem_iface_->Create(ppapi_->GetInstance(), filesystem_type);
  if (filesystem_resource_ == 0)
    return ENOSYS;

  // We can't block the main thread, so make an asynchronous call if on main
  // thread. If we are off-main-thread, then don't make an asynchronous call;
  // otherwise we require a message loop.
  bool main_thread = core_iface_->IsMainThread();
  PP_CompletionCallback cc =
      main_thread ? PP_MakeCompletionCallback(
                        &Html5Fs::FilesystemOpenCallbackThunk, this)
                  : PP_BlockUntilComplete();

  int32_t result =
      filesystem_iface_->Open(filesystem_resource_, expected_size, cc);

  if (!main_thread) {
    filesystem_open_has_result_ = true;
    filesystem_open_error_ = PPERROR_TO_ERRNO(result);

    return filesystem_open_error_;
  }

  // We have to assume the call to Open will succeed; there is no better
  // result to return here.
  return 0;
}

void Html5Fs::Destroy() {
  if (ppapi_ != NULL && filesystem_resource_ != 0)
    ppapi_->ReleaseResource(filesystem_resource_);
  pthread_cond_destroy(&filesystem_open_cond_);
}

Error Html5Fs::BlockUntilFilesystemOpen() {
  AUTO_LOCK(filesysem_open_lock_);
  while (!filesystem_open_has_result_) {
    pthread_cond_wait(&filesystem_open_cond_, filesysem_open_lock_.mutex());
  }
  return filesystem_open_error_;
}

// static
void Html5Fs::FilesystemOpenCallbackThunk(void* user_data, int32_t result) {
  Html5Fs* self = static_cast<Html5Fs*>(user_data);
  self->FilesystemOpenCallback(result);
}

void Html5Fs::FilesystemOpenCallback(int32_t result) {
  AUTO_LOCK(filesysem_open_lock_);
  filesystem_open_has_result_ = true;
  filesystem_open_error_ = PPERROR_TO_ERRNO(result);
  pthread_cond_signal(&filesystem_open_cond_);
}

}  // namespace nacl_io
