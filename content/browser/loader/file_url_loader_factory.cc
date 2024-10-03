// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/file_url_loader_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/directory_lister.h"
#include "net/base/directory_listing.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "storage/common/file_system/file_system_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/shortcut.h"
#endif

namespace content {
namespace {

constexpr size_t kDefaultFileDirectoryLoaderPipeSize = 65536;

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

net::Error ConvertMojoResultToNetError(MojoResult result) {
  switch (result) {
    case MOJO_RESULT_OK:
      return net::OK;
    case MOJO_RESULT_NOT_FOUND:
      return net::ERR_FILE_NOT_FOUND;
    case MOJO_RESULT_PERMISSION_DENIED:
      return net::ERR_ACCESS_DENIED;
    case MOJO_RESULT_RESOURCE_EXHAUSTED:
      return net::ERR_INSUFFICIENT_RESOURCES;
    case MOJO_RESULT_ABORTED:
      return net::ERR_ABORTED;
    default:
      return net::ERR_FAILED;
  }
}

MojoResult ConvertNetErrorToMojoResult(net::Error net_error) {
  // Note: For now, only return specific errors that our obervers care about.
  switch (net_error) {
    case net::OK:
      return MOJO_RESULT_OK;
    case net::ERR_FILE_NOT_FOUND:
      return MOJO_RESULT_NOT_FOUND;
    case net::ERR_ACCESS_DENIED:
      return MOJO_RESULT_PERMISSION_DENIED;
    case net::ERR_INSUFFICIENT_RESOURCES:
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    case net::ERR_ABORTED:
    case net::ERR_CONNECTION_ABORTED:
      return MOJO_RESULT_ABORTED;
    default:
      return MOJO_RESULT_UNKNOWN;
  }
}

class FileURLDirectoryLoader
    : public network::mojom::URLLoader,
      public net::DirectoryLister::DirectoryListerDelegate {
 public:
  static void CreateAndStart(
      const base::FilePath& profile_path,
      const network::ResourceRequest& request,
      network::mojom::FetchResponseType response_type,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
      std::unique_ptr<FileURLLoaderObserver> observer,
      scoped_refptr<net::HttpResponseHeaders> response_headers) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClient
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* file_url_loader = new FileURLDirectoryLoader;
    file_url_loader->Start(profile_path, request, response_type,
                           std::move(loader), std::move(client_remote),
                           std::move(observer), std::move(response_headers));
  }

  FileURLDirectoryLoader(const FileURLDirectoryLoader&) = delete;
  FileURLDirectoryLoader& operator=(const FileURLDirectoryLoader&) = delete;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  FileURLDirectoryLoader() = default;
  ~FileURLDirectoryLoader() override = default;

