// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_REF_RENDERER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_REF_RENDERER_HOST_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_system_host.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "url/gurl.h"

namespace content {

class PepperFileRefRendererHost : public ppapi::host::ResourceHost {
 public:
  PepperFileRefRendererHost(RendererPpapiHost* host,
                            PP_Instance instance,
                            PP_Resource resource,
                            PP_Resource file_system,
                            const std::string& internal_path);

  PepperFileRefRendererHost(RendererPpapiHost* host,
                            PP_Instance instance,
                            PP_Resource resource,
                            const base::FilePath& external_path);

  PepperFileRefRendererHost(const PepperFileRefRendererHost&) = delete;
  PepperFileRefRendererHost& operator=(const PepperFileRefRendererHost&) =
      delete;

  ~PepperFileRefRendererHost() override;

  PP_FileSystemType GetFileSystemType() const;
  GURL GetFileSystemURL() const;
  base::FilePath GetExternalFilePath() const;

  // ppapi::host::ResourceHost override.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
  bool IsFileRefHost() override;

 private:
  PP_FileSystemType file_system_type_;
  std::string internal_path_;
  base::FilePath external_path_;
  base::WeakPtr<PepperFileSystemHost> fs_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_REF_RENDERER_HOST_H_
