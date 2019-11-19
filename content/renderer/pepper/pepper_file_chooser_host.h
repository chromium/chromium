// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperFileChooserHost
    : public ppapi::host::ResourceHost,
      public base::SupportsWeakPtr<PepperFileChooserHost> {
 public:
  // Structure to store the information about chosen files.
  struct ChosenFileInfo {
    ChosenFileInfo(const base::FilePath& file_path,
                   const std::string& display_name);
    base::FilePath file_path;
    std::string display_name;  // May be empty.
  };

  PepperFileChooserHost(RendererPpapiHost* host,
                        PP_Instance instance,
                        PP_Resource resource);
  ~PepperFileChooserHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  void StoreChosenFiles(const std::vector<ChosenFileInfo>& files);

 private:
  class CompletionHandler;

  int32_t OnShow(ppapi::host::HostMessageContext* context,
                 bool save_as,
                 bool open_multiple,
                 const std::string& suggested_file_name,
                 const std::vector<std::string>& accept_mime_types);

  void DidCreateResourceHosts(const std::vector<base::FilePath>& file_paths,
                              const std::vector<std::string>& display_names,
                              const std::vector<int>& browser_ids);

  // Non-owning pointer.
  RendererPpapiHost* renderer_ppapi_host_;

  ppapi::host::ReplyMessageContext reply_context_;
  CompletionHandler* handler_;

  base::WeakPtrFactory<PepperFileChooserHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperFileChooserHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
