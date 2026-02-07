// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/browser_file_system_helper.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_permission_policy.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_options.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using storage::FileSystemOptions;

namespace content {

namespace {

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

bool CheckCanReadFileSystemFileOnUIThread(
    std::unique_ptr<ChildProcessSecurityPolicyImpl::Handle>
        security_policy_handle,
    const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return security_policy_handle->CanReadFileSystemFile(url);
}

void GrantReadAccessOnUIThread(
    std::unique_ptr<ChildProcessSecurityPolicyImpl::Handle>
        security_policy_handle,
    const base::FilePath& platform_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // It is only possible to grant new permissions if the RenderProcessHost still
  // exists by the time this task runs. If it does exist and doesn't yet have
  // read access to the platform path that corresponds to the FileSystemURL that
  // was verified in CheckCanReadFileSystemFileOnUIThread, grant access now.
  ChildProcessId process_id = security_policy_handle->child_id();
  if (RenderProcessHost::FromID(process_id) &&
      !security_policy_handle->CanReadFile(platform_path)) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(process_id,
                                                                 platform_path);
  }
}

// Helper function that used by GetPlatformPath() to get the platform
// path, grant read access, and send return the path via a callback.
void GetPlatformPathOnFileThread(
    scoped_refptr<storage::FileSystemContext> context,
    std::unique_ptr<ChildProcessSecurityPolicyImpl::Handle>
        security_policy_handle,
    const storage::FileSystemURL& url,
    DoGetPlatformPathCB callback,
    bool can_read_filesystem_file) {
  DCHECK(context->default_file_task_runner()->RunsTasksInCurrentSequence());

  if (!can_read_filesystem_file) {
    std::move(callback).Run(base::FilePath());
    return;
  }

  base::FilePath platform_path;
  context->operation_runner()->SyncGetPlatformPath(url, &platform_path);

  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&GrantReadAccessOnUIThread,
                     std::move(security_policy_handle), platform_path),
      base::BindOnce(std::move(callback), platform_path));
}

}  // namespace

scoped_refptr<storage::FileSystemContext> CreateFileSystemContext(
    BrowserContext* browser_context,
    const base::FilePath& profile_path,
    bool is_incognito,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy) {
  // Setting up additional filesystem backends.
  std::vector<std::unique_ptr<storage::FileSystemBackend>> additional_backends;
  GetContentClient()->browser()->GetAdditionalFileSystemBackends(
      browser_context, profile_path, &additional_backends);

  // Set up the auto mount handlers for url requests.
  std::vector<storage::URLRequestAutoMountHandler>
      url_request_auto_mount_handlers;
  GetContentClient()->browser()->GetURLRequestAutoMountHandlers(
      &url_request_auto_mount_handlers);

  auto options = CreateBrowserFileSystemOptions(
      browser_context->CanUseDiskWhenOffTheRecord() ? false : is_incognito);
  auto file_system_context = storage::FileSystemContext::Create(
      GetIOThreadTaskRunner({}),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      browser_context->GetMountPoints(),
      browser_context->GetSpecialStoragePolicy(),
      std::move(quota_manager_proxy), std::move(additional_backends),
      url_request_auto_mount_handlers, profile_path, options);

  for (const storage::FileSystemType& type :
       file_system_context->GetFileSystemTypes()) {
    // This can safely be called without a ChildProcessSecurityPolicy::Handle
    // because it does not involve per-process SecurityState.
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

void DoGetPlatformPath(scoped_refptr<storage::FileSystemContext> context,
                       std::unique_ptr<ChildProcessSecurityPolicyImpl::Handle>
                           security_policy_handle,
                       const GURL& path,
                       const blink::StorageKey& storage_key,
                       DoGetPlatformPathCB callback) {
  DCHECK(context->default_file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(callback);

  storage::FileSystemURL url(context->CrackURL(path, storage_key));
  if (!FileSystemURLIsValid(context.get(), url)) {
    // Note: Posting a task here so this function always returns
    // before the callback is called no matter which path is taken.
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::FilePath()));
    return;
  }

  // Make sure if this file is ok to be read (in the current architecture
  // which means roughly same as the renderer is allowed to get the platform
  // path to the file).
  //
  // The access check for `url` runs on the UI thread using a duplicated
  // ChildProcessSecurityPolicy::Handle, ensuring the SecurityState exists when
  // the task runs. If the access check passes, the file thread will determine
  // the platform path, then another Handle (which can be the one that was
  // passed to this function) is needed on the UI thread to check and grant
  // access to the platform path if needed. Using separate Handles for these
  // tasks allows each task to own and delete their Handle independently.
  std::unique_ptr<ChildProcessSecurityPolicyImpl::Handle> ui_handle =
      std::make_unique<ChildProcessSecurityPolicyImpl::Handle>(
          security_policy_handle->Duplicate());
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckCanReadFileSystemFileOnUIThread,
                     std::move(ui_handle), url),
      base::BindOnce(&GetPlatformPathOnFileThread, std::move(context),
                     std::move(security_policy_handle), url,
                     std::move(callback)));
}

