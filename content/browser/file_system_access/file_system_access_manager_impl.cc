// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_manager_impl.h"

#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access.pb.h"
#include "content/browser/file_system_access/file_system_access_access_handle_host_impl.h"
#include "content/browser/file_system_access/file_system_access_data_transfer_token_impl.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_file_writer_impl.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/browser/file_system_access/file_system_chooser.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "crypto/secure_hash.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/filename_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_modification_host.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#endif

namespace content {

using blink::mojom::FileSystemAccessStatus;
using PermissionStatus = FileSystemAccessPermissionGrant::PermissionStatus;
using SensitiveEntryResult =
    FileSystemAccessPermissionContext::SensitiveEntryResult;
using storage::FileSystemContext;
using HandleType = FileSystemAccessPermissionContext::HandleType;

namespace {

#if BUILDFLAG(IS_ANDROID)
// Adaptor between FileSystemChooser::ResultCallback and FileSelectListener
// used when delegating file choosing to WebContentsDelegate.
class WebContentsDelegateListener : public FileSelectListener {
 public:
  explicit WebContentsDelegateListener(
      FileSystemChooser::ResultCallback callback)
      : callback_(std::move(callback)) {}
  WebContentsDelegateListener(const WebContentsDelegateListener&) = delete;
  WebContentsDelegateListener& operator=(const WebContentsDelegateListener&) =
      delete;

 private:
  ~WebContentsDelegateListener() override = default;

  void FileSelected(std::vector<blink::mojom::FileChooserFileInfoPtr> files,
                    const base::FilePath& base_dir,
                    blink::mojom::FileChooserParams::Mode mode) override {
    std::vector<content::PathInfo> result;
    for (const auto& file : files) {
      CHECK(file->is_native_file());
      base::FilePath path = file->get_native_file()->file_path;
      if (path.empty()) {
        continue;
      }
      std::string display_name =
          base::UTF16ToUTF8(file->get_native_file()->display_name);
      if (display_name.empty()) {
        display_name = path.BaseName().AsUTF8Unsafe();
      }
      result.emplace_back(content::PathType::kLocal, std::move(path),
                          std::move(display_name));
    }
    std::move(callback_).Run(file_system_access_error::Ok(), std::move(result));
  }

  void FileSelectionCanceled() override {
    std::move(callback_).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationAborted),
        {});
  }

  FileSystemChooser::ResultCallback callback_;
};
#endif

void ShowFilePickerOnUIThread(const url::Origin& requesting_origin,
                              GlobalRenderFrameHostId frame_id,
                              const FileSystemChooser::Options& options,
                              FileSystemChooser::ResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  RenderFrameHost* outermost_rfh = rfh ? rfh->GetOutermostMainFrame() : nullptr;

  if (!web_contents || !outermost_rfh) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kOperationAborted),
                            {});
    return;
  }

  DCHECK(outermost_rfh->IsInPrimaryMainFrame());

  if (rfh->IsNestedWithinFencedFrame()) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied,
            "Fenced frames are not allowed to show a file picker."),
        {});
    return;
  }

  url::Origin embedding_origin = outermost_rfh->GetLastCommittedOrigin();
  if (embedding_origin != requesting_origin) {
    // Third party iframes are not allowed to show a file picker.
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied,
            "Third party iframes are not allowed to show a file picker."),
        {});
    return;
  }

  // Drop fullscreen mode so that the user sees the URL bar.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen(
          /*display_id=*/display::kInvalidDisplayId);

#if BUILDFLAG(IS_ANDROID)
  // Allow android WebView to handle chooser.
  WebContentsDelegate* delegate = web_contents->GetDelegate();
  if (delegate && delegate->UseFileChooserForFileSystemAccess()) {
    blink::mojom::FileChooserParams params;
    switch (options.type()) {
      case ui::SelectFileDialog::SELECT_OPEN_FILE:
        params.mode = blink::mojom::FileChooserParams::Mode::kOpen;
        break;
      case ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE:
        params.mode = blink::mojom::FileChooserParams::Mode::kOpenMultiple;
        break;
      case ui::SelectFileDialog::SELECT_SAVEAS_FILE:
        params.mode = blink::mojom::FileChooserParams::Mode::kSave;
        break;
      case ui::SelectFileDialog::SELECT_FOLDER:
        params.mode = blink::mojom::FileChooserParams::Mode::kOpenDirectory;
        break;
      default:
        NOTREACHED();
    }
    params.title = options.title();
    params.default_file_name = options.default_path();
    for (const auto& ext_list : options.file_type_info().extensions) {
      for (const auto& ext : ext_list) {
        params.accept_types.push_back(u"." + base::UTF8ToUTF16(ext));
      }
    }
    auto listener =
        base::MakeRefCounted<WebContentsDelegateListener>(std::move(callback));
    delegate->RunFileChooser(rfh, std::move(listener), params);
    return;
  }
#endif

  FileSystemChooser::CreateAndShow(web_contents, options, std::move(callback),
                                   std::move(fullscreen_block));
}

// Called after creating a file that was picked by a save file picker. If
// creation succeeded (or the file already existed) this will attempt to
// truncate the file to zero bytes, and call `callback` on `reply_runner`
// with the result of this operation.
void DidCreateFileToTruncate(
    storage::FileSystemURL url,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> reply_runner,
    storage::FileSystemOperationRunner* operation_runner,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    // Failed to create the file, don't even try to truncate it.
    reply_runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  operation_runner->Truncate(
      url, /*length=*/0,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback,
             scoped_refptr<base::SequencedTaskRunner> reply_runner,
             base::File::Error result) {
            reply_runner->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          result == base::File::FILE_OK));
          },
          std::move(callback), std::move(reply_runner)));
}

// Creates and truncates the file at `url`. Calls `callback` on `reply_runner`
// with true if this succeeded, or false if either creation or truncation
// failed.
void CreateAndTruncateFile(
    storage::FileSystemURL url,
    base::OnceCallback<void(bool)> callback,
    scoped_refptr<base::SequencedTaskRunner> reply_runner,
    storage::FileSystemOperationRunner* operation_runner) {
  // Binding operation_runner as a raw pointer is safe, since the callback is
  // invoked by the operation runner, and thus won't be invoked if the operation
  // runner has been destroyed.
  operation_runner->CreateFile(
      url, /*exclusive=*/false,
      base::BindOnce(&DidCreateFileToTruncate, url, std::move(callback),
                     std::move(reply_runner), operation_runner));
}

bool IsValidTransferToken(FileSystemAccessTransferTokenImpl* token,
                          const url::Origin& expected_origin,
                          HandleType expected_handle_type) {
  if (!token) {
    return false;
  }

  if (token->type() != expected_handle_type) {
    return false;
  }

  if (token->origin() != expected_origin) {
    return false;
  }

  return true;
}

HandleType HandleTypeFromFileInfo(base::File::Error result,
                                  const base::File::Info& file_info) {
  // If we couldn't determine if the url is a directory, it is treated
  // as a file. If the web-exposed API is ever changed to allow
  // reporting errors when getting a dropped file as a
  // FileSystemHandle, this would be one place such errors could be
  // triggered.
  if (result == base::File::FILE_OK && file_info.is_directory) {
    return HandleType::kDirectory;
  }
  return HandleType::kFile;
}

void HandleTransferTokenAsDefaultDirectory(
    FileSystemAccessTransferTokenImpl* token,
    PathInfo& info) {
  auto token_url_type = token->url().type();
  auto token_url_mount_type = token->url().mount_type();

  // Ignore sandboxed file system URLs.
  if (token_url_type == storage::kFileSystemTypeTemporary ||
      token_url_type == storage::kFileSystemTypePersistent) {
    return;
  }

  if (token_url_mount_type == storage::kFileSystemTypeExternal) {
    info.type = PathType::kExternal;
    info.path = token->type() == HandleType::kFile
                    ? token->url().virtual_path().DirName()
                    : token->url().virtual_path();
    return;
  }

  DCHECK(token_url_type == storage::kFileSystemTypeLocal);
  info.path = token->type() == HandleType::kFile ? token->url().path().DirName()
                                                 : token->url().path();
}

bool IsValidIdChar(const char c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_' || c == '-';
}

bool IsValidId(const std::string& id) {
  return id.size() <= 32 && base::ranges::all_of(id, &IsValidIdChar);
}

ui::SelectFileDialog::Type GetSelectFileDialogType(
    const blink::mojom::TypeSpecificFilePickerOptionsUnionPtr& options) {
  switch (options->which()) {
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kOpenFilePickerOptions:
      return options->get_open_file_picker_options()->can_select_multiple_files
                 ? ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE
                 : ui::SelectFileDialog::SELECT_OPEN_FILE;
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kSaveFilePickerOptions:
      return ui::SelectFileDialog::SELECT_SAVEAS_FILE;
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kDirectoryPickerOptions:
      return ui::SelectFileDialog::SELECT_FOLDER;
  }
  NOTREACHED_IN_MIGRATION();
  return ui::SelectFileDialog::SELECT_NONE;
}

