// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/file_data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/directory_lister.h"
#include "net/base/directory_listing.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/shortcut.h"
#endif

namespace content {
namespace {

constexpr size_t kDefaultFileUrlPipeSize = 65536;

// Because this makes things simpler.
static_assert(kDefaultFileUrlPipeSize >= net::kMaxBytesToSniff,
              "Default file data pipe size must be at least as large as a MIME-"
              "type sniffing buffer.");

// Policy to control how a FileURLLoader will handle directory URLs.
enum class DirectoryLoadingPolicy {
  // File paths which refer to directories are allowed and will load as an
  // HTML directory listing.
  kRespondWithListing,

  // File paths which refer to directories are treated as non-existent and
  // will result in FILE_NOT_FOUND errors.
  kFail,
};

// Policy to control whether or not file access constraints imposed by content
// or its embedder should be honored by a FileURLLoader.
enum class FileAccessPolicy {
  // Enforces file acccess policy defined by content and/or its embedder.
  kRestricted,

  // Ignores file access policy, allowing contents to be loaded from any
  // resolvable file path given.
  kUnrestricted,
};

// Policy to control whether or not a FileURLLoader should follow filesystem
// links (e.g. Windows shortcuts) where applicable.
enum class LinkFollowingPolicy {
  kFollow,
  kDoNotFollow,
};

GURL AppendUrlSeparator(const GURL& url) {
  std::string new_path = url.path() + '/';
  GURL::Replacements replacements;
  replacements.SetPathStr(new_path);
  return url.ReplaceComponents(replacements);
}

bool ShouldFailRequestDueToCORS(const network::ResourceRequest& request) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return false;
  }

  const auto mode = request.fetch_request_mode;
  if (mode == network::mojom::FetchRequestMode::kNavigate ||
      mode == network::mojom::FetchRequestMode::kNoCORS) {
    return false;
  }

  if (!request.request_initiator)
    return true;

  return !request.request_initiator->IsSameOriginWith(
      url::Origin::Create(request.url));
}

