// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/host/ppapi_host.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

class PepperFileSystemBrowserHost;

// Implementations of FileRef operations for internal filesystems.
class PepperInternalFileRefBackend : public PepperFileRefBackend {
 public:
  PepperInternalFileRefBackend(
      ppapi::host::PpapiHost* host,
      int render_process_id,
      base::WeakPtr<PepperFileSystemBrowserHost> fs_host,
      const std::string& path);

  PepperInternalFileRefBackend(const PepperInternalFileRefBackend&) = delete;
  PepperInternalFileRefBackend& operator=(const PepperInternalFileRefBackend&) =
      delete;

  ~PepperInternalFileRefBackend() override;

  // PepperFileRefBackend overrides.
  int32_t MakeDirectory(ppapi::host::ReplyMessageContext context,
                        int32_t make_directory_flags) override;
  int32_t Touch(ppapi::host::ReplyMessageContext context,
                PP_Time last_accessed_time,
                PP_Time last_modified_time) override;
  int32_t Delete(ppapi::host::ReplyMessageContext context) override;
  int32_t Rename(ppapi::host::ReplyMessageContext context,
                 PepperFileRefHost* new_file_ref) override;
  int32_t Query(ppapi::host::ReplyMessageContext context) override;
  int32_t ReadDirectoryEntries(
      ppapi::host::ReplyMessageContext context) override;
  int32_t GetAbsolutePath(ppapi::host::ReplyMessageContext context) override;
  storage::FileSystemURL GetFileSystemURL() const override;
  base::FilePath GetExternalFilePath() const override;

  int32_t CanRead() const override;
  int32_t CanWrite() const override;
  int32_t CanCreate() const override;
  int32_t CanReadWrite() const override;

 private:
  // Generic reply callback.
  void DidFinish(ppapi::host::ReplyMessageContext reply_context,
                 const IPC::Message& msg,
                 base::File::Error error);

  // Helper methods called on IO thread.
  static void DidFinishOnIOThread(
      base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
      ppapi::host::ReplyMessageContext reply_context,
      const IPC::Message& msg,
      base::File::Error error);
  static void ReadDirectoryCompleteOnIOThread(
      base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
      ppapi::host::ReplyMessageContext reply_context,
      storage::FileSystemOperation::FileEntryList* accumulated_file_list,
      base::File::Error error,
      storage::FileSystemOperation::FileEntryList file_list,
      bool has_more);
  static void GetMetadataCompleteOnIOThread(
      base::WeakPtr<PepperInternalFileRefBackend> weak_ptr,
      ppapi::host::ReplyMessageContext reply_context,
      base::File::Error result,
      const base::File::Info& file_info);

  // Operation specific callbacks.
  void GetMetadataComplete(ppapi::host::ReplyMessageContext reply_context,
                           base::File::Error error,
                           const base::File::Info& file_info);
  void ReadDirectoryComplete(
      ppapi::host::ReplyMessageContext context,
      storage::FileSystemOperation::FileEntryList* accumulated_file_list,
      base::File::Error error,
      storage::FileSystemOperation::FileEntryList file_list,
      bool has_more);

  scoped_refptr<storage::FileSystemContext> GetFileSystemContext() const;

  raw_ptr<ppapi::host::PpapiHost> host_;
  int render_process_id_;
  base::WeakPtr<PepperFileSystemBrowserHost> fs_host_;
  PP_FileSystemType fs_type_;
  std::string path_;

  mutable storage::FileSystemURL fs_url_;

  base::WeakPtrFactory<PepperInternalFileRefBackend> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_INTERNAL_FILE_REF_BACKEND_H_