void PrepareDropDataForChildProcess(
    DropData* drop_data,
    ChildProcessSecurityPolicyImpl* security_policy,
    ChildProcessId child_id,
    const storage::FileSystemContext* file_system_context) {
  // The permissions granted to `child_id` below only work if the corresponding
  // RenderProcessHost still exists on the UI thread.
  // TODO(crbug.com/482261047): Pass the RenderProcessHost instead of `child_id`
  // as proof that it still exists. This requires using a MockRenderProcessHost
  // in unit tests.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if BUILDFLAG(IS_CHROMEOS)
  // The externalfile:// scheme is used in Chrome OS to open external files in a
  // browser tab.
  // TODO(https://crbug.com/858972): This seems like it could be forged by the
  // renderer. This probably needs to check that this didn't originate from the
  // renderer... Also, this probably can just be GrantRequestURL (which doesn't
  // yet exist) instead of GrantCommitURL.
  if (!drop_data->url_infos.empty()) {
    const GURL& url = drop_data->url_infos.front().url;
    if (url.SchemeIs(content::kExternalFileScheme)) {
      // TODO(crbug.com/379869738) Remove GetUnsafeValue.
      security_policy->GrantCommitURL(child_id.GetUnsafeValue(), url);
    }
  }
#endif

  std::string filesystem_id = PrepareDataTransferFilenamesForChildProcess(
      drop_data->filenames, security_policy, child_id, file_system_context);
  drop_data->filesystem_id = base::UTF8ToUTF16(filesystem_id);

  storage::IsolatedContext* isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  for (auto& file_system_file : drop_data->file_system_files) {
    storage::FileSystemURL file_system_url =
        file_system_context->CrackURLInFirstPartyContext(file_system_file.url);

    // Sandboxed filesystem files should never be handled via this path, so
    // assert that none are sent from the renderer (wrapping these won't work
    // anyway).
    DCHECK(file_system_url.type() != storage::kFileSystemTypePersistent);
    DCHECK(file_system_url.type() != storage::kFileSystemTypeTemporary);

    std::string register_name;
    storage::IsolatedContext::ScopedFSHandle filesystem =
        isolated_context->RegisterFileSystemForPath(
            file_system_url.type(), file_system_url.filesystem_id(),
            file_system_url.path(), &register_name);

    if (filesystem.is_valid()) {
      // Grant the permission iff the ID is valid. This will also keep the FS
      // alive after |filesystem| goes out of scope.
      security_policy->GrantReadFileSystem(child_id, filesystem.id());
    }

    // Note: We are using the origin URL provided by the sender here. It may be
    // different from the receiver's.
    file_system_file.url = GURL(
        storage::GetIsolatedFileSystemRootURIString(
            file_system_url.origin().GetURL(), filesystem.id(), std::string())
            .append(register_name));
    file_system_file.filesystem_id = filesystem.id();
  }
}

std::string PrepareDataTransferFilenamesForChildProcess(
    std::vector<ui::FileInfo>& filenames,
    ChildProcessSecurityPolicyImpl* security_policy,
    ChildProcessId child_id,
    const storage::FileSystemContext* file_system_context) {
  // The permissions granted to `child_id` below only work if the corresponding
  // RenderProcessHost still exists on the UI thread.
  // TODO(crbug.com/482261047): Pass the RenderProcessHost instead of `child_id`
  // as proof that it still exists. This requires using a MockRenderProcessHost
  // in unit tests.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The filenames vector represents a capability to access the given files.
  storage::IsolatedContext::FileInfoSet files;
  for (auto& filename : filenames) {
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
    security_policy->GrantRequestOfSpecificFile(child_id, filename.path);

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

  std::string filesystem_id;
  if (!files.fileset().empty()) {
    filesystem_id = isolated_context->RegisterDraggedFileSystem(files);
    if (!filesystem_id.empty()) {
      // Grant the permission iff the ID is valid.
      security_policy->GrantReadFileSystem(child_id, filesystem_id);
    }
  }
  return filesystem_id;
}

}  // namespace content