class FileURLDirectoryLoader
    : public network::mojom::URLLoader,
      public net::DirectoryLister::DirectoryListerDelegate {
 public:
  static void CreateAndStart(
      const base::FilePath& profile_path,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info,
      std::unique_ptr<FileURLLoaderObserver> observer,
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* file_url_loader = new FileURLDirectoryLoader;
    file_url_loader->Start(profile_path, request, std::move(loader),
                           std::move(client_info), std::move(observer),
                           std::move(response_headers));
  }

  // network::mojom::URLLoader:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override {}
  void ProceedWithResponse() override { NOTREACHED(); }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  FileURLDirectoryLoader() : binding_(this) {}
  ~FileURLDirectoryLoader() override = default;

  void Start(const base::FilePath& profile_path,
             const network::ResourceRequest& request,
             network::mojom::URLLoaderRequest loader,
             network::mojom::URLLoaderClientPtrInfo client_info,
             std::unique_ptr<content::FileURLLoaderObserver> observer,
             scoped_refptr<net::HttpResponseHeaders> response_headers) {
    binding_.Bind(std::move(loader));
    binding_.set_connection_error_handler(base::BindOnce(
        &FileURLDirectoryLoader::OnConnectionError, base::Unretained(this)));

    network::mojom::URLLoaderClientPtr client;
    client.Bind(std::move(client_info));

    if (!net::FileURLToFilePath(request.url, &path_)) {
      client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    base::File::Info info;
    if (!base::GetFileInfo(path_, &info) || !info.is_directory) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FILE_NOT_FOUND));
      return;
    }

    if (!GetContentClient()->browser()->IsFileAccessAllowed(
            path_, base::MakeAbsoluteFilePath(path_), profile_path)) {
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
      return;
    }

    mojo::DataPipe pipe(kDefaultFileUrlPipeSize);
    if (!pipe.consumer_handle.is_valid()) {
      client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    network::ResourceResponseHead head;
    head.mime_type = "text/html";
    head.charset = "utf-8";
    client->OnReceiveResponse(head);
    client->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
    client_ = std::move(client);

    lister_ = std::make_unique<net::DirectoryLister>(path_, this);
    lister_->Start();

    data_producer_ = std::make_unique<mojo::StringDataPipeProducer>(
        std::move(pipe.producer_handle));
  }

  void OnConnectionError() {
    binding_.Close();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!binding_.is_bound() && !client_.is_bound() && !lister_)
      delete this;
  }

  // net::DirectoryLister::DirectoryListerDelegate:
  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override {
    if (!wrote_header_) {
      wrote_header_ = true;

#if defined(OS_WIN)
      const base::string16& title = path_.value();
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      const base::string16& title =
          base::WideToUTF16(base::SysNativeMBToWide(path_.value()));
#endif
      pending_data_.append(net::GetDirectoryListingHeader(title));

      // If not a top-level directory, add a link to the parent directory. To
      // figure this out, first normalize |path_| by stripping trailing
      // separators. Then compare the result to its DirName(). For the top-level
      // directory, e.g. "/" or "c:\\", the normalized path is equal to its own
      // DirName().
      base::FilePath stripped_path = path_.StripTrailingSeparators();
      if (stripped_path != stripped_path.DirName())
        pending_data_.append(net::GetParentDirectoryLink());
    }

    // Skip current / parent links from the listing.
    base::FilePath filename = data.info.GetName();
    if (filename.value() != base::FilePath::kCurrentDirectory &&
        filename.value() != base::FilePath::kParentDirectory) {
#if defined(OS_WIN)
      std::string raw_bytes;  // Empty on Windows means UTF-8 encoded name.
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
      const std::string& raw_bytes = filename.value();
#endif
      pending_data_.append(net::GetDirectoryListingEntry(
          filename.LossyDisplayName(), raw_bytes, data.info.IsDirectory(),
          data.info.GetSize(), data.info.GetLastModifiedTime()));
    }

    MaybeTransferPendingData();
  }

  void OnListDone(int error) override {
    listing_result_ = error;
    lister_.reset();
    MaybeDeleteSelf();
  }

  void MaybeTransferPendingData() {
    if (transfer_in_progress_)
      return;

    transfer_in_progress_ = true;
    data_producer_->Write(pending_data_,
                          mojo::StringDataPipeProducer::AsyncWritingMode::
                              STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION,
                          base::BindOnce(&FileURLDirectoryLoader::OnDataWritten,
                                         base::Unretained(this)));
    // The producer above will have already copied any parts of |pending_data_|
    // that couldn't be written immediately, so we can wipe it out here to begin
    // accumulating more data.
    pending_data_.clear();
  }

  void OnDataWritten(MojoResult result) {
    transfer_in_progress_ = false;

    int completion_status;
    if (result == MOJO_RESULT_OK) {
      if (!pending_data_.empty()) {
        // Keep flushing the data buffer as long as it's non-empty and pipe
        // writes are succeeding.
        MaybeTransferPendingData();
        return;
      }

      // If there's no pending data but the lister is still active, we simply
      // wait for more listing results.
      if (lister_)
        return;

      // At this point we know the listing is complete and all available data
      // has been transferred. We inherit the status of the listing operation.
      completion_status = listing_result_;
    } else {
      completion_status = net::ERR_FAILED;
    }

    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();

    client_->OnComplete(network::URLLoaderCompletionStatus(completion_status));
    client_.reset();

    MaybeDeleteSelf();
  }

  base::FilePath path_;
  std::unique_ptr<net::DirectoryLister> lister_;
  bool wrote_header_ = false;
  int listing_result_;

  mojo::Binding<network::mojom::URLLoader> binding_;
  network::mojom::URLLoaderClientPtr client_;

  std::unique_ptr<mojo::StringDataPipeProducer> data_producer_;
  std::string pending_data_;
  bool transfer_in_progress_ = false;

  DISALLOW_COPY_AND_ASSIGN(FileURLDirectoryLoader);
};

class FileURLLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      const base::FilePath& profile_path,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info,
      DirectoryLoadingPolicy directory_loading_policy,
      FileAccessPolicy file_access_policy,
      LinkFollowingPolicy link_following_policy,
      std::unique_ptr<FileURLLoaderObserver> observer,
      scoped_refptr<net::HttpResponseHeaders> extra_response_headers) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* file_url_loader = new FileURLLoader;
    file_url_loader->Start(
        profile_path, request, std::move(loader), std::move(client_info),
        directory_loading_policy, file_access_policy, link_following_policy,
        std::move(observer), std::move(extra_response_headers));
  }

  // network::mojom::URLLoader:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override {
    std::unique_ptr<RedirectData> redirect_data = std::move(redirect_data_);
    if (redirect_data->is_directory) {
      FileURLDirectoryLoader::CreateAndStart(
          redirect_data->profile_path, redirect_data->request,
          binding_.Unbind(), client_.PassInterface(),
          std::move(redirect_data->observer),
          std::move(redirect_data->extra_response_headers));
    } else {
      FileURLLoader::CreateAndStart(
          redirect_data->profile_path, redirect_data->request,
          binding_.Unbind(), client_.PassInterface(),
          redirect_data->directory_loading_policy,
          redirect_data->file_access_policy,
          redirect_data->link_following_policy,
          std::move(redirect_data->observer),
          std::move(redirect_data->extra_response_headers));
    }
    MaybeDeleteSelf();
  }
  void ProceedWithResponse() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  // Used to save outstanding redirect data while waiting for FollowRedirect
  // to be called. Values default to their most restrictive in case they are
  // not set.
  struct RedirectData {
    bool is_directory = false;
    base::FilePath profile_path;
    network::ResourceRequest request;
    network::mojom::URLLoaderRequest loader;
    DirectoryLoadingPolicy directory_loading_policy =
        DirectoryLoadingPolicy::kFail;
    FileAccessPolicy file_access_policy = FileAccessPolicy::kRestricted;
    LinkFollowingPolicy link_following_policy =
        LinkFollowingPolicy::kDoNotFollow;
    std::unique_ptr<FileURLLoaderObserver> observer;
    scoped_refptr<net::HttpResponseHeaders> extra_response_headers;
  };

  FileURLLoader() : binding_(this) {}
  ~FileURLLoader() override = default;

  void Start(const base::FilePath& profile_path,
             const network::ResourceRequest& request,
             network::mojom::URLLoaderRequest loader,
             network::mojom::URLLoaderClientPtrInfo client_info,
             DirectoryLoadingPolicy directory_loading_policy,
             FileAccessPolicy file_access_policy,
             LinkFollowingPolicy link_following_policy,
             std::unique_ptr<FileURLLoaderObserver> observer,
             scoped_refptr<net::HttpResponseHeaders> extra_response_headers) {
    network::ResourceResponseHead head;
    head.request_start = base::TimeTicks::Now();
    head.response_start = base::TimeTicks::Now();
    head.headers = extra_response_headers;
    binding_.Bind(std::move(loader));
    binding_.set_connection_error_handler(base::BindOnce(
        &FileURLLoader::OnConnectionError, base::Unretained(this)));

    client_.Bind(std::move(client_info));

    base::FilePath path;
    if (!net::FileURLToFilePath(request.url, &path)) {
      OnClientComplete(net::ERR_FAILED, std::move(observer));
      return;
    }

    base::File::Info info;
    if (!base::GetFileInfo(path, &info)) {
      OnClientComplete(net::ERR_FILE_NOT_FOUND, std::move(observer));
      return;
    }

    if (info.is_directory) {
      if (directory_loading_policy == DirectoryLoadingPolicy::kFail) {
        OnClientComplete(net::ERR_FILE_NOT_FOUND, std::move(observer));
        return;
      }

      DCHECK_EQ(directory_loading_policy,
                DirectoryLoadingPolicy::kRespondWithListing);

      net::RedirectInfo redirect_info;
      redirect_info.new_method = "GET";
      redirect_info.status_code = 301;
      redirect_info.new_url = path.EndsWithSeparator()
                                  ? request.url
                                  : AppendUrlSeparator(request.url);
      head.encoded_data_length = 0;

      redirect_data_ = std::make_unique<RedirectData>();
      redirect_data_->is_directory = true;
      redirect_data_->profile_path = std::move(profile_path);
      redirect_data_->request = request;
      redirect_data_->directory_loading_policy = directory_loading_policy;
      redirect_data_->file_access_policy = file_access_policy;
      redirect_data_->link_following_policy = link_following_policy;
      redirect_data_->request.url = redirect_info.new_url;
      redirect_data_->observer = std::move(observer);
      redirect_data_->extra_response_headers =
          std::move(extra_response_headers);

      client_->OnReceiveRedirect(redirect_info, head);
      return;
    }

#if defined(OS_WIN)
    base::FilePath shortcut_target;
    if (link_following_policy == LinkFollowingPolicy::kFollow &&
        base::LowerCaseEqualsASCII(path.Extension(), ".lnk") &&
        base::win::ResolveShortcut(path, &shortcut_target, nullptr)) {
      // Follow Windows shortcuts
      redirect_data_ = std::make_unique<RedirectData>();
      if (!base::GetFileInfo(shortcut_target, &info)) {
        OnClientComplete(net::ERR_FILE_NOT_FOUND, std::move(observer));
        return;
      }

      GURL new_url = net::FilePathToFileURL(shortcut_target);
      if (info.is_directory && !path.EndsWithSeparator())
        new_url = AppendUrlSeparator(new_url);

      net::RedirectInfo redirect_info;
      redirect_info.new_method = "GET";
      redirect_info.status_code = 301;
      redirect_info.new_url = new_url;
      head.encoded_data_length = 0;

      redirect_data_->is_directory = info.is_directory;
      redirect_data_->profile_path = std::move(profile_path);
      redirect_data_->request = request;
      redirect_data_->directory_loading_policy = directory_loading_policy;
      redirect_data_->file_access_policy = file_access_policy;
      redirect_data_->link_following_policy = link_following_policy;
      redirect_data_->request.url = redirect_info.new_url;
      redirect_data_->observer = std::move(observer);
      redirect_data_->extra_response_headers =
          std::move(extra_response_headers);

      client_->OnReceiveRedirect(redirect_info, head);
      return;
    }
#endif  // defined(OS_WIN)

    if (file_access_policy == FileAccessPolicy::kRestricted &&
        !GetContentClient()->browser()->IsFileAccessAllowed(
            path, base::MakeAbsoluteFilePath(path), profile_path)) {
      OnClientComplete(net::ERR_ACCESS_DENIED, std::move(observer));
      return;
    }

    mojo::DataPipe pipe(kDefaultFileUrlPipeSize);
    if (!pipe.consumer_handle.is_valid()) {
      OnClientComplete(net::ERR_FAILED, std::move(observer));
      return;
    }

    // Should never be possible for this to be a directory. If FileURLLoader
    // is used to respond to a directory request, it must be because the URL
    // path didn't have a trailing path separator. In that case we finish with
    // a redirect above which will in turn be handled by FileURLDirectoryLoader.
    DCHECK(!info.is_directory);
    if (observer)
      observer->OnStart();

    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file.IsValid()) {
      if (observer) {
        observer->OnBytesRead(nullptr, 0u, file.error_details());
        observer->OnDoneReading();
      }
      net::Error net_error = net::FileErrorToNetError(file.error_details());
      client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
      client_.reset();
      MaybeDeleteSelf();
      return;
    }
    char initial_read_buffer[net::kMaxBytesToSniff];
    int initial_read_result =
        file.ReadAtCurrentPos(initial_read_buffer, net::kMaxBytesToSniff);
    if (initial_read_result < 0) {
      base::File::Error read_error = base::File::GetLastFileError();
      DCHECK_NE(base::File::FILE_OK, read_error);
      if (observer) {
        // This can happen when the file is unreadable (which can happen during
        // corruption). We need to be sure to inform
        // the observer that we've finished reading so that it can proceed.
        observer->OnBytesRead(nullptr, 0u, read_error);
        observer->OnDoneReading();
      }
      net::Error net_error = net::FileErrorToNetError(read_error);
      client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
      client_.reset();
      MaybeDeleteSelf();
      return;
    } else if (observer) {
      observer->OnBytesRead(initial_read_buffer, initial_read_result,
                            base::File::FILE_OK);
    }
    size_t initial_read_size = static_cast<size_t>(initial_read_result);

    std::string range_header;
    net::HttpByteRange byte_range;
    if (request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                  &range_header)) {
      // Handle a simple Range header for a single range.
      std::vector<net::HttpByteRange> ranges;
      bool fail = false;
      if (net::HttpUtil::ParseRangeHeader(range_header, &ranges) &&
          ranges.size() == 1) {
        byte_range = ranges[0];
        if (!byte_range.ComputeBounds(info.size))
          fail = true;
      } else {
        fail = true;
      }

      if (fail) {
        OnClientComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
                         std::move(observer));
        return;
      }
    }

    size_t first_byte_to_send = 0;
    size_t total_bytes_to_send = static_cast<size_t>(info.size);

    if (byte_range.IsValid()) {
      first_byte_to_send =
          static_cast<size_t>(byte_range.first_byte_position());
      total_bytes_to_send =
          static_cast<size_t>(byte_range.last_byte_position()) -
          first_byte_to_send + 1;
    }

    total_bytes_written_ = static_cast<size_t>(total_bytes_to_send);

    head.content_length = base::saturated_cast<int64_t>(total_bytes_to_send);

    if (first_byte_to_send < initial_read_size) {
      // Write any data we read for MIME sniffing, constraining by range where
      // applicable. This will always fit in the pipe (see assertion near
      // |kDefaultFileUrlPipeSize| definition).
      uint32_t write_size = std::min(
          static_cast<uint32_t>(initial_read_size - first_byte_to_send),
          static_cast<uint32_t>(total_bytes_to_send));
      const uint32_t expected_write_size = write_size;
      MojoResult result = pipe.producer_handle->WriteData(
          &initial_read_buffer[first_byte_to_send], &write_size,
          MOJO_WRITE_DATA_FLAG_NONE);
      if (result != MOJO_RESULT_OK || write_size != expected_write_size) {
        OnFileWritten(std::move(observer), result);
        return;
      }

      // Discount the bytes we just sent from the total range.
      first_byte_to_send = initial_read_size;
      total_bytes_to_send -= write_size;
    }

    if (!net::GetMimeTypeFromFile(path, &head.mime_type)) {
      net::SniffMimeType(
          initial_read_buffer, initial_read_result, request.url, head.mime_type,
          GetContentClient()->browser()->ForceSniffingFileUrlsForHtml()
              ? net::ForceSniffFileUrlsForHtml::kEnabled
              : net::ForceSniffFileUrlsForHtml::kDisabled,
          &head.mime_type);
      head.did_mime_sniff = true;
    }
    if (head.headers) {
      head.headers->AddHeader(
          base::StringPrintf("%s: %s", net::HttpRequestHeaders::kContentType,
                             head.mime_type.c_str()));
    }
    client_->OnReceiveResponse(head);
    client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));

    if (total_bytes_to_send == 0) {
      // There's definitely no more data, so we're already done.
      OnFileWritten(std::move(observer), MOJO_RESULT_OK);
      return;
    }

    // In case of a range request, seek to the appropriate position before
    // sending the remaining bytes asynchronously. Under normal conditions
    // (i.e., no range request) this Seek is effectively a no-op.
    int new_position = file.Seek(base::File::FROM_BEGIN,
                                 static_cast<int64_t>(first_byte_to_send));
    if (observer)
      observer->OnSeekComplete(new_position);

    data_producer_ = std::make_unique<mojo::FileDataPipeProducer>(
        std::move(pipe.producer_handle), std::move(observer));
    data_producer_->WriteFromFile(
        std::move(file), total_bytes_to_send,
        base::BindOnce(&FileURLLoader::OnFileWritten, base::Unretained(this),
                       nullptr));
  }

  void OnConnectionError() {
    binding_.Close();
    MaybeDeleteSelf();
  }

  void OnClientComplete(net::Error net_error,
                        std::unique_ptr<FileURLLoaderObserver> observer) {
    client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    client_.reset();
    if (observer)
      observer->OnDoneReading();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!binding_.is_bound() && !client_.is_bound())
      delete this;
  }

  void OnFileWritten(std::unique_ptr<FileURLLoaderObserver> observer,
                     MojoResult result) {
    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();
    if (observer)
      observer->OnDoneReading();

    if (result == MOJO_RESULT_OK) {
      network::URLLoaderCompletionStatus status(net::OK);
      status.encoded_data_length = total_bytes_written_;
      status.encoded_body_length = total_bytes_written_;
      status.decoded_body_length = total_bytes_written_;
      client_->OnComplete(status);
    } else {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
    }
    client_.reset();
    MaybeDeleteSelf();
  }

  std::unique_ptr<mojo::FileDataPipeProducer> data_producer_;
  mojo::Binding<network::mojom::URLLoader> binding_;
  network::mojom::URLLoaderClientPtr client_;
  std::unique_ptr<RedirectData> redirect_data_;

  // In case of successful loads, this holds the total number of bytes written
  // to the response (this may be smaller than the total size of the file when
  // a byte range was requested).
  // It is used to set some of the URLLoaderCompletionStatus data passed back
  // to the URLLoaderClients (eg SimpleURLLoader).
  size_t total_bytes_written_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FileURLLoader);
};

}  // namespace