  void Start(const base::FilePath& profile_path,
             const network::ResourceRequest& request,
             network::mojom::FetchResponseType response_type,
             mojo::PendingReceiver<network::mojom::URLLoader> loader,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
             std::unique_ptr<FileURLLoaderObserver> observer,
             scoped_refptr<net::HttpResponseHeaders> response_headers) {
    receiver_.Bind(std::move(loader));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FileURLDirectoryLoader::OnMojoDisconnect, base::Unretained(this)));

    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(client_remote));

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

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    if (mojo::CreateDataPipe(kDefaultFileDirectoryLoaderPipeSize,
                             producer_handle,
                             consumer_handle) != MOJO_RESULT_OK) {
      client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }

    auto head = network::mojom::URLResponseHead::New();
    head->mime_type = "text/html";
    head->charset = "utf-8";
    head->response_type = response_type;
    client->OnReceiveResponse(std::move(head), std::move(consumer_handle),
                              std::nullopt);
    client_ = std::move(client);

    lister_ = std::make_unique<net::DirectoryLister>(path_, this);
    lister_->Start();

    data_producer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));

    const std::u16string& title = path_.AsUTF16Unsafe();
    pending_data_.append(net::GetDirectoryListingHeader(title));

    // If not a top-level directory, add a link to the parent directory. To
    // figure this out, first normalize |path_| by stripping trailing
    // separators. Then compare the result to its DirName(). For the top-level
    // directory, e.g. "/" or "c:\\", the normalized path is equal to its own
    // DirName().
    base::FilePath stripped_path = path_.StripTrailingSeparators();
    if (stripped_path != stripped_path.DirName()) {
      pending_data_.append(net::GetParentDirectoryLink());
    }
    MaybeTransferPendingData();
  }

  void OnMojoDisconnect() {
    lister_.reset();
    data_producer_.reset();
    receiver_.reset();
    client_.reset();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!receiver_.is_bound() && !client_.is_bound() && !lister_) {
      delete this;
    }
  }

  // net::DirectoryLister::DirectoryListerDelegate:
  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override {
    // Skip current / parent links from the listing.
    base::FilePath filename = data.info.GetName();
    if (filename.value() != base::FilePath::kCurrentDirectory &&
        filename.value() != base::FilePath::kParentDirectory) {
#if BUILDFLAG(IS_WIN)
      std::string raw_bytes;  // Empty on Windows means UTF-8 encoded name.
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
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
    if (!pending_data_.empty()) {
      MaybeTransferPendingData();
    } else {
      MaybeDeleteSelf();
    }
  }

  void MaybeTransferPendingData() {
    if (transfer_in_progress_) {
      return;
    }

    transfer_in_progress_ = true;
    data_producer_->Write(
        std::make_unique<mojo::StringDataSource>(
            pending_data_, mojo::StringDataSource::AsyncWritingMode::
                               STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(&FileURLDirectoryLoader::OnDataWritten,
                       base::Unretained(this)));
    // The producer above will have already copied any parts of |pending_data_|
    // that couldn't be written immediately, so we can wipe it out here to begin
    // accumulating more data.
    total_bytes_written_ += pending_data_.size();
    pending_data_.clear();
  }

  void OnDataWritten(MojoResult result) {
    transfer_in_progress_ = false;

    int status;
    if (result == MOJO_RESULT_OK) {
      if (!pending_data_.empty()) {
        // Keep flushing the data buffer as long as it's non-empty and pipe
        // writes are succeeding.
        MaybeTransferPendingData();
        return;
      }

      // If there's no pending data but the lister is still active, we simply
      // wait for more listing results.
      if (lister_) {
        return;
      }

      // At this point we know the listing is complete and all available data
      // has been transferred. We inherit the status of the listing operation.
      status = listing_result_;
    } else {
      status = net::ERR_FAILED;
    }

    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();

    network::URLLoaderCompletionStatus completion_status(status);
    completion_status.encoded_data_length = total_bytes_written_;
    completion_status.encoded_body_length = total_bytes_written_;
    completion_status.decoded_body_length = total_bytes_written_;

    client_->OnComplete(completion_status);
    client_.reset();

    MaybeDeleteSelf();
  }

  base::FilePath path_;
  std::unique_ptr<net::DirectoryLister> lister_;
  int listing_result_;

  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;

  std::unique_ptr<mojo::DataPipeProducer> data_producer_;
  std::string pending_data_;
  bool transfer_in_progress_ = false;

  // In case of successful loads, this holds the total number of bytes written
  // to the response. It is used to set some of the URLLoaderCompletionStatus
  // data passed back to the URLLoaderClients (eg SimpleURLLoader).
  uint64_t total_bytes_written_ = 0;
};

class FileURLLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      const base::FilePath& profile_path,
      const network::ResourceRequest& request,
      network::mojom::FetchResponseType response_type,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
      DirectoryLoadingPolicy directory_loading_policy,
      FileAccessPolicy file_access_policy,
      LinkFollowingPolicy link_following_policy,
      std::unique_ptr<FileURLLoaderObserver> observer,
      scoped_refptr<net::HttpResponseHeaders> extra_response_headers,
      file_access::ScopedFileAccess file_access) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClient
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* file_url_loader = new FileURLLoader;
    file_url_loader->Start(
        profile_path, request, response_type, std::move(loader),
        std::move(client_remote), directory_loading_policy, file_access_policy,
        link_following_policy, std::move(observer),
        std::move(extra_response_headers), std::move(file_access));
  }

  FileURLLoader(const FileURLLoader&) = delete;
  FileURLLoader& operator=(const FileURLLoader&) = delete;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    // |removed_headers| and |modified_headers| are unused. It doesn't make
    // sense for files. The FileURLLoader can redirect only to another file.
    std::unique_ptr<RedirectData> redirect_data = std::move(redirect_data_);
    if (redirect_data->is_directory) {
      FileURLDirectoryLoader::CreateAndStart(
          redirect_data->profile_path, redirect_data->request,
          redirect_data->response_type, receiver_.Unbind(), client_.Unbind(),
          std::move(redirect_data->observer),
          std::move(redirect_data->extra_response_headers));
    } else {
      FileURLLoader::CreateAndStart(
          redirect_data->profile_path, redirect_data->request,
          redirect_data->response_type, receiver_.Unbind(), client_.Unbind(),
          redirect_data->directory_loading_policy,
          redirect_data->file_access_policy,
          redirect_data->link_following_policy,
          std::move(redirect_data->observer),
          std::move(redirect_data->extra_response_headers),
          std::move(*redirect_data->file_access.release()));
    }
    MaybeDeleteSelf();
  }
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
    network::mojom::FetchResponseType response_type;
    mojo::PendingReceiver<network::mojom::URLLoader> loader;
    DirectoryLoadingPolicy directory_loading_policy =
        DirectoryLoadingPolicy::kFail;
    FileAccessPolicy file_access_policy = FileAccessPolicy::kRestricted;
    LinkFollowingPolicy link_following_policy =
        LinkFollowingPolicy::kDoNotFollow;
    std::unique_ptr<FileURLLoaderObserver> observer;
    scoped_refptr<net::HttpResponseHeaders> extra_response_headers;
    std::unique_ptr<file_access::ScopedFileAccess> file_access;
  };

  FileURLLoader() = default;
  ~FileURLLoader() override = default;

  void Start(const base::FilePath& profile_path,
             const network::ResourceRequest& request,
             network::mojom::FetchResponseType response_type,
             mojo::PendingReceiver<network::mojom::URLLoader> loader,
             mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
             DirectoryLoadingPolicy directory_loading_policy,
             FileAccessPolicy file_access_policy,
             LinkFollowingPolicy link_following_policy,
             std::unique_ptr<FileURLLoaderObserver> observer,
             scoped_refptr<net::HttpResponseHeaders> extra_response_headers,
             file_access::ScopedFileAccess file_access) {
    // ClusterFuzz depends on the following VLOG to get resource dependencies.
    // See crbug.com/715656.
    VLOG(1) << "FileURLLoader::Start: " << request.url;

    auto head = network::mojom::URLResponseHead::New();
    head->request_start = base::TimeTicks::Now();
    head->response_start = base::TimeTicks::Now();
    head->response_type = response_type;
    head->headers = extra_response_headers;
    receiver_.Bind(std::move(loader));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FileURLLoader::OnMojoDisconnect, base::Unretained(this)));

    client_.Bind(std::move(client_remote));

    if (!file_access.is_allowed()) {
      OnClientComplete(net::ERR_FAILED, std::move(observer));
      return;
    }

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
      head->encoded_data_length = 0;

      redirect_data_ = std::make_unique<RedirectData>();
      redirect_data_->is_directory = true;
      redirect_data_->profile_path = std::move(profile_path);
      redirect_data_->request = request;
      redirect_data_->directory_loading_policy = directory_loading_policy;
      redirect_data_->file_access_policy = file_access_policy;
      redirect_data_->link_following_policy = link_following_policy;
      redirect_data_->request.url = redirect_info.new_url;
      redirect_data_->observer = std::move(observer);
      redirect_data_->response_type = response_type;
      redirect_data_->extra_response_headers =
          std::move(extra_response_headers);
      redirect_data_->file_access =
          std::make_unique<file_access::ScopedFileAccess>(
              std::move(file_access));

      client_->OnReceiveRedirect(redirect_info, std::move(head));
      return;
    }

    // Full file path with all symbolic links resolved.
    base::FilePath full_path = base::MakeAbsoluteFilePath(path);
    if (file_access_policy == FileAccessPolicy::kRestricted &&
        !GetContentClient()->browser()->IsFileAccessAllowed(path, full_path,
                                                            profile_path)) {
      OnClientComplete(net::ERR_ACCESS_DENIED, std::move(observer));
      return;
    }