blink::mojom::AcceptsTypesInfoPtr GetAndMoveAcceptsTypesInfo(
    const blink::mojom::TypeSpecificFilePickerOptionsUnionPtr& options) {
  switch (options->which()) {
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kOpenFilePickerOptions:
      return std::move(
          options->get_open_file_picker_options()->accepts_types_info);
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kSaveFilePickerOptions:
      return std::move(
          options->get_save_file_picker_options()->accepts_types_info);
    case blink::mojom::TypeSpecificFilePickerOptionsUnion::Tag::
        kDirectoryPickerOptions:
      return blink::mojom::AcceptsTypesInfo::New(
          /*accepts=*/std::vector<
              blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr>(),
          /*include_accepts_all=*/false);
  }
}

void DidCheckIfDefaultDirectoryExists(
    const storage::FileSystemURL& default_directory_url,
    base::OnceCallback<void(bool)> callback,
    base::File::Error result) {
  if (result == base::File::Error::FILE_OK) {
    std::move(callback).Run(/*default_directory_exists=*/true);
    return;
  }

  if (default_directory_url.type() == storage::kFileSystemTypeLocal) {
    // Symlinks don't "exist" according to the FileSystemOperationRunner, but
    // they do to the user. If directory is a symlink, allow it to be set as the
    // default starting directory.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::IsLink, default_directory_url.path()),
        std::move(callback));
  } else {
    std::move(callback).Run(/*default_directory_exists=*/false);
  }
}

}  // namespace

FileSystemAccessManagerImpl::SharedHandleState::SharedHandleState(
    scoped_refptr<FileSystemAccessPermissionGrant> read_grant,
    scoped_refptr<FileSystemAccessPermissionGrant> write_grant)
    : read_grant(std::move(read_grant)), write_grant(std::move(write_grant)) {
  DCHECK(this->read_grant);
  DCHECK(this->write_grant);
}

FileSystemAccessManagerImpl::SharedHandleState::SharedHandleState(
    const SharedHandleState& other) = default;
FileSystemAccessManagerImpl::SharedHandleState::~SharedHandleState() = default;

FileSystemAccessManagerImpl::FileSystemAccessManagerImpl(
    scoped_refptr<storage::FileSystemContext> context,
    scoped_refptr<ChromeBlobStorageContext> blob_context,
    FileSystemAccessPermissionContext* permission_context,
    bool off_the_record)
    : context_(std::move(context)),
      blob_context_(std::move(blob_context)),
      permission_context_(permission_context),
      lock_manager_(
          base::MakeRefCounted<FileSystemAccessLockManager>(PassKey())),
      watcher_manager_(this, PassKey()),
      off_the_record_(off_the_record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_context_);
}

FileSystemAccessManagerImpl::~FileSystemAccessManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileSystemAccessManagerImpl::BindReceiver(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network::IsOriginPotentiallyTrustworthy(
          binding_context.storage_key.origin())) {
    mojo::ReportBadMessage("File System Access access from Unsecure Origin");
    return;
  }

  receivers_.Add(this, std::move(receiver), binding_context);
}

void FileSystemAccessManagerImpl::BindInternalsReceiver(
    mojo::PendingReceiver<storage::mojom::FileSystemAccessContext> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  internals_receivers_.Add(this, std::move(receiver));
}

void FileSystemAccessManagerImpl::GetSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetSandboxedFileSystem(receivers_.current_context(),
                         /*bucket=*/std::nullopt,
                         /*directory_path_components=*/{}, std::move(callback));
}

void FileSystemAccessManagerImpl::GetSandboxedFileSystemForDevtools(
    const std::vector<std::string>& directory_path_components,
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetSandboxedFileSystem(receivers_.current_context(),
                         /*bucket=*/std::nullopt, directory_path_components,
                         std::move(callback));
}

void FileSystemAccessManagerImpl::GetSandboxedFileSystem(
    const BindingContext& binding_context,
    const std::optional<storage::BucketLocator>& bucket,
    const std::vector<std::string>& directory_path_components,
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto response_callback = base::BindOnce(
      [](base::WeakPtr<FileSystemAccessManagerImpl> manager,
         const BindingContext& callback_binding_context,
         GetSandboxedFileSystemCallback callback,
         scoped_refptr<base::SequencedTaskRunner> task_runner,
         const std::vector<std::string>& directory_path_components,
         const storage::FileSystemURL& root, const std::string& fs_name,
         base::File::Error result) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                &FileSystemAccessManagerImpl::DidOpenSandboxedFileSystem,
                std::move(manager), callback_binding_context,
                std::move(callback), root, fs_name, result,
                directory_path_components));
      },
      weak_factory_.GetWeakPtr(), binding_context, std::move(callback),
      base::SequencedTaskRunner::GetCurrentDefault(),
      directory_path_components);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemContext::OpenFileSystem, context(),
                                binding_context.storage_key, bucket,
                                storage::kFileSystemTypeTemporary,
                                storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                                std::move(response_callback)));
}

void FileSystemAccessManagerImpl::ChooseEntries(
    blink::mojom::FilePickerOptionsPtr options,
    ChooseEntriesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const BindingContext& context = receivers_.current_context();

  // ChooseEntries API is only available to windows, as we need a frame to
  // anchor the picker to.
  if (context.is_worker) {
    receivers_.ReportBadMessage("ChooseEntries called from a worker");
    return;
  }

  // Non-compromised renderers shouldn't be able to send an invalid id.
  if (!IsValidId(options->starting_directory_id)) {
    receivers_.ReportBadMessage("Invalid starting directory ID in browser");
    return;
  }

  if (permission_context_) {
    // When site setting is block, it's better not to show file chooser.
    // Write permission will be requested for either a save file picker or
    // a directory picker with `request_writable` true.
    if (!permission_context_->CanObtainReadPermission(
            context.storage_key.origin()) ||
        ((options->type_specific_options->is_save_file_picker_options() ||
          (options->type_specific_options->is_directory_picker_options() &&
           options->type_specific_options->get_directory_picker_options()
               ->request_writable)) &&
         !permission_context_->CanObtainWritePermission(
             context.storage_key.origin()))) {
      std::move(callback).Run(
          file_system_access_error::FromStatus(
              FileSystemAccessStatus::kPermissionDenied),
          std::vector<blink::mojom::FileSystemAccessEntryPtr>());
      return;
    }
  }

  RenderFrameHost* rfh = RenderFrameHost::FromID(context.frame_id);
  if (!rfh) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kOperationAborted),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }

  // Renderer process should already check for user activation before sending
  // IPC, but just to be sure double check here as well. This is not treated
  // as a BadMessage because it is possible for the transient user activation
  // to expire between the renderer side check and this check.
  ContentBrowserClient* content_browser_client = GetContentClient()->browser();
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!rfh->HasTransientUserActivation() &&
      content_browser_client
          ->IsTransientActivationRequiredForShowFileOrDirectoryPicker(
              web_contents)) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied,
            "User activation is required to show a file picker."),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }

  // Don't show the file picker if there is an already active file picker for
  // this render frame host.
  GlobalRenderFrameHostId global_rfh_id = rfh->GetGlobalId();
  if (rfhs_with_active_file_pickers_.contains(global_rfh_id)) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kPermissionDenied,
            "File picker already active."),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }
  rfhs_with_active_file_pickers_.insert(global_rfh_id);
  ChooseEntriesCallback wrapped_callback = std::move(callback).Then(
      base::BindOnce(&FileSystemAccessManagerImpl::FilePickerDeactivated,
                     weak_factory_.GetWeakPtr(), global_rfh_id));

  if (!options->start_in_options.is_null() &&
      options->start_in_options->is_directory_token() &&
      options->start_in_options->get_directory_token().is_valid()) {
    auto token = std::move(options->start_in_options->get_directory_token());
    options->start_in_options->set_directory_token(mojo::NullRemote());
    ResolveTransferToken(
        std::move(token),
        base::BindOnce(&FileSystemAccessManagerImpl::ResolveDefaultDirectory,
                       weak_factory_.GetWeakPtr(), context, std::move(options),
                       std::move(wrapped_callback)));
    return;
  }

  ResolveDefaultDirectory(context, std::move(options),
                          std::move(wrapped_callback),
                          /*resolved_directory_token=*/nullptr);
}