FileURLLoaderFactory::FileURLLoaderFactory(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : profile_path_(profile_path), task_runner_(std::move(task_runner)) {}

FileURLLoaderFactory::~FileURLLoaderFactory() = default;

void FileURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  base::FilePath file_path;
  if (!net::FileURLToFilePath(request.url, &file_path)) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }

  // FileURLLoader doesn't support CORS and it's not covered by CORSURLLoader,
  // so we need to reject requests that need CORS manually.
  if (ShouldFailRequestDueToCORS(request)) {
    client->OnComplete(
        network::URLLoaderCompletionStatus(network::CORSErrorStatus(
            network::mojom::CORSError::kCORSDisabledScheme)));
    return;
  }

  if (file_path.EndsWithSeparator() && file_path.IsAbsolute()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileURLDirectoryLoader::CreateAndStart, profile_path_,
                       request, std::move(loader), client.PassInterface(),
                       std::unique_ptr<FileURLLoaderObserver>(), nullptr));
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileURLLoader::CreateAndStart, profile_path_, request,
                       std::move(loader), client.PassInterface(),
                       DirectoryLoadingPolicy::kRespondWithListing,
                       FileAccessPolicy::kRestricted,
                       LinkFollowingPolicy::kFollow,
                       std::unique_ptr<FileURLLoaderObserver>(),
                       nullptr /* extra_response_headers */));
  }
}

void FileURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader) {
  bindings_.AddBinding(this, std::move(loader));
}

void CreateFileURLLoader(
    const network::ResourceRequest& request,
    network::mojom::URLLoaderRequest loader,
    network::mojom::URLLoaderClientPtr client,
    std::unique_ptr<FileURLLoaderObserver> observer,
    scoped_refptr<net::HttpResponseHeaders> extra_response_headers) {
  auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&FileURLLoader::CreateAndStart, base::FilePath(), request,
                     std::move(loader), client.PassInterface(),
                     DirectoryLoadingPolicy::kFail,
                     FileAccessPolicy::kUnrestricted,
                     LinkFollowingPolicy::kDoNotFollow, std::move(observer),
                     std::move(extra_response_headers)));
}

std::unique_ptr<network::mojom::URLLoaderFactory> CreateFileURLLoaderFactory(
    const base::FilePath& profile_path) {
  return std::make_unique<content::FileURLLoaderFactory>(
      profile_path, base::CreateSequencedTaskRunnerWithTraits(
                        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}

}  // namespace content