#if BUILDFLAG(IS_WIN)
    base::FilePath shortcut_target;
    if (link_following_policy == LinkFollowingPolicy::kFollow &&
        base::EqualsCaseInsensitiveASCII(path.Extension(), ".lnk") &&
        base::win::ResolveShortcut(path, &shortcut_target, nullptr)) {
      // Follow Windows shortcuts
      redirect_data_ = std::make_unique<RedirectData>();
      if (!base::GetFileInfo(shortcut_target, &info)) {
        OnClientComplete(net::ERR_FILE_NOT_FOUND, std::move(observer));
        return;
      }

      GURL new_url = net::FilePathToFileURL(shortcut_target);
      if (info.is_directory && !path.EndsWithSeparator()) {
        new_url = AppendUrlSeparator(new_url);
      }

      net::RedirectInfo redirect_info;
      redirect_info.new_method = "GET";
      redirect_info.status_code = 301;
      redirect_info.new_url = new_url;
      head->encoded_data_length = 0;

      redirect_data_->is_directory = info.is_directory;
      redirect_data_->profile_path = std::move(profile_path);
      redirect_data_->request = request;
      redirect_data_->response_type = response_type;
      redirect_data_->directory_loading_policy = directory_loading_policy;
      redirect_data_->file_access_policy = file_access_policy;
      redirect_data_->link_following_policy = link_following_policy;
      redirect_data_->request.url = redirect_info.new_url;
      redirect_data_->observer = std::move(observer);
      redirect_data_->extra_response_headers =
          std::move(extra_response_headers);
      redirect_data_->file_access =
          std::make_unique<file_access::ScopedFileAccess>(
              std::move(file_access));

      client_->OnReceiveRedirect(redirect_info, std::move(head));
      return;
    }