void FileSystemAccessManagerImpl::FilePickerDeactivated(
    GlobalRenderFrameHostId global_rfh_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rfhs_with_active_file_pickers_.erase(global_rfh_id);
}

void FileSystemAccessManagerImpl::ResolveDefaultDirectory(
    const BindingContext& context,
    blink::mojom::FilePickerOptionsPtr options,
    ChooseEntriesCallback callback,
    FileSystemAccessTransferTokenImpl* resolved_directory_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PathInfo path_info;
  if (resolved_directory_token) {
    // Prioritize an explicitly stated directory handle to start in over an `id`
    // or a well-known directory.
    HandleTransferTokenAsDefaultDirectory(resolved_directory_token, path_info);
  }

  if (path_info.path.empty() && permission_context_) {
    if (!options->starting_directory_id.empty()) {
      // Prioritize an `id` over a well-known directory.
      path_info = permission_context_->GetLastPickedDirectory(
          context.storage_key.origin(), options->starting_directory_id);
    }
    if (path_info.path.empty()) {
      if (!options->start_in_options.is_null() &&
          options->start_in_options->is_well_known_directory()) {
        // Prioritize an explicitly stated well-known directory over an
        // implicitly remembered LastPicked directory.
        path_info.path = permission_context_->GetWellKnownDirectoryPath(
            options->start_in_options->get_well_known_directory(),
            context.storage_key.origin());
      } else {
        // If `id` empty or unset, fall back to the default LastPickedDirectory.
        path_info = permission_context_->GetLastPickedDirectory(
            context.storage_key.origin(), std::string());
      }
    }
  }

  auto fs_url = CreateFileSystemURLFromPath(path_info);
  base::FilePath default_directory = fs_url.path();

  auto wrapped_callback =
      base::BindOnce(&FileSystemAccessManagerImpl::SetDefaultPathAndShowPicker,
                     weak_factory_.GetWeakPtr(), context, std::move(options),
                     default_directory, std::move(callback));
  DoFileSystemOperation(FROM_HERE,
                        &storage::FileSystemOperationRunner::DirectoryExists,
                        base::BindOnce(&DidCheckIfDefaultDirectoryExists,
                                       fs_url, std::move(wrapped_callback)),
                        fs_url);
}

void FileSystemAccessManagerImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  operation_runner_.Reset();
  permission_context_ = nullptr;
}

void FileSystemAccessManagerImpl::SetDefaultPathAndShowPicker(
    const BindingContext& context,
    blink::mojom::FilePickerOptionsPtr options,
    base::FilePath default_directory,
    ChooseEntriesCallback callback,
    bool default_directory_exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!default_directory_exists && permission_context_) {
    default_directory = permission_context_->GetWellKnownDirectoryPath(
        blink::mojom::WellKnownDirectory::kDirDocuments,
        context.storage_key.origin());
  }

  auto request_directory_write_access =
      options->type_specific_options->is_directory_picker_options() &&
      options->type_specific_options->get_directory_picker_options()
          ->request_writable;

  auto suggested_name =
      options->type_specific_options->is_save_file_picker_options()
          ? options->type_specific_options->get_save_file_picker_options()
                ->suggested_name
          : std::string();

  base::FilePath suggested_name_path;
  if (!suggested_name.empty()) {
    // `net::GenerateFileName` does not strip "%" characters.
    base::ReplaceChars(suggested_name, "%", "_", &suggested_name);
    suggested_name_path =
        net::GenerateFileName(GURL(), std::string(), std::string(),
                              suggested_name, std::string(), std::string());

    auto suggested_extension = suggested_name_path.Extension();
    // Our version of `IsShellIntegratedExtension()` is more stringent than
    // the version used in `net::GenerateFileName()`. See
    // `FileSystemChooser::IsShellIntegratedExtension()` for details.
    if (FileSystemChooser::IsShellIntegratedExtension(suggested_extension)) {
      suggested_extension = FILE_PATH_LITERAL("download");
      suggested_name_path =
          suggested_name_path.ReplaceExtension(suggested_extension);
    }
  }

  std::u16string title = permission_context_
                             ? permission_context_->GetPickerTitle(options)
                             : std::u16string();
  FileSystemChooser::Options file_system_chooser_options(
      GetSelectFileDialogType(options->type_specific_options),
      GetAndMoveAcceptsTypesInfo(options->type_specific_options),
      std::move(title), std::move(default_directory),
      std::move(suggested_name_path));

  if (auto_file_picker_result_for_test_) {
    DidChooseEntries(
        context, file_system_chooser_options, options->starting_directory_id,
        request_directory_write_access, std::move(callback),
        file_system_access_error::Ok(), {*auto_file_picker_result_for_test_});
    return;
  }

  ShowFilePickerOnUIThread(
      context.storage_key.origin(), context.frame_id,
      file_system_chooser_options,
      base::BindOnce(&FileSystemAccessManagerImpl::DidChooseEntries,
                     weak_factory_.GetWeakPtr(), context,
                     file_system_chooser_options,
                     options->starting_directory_id,
                     request_directory_write_access, std::move(callback)));
}

void FileSystemAccessManagerImpl::CreateFileSystemAccessDataTransferToken(
    const content::PathInfo& file_path_info,
    int renderer_id,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto data_transfer_token_impl =
      std::make_unique<FileSystemAccessDataTransferTokenImpl>(
          this, file_path_info, renderer_id, std::move(receiver));
  auto token = data_transfer_token_impl->token();
  data_transfer_tokens_.emplace(token, std::move(data_transfer_token_impl));
}

void FileSystemAccessManagerImpl::GetEntryFromDataTransferToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessDataTransferToken> token,
    GetEntryFromDataTransferTokenCallback token_resolved_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<blink::mojom::FileSystemAccessDataTransferToken>
      data_transfer_token_remote(std::move(token));

  // Get a failure callback in case this token ends up not being valid (i.e.
  // unrecognized token or wrong renderer process ID).
  mojo::ReportBadMessageCallback failed_token_redemption_callback =
      receivers_.GetBadMessageCallback();

  // Must pass `data_transfer_token_remote` into GetInternalId in order to
  // ensure it stays in scope long enough for the callback to be called.
  auto* raw_data_transfer_token_remote = data_transfer_token_remote.get();
  raw_data_transfer_token_remote->GetInternalId(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &FileSystemAccessManagerImpl::ResolveDataTransferToken,
              weak_factory_.GetWeakPtr(), std::move(data_transfer_token_remote),
              receivers_.current_context(), std::move(token_resolved_callback),
              std::move(failed_token_redemption_callback)),
          base::UnguessableToken()));
}

void FileSystemAccessManagerImpl::ResolveDataTransferToken(
    mojo::Remote<blink::mojom::FileSystemAccessDataTransferToken>,
    const BindingContext& binding_context,
    GetEntryFromDataTransferTokenCallback token_resolved_callback,
    mojo::ReportBadMessageCallback failed_token_redemption_callback,
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto data_transfer_token_impl = data_transfer_tokens_.find(token);

  // Call `token_resolved_callback` with an error if the token isn't registered.
  if (data_transfer_token_impl == data_transfer_tokens_.end()) {
    std::move(failed_token_redemption_callback)
        .Run("Unrecognized drag drop token.");
    return;
  }

  // Call `token_resolved_callback` with an error if the process redeeming the
  // token isn't the same process that the token is registered to.
  if (data_transfer_token_impl->second->renderer_process_id() !=
      binding_context.process_id()) {
    std::move(failed_token_redemption_callback).Run("Invalid renderer ID.");
    return;
  }

  // Look up whether the file path that's associated with the token is a file or
  // directory and call ResolveDataTransferTokenWithFileType with the result.
  auto fs_url = CreateFileSystemURLFromPath(
      data_transfer_token_impl->second->file_path_info());
  DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::GetMetadata,
      base::BindOnce(&HandleTypeFromFileInfo)
          .Then(
              base::BindOnce(&FileSystemAccessManagerImpl::
                                 ResolveDataTransferTokenWithFileType,
                             weak_factory_.GetWeakPtr(), binding_context,
                             data_transfer_token_impl->second->file_path_info(),
                             fs_url, std::move(token_resolved_callback))),
      fs_url,
      storage::FileSystemOperation::GetMetadataFieldSet(
          {storage::FileSystemOperation::GetMetadataField::kIsDirectory}));
}

