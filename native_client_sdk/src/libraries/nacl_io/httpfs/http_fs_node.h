// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_NODE_H_
#define LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_NODE_H_

#include <map>
#include <string>
#include <vector>

#include "nacl_io/error.h"
#include "nacl_io/node.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

typedef std::map<std::string, std::string> StringMap_t;

class HttpFsNode : public Node {
 public:
  virtual Error FSync();
  virtual Error GetDents(size_t offs,
                         struct dirent* pdir,
                         size_t count,
                         int* out_bytes);
  virtual Error GetStat(struct stat* stat);
  virtual Error Read(const HandleAttr& attr,
                     void* buf,
                     size_t count,
                     int* out_bytes);
  virtual Error FTruncate(off_t size);
  virtual Error Write(const HandleAttr& attr,
                      const void* buf,
                      size_t count,
                      int* out_bytes);
  virtual Error GetSize(off_t* out_size);

  void SetCachedSize(off_t size);

 protected:
  HttpFsNode(Filesystem* filesystem,
             const std::string& url,
             bool cache_content);

  virtual ~HttpFsNode();

 private:
  Error GetStat_Locked(struct stat* stat);
  Error OpenUrl(const char* method,
                StringMap_t* request_headers,
                ScopedResource* out_loader,
                ScopedResource* out_request,
                ScopedResource* out_response,
                int32_t* out_statuscode,
                StringMap_t* out_response_headers);

  Error DownloadToCache();
  Error ReadPartialFromCache(const HandleAttr& attr,
                             void* buf,
                             int count,
                             int* out_bytes);
  Error DownloadPartial(const HandleAttr& attr,
                        void* buf,
                        off_t count,
                        int* out_bytes);

  Error DownloadToTemp(off_t* out_bytes);

  // Read as much as possible from |loader|, using |buffer_| as a scratch area.
  Error ReadEntireResponseToTemp(const ScopedResource& loader,
                                 off_t* out_bytes);
  // Read as much as possible from |loader|, storing the result in
  // |cached_data_|.
  Error ReadEntireResponseToCache(const ScopedResource& loader, int* out_bytes);

  // Read up to |count| bytes from |loader|, using |buffer_| as a scratch area.
  Error ReadResponseToTemp(const ScopedResource& loader,
                           int count,
                           int* out_bytes);

  // Read up to |count| bytes from |loader|, and store the result in |buf|,
  // which is assumed to have |count| bytes free.
  Error ReadResponseToBuffer(const ScopedResource& loader,
                             void* buf,
                             int count,
                             int* out_bytes);

  std::string url_;
  char* buffer_;
  int buffer_len_;

  bool cache_content_;
  bool has_cached_size_;
  std::vector<char> cached_data_;

  friend class HttpFs;
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_HTTPFS_HTTP_FS_NODE_H_