#endif  // BUILDFLAG(IS_WIN)

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;

    // Request the larger size data pipe for file:// URL loading.
    uint32_t data_pipe_size =
        network::features::GetDataPipeDefaultAllocationSize(
            network::features::DataPipeAllocationSize::kLargerSizeIfPossible);
    // This should already be static_asserted in network::features, but good
    // to double-check.
    DCHECK(data_pipe_size >= net::kMaxBytesToSniff)
        << "Default file data pipe size must be at least as large as a "
           "MIME-type sniffing buffer.";

    if (mojo::CreateDataPipe(data_pipe_size, producer_handle,
                             consumer_handle) != MOJO_RESULT_OK) {
      OnClientComplete(net::ERR_FAILED, std::move(observer));
      return;
    }

    // Should never be possible for this to be a directory. If FileURLLoader
    // is used to respond to a directory request, it must be because the URL
    // path didn't have a trailing path separator. In that case we finish with
    // a redirect above which will in turn be handled by FileURLDirectoryLoader.
    DCHECK(!info.is_directory);
    if (observer) {
      observer->OnStart();
    }

    auto file_data_source = std::make_unique<mojo::FileDataSource>(
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));

    std::vector<char> initial_read_buffer(net::kMaxBytesToSniff);
    auto read_result =
        file_data_source->Read(0u, base::span<char>(initial_read_buffer));
    if (read_result.result != MOJO_RESULT_OK) {
      // This can happen when the file is unreadable (which can happen during
      // corruption). We need to be sure to inform the observer that we've
      // finished reading so that it can proceed.
      if (observer) {
        observer->OnRead(base::span<char>(), &read_result);
        observer->OnDone();
      }
      client_->OnComplete(network::URLLoaderCompletionStatus(
          ConvertMojoResultToNetError(read_result.result)));
      client_.reset();
      MaybeDeleteSelf();
      return;
    }
    if (observer) {
      observer->OnRead(base::span<char>(initial_read_buffer), &read_result);
    }

    uint64_t initial_read_size = read_result.bytes_read;

    net::HttpByteRange byte_range;
    if (std::optional<std::string> range_header =
            request.headers.GetHeader(net::HttpRequestHeaders::kRange);
        range_header) {
      // Handle a simple Range header for a single range.
      std::vector<net::HttpByteRange> ranges;
      bool fail = false;
      if (net::HttpUtil::ParseRangeHeader(*range_header, &ranges) &&
          ranges.size() == 1) {
        byte_range = ranges[0];
        if (!byte_range.ComputeBounds(info.size)) {
          fail = true;
        }
      } else {
        fail = true;
      }

      if (fail) {
        OnClientComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
                         std::move(observer));
        return;
      }
    }

    uint64_t first_byte_to_send = 0;
    uint64_t total_bytes_to_send = info.size;

    if (byte_range.IsValid()) {
      first_byte_to_send = byte_range.first_byte_position();
      total_bytes_to_send =
          byte_range.last_byte_position() - first_byte_to_send + 1;
    }

    total_bytes_written_ = total_bytes_to_send;

    head->content_length = base::saturated_cast<int64_t>(total_bytes_to_send);

    if (first_byte_to_send < initial_read_size) {
      // Write any data we read for MIME sniffing, constraining by range where
      // applicable. This will always fit in the pipe (see DCHECK above, and
      // assertions near network::features::GetDataPipeDefaultAllocationSize()).
      base::span<const uint8_t> bytes_to_write =
          base::as_byte_span(initial_read_buffer).subspan(first_byte_to_send);
      bytes_to_write = bytes_to_write.first(
          std::min(bytes_to_write.size(),
                   base::checked_cast<size_t>(total_bytes_to_send)));
      size_t actually_written_bytes = 0;
      MojoResult result = producer_handle->WriteData(
          bytes_to_write, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
      if (result != MOJO_RESULT_OK ||
          actually_written_bytes != bytes_to_write.size()) {
        OnFileWritten(std::move(observer), result);
        return;
      }

      // Discount the bytes we just sent from the total range.
      first_byte_to_send = initial_read_size;
      total_bytes_to_send -= actually_written_bytes;
    }

    if (!net::GetMimeTypeFromFile(full_path, &head->mime_type)) {
      std::string new_type;
      net::SniffMimeType(
          std::string_view(initial_read_buffer.data(), read_result.bytes_read),
          request.url, head->mime_type,
          GetContentClient()->browser()->ForceSniffingFileUrlsForHtml()
              ? net::ForceSniffFileUrlsForHtml::kEnabled
              : net::ForceSniffFileUrlsForHtml::kDisabled,
          &new_type);
      head->mime_type.assign(new_type);
      head->did_mime_sniff = true;
    }
    if (!head->headers) {
      head->headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    }
    head->headers->AddHeader(net::HttpRequestHeaders::kContentType,
                             head->mime_type);
    // We add a Last-Modified header to file responses so that our
    // implementation of document.lastModified can access it (crbug.com/875299).
    head->headers->AddHeader(net::HttpResponseHeaders::kLastModified,
                             base::TimeFormatHTTP(info.last_modified));
    client_->OnReceiveResponse(std::move(head), std::move(consumer_handle),
                               std::nullopt);

    if (total_bytes_to_send == 0) {
      // There's definitely no more data, so we're already done.
      OnFileWritten(std::move(observer), MOJO_RESULT_OK);
      return;
    }

    // In case of a range request, seek to the appropriate position before
    // sending the remaining bytes asynchronously. Under normal conditions
    // (i.e., no range request) this Seek is effectively a no-op.
    file_data_source->SetRange(first_byte_to_send,
                               first_byte_to_send + total_bytes_to_send);
    if (observer) {
      observer->OnSeekComplete(first_byte_to_send);
    }

    data_producer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    data_producer_->Write(std::make_unique<mojo::FilteredDataSource>(
                              std::move(file_data_source), std::move(observer)),
                          base::BindOnce(&FileURLLoader::OnFileWritten,
                                         base::Unretained(this), nullptr));
  }

  void OnMojoDisconnect() {
    data_producer_.reset();
    receiver_.reset();
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnClientComplete(net::Error net_error,
                        std::unique_ptr<FileURLLoaderObserver> observer) {
    client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    client_.reset();
    if (observer) {
      if (net_error != net::OK) {
        mojo::DataPipeProducer::DataSource::ReadResult result;
        result.result = ConvertNetErrorToMojoResult(net_error);
        observer->OnRead(base::span<char>(), &result);
      }
      observer->OnDone();
    }
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!receiver_.is_bound() && !client_.is_bound()) {
      delete this;
    }
  }

  void OnFileWritten(std::unique_ptr<FileURLLoaderObserver> observer,
                     MojoResult result) {
    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();
    if (observer) {
      observer->OnDone();
    }

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

  std::unique_ptr<mojo::DataPipeProducer> data_producer_;
  mojo::Receiver<network::mojom::URLLoader> receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  std::unique_ptr<RedirectData> redirect_data_;

  // In case of successful loads, this holds the total number of bytes written
  // to the response (this may be smaller than the total size of the file when
  // a byte range was requested).
  // It is used to set some of the URLLoaderCompletionStatus data passed back
  // to the URLLoaderClients (eg SimpleURLLoader).
  uint64_t total_bytes_written_ = 0;
};

}  // namespace