void FileSystemAccessManagerImpl::ResolveDataTransferTokenWithFileType(
    const BindingContext& binding_context,
    const content::PathInfo& path_info,
    const storage::FileSystemURL& url,
    GetEntryFromDataTransferTokenCallback token_resolved_callback,
    HandleType file_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't perform sensitive entry access checks on D&D files.
  if (!permission_context_ || file_type == HandleType::kFile ||
      !base::FeatureList::IsEnabled(
          features::kFileSystemAccessDragAndDropCheckBlocklist)) {
    DidVerifySensitiveDirectoryAccessForDataTransfer(
        binding_context, path_info, url, file_type,
        std::move(token_resolved_callback), SensitiveEntryResult::kAllowed);
    return;
  }

  // Drag-and-dropped files cannot be from a sandboxed file system.
  DCHECK(url.type() == storage::FileSystemType::kFileSystemTypeLocal ||
         url.type() == storage::FileSystemType::kFileSystemTypeExternal);
  // TODO(crbug.com/40061211): Add a prompt specific to D&D. For now, run
  // the same security checks and show the same prompt for D&D as for the file
  // picker.
  permission_context_->ConfirmSensitiveEntryAccess(
      binding_context.storage_key.origin(), path_info, file_type,
      UserAction::kDragAndDrop, binding_context.frame_id,
      base::BindOnce(&FileSystemAccessManagerImpl::
                         DidVerifySensitiveDirectoryAccessForDataTransfer,
                     weak_factory_.GetWeakPtr(), binding_context, path_info,
                     url, file_type, std::move(token_resolved_callback)));
}

void FileSystemAccessManagerImpl::
    DidVerifySensitiveDirectoryAccessForDataTransfer(
        const BindingContext& binding_context,
        const content::PathInfo& path_info,
        const storage::FileSystemURL& url,
        HandleType file_type,
        GetEntryFromDataTransferTokenCallback token_resolved_callback,
        SensitiveEntryResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != SensitiveEntryResult::kAllowed) {
    std::move(token_resolved_callback)
        .Run(file_system_access_error::FromStatus(
                 blink::mojom::FileSystemAccessStatus::kOperationAborted),
             blink::mojom::FileSystemAccessEntryPtr());
    return;
  }

  SharedHandleState shared_handle_state =
      GetSharedHandleStateForNonSandboxedPath(
          path_info, binding_context.storage_key, file_type,
          UserAction::kDragAndDrop);

  blink::mojom::FileSystemAccessEntryPtr entry;
  if (file_type == HandleType::kDirectory) {
    entry = blink::mojom::FileSystemAccessEntry::New(
        blink::mojom::FileSystemAccessHandle::NewDirectory(
            CreateDirectoryHandle(binding_context, url, shared_handle_state)),
        path_info.display_name);
  } else {
    entry = blink::mojom::FileSystemAccessEntry::New(
        blink::mojom::FileSystemAccessHandle::NewFile(
            CreateFileHandle(binding_context, url, shared_handle_state)),
        path_info.display_name);
  }

  std::move(token_resolved_callback)
      .Run(file_system_access_error::Ok(), std::move(entry));
}

void FileSystemAccessManagerImpl::GetFileHandleFromToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileHandle>
        file_handle_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResolveTransferToken(
      std::move(token),
      base::BindOnce(
          &FileSystemAccessManagerImpl::DidResolveTransferTokenForFileHandle,
          weak_factory_.GetWeakPtr(), receivers_.current_context(),
          std::move(file_handle_receiver)));
}

void FileSystemAccessManagerImpl::GetDirectoryHandleFromToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessDirectoryHandle>
        directory_handle_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResolveTransferToken(
      std::move(token),
      base::BindOnce(&FileSystemAccessManagerImpl::
                         DidResolveTransferTokenForDirectoryHandle,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(directory_handle_receiver)));
}

void FileSystemAccessManagerImpl::BindObserverHost(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
        host_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BindingContext& context = receivers_.current_context();
  watcher_manager().BindObserverHost(context, std::move(host_receiver));
}

