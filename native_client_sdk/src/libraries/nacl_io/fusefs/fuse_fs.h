// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_H_
#define LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_H_

#include <map>

#include "nacl_io/filesystem.h"
#include "nacl_io/fuse.h"
#include "nacl_io/node.h"

namespace nacl_io {

class FuseFs : public Filesystem {
 public:
  FuseFs(const FuseFs&) = delete;
  FuseFs& operator=(const FuseFs&) = delete;

 protected:
  FuseFs();

  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();

 public:
  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int perm);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

 private:
  struct fuse_operations* fuse_ops_;
  void* fuse_user_data_;

  friend class FuseFsNode;
  friend class FuseFsFactory;
};

class FuseFsNode : public Node {
 protected:
  FuseFsNode(Filesystem* filesystem,
             struct fuse_operations* fuse_ops,
             struct fuse_file_info& info,
             const std::string& path);

 public:
  virtual bool CanOpen(int open_flags);
  virtual Error GetStat(struct stat* stat);
  virtual Error VIoctl(int request, va_list args);
  virtual Error Tcflush(int queue_selector);
  virtual Error Tcgetattr(struct termios* termios_p);
  virtual Error Tcsetattr(int optional_actions,
                          const struct termios* termios_p);
  virtual Error GetSize(off_t* out_size);
  virtual Error Futimens(const struct timespec times[2]);
  virtual Error Fchmod(mode_t mode);

 protected:
  struct fuse_operations* fuse_ops_;
  struct fuse_file_info info_;
  std::string path_;
};

class FileFuseFsNode : public FuseFsNode {
 public:
  FileFuseFsNode(Filesystem* filesystem,
                 struct fuse_operations* fuse_ops,
                 struct fuse_file_info& info,
                 const std::string& path);

  FileFuseFsNode(const FileFuseFsNode&) = delete;
  FileFuseFsNode& operator=(const FileFuseFsNode&) = delete;

 protected:
  virtual void Destroy();

 public:
  virtual Error FSync();
  virtual Error FTruncate(off_t length);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  friend class FuseFs;
};

class DirFuseFsNode : public FuseFsNode {
 public:
  DirFuseFsNode(Filesystem* filesystem,
                struct fuse_operations* fuse_ops,
                struct fuse_file_info& info,
                const std::string& path);

  DirFuseFsNode(const DirFuseFsNode&) = delete;
  DirFuseFsNode& operator=(const DirFuseFsNode&) = delete;

 protected:
  virtual void Destroy();

 public:
  virtual Error FSync();
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);

 private:
  static int FillDirCallback(void* buf,
                             const char* name,
                             const struct stat* stbuf,
                             off_t off);

 private:
  friend class FuseFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_FUSEFS_FUSE_FS_H_