FileURLLoaderFactory::FileURLLoaderFactory(
    const base::FilePath& profile_path,
    scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list,
    base::TaskPriority task_priority,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
    : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
      profile_path_(profile_path),
      shared_cors_origin_access_list_(
          std::move(shared_cors_origin_access_list)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), task_priority,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

FileURLLoaderFactory::~FileURLLoaderFactory() = default;

void FileURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // CORS mode requires a valid |request_initiator|.
  if (network::cors::IsCorsEnabledRequestMode(request.mode) &&
      !request.request_initiator) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  // |mode| should be kNoCors for the case of |shared_cors_origin_access_list_|
  // being nullptr. Only internal call sites, such as ExtensionDownloader, is
  // permitted to specify nullptr.
  DCHECK(!network::cors::IsCorsEnabledRequestMode(request.mode) ||
         shared_cors_origin_access_list_);

  // If kDisableWebSecurity flag is specified, make all requests pretend as
  // "no-cors" requests. Otherwise, call IsSameOriginWith for a file scheme
  // check that takes --allow-file-access-from-files into account.
  // CORS is not available for the file scheme, but can be exceptionally
  // permitted by the access lists.
  bool is_request_considered_same_origin =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity) ||
      (request.request_initiator &&
       (request.request_initiator->IsSameOriginWith(request.url) ||
        (shared_cors_origin_access_list_ &&
         shared_cors_origin_access_list_->GetOriginAccessList()
                 .CheckAccessState(*request.request_initiator, request.url) ==
             network::cors::OriginAccessList::AccessState::kAllowed)));

  // TODO(toyoshim, lukasza): https://crbug.com/1105256: Extract CORS checks
  // into a separate base class (i.e. to reuse similar checks in
  // FileURLLoaderFactory and ExtensionURLLoaderFactory.
  network::mojom::FetchResponseType response_type =
      network::cors::CalculateResponseType(request.mode,
                                           is_request_considered_same_origin);

  CreateLoaderAndStartInternal(request, response_type, std::move(loader),
                               std::move(client));
}