void FileSystemAccessManagerImpl::SerializeHandle(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    SerializeHandleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResolveTransferToken(
      std::move(token),
      base::BindOnce(&FileSystemAccessManagerImpl::DidResolveForSerializeHandle,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

namespace {

std::string SerializePath(const base::FilePath& path) {
  auto path_bytes = base::as_bytes(base::make_span(path.value()));
  return std::string(path_bytes.begin(), path_bytes.end());
}

base::FilePath DeserializePath(const std::string& bytes) {
  base::FilePath::StringType s;
  s.resize(bytes.size() / sizeof(base::FilePath::CharType));
  std::memcpy(&s[0], bytes.data(), s.size() * sizeof(base::FilePath::CharType));
  return base::FilePath(s);
}

std::string SerializeURLImpl(const storage::FileSystemURL& url,
                             FileSystemAccessPermissionContext::HandleType type,
                             base::FilePath root_permission_path,
                             const std::string& display_name) {
  FileSystemAccessHandleData data;
  data.set_handle_type(type == HandleType::kFile
                           ? FileSystemAccessHandleData::kFile
                           : FileSystemAccessHandleData::kDirectory);

  if (url.type() == storage::kFileSystemTypeLocal ||
      url.mount_type() == storage::kFileSystemTypeExternal) {
    // Files from non-sandboxed file systems should not include bucket info.
    DCHECK(!url.bucket().has_value());

    // A url can have mount_type = external and type = native local at the same
    // time. In that case we want to still treat it as an external path.
    const bool is_external =
        url.mount_type() == storage::kFileSystemTypeExternal;
    content::LocalFileData* file_data =
        is_external ? data.mutable_external() : data.mutable_local();

    base::FilePath url_path = is_external ? url.virtual_path() : url.path();
    if (root_permission_path.empty()) {
      root_permission_path = url_path;
    }
    file_data->set_root_path(SerializePath(root_permission_path));

    base::FilePath relative_path;
    // We want `relative_path` to be the path of the file or directory
    // relative to `root_permission_path`. FilePath::AppendRelativePath gets us
    // that, but fails if the path we're looking for is equal to the
    // `root_permission_path`. So special case that case (in which case relative
    // path would be empty anyway).
    if (root_permission_path != url_path) {
      bool relative_path_result =
          root_permission_path.AppendRelativePath(url_path, &relative_path);
      DCHECK(relative_path_result);
    }

    file_data->set_relative_path(SerializePath(relative_path));
    file_data->set_display_name(display_name);
  } else if (url.type() == storage::kFileSystemTypeTemporary) {
    base::FilePath virtual_path = url.virtual_path();
    data.mutable_sandboxed()->set_virtual_path(SerializePath(virtual_path));
    // Files in the sandboxed file system must include bucket info.
    DCHECK(url.bucket().has_value());
    if (!url.bucket()->is_default) {
      data.mutable_sandboxed()->set_bucket_id(url.bucket()->id.value());
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  std::string value;
  bool success = data.SerializeToString(&value);
  DCHECK(success);
  return value;
}

}  // namespace

std::string FileSystemAccessManagerImpl::SerializeURL(
    const storage::FileSystemURL& url,
    HandleType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return SerializeURLImpl(url, type,
                          /*root_permission_path=*/base::FilePath(),
                          /*display_name=*/std::string());
}

std::string FileSystemAccessManagerImpl::SerializeURLWithPermissionRoot(
    const storage::FileSystemURL& url,
    HandleType type,
    const base::FilePath& root_permission_path,
    const std::string& display_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return SerializeURLImpl(url, type, root_permission_path, display_name);
}

void FileSystemAccessManagerImpl::DidResolveForSerializeHandle(
    SerializeHandleCallback callback,
    FileSystemAccessTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_token) {
    std::move(callback).Run({});
    return;
  }

  auto value = SerializeURLWithPermissionRoot(
      resolved_token->url(), resolved_token->type(),
      resolved_token->GetWriteGrant()->GetPath(),
      resolved_token->GetWriteGrant()->GetDisplayName());
  std::vector<uint8_t> result(value.begin(), value.end());
  std::move(callback).Run(result);
}

void FileSystemAccessManagerImpl::DidGetSandboxedBucketForDeserializeHandle(
    const FileSystemAccessHandleData& data,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token,
    const storage::FileSystemURL& url) {
  CreateTransferTokenImpl(
      url, url.storage_key(), GetSharedHandleStateForSandboxedPath(),
      data.handle_type() == FileSystemAccessHandleData::kDirectory
          ? HandleType::kDirectory
          : HandleType::kFile,
      std::move(token));
}

void FileSystemAccessManagerImpl::DeserializeHandle(
    const blink::StorageKey& storage_key,
    const std::vector<uint8_t>& bits,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!bits.empty());

  std::string bits_as_string(bits.begin(), bits.end());
  FileSystemAccessHandleData data;
  if (!data.ParseFromString(bits_as_string)) {
    // Drop `token`, and directly return.
    return;
  }

  switch (data.data_case()) {
    case FileSystemAccessHandleData::kSandboxed: {
      base::FilePath virtual_path =
          DeserializePath(data.sandboxed().virtual_path());
      storage::FileSystemURL url = context()->CreateCrackedFileSystemURL(
          storage_key, storage::kFileSystemTypeTemporary, virtual_path);
      // Apply bucket information.
      auto bucket_callback = base::BindOnce(
          [](storage::FileSystemURL url,
             base::OnceCallback<void(const storage::FileSystemURL&)> callback,
             storage::QuotaErrorOr<storage::BucketInfo> result) {
            if (!result.has_value()) {
              // Drop `token`, and directly return.
              return;
            }
            url.SetBucket(result->ToBucketLocator());
            std::move(callback).Run(url);
          },
          url,
          base::BindOnce(&FileSystemAccessManagerImpl::
                             DidGetSandboxedBucketForDeserializeHandle,
                         weak_factory_.GetWeakPtr(), data, std::move(token)));
      if (!data.sandboxed().has_bucket_id()) {
        // Use the default storage bucket.
        context_->quota_manager_proxy()->UpdateOrCreateBucket(
            storage::BucketInitParams::ForDefaultBucket(storage_key),
            base::SequencedTaskRunner::GetCurrentDefault(),
            std::move(bucket_callback));
      } else {
        context_->quota_manager_proxy()->GetBucketById(
            storage::BucketId::FromUnsafeValue(data.sandboxed().bucket_id()),
            base::SequencedTaskRunner::GetCurrentDefault(),
            std::move(bucket_callback));
      }
      break;
    }
    case FileSystemAccessHandleData::kLocal:
    case FileSystemAccessHandleData::kExternal: {
      const content::LocalFileData& file_data =
          data.data_case() == FileSystemAccessHandleData::kLocal
              ? data.local()
              : data.external();

      base::FilePath root_path = DeserializePath(file_data.root_path());
      base::FilePath relative_path = DeserializePath(file_data.relative_path());
      PathInfo path_info(data.data_case() == FileSystemAccessHandleData::kLocal
                             ? PathType::kLocal
                             : PathType::kExternal,
                         root_path, file_data.display_name());
      storage::FileSystemURL root = CreateFileSystemURLFromPath(path_info);

      storage::FileSystemURL child = context()->CreateCrackedFileSystemURL(
          root.storage_key(), root.mount_type(),
          root.virtual_path().Append(relative_path));

      const bool is_directory =
          data.handle_type() == FileSystemAccessHandleData::kDirectory;

      // Permissions are scoped to `root_path`, rather than the individual
      // handle. So if `relative_path` is not empty, this creates a
      // SharedHandleState for a directory even if the handle represents a
      // file.
      SharedHandleState handle_state = GetSharedHandleStateForNonSandboxedPath(
          path_info, storage_key,
          (is_directory || !relative_path.empty()) ? HandleType::kDirectory
                                                   : HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kLoadFromStorage);
      CreateTransferTokenImpl(
          child, storage_key, handle_state,
          is_directory ? HandleType::kDirectory : HandleType::kFile,
          std::move(token));
      break;
    }
    case FileSystemAccessHandleData::DATA_NOT_SET:
      NOTREACHED_IN_MIGRATION();
  }
}

void FileSystemAccessManagerImpl::Clone(
    mojo::PendingReceiver<storage::mojom::FileSystemAccessContext> receiver) {
  BindInternalsReceiver(std::move(receiver));
}

blink::mojom::FileSystemAccessEntryPtr
FileSystemAccessManagerImpl::CreateFileEntryFromPath(
    const BindingContext& binding_context,
    const content::PathInfo& file_path_info,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage::FileSystemURL url = CreateFileSystemURLFromPath(file_path_info);

  SharedHandleState shared_handle_state =
      GetSharedHandleStateForNonSandboxedPath(file_path_info,
                                              binding_context.storage_key,
                                              HandleType::kFile, user_action);

  return blink::mojom::FileSystemAccessEntry::New(
      blink::mojom::FileSystemAccessHandle::NewFile(
          CreateFileHandle(binding_context, url, shared_handle_state)),
      file_path_info.display_name);
}

blink::mojom::FileSystemAccessEntryPtr
FileSystemAccessManagerImpl::CreateDirectoryEntryFromPath(
    const BindingContext& binding_context,
    const content::PathInfo& file_path_info,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage::FileSystemURL url = CreateFileSystemURLFromPath(file_path_info);

  SharedHandleState shared_handle_state =
      GetSharedHandleStateForNonSandboxedPath(
          file_path_info, binding_context.storage_key, HandleType::kDirectory,
          user_action);

  return blink::mojom::FileSystemAccessEntry::New(
      blink::mojom::FileSystemAccessHandle::NewDirectory(
          CreateDirectoryHandle(binding_context, url, shared_handle_state)),
      file_path_info.display_name);
}

mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle>
FileSystemAccessManagerImpl::CreateFileHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileHandle> result;
  file_receivers_.Add(std::make_unique<FileSystemAccessFileHandleImpl>(
                          this, binding_context, url, handle_state),
                      result.InitWithNewPipeAndPassReceiver());
  return result;
}

mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle>
FileSystemAccessManagerImpl::CreateDirectoryHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url.is_valid());

  mojo::PendingRemote<blink::mojom::FileSystemAccessDirectoryHandle> result;
  directory_receivers_.Add(
      std::make_unique<FileSystemAccessDirectoryHandleImpl>(
          this, binding_context, url, handle_state),
      result.InitWithNewPipeAndPassReceiver());
  return result;
}
void FileSystemAccessManagerImpl::TakeLock(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    FileSystemAccessLockManager::LockType lock_type,
    FileSystemAccessLockManager::TakeLockCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lock_manager_->TakeLock(binding_context.frame_id, url, lock_type,
                          std::move(callback));
}
bool FileSystemAccessManagerImpl::IsContentious(
    const storage::FileSystemURL& url,
    FileSystemAccessLockManager::LockType lock_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lock_manager_->IsContentious(url, lock_type);
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::CreateSharedLockTypeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lock_manager_->CreateSharedLockType();
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::GetExclusiveLockType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lock_manager_->GetExclusiveLockType();
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::GetSAHReadOnlyLockType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sah_read_only_lock_type_;
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::GetSAHReadwriteUnsafeLockType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sah_readwrite_unsafe_lock_type_;
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::GetWFSSiloedLockType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return wfs_siloed_lock_type_;
}
FileSystemAccessLockManager::LockType
FileSystemAccessManagerImpl::GetAncestorLockTypeForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lock_manager_->GetAncestorLockTypeForTesting();  // IN-TEST
}

base::WeakPtr<FileSystemAccessLockManager>
FileSystemAccessManagerImpl::GetLockManagerWeakPtrForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lock_manager_->GetWeakPtrForTesting();  // IN-TEST
}

mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>
FileSystemAccessManagerImpl::CreateFileWriter(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    const SharedHandleState& handle_state,
    bool auto_close) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter> result;

  RenderFrameHost* rfh = RenderFrameHost::FromID(binding_context.frame_id);
  bool has_transient_user_activation = rfh && rfh->HasTransientUserActivation();

  CreateFileWriter(
      binding_context, url, swap_url, std::move(lock), std::move(swap_lock),
      handle_state, result.InitWithNewPipeAndPassReceiver(),
      has_transient_user_activation, auto_close,
      GetContentClient()->browser()->GetQuarantineConnectionCallback());
  return result;
}

base::WeakPtr<FileSystemAccessFileWriterImpl>
FileSystemAccessManagerImpl::CreateFileWriter(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> swap_lock,
    const SharedHandleState& handle_state,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileWriter> receiver,
    bool has_transient_user_activation,
    bool auto_close,
    download::QuarantineConnectionCallback quarantine_connection_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto writer = std::make_unique<FileSystemAccessFileWriterImpl>(
      this, PassKey(), binding_context, url, swap_url, std::move(lock),
      std::move(swap_lock), handle_state, std::move(receiver),
      has_transient_user_activation, auto_close,
      quarantine_connection_callback);

  base::WeakPtr<FileSystemAccessFileWriterImpl> writer_weak =
      writer->weak_ptr();
  writer_receivers_.insert(std::move(writer));
  return writer_weak;
}

mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost>
FileSystemAccessManagerImpl::CreateAccessHandleHost(
    const storage::FileSystemURL& url,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileDelegateHost>
        file_delegate_receiver,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileModificationHost>
        file_modification_host_receiver,
    int64_t file_size,
    scoped_refptr<FileSystemAccessLockManager::LockHandle> lock,
    base::ScopedClosureRunner on_close_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<blink::mojom::FileSystemAccessAccessHandleHost> result;
  auto receiver = result.InitWithNewPipeAndPassReceiver();
  auto access_handle_host =
      std::make_unique<FileSystemAccessAccessHandleHostImpl>(
          this, url, std::move(lock), PassKey(), std::move(receiver),
          std::move(file_delegate_receiver),
          std::move(file_modification_host_receiver), file_size,
          std::move(on_close_callback));
  access_handle_host_receivers_.insert(std::move(access_handle_host));

  return result;
}

