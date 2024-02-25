// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {
class PepperFileRefHost;
class PepperFileSystemBrowserHost;

// Internal and external filesystems have very different codepaths for
// performing FileRef operations. The logic is split into separate classes
// to make it easier to read.
class PepperFileRefBackend {
 public:
  virtual ~PepperFileRefBackend();

  virtual int32_t MakeDirectory(ppapi::host::ReplyMessageContext context,
                                int32_t make_directory_flags) = 0;
  virtual int32_t Touch(ppapi::host::ReplyMessageContext context,
                        PP_Time last_accessed_time,
                        PP_Time last_modified_time) = 0;
  virtual int32_t Delete(ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t Rename(ppapi::host::ReplyMessageContext context,
                         PepperFileRefHost* new_file_ref) = 0;
  virtual int32_t Query(ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t ReadDirectoryEntries(
      ppapi::host::ReplyMessageContext context) = 0;
  virtual int32_t GetAbsolutePath(ppapi::host::ReplyMessageContext context) = 0;
  virtual storage::FileSystemURL GetFileSystemURL() const = 0;
  virtual base::FilePath GetExternalFilePath() const = 0;

  // Returns an error from the pp_errors.h enum.
  virtual int32_t CanRead() const = 0;
  virtual int32_t CanWrite() const = 0;
  virtual int32_t CanCreate() const = 0;
  virtual int32_t CanReadWrite() const = 0;
};

class PepperFileRefHost : public ppapi::host::ResourceHost {
 public:
  PepperFileRefHost(BrowserPpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource,
                    PP_Resource file_system,
                    const std::string& internal_path);

  PepperFileRefHost(BrowserPpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource,
                    const base::FilePath& external_path);

  PepperFileRefHost(const PepperFileRefHost&) = delete;
  PepperFileRefHost& operator=(const PepperFileRefHost&) = delete;

  ~PepperFileRefHost() override;

  // ResourceHost overrides.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;
  bool IsFileRefHost() override;

  // Required to support Rename().
  PP_FileSystemType GetFileSystemType() const;
  storage::FileSystemURL GetFileSystemURL() const;

  // Required to support FileIO.
  base::FilePath GetExternalFilePath() const;
  base::WeakPtr<PepperFileSystemBrowserHost> GetFileSystemHost() const;

  int32_t CanRead() const;
  int32_t CanWrite() const;
  int32_t CanCreate() const;
  int32_t CanReadWrite() const;

 private:
  int32_t OnMakeDirectory(ppapi::host::HostMessageContext* context,
                          int32_t make_directory_flags);
  int32_t OnTouch(ppapi::host::HostMessageContext* context,
                  PP_Time last_access_time,
                  PP_Time last_modified_time);
  int32_t OnDelete(ppapi::host::HostMessageContext* context);
  int32_t OnRename(ppapi::host::HostMessageContext* context,
                   PP_Resource new_file_ref);
  int32_t OnQuery(ppapi::host::HostMessageContext* context);
  int32_t OnReadDirectoryEntries(ppapi::host::HostMessageContext* context);
  int32_t OnGetAbsolutePath(ppapi::host::HostMessageContext* context);

  raw_ptr<BrowserPpapiHost> host_;
  std::unique_ptr<PepperFileRefBackend> backend_;
  base::WeakPtr<PepperFileSystemBrowserHost> file_system_host_;
  PP_FileSystemType fs_type_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_FILE_REF_HOST_H_
