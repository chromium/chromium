// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_

#include <stdint.h>

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/private/ppb_isolated_file_system_private.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "url/gurl.h"

namespace content {

class RendererPpapiHost;

class PepperFileSystemHost
    : public ppapi::host::ResourceHost,
      public base::SupportsWeakPtr<PepperFileSystemHost> {
 public:
  // Creates a new PepperFileSystemHost for a file system of a given |type|. The
  // host will not be connected to any specific file system, and will need to
  // subsequently be opened before use.
  PepperFileSystemHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource,
                       PP_FileSystemType type);
  // Creates a new PepperFileSystemHost with an existing file system at the
  // given |root_url| and of the given |type|. The file system must already be
  // opened. Once created, the PepperFileSystemHost is already opened for use.
  PepperFileSystemHost(RendererPpapiHost* host,
                       PP_Instance instance,
                       PP_Resource resource,
                       const GURL& root_url,
                       PP_FileSystemType type);
  ~PepperFileSystemHost() override;

  // ppapi::host::ResourceHost override.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
  bool IsFileSystemHost() override;

  // Supports FileRefs direct access on the host side.
  PP_FileSystemType GetType() const { return type_; }
  bool IsOpened() const { return opened_; }
  GURL GetRootUrl() const { return root_url_; }

 private:
  // Callback for OpenFileSystem.
  void DidOpenFileSystem(const std::string& name_unused,
                         const GURL& root,
                         base::File::Error error);
  void DidFailOpenFileSystem(base::File::Error error);

  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        int64_t expected_size);
  int32_t OnHostMsgInitIsolatedFileSystem(
      ppapi::host::HostMessageContext* context,
      const std::string& fsid,
      PP_IsolatedFileSystemType_Private type);

  blink::mojom::FileSystemManager& GetFileSystemManager();

  RendererPpapiHost* renderer_ppapi_host_;
  ppapi::host::ReplyMessageContext reply_context_;

  PP_FileSystemType type_;
  bool opened_;  // whether open is successful.
  GURL root_url_;
  bool called_open_;  // whether open has been called.
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager_;

  DISALLOW_COPY_AND_ASSIGN(PepperFileSystemHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_FILE_SYSTEM_HOST_H_
