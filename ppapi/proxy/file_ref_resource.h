// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_REF_RESOURCE_H_
#define PPAPI_PROXY_FILE_REF_RESOURCE_H_

#include <stdint.h>

#include <vector>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/ppb_file_ref_api.h"

namespace ppapi {
class StringVar;

namespace proxy {

class PPAPI_PROXY_EXPORT FileRefResource
    : public PluginResource,
      public thunk::PPB_FileRef_API {
 public:
  static PP_Resource CreateFileRef(Connection connection,
                                   PP_Instance instance,
                                   const FileRefCreateInfo& info);

  FileRefResource(const FileRefResource&) = delete;
  FileRefResource& operator=(const FileRefResource&) = delete;

  ~FileRefResource() override;

  // Resource implementation.
  thunk::PPB_FileRef_API* AsPPB_FileRef_API() override;

  // PPB_FileRef_API implementation.
  PP_FileSystemType GetFileSystemType() const override;
  PP_Var GetName() const override;
  PP_Var GetPath() const override;
  PP_Resource GetParent() override;
  int32_t MakeDirectory(int32_t make_directory_flags,
                        scoped_refptr<TrackedCallback> callback) override;
  int32_t Touch(PP_Time last_access_time,
                PP_Time last_modified_time,
                scoped_refptr<TrackedCallback> callback) override;
  int32_t Delete(scoped_refptr<TrackedCallback> callback) override;
  int32_t Rename(PP_Resource new_file_ref,
                 scoped_refptr<TrackedCallback> callback) override;
  int32_t Query(PP_FileInfo* info,
                scoped_refptr<TrackedCallback> callback) override;
  int32_t ReadDirectoryEntries(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) override;
  const FileRefCreateInfo& GetCreateInfo() const override;

  // Private API
  PP_Var GetAbsolutePath() override;

 private:
  FileRefResource(Connection connection,
                  PP_Instance instance,
                  const FileRefCreateInfo& info);

  void RunTrackedCallback(scoped_refptr<TrackedCallback> callback,
                          const ResourceMessageReplyParams& params);

  void OnQueryReply(PP_FileInfo* out_info,
                    scoped_refptr<TrackedCallback> callback,
                    const ResourceMessageReplyParams& params,
                    const PP_FileInfo& info);

  void OnDirectoryEntriesReply(
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback,
      const ResourceMessageReplyParams& params,
      const std::vector<ppapi::FileRefCreateInfo>& infos,
      const std::vector<PP_FileType>& file_types);

  bool uses_internal_paths() const;

  // Populated after creation.
  FileRefCreateInfo create_info_;

  // Some file ref operations may fail if the the file system resource inside
  // create_info_ is destroyed. Therefore, we explicitly hold a reference to
  // the file system resource to make sure it outlives the file ref.
  ScopedPPResource file_system_resource_;

  scoped_refptr<StringVar> name_var_;
  scoped_refptr<StringVar> path_var_;
  scoped_refptr<StringVar> absolute_path_var_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_REF_RESOURCE_H_