void FileSystemAccessManagerImpl::CreateTransferToken(
    const FileSystemAccessFileHandleImpl& file,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateTransferTokenImpl(file.url(), file.context().storage_key,
                                 file.handle_state(), HandleType::kFile,
                                 std::move(receiver));
}

void FileSystemAccessManagerImpl::CreateTransferToken(
    const FileSystemAccessDirectoryHandleImpl& directory,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return CreateTransferTokenImpl(
      directory.url(), directory.context().storage_key,
      directory.handle_state(), HandleType::kDirectory, std::move(receiver));
}

void FileSystemAccessManagerImpl::ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    ResolvedTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::Remote<blink::mojom::FileSystemAccessTransferToken> token_remote(
      std::move(token));
  auto* raw_token = token_remote.get();
  raw_token->GetInternalID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&FileSystemAccessManagerImpl::DoResolveTransferToken,
                     weak_factory_.GetWeakPtr(), std::move(token_remote),
                     std::move(callback)),
      base::UnguessableToken()));
}

void FileSystemAccessManagerImpl::DidResolveTransferTokenForFileHandle(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessFileHandle>
        file_handle_receiver,
    FileSystemAccessTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidTransferToken(resolved_token,
                            binding_context.storage_key.origin(),
                            HandleType::kFile)) {
    // Fail silently. In practice, the FileSystemAccessManager should not
    // receive any invalid tokens. Before redeeming a token, the render process
    // performs an origin check to ensure the token is valid. Invalid tokens
    // indicate a code bug or a compromised render process.
    //
    // After receiving an invalid token, the FileSystemAccessManager
    // cannot determine which render process is compromised. Is it the post
    // message sender or receiver? Because of this, the FileSystemAccessManager
    // closes the FileHandle pipe and ignores the error.
    return;
  }

  file_receivers_.Add(resolved_token->CreateFileHandle(binding_context),
                      std::move(file_handle_receiver));
}

void FileSystemAccessManagerImpl::DidResolveTransferTokenForDirectoryHandle(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessDirectoryHandle>
        directory_handle_receiver,
    FileSystemAccessTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidTransferToken(resolved_token,
                            binding_context.storage_key.origin(),
                            HandleType::kDirectory)) {
    // Fail silently. See comment above in
    // DidResolveTransferTokenForFileHandle() for details.
    return;
  }

  directory_receivers_.Add(
      resolved_token->CreateDirectoryHandle(binding_context),
      std::move(directory_handle_receiver));
}

const base::SequenceBound<storage::FileSystemOperationRunner>&
FileSystemAccessManagerImpl::operation_runner() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!operation_runner_) {
    operation_runner_ =
        context()->CreateSequenceBoundFileSystemOperationRunner();
  }
  return operation_runner_;
}

void FileSystemAccessManagerImpl::DidOpenSandboxedFileSystem(
    const BindingContext& binding_context,
    GetSandboxedFileSystemCallback callback,
    const storage::FileSystemURL& root,
    const std::string& filesystem_name,
    base::File::Error result,
    const std::vector<std::string>& directory_path_components) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  if (directory_path_components.size() == 0) {
    std::move(callback).Run(
        file_system_access_error::Ok(),
        CreateDirectoryHandle(binding_context, root,
                              GetSharedHandleStateForSandboxedPath()));
    return;
  }

  base::FilePath file_path = base::FilePath(root.path());
  for (const auto& component : directory_path_components) {
    file_path = file_path.AppendASCII(component);
  }

  auto url = context()->CreateCrackedFileSystemURL(
      root.storage_key(), root.mount_type(), file_path);
  if (root.bucket().has_value()) {
    url.SetBucket(root.bucket().value());
  }

  DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::DirectoryExists,
      base::BindOnce(&FileSystemAccessManagerImpl::
                         DidResolveUrlAfterOpeningSandboxedFileSystem,
                     weak_factory_.GetWeakPtr(), binding_context,
                     std::move(callback), url),
      url);
}

void FileSystemAccessManagerImpl::DidResolveUrlAfterOpeningSandboxedFileSystem(
    const BindingContext& binding_context,
    GetSandboxedFileSystemCallback callback,
    const storage::FileSystemURL& url,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(file_system_access_error::FromFileError(result),
                            mojo::NullRemote());
    return;
  }

  std::move(callback).Run(
      file_system_access_error::Ok(),
      CreateDirectoryHandle(binding_context, url,
                            GetSharedHandleStateForSandboxedPath()));
}

void FileSystemAccessManagerImpl::DidChooseEntries(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    const std::string& starting_directory_id,
    const bool request_directory_write_access,
    ChooseEntriesCallback callback,
    blink::mojom::FileSystemAccessErrorPtr result,
    std::vector<PathInfo> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->status != FileSystemAccessStatus::kOk || entries.empty()) {
    std::move(callback).Run(
        std::move(result),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }

  if (!permission_context_) {
    DidVerifySensitiveDirectoryAccess(
        binding_context, options, starting_directory_id,
        request_directory_write_access, std::move(callback), std::move(entries),
        SensitiveEntryResult::kAllowed);
    return;
  }

  // It is enough to only verify access to the first path, as multiple
  // file selection is only supported if all files are in the same
  // directory.
  PathInfo first_entry = entries.front();
  const bool is_directory =
      options.type() == ui::SelectFileDialog::SELECT_FOLDER;
  permission_context_->ConfirmSensitiveEntryAccess(
      binding_context.storage_key.origin(), first_entry,
      is_directory ? HandleType::kDirectory : HandleType::kFile,
      options.type() == ui::SelectFileDialog::SELECT_SAVEAS_FILE
          ? UserAction::kSave
          : UserAction::kOpen,
      binding_context.frame_id,
      base::BindOnce(
          &FileSystemAccessManagerImpl::DidVerifySensitiveDirectoryAccess,
          weak_factory_.GetWeakPtr(), binding_context, options,
          starting_directory_id, request_directory_write_access,
          std::move(callback), std::move(entries)));
}

void FileSystemAccessManagerImpl::DidVerifySensitiveDirectoryAccess(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    const std::string& starting_directory_id,
    const bool request_directory_write_access,
    ChooseEntriesCallback callback,
    std::vector<PathInfo> entries,
    SensitiveEntryResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == SensitiveEntryResult::kAbort) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            FileSystemAccessStatus::kOperationAborted),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }
  if (result == SensitiveEntryResult::kTryAgain) {
    ShowFilePickerOnUIThread(
        binding_context.storage_key.origin(), binding_context.frame_id, options,
        base::BindOnce(&FileSystemAccessManagerImpl::DidChooseEntries,
                       weak_factory_.GetWeakPtr(), binding_context, options,
                       starting_directory_id, request_directory_write_access,
                       std::move(callback)));
    return;
  }

  // There is no need to scan the file in case of saving, since it's
  // data is truncated at this point, so it won't be available for
  // the web page.
  if (permission_context_ &&
      options.type() != ui::SelectFileDialog::SELECT_SAVEAS_FILE) {
    permission_context_->CheckPathsAgainstEnterprisePolicy(
        std::move(entries), binding_context.frame_id,
        base::BindOnce(
            &FileSystemAccessManagerImpl::OnCheckPathsAgainstEnterprisePolicy,
            weak_factory_.GetWeakPtr(), binding_context, options,
            starting_directory_id, request_directory_write_access,
            std::move(callback)));
    return;
  }

  OnCheckPathsAgainstEnterprisePolicy(
      binding_context, options, starting_directory_id,
      request_directory_write_access, std::move(callback), std::move(entries));
}

