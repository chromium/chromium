// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/browser_file_system_helper.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_permission_policy.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_options.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using storage::FileSystemOptions;

namespace content {

namespace {

// All FileSystemContexts currently need to share the same sequence per sharing
// global objects: https://codereview.chromium.org/2883403002#msg14.
base::LazySequencedTaskRunner g_fileapi_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::USER_VISIBLE,
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN));

FileSystemOptions CreateBrowserFileSystemOptions(bool is_incognito) {
  FileSystemOptions::ProfileMode profile_mode =
      is_incognito ? FileSystemOptions::PROFILE_MODE_INCOGNITO
                   : FileSystemOptions::PROFILE_MODE_NORMAL;
  std::vector<std::string> additional_allowed_schemes;
  GetContentClient()->browser()->GetAdditionalAllowedSchemesForFileSystem(
      &additional_allowed_schemes);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles)) {
    additional_allowed_schemes.push_back(url::kFileScheme);
  }
  return FileSystemOptions(profile_mode, is_incognito,
                           additional_allowed_schemes);
}

}  // namespace

scoped_refptr<storage::FileSystemContext> CreateFileSystemContext(
    BrowserContext* browser_context,
    const base::FilePath& profile_path,
    bool is_incognito,
    storage::QuotaManagerProxy* quota_manager_proxy) {
  // Setting up additional filesystem backends.
  std::vector<std::unique_ptr<storage::FileSystemBackend>> additional_backends;
  GetContentClient()->browser()->GetAdditionalFileSystemBackends(
      browser_context,
      profile_path,
      &additional_backends);

  // Set up the auto mount handlers for url requests.
  std::vector<storage::URLRequestAutoMountHandler>
      url_request_auto_mount_handlers;
  GetContentClient()->browser()->GetURLRequestAutoMountHandlers(
      &url_request_auto_mount_handlers);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      new storage::FileSystemContext(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})
              .get(),
          g_fileapi_task_runner.Get().get(),
          BrowserContext::GetMountPoints(browser_context),
          browser_context->GetSpecialStoragePolicy(), quota_manager_proxy,
          std::move(additional_backends), url_request_auto_mount_handlers,
          profile_path, CreateBrowserFileSystemOptions(is_incognito));

  for (const storage::FileSystemType& type :
       file_system_context->GetFileSystemTypes()) {
    ChildProcessSecurityPolicyImpl::GetInstance()
        ->RegisterFileSystemPermissionPolicy(
            type, storage::FileSystemContext::GetPermissionPolicy(type));
  }

  return file_system_context;
}

bool FileSystemURLIsValid(storage::FileSystemContext* context,
                          const storage::FileSystemURL& url) {
  if (!url.is_valid())
    return false;

  return context->GetFileSystemBackend(url.type()) != nullptr;
}

void SyncGetPlatformPath(storage::FileSystemContext* context,
                         int process_id,
                         const GURL& path,
                         base::FilePath* platform_path) {
  DCHECK(context->default_file_task_runner()->
         RunsTasksInCurrentSequence());
  DCHECK(platform_path);
  *platform_path = base::FilePath();
  storage::FileSystemURL url(context->CrackURL(path));
  if (!FileSystemURLIsValid(context, url))
    return;

  // Make sure if this file is ok to be read (in the current architecture
  // which means roughly same as the renderer is allowed to get the platform
  // path to the file).
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanReadFileSystemFile(process_id, url))
    return;

  context->operation_runner()->SyncGetPlatformPath(url, platform_path);

  // The path is to be attached to URLLoader so we grant read permission
  // for the file. (We need to check first because a parent directory may
  // already have the permissions and we don't need to grant it to the file.)
  if (!policy->CanReadFile(process_id, *platform_path))
    policy->GrantReadFile(process_id, *platform_path);
}

void PrepareDropDataForChildProcess(
    DropData* drop_data,
    ChildProcessSecurityPolicyImpl* security_policy,
    int child_id,
    const storage::FileSystemContext* file_system_context) {
#if defined(OS_CHROMEOS)
  // The externalfile:// scheme is used in Chrome OS to open external files in a
  // browser tab.
  // TODO(https://crbug.com/858972): This seems like it could be forged by the
  // renderer. This probably needs to check that this didn't originate from the
  // renderer... Also, this probably can just be GrantRequestURL (which doesn't
  // yet exist) instead of GrantCommitURL.
  if (drop_data->url.SchemeIs(content::kExternalFileScheme))
    security_policy->GrantCommitURL(child_id, drop_data->url);
#endif

  // The filenames vector represents a capability to access the given files.
  storage::IsolatedContext::FileInfoSet files;
  for (auto& filename : drop_data->filenames) {
    // Make sure we have the same display_name as the one we register.
    if (filename.display_name.empty()) {
      std::string name;
      files.AddPath(filename.path, &name);
      filename.display_name = base::FilePath::FromUTF8Unsafe(name);
    } else {
      files.AddPathWithName(filename.path,
                            filename.display_name.AsUTF8Unsafe());
    }
    // A dragged file may wind up as the value of an input element, or it
    // may be used as the target of a navigation instead.  We don't know
    // which will happen at this point, so generously grant both access
    // and request permissions to the specific file to cover both cases.
    // We do not give it the permission to request all file:// URLs.
    security_policy->GrantRequestSpecificFileURL(
        child_id, net::FilePathToFileURL(filename.path));

    // If the renderer already has permission to read these paths, we don't need
    // to re-grant them. This prevents problems with DnD for files in the CrOS
    // file manager--the file manager already had read/write access to those
    // directories, but dragging a file would cause the read/write access to be
    // overwritten with read-only access, making them impossible to delete or
    // rename until the renderer was killed.
    if (!security_policy->CanReadFile(child_id, filename.path))
      security_policy->GrantReadFile(child_id, filename.path);
  }

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  if (!files.fileset().empty()) {
    std::string filesystem_id =
        isolated_context->RegisterDraggedFileSystem(files);
    if (!filesystem_id.empty()) {
      // Grant the permission iff the ID is valid.
      security_policy->GrantReadFileSystem(child_id, filesystem_id);
    }
    drop_data->filesystem_id = base::UTF8ToUTF16(filesystem_id);
  }

  for (auto& file_system_file : drop_data->file_system_files) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(file_system_file.url);

    std::string register_name;
    std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
        file_system_url.type(), file_system_url.filesystem_id(),
        file_system_url.path(), &register_name);

    if (!filesystem_id.empty()) {
      // Grant the permission iff the ID is valid.
      security_policy->GrantReadFileSystem(child_id, filesystem_id);
    }

    // Note: We are using the origin URL provided by the sender here. It may be
    // different from the receiver's.
    file_system_file.url =
        GURL(storage::GetIsolatedFileSystemRootURIString(
                 file_system_url.origin(), filesystem_id, std::string())
                 .append(register_name));
    file_system_file.filesystem_id = filesystem_id;
  }
}

}  // namespace content
