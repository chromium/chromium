// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
#define PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/thunk/ppb_file_system_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT FileSystemResource : public PluginResource,
                                              public thunk::PPB_FileSystem_API {
 public:
  // Creates a new FileSystemResource. The resource must be subsequently opened
  // via Open() before use.
  FileSystemResource(Connection connection,
                     PP_Instance instance,
                     PP_FileSystemType type);
  // Creates a FileSystemResource, attached to an existing pending host
  // resource. The |pending_renderer_id| and |pending_browser_id| must be
  // already-opened file systems.
  FileSystemResource(Connection connection,
                     PP_Instance instance,
                     int pending_renderer_id,
                     int pending_browser_id,
                     PP_FileSystemType type);

  FileSystemResource(const FileSystemResource&) = delete;
  FileSystemResource& operator=(const FileSystemResource&) = delete;

  ~FileSystemResource() override;

  // Resource overrides.
  thunk::PPB_FileSystem_API* AsPPB_FileSystem_API() override;

  // PPB_FileSystem_API implementation.
  int32_t Open(int64_t expected_size,
               scoped_refptr<TrackedCallback> callback) override;
  PP_FileSystemType GetType() override;
  void OpenQuotaFile(PP_Resource file_io) override;
  void CloseQuotaFile(PP_Resource file_io) override;
  int64_t RequestQuota(
      int64_t amount,
      thunk::PPB_FileSystem_API::RequestQuotaCallback callback) override;

  int32_t InitIsolatedFileSystem(const std::string& fsid,
                                 PP_IsolatedFileSystemType_Private type,
                                 base::OnceCallback<void(int32_t)> callback);

 private:
  struct QuotaRequest {
    QuotaRequest(int64_t amount,
                 thunk::PPB_FileSystem_API::RequestQuotaCallback callback);
    QuotaRequest(QuotaRequest&& other);
    QuotaRequest& operator=(QuotaRequest&& other);
    ~QuotaRequest();

    int64_t amount;
    thunk::PPB_FileSystem_API::RequestQuotaCallback callback;
  };

  // Called when the host has responded to our open request.
  void OpenComplete(scoped_refptr<TrackedCallback> callback,
                    const ResourceMessageReplyParams& params);

  // Called when the host has responded to our InitIsolatedFileSystem request.
  void InitIsolatedFileSystemReply(base::OnceClosure callback,
                                   const ResourceMessageReplyParams& params);
  void InitIsolatedFileSystemComplete(
      base::OnceCallback<void(int32_t)> callback);

  void ReserveQuota(int64_t amount);
  typedef std::map<int32_t, int64_t> OffsetMap;
  void ReserveQuotaComplete(const ResourceMessageReplyParams& params,
                            int64_t amount,
                            const OffsetMap& max_written_offsets);

  PP_FileSystemType type_;
  bool called_open_;
  uint32_t callback_count_;
  int32_t callback_result_;

  std::set<PP_Resource> files_;
  base::queue<QuotaRequest> pending_quota_requests_;
  int64_t reserved_quota_;
  bool reserving_quota_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_SYSTEM_RESOURCE_H_