void FileSystemAccessManagerImpl::OnCheckPathsAgainstEnterprisePolicy(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    const std::string& starting_directory_id,
    bool request_directory_write_access,
    ChooseEntriesCallback callback,
    std::vector<PathInfo> entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It is possible for `entries` to be empty if enterprise policy blocked all
  // files or folders selected by the user.  If there are no entries, simulate
  // a user abort.
  if (entries.empty()) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationAborted),
        std::vector<blink::mojom::FileSystemAccessEntryPtr>());
    return;
  }

  if (permission_context_) {
    auto picked_directory = entries.front();
    if (options.type() != ui::SelectFileDialog::SELECT_FOLDER) {
      picked_directory.path = picked_directory.path.DirName();
      picked_directory.display_name =
          picked_directory.path.BaseName().AsUTF8Unsafe();
    }
    permission_context_->SetLastPickedDirectory(
        binding_context.storage_key.origin(), starting_directory_id,
        picked_directory);
  }

  if (options.type() == ui::SelectFileDialog::SELECT_FOLDER) {
    DCHECK_EQ(entries.size(), 1u);
    SharedHandleState shared_handle_state =
        GetSharedHandleStateForNonSandboxedPath(
            entries.front(), binding_context.storage_key,
            HandleType::kDirectory,
            FileSystemAccessPermissionContext::UserAction::kOpen);
    // Ask for both read and write permission at the same time. The permission
    // context should coalesce these into one prompt.
    if (request_directory_write_access) {
      shared_handle_state.write_grant->RequestPermission(
          binding_context.frame_id,
          FileSystemAccessPermissionGrant::UserActivationState::kNotRequired,
          base::DoNothing());
    }
    shared_handle_state.read_grant->RequestPermission(
        binding_context.frame_id,
        FileSystemAccessPermissionGrant::UserActivationState::kNotRequired,
        base::BindOnce(&FileSystemAccessManagerImpl::DidChooseDirectory, this,
                       binding_context, entries.front(), std::move(callback),
                       shared_handle_state));
    return;
  }

  if (options.type() == ui::SelectFileDialog::SELECT_SAVEAS_FILE) {
    DCHECK_EQ(entries.size(), 1u);
    // Create file if it doesn't yet exist, and truncate file if it does
    // exist.
    auto fs_url = CreateFileSystemURLFromPath(entries.front());

    operation_runner().PostTaskWithThisObject(base::BindOnce(
        &CreateAndTruncateFile, fs_url,
        base::BindOnce(
            &FileSystemAccessManagerImpl::DidCreateAndTruncateSaveFile, this,
            binding_context, entries.front(), fs_url, std::move(callback)),
        base::SequencedTaskRunner::GetCurrentDefault()));
    return;
  }

  std::vector<blink::mojom::FileSystemAccessEntryPtr> result_entries;
  result_entries.reserve(entries.size());
  for (const auto& entry : entries) {
    result_entries.push_back(
        CreateFileEntryFromPath(binding_context, entry, UserAction::kOpen));
  }

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(result_entries));
}

void FileSystemAccessManagerImpl::DidCreateAndTruncateSaveFile(
    const BindingContext& binding_context,
    const PathInfo& entry,
    const storage::FileSystemURL& url,
    ChooseEntriesCallback callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<blink::mojom::FileSystemAccessEntryPtr> result_entries;
  if (!success) {
    // TODO(crbug.com/40717501): Failure to create or truncate the file
    // should probably not just result in a generic error, but instead inform
    // the user of the problem?
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kOperationFailed,
            "Failed to create or truncate file"),
        std::move(result_entries));
    return;
  }

  if (permission_context_) {
    permission_context_->OnFileCreatedFromShowSaveFilePicker(
        /*file_picker_binding_context=*/binding_context.url, url);
  }

  SharedHandleState shared_handle_state =
      GetSharedHandleStateForNonSandboxedPath(
          entry, binding_context.storage_key, HandleType::kFile,
          UserAction::kSave);

  result_entries.push_back(blink::mojom::FileSystemAccessEntry::New(
      blink::mojom::FileSystemAccessHandle::NewFile(
          CreateFileHandle(binding_context, url, shared_handle_state)),
      entry.display_name));

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(result_entries));
}

void FileSystemAccessManagerImpl::DidChooseDirectory(
    const BindingContext& binding_context,
    const PathInfo& entry,
    ChooseEntriesCallback callback,
    const SharedHandleState& shared_handle_state,
    FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<blink::mojom::FileSystemAccessEntryPtr> result_entries;
  if (shared_handle_state.read_grant->GetStatus() !=
      PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
                                FileSystemAccessStatus::kOperationAborted),
                            std::move(result_entries));
    return;
  }

  storage::FileSystemURL url = CreateFileSystemURLFromPath(entry);

  result_entries.push_back(blink::mojom::FileSystemAccessEntry::New(
      blink::mojom::FileSystemAccessHandle::NewDirectory(CreateDirectoryHandle(
          binding_context, url,
          SharedHandleState(shared_handle_state.read_grant,
                            shared_handle_state.write_grant))),
      entry.display_name));
  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(result_entries));
}

void FileSystemAccessManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    const blink::StorageKey& storage_key,
    const SharedHandleState& handle_state,
    HandleType handle_type,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
        receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto token_impl = std::make_unique<FileSystemAccessTransferTokenImpl>(
      url, storage_key.origin(), handle_state, handle_type, this,
      std::move(receiver));
  auto token = token_impl->token();
  transfer_tokens_.emplace(token, std::move(token_impl));
}

void FileSystemAccessManagerImpl::RemoveFileWriter(
    FileSystemAccessFileWriterImpl* writer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = writer_receivers_.erase(writer);
  DCHECK_EQ(1u, count_removed);
}

void FileSystemAccessManagerImpl::RemoveAccessHandleHost(
    FileSystemAccessAccessHandleHostImpl* access_handle_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(access_handle_host);

  // Capacity allocations only exist in non-incognito mode.
  if (context()->is_incognito()) {
    DidCleanupAccessHandleCapacityAllocation(access_handle_host);
    return;
  }
  CleanupAccessHandleCapacityAllocation(
      access_handle_host->url(), access_handle_host->granted_capacity(),
      base::BindOnce(&FileSystemAccessManagerImpl::
                         DidCleanupAccessHandleCapacityAllocation,
                     weak_factory_.GetWeakPtr(), access_handle_host));
}

void FileSystemAccessManagerImpl::RemoveToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void FileSystemAccessManagerImpl::RemoveDataTransferToken(
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = data_transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void FileSystemAccessManagerImpl::DoResolveTransferToken(
    mojo::Remote<blink::mojom::FileSystemAccessTransferToken>,
    ResolvedTokenCallback callback,
    const base::UnguessableToken& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = transfer_tokens_.find(token);
  if (it == transfer_tokens_.end()) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(it->second.get());
  }
}

storage::FileSystemURL FileSystemAccessManagerImpl::CreateFileSystemURLFromPath(
    const PathInfo& path_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context()->CreateCrackedFileSystemURL(
      blink::StorageKey(),
      path_info.type == PathType::kLocal ? storage::kFileSystemTypeLocal
                                         : storage::kFileSystemTypeExternal,
      path_info.path);
}

FileSystemAccessManagerImpl::SharedHandleState
FileSystemAccessManagerImpl::GetSharedHandleStateForNonSandboxedPath(
    const content::PathInfo& path_info,
    const blink::StorageKey& storage_key,
    HandleType handle_type,
    FileSystemAccessPermissionContext::UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scoped_refptr<FileSystemAccessPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        storage_key.origin(), path_info, handle_type, user_action);
    write_grant = permission_context_->GetWritePermissionGrant(
        storage_key.origin(), path_info, handle_type, user_action);
  } else {
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED,
        path_info);
    switch (user_action) {
      case FileSystemAccessPermissionContext::UserAction::kNone:
      case FileSystemAccessPermissionContext::UserAction::kLoadFromStorage:
        read_grant = write_grant;
        break;
      case FileSystemAccessPermissionContext::UserAction::kOpen:
      case FileSystemAccessPermissionContext::UserAction::kSave:
      case FileSystemAccessPermissionContext::UserAction::kDragAndDrop:
        // Grant read permission even without a permission_context_, as the
        // picker itself is enough UI to assume user intent.
        read_grant = base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
            PermissionStatus::GRANTED, path_info);
        break;
    }
  }
  return SharedHandleState(std::move(read_grant), std::move(write_grant));
}

FileSystemAccessHandleBase::SharedHandleState
FileSystemAccessManagerImpl::GetSharedHandleStateForSandboxedPath() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/40198034): This is a hack which is only viable since
  // permission grants always return GRANTED in sandboxed file systems.
  //  - Ideally we would not need to special-case the permission logic for
  //  files
  //    in the sandboxed file system. It should be the same as for local and
  //    external file systems.
  //  - At minimum, should not be creating new grants every time a
  //    SharedHandleState is needed for a handle in a sandboxed file system.
  //    Once a permission grant for the root of a bucket file system is
  //    created, that permission grant should be used for all handles in the
  //    file system. That this is not the case currently breaks any logic
  //    relying on a FileSystemAccessPermissionGrant::Observer.
  auto permission_grant =
      base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
          PermissionStatus::GRANTED, PathInfo());
  return SharedHandleState(permission_grant, permission_grant);
}

