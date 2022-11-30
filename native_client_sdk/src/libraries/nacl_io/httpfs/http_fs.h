// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_H_
#define LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_H_

#include <string>
#include "nacl_io/filesystem.h"
#include "nacl_io/pepper_interface.h"
#include "nacl_io/typed_fs_factory.h"

namespace nacl_io {

std::string NormalizeHeaderKey(const std::string& s);

class HttpFs : public Filesystem {
 public:
  typedef std::map<std::string, ScopedNode> NodeMap_t;

  virtual Error OpenWithMode(const Path& path, int open_flags, mode_t mode,
                             ScopedNode* out_node);
  virtual Error Unlink(const Path& path);
  virtual Error Mkdir(const Path& path, int permissions);
  virtual Error Rmdir(const Path& path);
  virtual Error Remove(const Path& path);
  virtual Error Rename(const Path& path, const Path& newpath);

  PP_Resource MakeUrlRequestInfo(const std::string& url,
                                 const char* method,
                                 StringMap_t* additional_headers);

 protected:
  HttpFs();

  virtual Error Init(const FsInitArgs& args);
  virtual void Destroy();
  ScopedNode FindExistingNode(const Path& path);
  Error FindOrCreateDir(const Path& path, ScopedNode* out_node);
  Error LoadManifest(const std::string& path, char** out_manifest);
  Error ParseManifest(const char* text);

  NodeMap_t* GetNodeCacheForTesting() { return &node_cache_; }

 private:

  // Gets the URL to fetch for |path|.
  // |path| is relative to the mount point for the HTTP filesystem.
  std::string MakeUrl(const Path& path);

  std::string url_root_;
  StringMap_t headers_;
  NodeMap_t node_cache_;
  bool allow_cors_;
  bool allow_credentials_;
  bool cache_stat_;
  bool cache_content_;
  bool is_blob_url_;

  friend class TypedFsFactory<HttpFs>;
  friend class HttpFsNode;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_H_
