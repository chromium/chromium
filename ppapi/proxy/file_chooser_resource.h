// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_CHOOSER_RESOURCE_H_
#define PPAPI_PROXY_FILE_CHOOSER_RESOURCE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_file_chooser_api.h"

namespace ppapi {

struct FileRefCreateInfo;

namespace proxy {

class PPAPI_PROXY_EXPORT FileChooserResource
    : public PluginResource,
      public thunk::PPB_FileChooser_API {
 public:
  FileChooserResource(Connection connection,
                      PP_Instance instance,
                      PP_FileChooserMode_Dev mode,
                      const std::string& accept_types);

  FileChooserResource(const FileChooserResource&) = delete;
  FileChooserResource& operator=(const FileChooserResource&) = delete;

  ~FileChooserResource() override;

  // Resource overrides.
  thunk::PPB_FileChooser_API* AsPPB_FileChooser_API() override;

  // PPB_FileChooser_API.
  int32_t Show(const PP_ArrayOutput& output,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t ShowWithoutUserGesture(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      const PP_ArrayOutput& output,
      scoped_refptr<TrackedCallback> callback) override;
  int32_t Show0_5(scoped_refptr<TrackedCallback> callback) override;
  PP_Resource GetNextChosenFile() override;
  int32_t ShowWithoutUserGesture0_5(
      PP_Bool save_as,
      PP_Var suggested_file_name,
      scoped_refptr<TrackedCallback> callback) override;

  // Parses the accept string into the given vector.
  static void PopulateAcceptTypes(const std::string& input,
                                  std::vector<std::string>* output);

 private:
  void OnPluginMsgShowReply(
      const ResourceMessageReplyParams& params,
      const std::vector<FileRefCreateInfo>& chosen_files);

  int32_t ShowInternal(PP_Bool save_as,
                       const PP_Var& suggested_file_name,
                       scoped_refptr<TrackedCallback> callback);

  PP_FileChooserMode_Dev mode_;
  std::vector<std::string> accept_types_;

  // When using v0.6 of the API, contains the array output info.
  ArrayWriter output_;

  // When using v0.5 of the API, contains all files returned by the current
  // show callback that haven't yet been given to the plugin. The plugin will
  // repeatedly call us to get the next file, and we'll vend those out of this
  // queue, removing them when ownership has transferred to the plugin.
  base::queue<PP_Resource> file_queue_;

  scoped_refptr<TrackedCallback> callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_CHOOSER_RESOURCE_H_
