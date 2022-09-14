// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WIN32)
#define _CRT_RAND_S
#endif

#include "nacl_io/devfs/dev_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "nacl_io/devfs/jspipe_node.h"
#include "nacl_io/devfs/tty_node.h"
#include "nacl_io/dir_node.h"
#include "nacl_io/kernel_wrap_real.h"
#include "nacl_io/node.h"
#include "nacl_io/osunistd.h"
#include "nacl_io/passthroughfs/real_node.h"
#include "nacl_io/pepper_interface.h"
#include "sdk_util/auto_lock.h"

#if defined(__native_client__)
#include <irt.h>
#elif defined(WIN32)
#include <stdlib.h>
#endif

namespace nacl_io {

namespace {

class NullNode : public CharNode {
 public:
  explicit NullNode(Filesystem* filesystem) : CharNode(filesystem) {}

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class ConsoleNode : public CharNode {
 public:
  ConsoleNode(Filesystem* filesystem, PP_LogLevel level);

  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
  PP_LogLevel level_;
};

class ZeroNode : public Node {
 public:
  explicit ZeroNode(Filesystem* filesystem);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
};

class UrandomNode : public Node {
 public:
  explicit UrandomNode(Filesystem* filesystem);

  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);

 private:
#if defined(__native_client__)
  nacl_irt_random random_interface_;
  bool interface_ok_;
#endif
};

class FsNode : public Node {
 public:
  FsNode(Filesystem* filesystem, Filesystem* other_fs);

  virtual Error VIoctl(int request, va_list args);

 private:
  // Don't addref the filesystem. We are relying on the fact that the
  // KernelObject will keep the filsystem around as long as we need it, and
  // this node will be destroyed when the filesystem is destroyed.
  Filesystem* other_fs_;
};

Error NullNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  *out_bytes = 0;
  return 0;
}

Error NullNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

ConsoleNode::ConsoleNode(Filesystem* filesystem, PP_LogLevel level)
    : CharNode(filesystem), level_(level) {
}

Error ConsoleNode::Write(const HandleAttr& attr,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = 0;

  ConsoleInterface* con_iface = filesystem_->ppapi()->GetConsoleInterface();
  VarInterface* var_iface = filesystem_->ppapi()->GetVarInterface();

  if (!(var_iface && con_iface)) {
    LOG_ERROR("Got NULL interface(s): %s%s",
              con_iface ? "" : "Console ",
              var_iface ? "" : "Var");
    return ENOSYS;
  }

  const char* var_data = static_cast<const char*>(buf);
  uint32_t len = static_cast<uint32_t>(count);
  struct PP_Var val = var_iface->VarFromUtf8(var_data, len);
  con_iface->Log(filesystem_->ppapi()->GetInstance(), level_, val);
  var_iface->Release(val);

  *out_bytes = count;
  return 0;
}

ZeroNode::ZeroNode(Filesystem* filesystem) : Node(filesystem) {
  SetType(S_IFCHR);
}

Error ZeroNode::Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes) {
  memset(buf, 0, count);
  *out_bytes = count;
  return 0;
}

Error ZeroNode::Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes) {
  *out_bytes = count;
  return 0;
}

UrandomNode::UrandomNode(Filesystem* filesystem) : Node(filesystem) {
  SetType(S_IFCHR);
#if defined(__native_client__)
  size_t result = nacl_interface_query(
      NACL_IRT_RANDOM_v0_1, &random_interface_, sizeof(random_interface_));
  interface_ok_ = result != 0;
#endif
}

Error UrandomNode::Read(const HandleAttr& attr,
                        void* buf,
                        size_t count,
                        int* out_bytes) {
  *out_bytes = 0;

#if defined(__native_client__)
  if (!interface_ok_) {
    LOG_ERROR("NACL_IRT_RANDOM_v0_1 interface not avaiable.");
    return EBADF;
  }

  size_t nread;
  int error = (*random_interface_.get_random_bytes)(buf, count, &nread);
  if (error)
    return error;
#elif defined(WIN32)
  char* out = static_cast<char*>(buf);
  size_t bytes_left = count;
  while (bytes_left) {
    unsigned int random_int;
    errno_t err = rand_s(&random_int);
    if (err) {
      *out_bytes = count - bytes_left;
      return err;
    }

    int bytes_to_copy = std::min(bytes_left, sizeof(random_int));
    memcpy(out, &random_int, bytes_to_copy);
    out += bytes_to_copy;
    bytes_left -= bytes_to_copy;
  }
#endif

  *out_bytes = count;
  return 0;
}

Error UrandomNode::Write(const HandleAttr& attr,
                         const void* buf,
                         size_t count,
                         int* out_bytes) {
  *out_bytes = count;
  return 0;
}

