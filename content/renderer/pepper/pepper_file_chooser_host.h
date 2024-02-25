// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

class RendererPpapiHost;

class CONTENT_EXPORT PepperFileChooserHost final
    : public ppapi::host::ResourceHost {
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

  PepperFileChooserHost(const PepperFileChooserHost&) = delete;
  PepperFileChooserHost& operator=(const PepperFileChooserHost&) = delete;

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
  raw_ptr<RendererPpapiHost> renderer_ppapi_host_;

  ppapi::host::ReplyMessageContext reply_context_;
  raw_ptr<CompletionHandler, DanglingUntriaged> handler_;

  base::WeakPtrFactory<PepperFileChooserHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_CHOOSER_HOST_H_
