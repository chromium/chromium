// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/mime_util.h"

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_url.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/file_handlers/non_native_file_system_delegate.h"
#endif

namespace extensions::app_file_handler_util {

const char kMimeTypeApplicationOctetStream[] = "application/octet-stream";
const char kMimeTypeInodeDirectory[] = "inode/directory";

namespace {

// Detects MIME type by reading initial bytes from the file. If found, then
// writes the MIME type to |result|.
void SniffMimeType(const base::FilePath& local_path, std::string* result) {
  std::vector<char> content(net::kMaxBytesToSniff);

  const int bytes_read =
      base::ReadFile(local_path, &content[0], static_cast<int>(content.size()));

  if (bytes_read >= 0) {
    net::SniffMimeType(std::string_view(&content[0], bytes_read),
                       net::FilePathToFileURL(local_path),
                       std::string(),  // type_hint (passes no hint)
                       net::ForceSniffFileUrlsForHtml::kDisabled, result);
    if (*result == "text/plain") {
      // text/plain misidentifies AMR files, which look like scripts because
      // they start with "#!". Use SniffMimeTypeFromLocalData to try and get a
      // better match.
      // TODO(amistry): Potentially add other types (i.e. SVG).
      std::string secondary_result;
      net::SniffMimeTypeFromLocalData(std::string_view(&content[0], bytes_read),
                                      &secondary_result);
      if (!secondary_result.empty())
        *result = secondary_result;
    }
  } else if (base::DirectoryExists(local_path)) {
    // XDG defines directories to have mime type inode/directory.
    // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html#idm45070737701600
    *result = kMimeTypeInodeDirectory;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// Converts a result passed as a scoped pointer to a dereferenced value passed
// to |callback|.
void OnGetMimeTypeFromFileForNonNativeLocalPathCompleted(
    std::unique_ptr<std::string> mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(*mime_type);
}

// Called when fetching MIME type for a non-native local path is completed.
// If |success| is false, then tries to guess the MIME type by looking at the
// file name.
void OnGetMimeTypeFromMetadataForNonNativeLocalPathCompleted(
    const base::FilePath& local_path,
    base::OnceCallback<void(const std::string&)> callback,
    const std::optional<std::string>& mime_type) {
  if (mime_type) {
    std::move(callback).Run(mime_type.value());
    return;
  }

  // MIME type not available with metadata, hence try to guess it from the
  // file's extension.
  std::unique_ptr<std::string> mime_type_from_extension(new std::string);
  std::string* const mime_type_from_extension_ptr =
      mime_type_from_extension.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&net::GetMimeTypeFromFile), local_path,
                     mime_type_from_extension_ptr),
      base::BindOnce(&OnGetMimeTypeFromFileForNonNativeLocalPathCompleted,
                     std::move(mime_type_from_extension), std::move(callback)));
}
#endif

// Called when sniffing for MIME type in the native local file is completed.
void OnSniffMimeTypeForNativeLocalPathCompleted(
    std::unique_ptr<std::string> mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  // Do not return application/zip as sniffed result. If the file has .zip
  // extension, it should be already returned as application/zip. If the file
  // does not have .zip extension and couldn't find mime type from the
  // extension, it might be unknown internally zipped file.
  if (*mime_type == "application/zip") {
    std::move(callback).Run(kMimeTypeApplicationOctetStream);
    return;
  }

  std::move(callback).Run(*mime_type);
}

}  // namespace

// Handles response of net::GetMimeTypeFromFile for native file systems. If
// MIME type is available, then forwards it to |callback|. Otherwise, fallbacks
// to sniffing.
void OnGetMimeTypeFromFileForNativeLocalPathCompleted(
    const base::FilePath& local_path,
    std::unique_ptr<std::string> mime_type,
    base::OnceCallback<void(const std::string&)> callback) {
  if (!mime_type->empty()) {
    std::move(callback).Run(*mime_type);
    return;
  }

  std::unique_ptr<std::string> sniffed_mime_type(
      new std::string(kMimeTypeApplicationOctetStream));
  std::string* const sniffed_mime_type_ptr = sniffed_mime_type.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SniffMimeType, local_path, sniffed_mime_type_ptr),
      base::BindOnce(&OnSniffMimeTypeForNativeLocalPathCompleted,
                     std::move(sniffed_mime_type), std::move(callback)));
}

// Fetches MIME type for a local path and returns it with a |callback|.
void GetMimeTypeForLocalPath(
    content::BrowserContext* context,
    const base::FilePath& local_path,
    base::OnceCallback<void(const std::string&)> callback) {
#if BUILDFLAG(IS_CHROMEOS)
  NonNativeFileSystemDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNonNativeFileSystemDelegate();
  if (delegate && delegate->HasNonNativeMimeTypeProvider(context, local_path)) {
    // For non-native files, try to get the MIME type from metadata. If not
    // available, then try to guess from the extension. Never sniff (because
    // it can be very slow).
    delegate->GetNonNativeLocalPathMimeType(
        context, local_path,
        base::BindOnce(&OnGetMimeTypeFromMetadataForNonNativeLocalPathCompleted,
                       local_path, std::move(callback)));
    return;
  }
#endif

  // For native local files, try to guess the mime from the extension. If
  // not available, then try to sniff if.
  std::unique_ptr<std::string> mime_type_from_extension(new std::string);
  std::string* const mime_type_from_extension_ptr =
      mime_type_from_extension.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&net::GetMimeTypeFromFile), local_path,
                     mime_type_from_extension_ptr),
      base::BindOnce(&OnGetMimeTypeFromFileForNativeLocalPathCompleted,
                     local_path, std::move(mime_type_from_extension),
                     std::move(callback)));
}

MimeTypeCollector::MimeTypeCollector(content::BrowserContext* context)
    : context_(context), left_(0) {}

MimeTypeCollector::~MimeTypeCollector() = default;

void MimeTypeCollector::CollectForURLs(
    const std::vector<storage::FileSystemURL>& urls,
    CompletionCallback callback) {
  std::vector<base::FilePath> local_paths;
  for (size_t i = 0; i < urls.size(); ++i) {
    local_paths.push_back(urls[i].path());
  }

  CollectForLocalPaths(local_paths, std::move(callback));
}

void MimeTypeCollector::CollectForLocalPaths(
    const std::vector<base::FilePath>& local_paths,
    CompletionCallback callback) {
  DCHECK(!callback.is_null());
  callback_ = std::move(callback);

  DCHECK(!result_.get());
  result_ = std::make_unique<std::vector<std::string>>(local_paths.size());
  left_ = local_paths.size();

  if (!left_) {
    // Nothing to process.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    callback_.Reset();
    return;
  }

  for (size_t i = 0; i < local_paths.size(); ++i) {
    GetMimeTypeForLocalPath(
        context_, local_paths[i],
        base::BindOnce(&MimeTypeCollector::OnMimeTypeCollected,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }
}

void MimeTypeCollector::OnMimeTypeCollected(size_t index,
                                            const std::string& mime_type) {
  (*result_)[index] = mime_type;
  if (!--left_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(result_)));
    // Release the callback to avoid a circullar reference in case an instance
    // of this class is a member of a ref counted class, which instance is bound
    // to this callback.
    callback_.Reset();
  }
}

}  // namespace extensions::app_file_handler_util