void FileURLLoaderFactory::CreateLoaderAndStartInternal(
    const network::ResourceRequest request,
    network::mojom::FetchResponseType response_type,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (response_type == network::mojom::FetchResponseType::kCors) {
    // FileURLLoader doesn't support CORS and it's not covered by CorsURLLoader,
    // so we need to reject requests that need CORS manually.
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            network::URLLoaderCompletionStatus(network::CorsErrorStatus(
                network::mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  // Check file path just after all CORS flag checks are handled.
  base::FilePath file_path;
  if (!net::FileURLToFilePath(request.url, &file_path)) {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }

  if (file_path.EndsWithSeparator() && file_path.IsAbsolute()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileURLDirectoryLoader::CreateAndStart, profile_path_,
                       request, response_type, std::move(loader),
                       std::move(client),
                       std::unique_ptr<FileURLLoaderObserver>(), nullptr));
  } else {
    auto cb = base::BindPostTask(
        task_runner_,
        base::BindOnce(&FileURLLoader::CreateAndStart, profile_path_, request,
                       response_type, std::move(loader), std::move(client),
                       DirectoryLoadingPolicy::kRespondWithListing,
                       FileAccessPolicy::kRestricted,
                       LinkFollowingPolicy::kFollow,
                       std::unique_ptr<FileURLLoaderObserver>(),
                       nullptr /* extra_response_headers */));
    if (auto* file_access = file_access::ScopedFileAccessDelegate::Get()) {
      // If the request has an initiator use it as source for the dlp check.
      // Requests with no initiator e.g. user actions the request should be
      // granted.
      if (request.request_initiator && !request.request_initiator->opaque()) {
        file_access->RequestFilesAccess(
            {file_path}, request.request_initiator->GetURL(), std::move(cb));
      } else {
        file_access->RequestFilesAccessForSystem({file_path}, std::move(cb));
      }
    } else {
      std::move(cb).Run(file_access::ScopedFileAccess::Allowed());
    }
  }
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
FileURLLoaderFactory::Create(
    const base::FilePath& profile_path,
    scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list,
    base::TaskPriority task_priority) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The FileURLLoaderFactory will delete itself when there are no more
  // receivers - see the network::SelfDeletingURLLoaderFactory::OnDisconnect
  // method.
  new FileURLLoaderFactory(
      profile_path, std::move(shared_cors_origin_access_list), task_priority,
      pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

void CreateFileURLLoaderBypassingSecurityChecks(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    std::unique_ptr<FileURLLoaderObserver> observer,
    bool allow_directory_listing,
    scoped_refptr<net::HttpResponseHeaders> extra_response_headers) {
  // TODO(crbug.com/41436919): Re-evaluate how TaskPriority is set here and in
  // other file URL-loading-related code. Some callers require USER_VISIBLE
  // (i.e., BEST_EFFORT is not enough).
  base::FilePath path;
  if (net::FileURLToFilePath(request.url, &path)) {
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    auto create_and_start_cb = base::BindPostTask(
        task_runner,
        base::BindOnce(&FileURLLoader::CreateAndStart, base::FilePath(),
                       request, network::mojom::FetchResponseType::kBasic,
                       std::move(loader), std::move(client),
                       allow_directory_listing
                           ? DirectoryLoadingPolicy::kRespondWithListing
                           : DirectoryLoadingPolicy::kFail,
                       FileAccessPolicy::kUnrestricted,
                       LinkFollowingPolicy::kDoNotFollow, std::move(observer),
                       std::move(extra_response_headers)));
    file_access::ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
        {path}, std::move(create_and_start_cb));
  } else {
    mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(network::URLLoaderCompletionStatus(net::ERR_INVALID_URL));
    return;
  }
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateFileURLLoaderFactory(
    const base::FilePath& profile_path,
    scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list) {
  // TODO(crbug.com/41436919): Re-evaluate TaskPriority: Should the caller
  // provide it?
  return FileURLLoaderFactory::Create(profile_path,
                                      shared_cors_origin_access_list,
                                      base::TaskPriority::USER_VISIBLE);
}

}  // namespace content
