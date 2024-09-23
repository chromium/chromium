// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_ref_renderer_host.h"

#include <string>

#include "base/strings/escape.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/shared_impl/file_ref_util.h"

namespace content {

PepperFileRefRendererHost::PepperFileRefRendererHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    PP_Resource file_system,
    const std::string& internal_path)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      internal_path_(internal_path) {
  if (!ppapi::IsValidInternalPath(internal_path))
    return;
  ResourceHost* fs_host = host->GetPpapiHost()->GetResourceHost(file_system);
  if (fs_host && fs_host->IsFileSystemHost()) {
    fs_host_ = static_cast<PepperFileSystemHost*>(fs_host)->AsWeakPtr();
    file_system_type_ = fs_host_->GetType();
  }
}

PepperFileRefRendererHost::PepperFileRefRendererHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const base::FilePath& external_path)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      file_system_type_(PP_FILESYSTEMTYPE_EXTERNAL),
      external_path_(external_path) {
  if (!ppapi::IsValidExternalPath(external_path))
    file_system_type_ = PP_FILESYSTEMTYPE_INVALID;
}

PepperFileRefRendererHost::~PepperFileRefRendererHost() {}

PP_FileSystemType PepperFileRefRendererHost::GetFileSystemType() const {
  return file_system_type_;
}

GURL PepperFileRefRendererHost::GetFileSystemURL() const {
  if (fs_host_.get() && fs_host_->IsOpened() &&
      fs_host_->GetRootUrl().is_valid()) {
    CHECK(!internal_path_.empty() && internal_path_[0] == '/');
    // We strip off the leading slash when passing the URL to Resolve().
    // Internal paths are required to be absolute, so we can require this.
    return fs_host_->GetRootUrl().Resolve(
        base::EscapePath(internal_path_.substr(1)));
  }
  return GURL();
}

base::FilePath PepperFileRefRendererHost::GetExternalFilePath() const {
  if (file_system_type_ != PP_FILESYSTEMTYPE_EXTERNAL)
    return base::FilePath();
  return external_path_;
}

int32_t PepperFileRefRendererHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  // We don't handle any messages from the plugin in this host.
  NOTREACHED_IN_MIGRATION();
  return PP_ERROR_FAILED;
}

bool PepperFileRefRendererHost::IsFileRefHost() { return true; }

}  // namespace content