FsNode::FsNode(Filesystem* filesystem, Filesystem* other_fs)
    : Node(filesystem), other_fs_(other_fs) {
}

Error FsNode::VIoctl(int request, va_list args) {
  return other_fs_->Filesystem_VIoctl(request, args);
}

}  // namespace

Error DevFs::OpenWithMode(const Path& path, int open_flags,
                          mode_t mode, ScopedNode* out_node) {
  out_node->reset(NULL);
  int error;
  if (path.Part(1) == "fs") {
    if (path.Size() == 3) {
      error = fs_dir_->FindChild(path.Part(2), out_node);
    } else {
      LOG_TRACE("Bad devfs path: %s", path.Join().c_str());
      error = ENOENT;
    }
  } else {
    error = root_->FindChild(path.Join(), out_node);
  }

  // Only return EACCES when trying to create a node that does not exist.
  if ((error == ENOENT) && (open_flags & O_CREAT)) {
    LOG_TRACE("Cannot create devfs node: %s", path.Join().c_str());
    return EACCES;
  }

  return error;
}

Error DevFs::Unlink(const Path& path) {
  LOG_ERROR("unlink not supported.");
  return EPERM;
}

Error DevFs::Mkdir(const Path& path, int permissions) {
  LOG_ERROR("mkdir not supported.");
  return EPERM;
}

Error DevFs::Rmdir(const Path& path) {
  LOG_ERROR("rmdir not supported.");
  return EPERM;
}

Error DevFs::Remove(const Path& path) {
  LOG_ERROR("remove not supported.");
  return EPERM;
}

Error DevFs::Rename(const Path& path, const Path& newpath) {
  LOG_ERROR("rename not supported.");
  return EPERM;
}

Error DevFs::CreateFsNode(Filesystem* other_fs) {
  int dev = other_fs->dev();
  char path[32];
  snprintf(path, 32, "%d", dev);
  ScopedNode new_node(new FsNode(this, other_fs));
  return fs_dir_->AddChild(path, new_node);
}

Error DevFs::DestroyFsNode(Filesystem* other_fs) {
  int dev = other_fs->dev();
  char path[32];
  snprintf(path, 32, "%d", dev);
  return fs_dir_->RemoveChild(path);
}

DevFs::DevFs() {
}

#define INITIALIZE_DEV_NODE(path, klass)   \
  new_node = ScopedNode(new klass(this));  \
  error = root_->AddChild(path, new_node); \
  if (error)                               \
    return error;

#define INITIALIZE_DEV_NODE_1(path, klass, arg) \
  new_node = ScopedNode(new klass(this, arg));  \
  error = root_->AddChild(path, new_node);      \
  if (error)                                    \
    return error;

Error DevFs::Init(const FsInitArgs& args) {
  Error error = Filesystem::Init(args);
  if (error)
    return error;

  root_.reset(new DirNode(this, S_IRALL | S_IXALL));

  ScopedNode new_node;
  INITIALIZE_DEV_NODE("/null", NullNode);
  INITIALIZE_DEV_NODE("/zero", ZeroNode);
  INITIALIZE_DEV_NODE("/urandom", UrandomNode);
  INITIALIZE_DEV_NODE_1("/console0", ConsoleNode, PP_LOGLEVEL_TIP);
  INITIALIZE_DEV_NODE_1("/console1", ConsoleNode, PP_LOGLEVEL_LOG);
  INITIALIZE_DEV_NODE_1("/console2", ConsoleNode, PP_LOGLEVEL_WARNING);
  INITIALIZE_DEV_NODE_1("/console3", ConsoleNode, PP_LOGLEVEL_ERROR);
  INITIALIZE_DEV_NODE("/tty", TtyNode);
  INITIALIZE_DEV_NODE_1("/stdin", RealNode, 0);
  INITIALIZE_DEV_NODE_1("/stdout", RealNode, 1);
  INITIALIZE_DEV_NODE_1("/stderr", RealNode, 2);
  INITIALIZE_DEV_NODE("/jspipe1", JSPipeNode);
  new_node->Ioctl(NACL_IOC_PIPE_SETNAME, "jspipe1");
  INITIALIZE_DEV_NODE("/jspipe2", JSPipeNode);
  new_node->Ioctl(NACL_IOC_PIPE_SETNAME, "jspipe2");
  INITIALIZE_DEV_NODE("/jspipe3", JSPipeNode);
  new_node->Ioctl(NACL_IOC_PIPE_SETNAME, "jspipe3");

  // Add a directory for "fs" nodes; they represent all currently-mounted
  // filesystems. We can ioctl these nodes to make changes or provide input to
  // a mounted filesystem.
  INITIALIZE_DEV_NODE_1("/fs", DirNode, S_IRALL | S_IWALL | S_IXALL);
  fs_dir_ = new_node;

  return 0;
}

}  // namespace nacl_io