base::Uuid FileSystemAccessManagerImpl::GetUniqueId(
    const FileSystemAccessFileHandleImpl& file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40852050): This is a temporary hack to put something
  // that works behind a flag. Persist handle IDs such that they're stable
  // across browsing sessions.

  auto it = file_ids_.find(file.url());
  if (it != file_ids_.end()) {
    return it->second;
  }

  // Generate and store a new uuid for this file.
  auto uuid = base::Uuid::GenerateRandomV4();
  file_ids_[file.url()] = uuid;
  return uuid;
}

base::Uuid FileSystemAccessManagerImpl::GetUniqueId(
    const FileSystemAccessDirectoryHandleImpl& directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/40852050): This is a temporary hack to put something
  // that works behind a flag. Persist handle IDs such that they're stable
  // across browsing sessions.

  auto it = directory_ids_.find(directory.url());
  if (it != directory_ids_.end()) {
    return it->second;
  }

  // Generate and store a new uuid for this directory.
  auto uuid = base::Uuid::GenerateRandomV4();
  directory_ids_[directory.url()] = uuid;
  return uuid;
}

void FileSystemAccessManagerImpl::CleanupAccessHandleCapacityAllocation(
    const storage::FileSystemURL& url,
    int64_t allocated_file_size,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(allocated_file_size, 0);

  DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::GetMetadata,
      base::BindOnce(&FileSystemAccessManagerImpl::
                         CleanupAccessHandleCapacityAllocationImpl,
                     weak_factory_.GetWeakPtr(), url, allocated_file_size,
                     std::move(callback)),
      url,
      storage::FileSystemOperation::GetMetadataFieldSet(
          {storage::FileSystemOperation::GetMetadataField::kSize}));
}

void FileSystemAccessManagerImpl::CleanupAccessHandleCapacityAllocationImpl(
    const storage::FileSystemURL& url,
    int64_t allocated_file_size,
    base::OnceClosure callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != base::File::FILE_OK) {
    return;
  }
  DCHECK_GE(file_info.size, 0);
  // if the QuotaManagerProxy is gone, no changes are possible.
  if (!context_->quota_manager_proxy()) {
    return;
  }
  DCHECK_GE(allocated_file_size, 0);

  int64_t overallocation = allocated_file_size - file_info.size;
  DCHECK_GE(overallocation, 0)
      << "An Access Handle should not use more capacity than allocated.";

  DCHECK(url.bucket().has_value())
      << "Capacity allocation is only relevant for sandboxed file systems, "
         "which should have an associated bucket.";
  context_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kFileSystem, *url.bucket(), -overallocation,
      base::Time::Now(),
      /*callback_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
      std::move(callback));
}

void FileSystemAccessManagerImpl::DidCleanupAccessHandleCapacityAllocation(
    FileSystemAccessAccessHandleHostImpl* access_handle_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(access_handle_host);

  // We cannot destroy `access_handle_host` by erasing it from the
  // `access_handle_host_receivers_` set.
  //
  // The destruction of a `FileSystemAccessAccessHandleHostImpl` can trigger
  // the creation of another. This means that if we directly erase
  // `access_handle_host` from the set, `access_handle_host_receivers_`
  // `erase` could call into `access_handle_host_receivers_` `insert` (in
  // `CreateAccessHandleHost`) which is undefined behavior. Instead, we'll
  // move it out of the set before erasing and then destroying.
  size_t initial_size = access_handle_host_receivers_.size();

  auto iter = access_handle_host_receivers_.find(access_handle_host);
  CHECK(iter != access_handle_host_receivers_.end());

  auto access_handle_host_receiver = std::move(*iter);
  access_handle_host_receivers_.erase(iter);

  size_t count_removed = initial_size - access_handle_host_receivers_.size();
  DCHECK_EQ(1u, count_removed);
}

void FileSystemAccessManagerImpl::ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        transfer_token,
    ResolveTransferTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResolveTransferToken(std::move(transfer_token),
                       base::BindOnce(
                           [](ResolveTransferTokenCallback callback,
                              FileSystemAccessTransferTokenImpl* token) {
                             if (!token) {
                               std::move(callback).Run(std::nullopt);
                               return;
                             }
                             std::move(callback).Run(token->url());
                           },
                           std::move(callback)));
}

base::WeakPtr<FileSystemAccessManagerImpl>
FileSystemAccessManagerImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

bool FileSystemAccessManagerImpl::IsSafePathComponent(
    storage::FileSystemType type,
    const url::Origin& origin,
    const std::string& name) {
  // This method is similar to net::IsSafePortablePathComponent, with a few
  // notable differences where the net version does not consider names safe
  // while here we do want to allow them. These cases are:
  //  - Files in sandboxed file systems are subject to far fewer restrictions,
  //    i.e. base::i18n::IsFilenameLegal is not called.
  //  - Names starting with a '.'. These would be hidden files in most file
  //    managers, but are something we explicitly want to support for the
  //    File System Access API, for names like .git.
  //  - safe_browsing::DownloadFileType::DangerLevel::kDangerous are considered
  //    not safe, with an exception of '.local'. For downloads writing to such
  //    files is dangerous since it might modify what code is executed when an
  //    executable is ran from the same directory. For the File System Access
  //    API this isn't really a problem though, since if a website can write to
  //    a .local file via a FileSystemDirectoryHandle they can also just modify
  //    the executables in the directory directly.
  //
  // TODO(crbug.com/40159607): Unify this with
  // net::IsSafePortablePathComponent, with the result probably ending up in
  // base/i18n/file_util_icu.h.

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::FilePath component = storage::StringToFilePath(name);
  // Empty names, or names that contain path separators are invalid.
  if (component.empty() ||
      component != storage::VirtualPath::BaseName(component) ||
      component != component.StripTrailingSeparators()) {
    return false;
  }

  std::u16string component16;
#if BUILDFLAG(IS_WIN)
  component16.assign(component.value().begin(), component.value().end());
#else
  std::string component8 = component.AsUTF8Unsafe();
  if (!base::UTF8ToUTF16(component8.c_str(), component8.size(), &component16)) {
    return false;
  }
#endif

  // The names of files in sandboxed file systems are obfuscated before they end
  // up on disk (if they ever end up on disk). We don't need to worry about
  // platform-specific restrictions. More restrictions would need to be added if
  // we ever revisit allowing file moves across the local/sandboxed file system
  // boundary. See https://crbug.com/1408211.
  if (type == storage::kFileSystemTypeTemporary) {
    // Check for both '/' and '\' as path separators, regardless of what OS
    // we're running on.
    return component16 != u"." && component16 != u".." &&
           !base::Contains(component16, '/') &&
           !base::Contains(component16, '\\');
  }

  // base::i18n::IsFilenameLegal blocks names that start with '.', so strip out
  // a leading '.' before passing it to that method.
  // TODO(mek): Consider making IsFilenameLegal more flexible to support this
  // use case.
  if (component16[0] == '.') {
    component16 = component16.substr(1);
  }
  if (!base::i18n::IsFilenameLegal(component16)) {
    return false;
  }

  base::FilePath::StringType extension = component.Extension();
  if (!extension.empty()) {
    extension.erase(extension.begin());  // Erase preceding '.'.
  }

  base::FilePath::StringType extension_lower = base::ToLowerASCII(extension);
  // .lnk and .scf files may be used to execute arbitrary code (see
  // https://nvd.nist.gov/vuln/detail/CVE-2010-2568 and
  // https://crbug.com/1227995, respectively). '.url' files can be used to read
  // arbitrary files (see https://crbug.com/1307930 and
  // https://crbug.com/1354518).
  if (extension_lower == FILE_PATH_LITERAL("lnk") ||
      extension_lower == FILE_PATH_LITERAL("scf") ||
      extension_lower == FILE_PATH_LITERAL("url")) {
    return false;
  }

  // Setting a file's extension to a CLSID may conceal its actual file type on
  // some Windows versions (see https://nvd.nist.gov/vuln/detail/CVE-2004-0420).
  if (!extension_lower.empty() &&
      (extension_lower.front() == FILE_PATH_LITERAL('{')) &&
      (extension_lower.back() == FILE_PATH_LITERAL('}'))) {
    return false;
  }

  // Extensions with `safe_browsing::DownloadFileType::DANGEROUS` type, per
  // components/safe_browsing/content/resources/download_file_types.asciipb,
  // are considered unsafe, with an exception of ".local" extensions.
  if (extension_lower != FILE_PATH_LITERAL("local") && permission_context_ &&
      permission_context_->IsFileTypeDangerous(component, origin)) {
    return false;
  }

  if (base::TrimString(component.value(), FILE_PATH_LITERAL("."),
                       base::TRIM_TRAILING) != component.value()) {
    return false;
  }

  if (net::IsReservedNameOnWindows(component.value())) {
    return false;
  }

  return true;
}

}  // namespace content
