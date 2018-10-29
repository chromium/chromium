// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/file_system_url_loader_factory.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/string_data_pipe_producer.h"
#include "net/base/directory_listing.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_util.h"

using filesystem::mojom::DirectoryEntry;
using storage::FileStreamReader;
using storage::FileSystemContext;
using storage::FileSystemOperation;
using storage::FileSystemRequestInfo;
using storage::FileSystemURL;
using storage::VirtualPath;

namespace content {
namespace {

struct FactoryParams {
  int render_process_host_id;
  int frame_tree_node_id;
  scoped_refptr<FileSystemContext> file_system_context;
  std::string storage_domain;
};

constexpr size_t kDefaultFileSystemUrlPipeSize = 65536;

// Implementation sniffs the first file chunk to determine the mime-type.
static_assert(kDefaultFileSystemUrlPipeSize >= net::kMaxBytesToSniff,
              "Default file data pipe size must be at least as large as a "
              "MIME-type sniffing buffer.");

scoped_refptr<net::HttpResponseHeaders> CreateHttpResponseHeaders(
    int response_code) {
  std::string raw_headers;
  raw_headers.append(base::StringPrintf("HTTP/1.1 %d OK", response_code));

  // Tell WebKit never to cache this content.
  raw_headers.append(1, '\0');
  raw_headers.append(net::HttpRequestHeaders::kCacheControl);
  raw_headers.append(": no-cache");

  raw_headers.append(2, '\0');
  return base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
}

bool GetMimeType(const FileSystemURL& url, std::string* mime_type) {
  DCHECK(url.is_valid());
  base::FilePath::StringType extension = url.path().Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  return net::GetWellKnownMimeTypeFromExtension(extension, mime_type);
}

// Common implementation shared between the file and directory URLLoaders.
class FileSystemEntryURLLoader
    : public network::mojom::URLLoader,
      public base::SupportsWeakPtr<FileSystemEntryURLLoader> {
 public:
  explicit FileSystemEntryURLLoader(FactoryParams params)
      : binding_(this), params_(std::move(params)) {}

  // network::mojom::URLLoader:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override {}
  void ProceedWithResponse() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 protected:
  virtual void FileSystemIsMounted() = 0;

  void Start(const network::ResourceRequest& request,
             network::mojom::URLLoaderRequest loader,
             network::mojom::URLLoaderClientPtrInfo client_info,
             scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
    io_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSystemEntryURLLoader::StartOnIOThread, AsWeakPtr(),
                       request, std::move(loader), std::move(client_info)));
  }

  void MaybeDeleteSelf() {
    if (!binding_.is_bound() && !client_.is_bound())
      delete this;
  }

  void OnClientComplete(network::URLLoaderCompletionStatus status) {
    client_->OnComplete(status);
    client_.reset();
    MaybeDeleteSelf();
  }

  void OnClientComplete(base::File::Error file_error) {
    OnClientComplete(net::FileErrorToNetError(file_error));
  }

  void OnClientComplete(net::Error net_error) {
    OnClientComplete(network::URLLoaderCompletionStatus(net_error));
  }

  mojo::Binding<network::mojom::URLLoader> binding_;
  network::mojom::URLLoaderClientPtr client_;
  FactoryParams params_;
  std::unique_ptr<mojo::StringDataPipeProducer> data_producer_;
  net::HttpByteRange byte_range_;
  FileSystemURL url_;

 private:
  void StartOnIOThread(const network::ResourceRequest& request,
                       network::mojom::URLLoaderRequest loader,
                       network::mojom::URLLoaderClientPtrInfo client_info) {
    binding_.Bind(std::move(loader));
    binding_.set_connection_error_handler(base::BindOnce(
        &FileSystemEntryURLLoader::OnConnectionError, base::Unretained(this)));

    client_.Bind(std::move(client_info));

    if (!request.url.is_valid()) {
      OnClientComplete(net::ERR_INVALID_URL);
      return;
    }

    if (params_.render_process_host_id != ChildProcessHost::kInvalidUniqueID &&
        !ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
            params_.render_process_host_id, request.url)) {
      DVLOG(1) << "Denied unauthorized request for "
               << request.url.possibly_invalid_spec();
      OnClientComplete(net::ERR_INVALID_URL);
      return;
    }

    std::string range_header;
    if (request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                  &range_header)) {
      std::vector<net::HttpByteRange> ranges;
      if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
        if (ranges.size() == 1) {
          byte_range_ = ranges[0];
        } else {
          // We don't support multiple range requests in one single URL request.
          // TODO(adamk): decide whether we want to support multiple range
          // requests.
          OnClientComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
          return;
        }
      }
    }

    url_ = params_.file_system_context->CrackURL(request.url);
    if (!url_.is_valid()) {
      const FileSystemRequestInfo request_info = {request.url, nullptr,
                                                  params_.storage_domain,
                                                  params_.frame_tree_node_id};
      params_.file_system_context->AttemptAutoMountForURLRequest(
          request_info,
          base::BindOnce(&FileSystemEntryURLLoader::DidAttemptAutoMount,
                         AsWeakPtr(), request));
      return;
    }
    FileSystemIsMounted();
  }

  void DidAttemptAutoMount(const network::ResourceRequest& request,
                           base::File::Error result) {
    if (result != base::File::Error::FILE_OK) {
      OnClientComplete(result);
      return;
    }
    url_ = params_.file_system_context->CrackURL(request.url);
    if (!url_.is_valid()) {
      OnClientComplete(net::ERR_FILE_NOT_FOUND);
      return;
    }
    FileSystemIsMounted();
  }

  void OnConnectionError() {
    binding_.Close();
    MaybeDeleteSelf();
  }

  DISALLOW_COPY_AND_ASSIGN(FileSystemEntryURLLoader);
};

class FileSystemDirectoryURLLoader : public FileSystemEntryURLLoader {
 public:
  static void CreateAndStart(
      const network::ResourceRequest& request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info,
      FactoryParams params,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // file directory has been sent to it.
    auto* filesystem_loader =
        new FileSystemDirectoryURLLoader(std::move(params));
    filesystem_loader->Start(request, std::move(loader), std::move(client_info),
                             io_task_runner);
  }

 private:
  explicit FileSystemDirectoryURLLoader(FactoryParams params)
      : FileSystemEntryURLLoader(params) {}

  void FileSystemIsMounted() override {
    DCHECK(url_.is_valid());
    if (!params_.file_system_context->CanServeURLRequest(url_)) {
      // In incognito mode the API is not usable and there should be no data.
      if (VirtualPath::IsRootPath(url_.virtual_path())) {
        // Return an empty directory if the filesystem root is queried.
        DidReadDirectory(base::File::FILE_OK, std::vector<DirectoryEntry>(),
                         /*has_more=*/false);
        return;
      }
      // In incognito mode the API is not usable and there should be no data.
      OnClientComplete(net::ERR_FILE_NOT_FOUND);
      return;
    }
    params_.file_system_context->operation_runner()->ReadDirectory(
        url_,
        base::BindRepeating(&FileSystemDirectoryURLLoader::DidReadDirectory,
                            base::AsWeakPtr(this)));
  }

  void DidReadDirectory(base::File::Error result,
                        std::vector<DirectoryEntry> entries,
                        bool has_more) {
    if (result != base::File::FILE_OK) {
      net::Error rv = net::ERR_FILE_NOT_FOUND;
      if (result == base::File::FILE_ERROR_INVALID_URL)
        rv = net::ERR_INVALID_URL;
      OnClientComplete(rv);
      return;
    }

    if (data_.empty()) {
      base::FilePath relative_path = url_.path();
#if defined(OS_POSIX)
      relative_path =
          base::FilePath(FILE_PATH_LITERAL("/") + relative_path.value());
#endif
      const base::string16& title = relative_path.LossyDisplayName();
      data_.append(net::GetDirectoryListingHeader(title));
    }

    entries_.insert(entries_.end(), entries.begin(), entries.end());

    if (!has_more) {
      if (entries_.size())
        GetMetadata(/*index=*/0);
      else
        WriteDirectoryData();
    }
  }

  void GetMetadata(size_t index) {
    const DirectoryEntry& entry = entries_[index];
    const FileSystemURL entry_url =
        params_.file_system_context->CreateCrackedFileSystemURL(
            url_.origin(), url_.type(),
            url_.path().Append(base::FilePath(entry.name)));
    DCHECK(entry_url.is_valid());
    params_.file_system_context->operation_runner()->GetMetadata(
        entry_url,
        FileSystemOperation::GET_METADATA_FIELD_SIZE |
            FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
        base::BindRepeating(&FileSystemDirectoryURLLoader::DidGetMetadata,
                            base::AsWeakPtr(this), index));
  }

  void DidGetMetadata(size_t index,
                      base::File::Error result,
                      const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      OnClientComplete(result);
      return;
    }

    const DirectoryEntry& entry = entries_[index];
    const base::string16& name = base::FilePath(entry.name).LossyDisplayName();
    data_.append(net::GetDirectoryListingEntry(
        name, std::string(),
        entry.type == filesystem::mojom::FsFileType::DIRECTORY, file_info.size,
        file_info.last_modified));

    if (index < entries_.size() - 1)
      GetMetadata(index + 1);
    else
      WriteDirectoryData();
  }

  void WriteDirectoryData() {
    mojo::DataPipe pipe(std::max(data_.size(), kDefaultFileSystemUrlPipeSize));
    if (!pipe.consumer_handle.is_valid()) {
      OnClientComplete(net::ERR_FAILED);
      return;
    }

    network::ResourceResponseHead head;
    head.mime_type = "text/plain";
    head.charset = "utf-8";
    head.content_length = data_.size();
    head.headers = CreateHttpResponseHeaders(200);

    client_->OnReceiveResponse(head);
    client_->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));

    data_producer_ = std::make_unique<mojo::StringDataPipeProducer>(
        std::move(pipe.producer_handle));

    data_producer_->Write(
        base::StringPiece(data_),
        mojo::StringDataPipeProducer::AsyncWritingMode::
            STRING_STAYS_VALID_UNTIL_COMPLETION,
        base::BindOnce(&FileSystemDirectoryURLLoader::OnDirectoryWritten,
                       base::Unretained(this)));
  }

  void OnDirectoryWritten(MojoResult result) {
    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();
    directory_data_ = nullptr;
    entries_.clear();
    data_.clear();

    OnClientComplete(result == MOJO_RESULT_OK ? net::OK : net::ERR_FAILED);
  }

  std::string data_;
  std::vector<DirectoryEntry> entries_;
  scoped_refptr<net::IOBuffer> directory_data_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDirectoryURLLoader);
};

class FileSystemFileURLLoader : public FileSystemEntryURLLoader {
 public:
  static void CreateAndStart(
      const network::ResourceRequest& request,
      network::mojom::URLLoaderRequest loader,
      network::mojom::URLLoaderClientPtrInfo client_info,
      FactoryParams params,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* filesystem_loader =
        new FileSystemFileURLLoader(std::move(params), request, io_task_runner);

    filesystem_loader->Start(request, std::move(loader), std::move(client_info),
                             io_task_runner);
  }

 private:
  FileSystemFileURLLoader(
      FactoryParams params,
      const network::ResourceRequest& request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner)
      : FileSystemEntryURLLoader(std::move(params)),
        original_request_(request),
        io_task_runner_(io_task_runner) {}

  void FileSystemIsMounted() override {
    DCHECK(url_.is_valid());
    if (!params_.file_system_context->CanServeURLRequest(url_)) {
      // In incognito mode the API is not usable and there should be no data.
      OnClientComplete(net::ERR_FILE_NOT_FOUND);
      return;
    }
    params_.file_system_context->operation_runner()->GetMetadata(
        url_,
        FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
            FileSystemOperation::GET_METADATA_FIELD_SIZE,
        base::AdaptCallbackForRepeating(base::BindOnce(
            &FileSystemFileURLLoader::DidGetMetadata, base::AsWeakPtr(this))));
  }

  void DidGetMetadata(base::File::Error error_code,
                      const base::File::Info& file_info) {
    if (error_code != base::File::FILE_OK) {
      OnClientComplete(error_code == base::File::FILE_ERROR_INVALID_URL
                           ? net::ERR_INVALID_URL
                           : net::ERR_FILE_NOT_FOUND);
      return;
    }

    if (!byte_range_.ComputeBounds(file_info.size)) {
      OnClientComplete(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      return;
    }

    if (file_info.is_directory) {
      // Redirect to the directory URLLoader.
      GURL::Replacements replacements;
      std::string new_path = original_request_.url.path();
      new_path.push_back('/');
      replacements.SetPathStr(new_path);
      const GURL directory_url =
          original_request_.url.ReplaceComponents(replacements);

      net::RedirectInfo redirect_info;
      redirect_info.new_method = "GET";
      redirect_info.status_code = 301;
      head_.headers = CreateHttpResponseHeaders(redirect_info.status_code);
      redirect_info.new_url =
          original_request_.url.ReplaceComponents(replacements);
      head_.encoded_data_length = 0;
      client_->OnReceiveRedirect(redirect_info, head_);
      return;
    }

    remaining_bytes_ = byte_range_.last_byte_position() -
                       byte_range_.first_byte_position() + 1;
    DCHECK_GE(remaining_bytes_, 0);

    DCHECK(!reader_.get());
    reader_ = params_.file_system_context->CreateFileStreamReader(
        url_, byte_range_.first_byte_position(), remaining_bytes_,
        base::Time());

    mojo::DataPipe pipe(remaining_bytes_);
    if (!pipe.consumer_handle.is_valid()) {
      OnClientComplete(net::ERR_FAILED);
      return;
    }
    consumer_handle_ = std::move(pipe.consumer_handle);

    head_.mime_type = "text/html";  // Will sniff file and possibly override.
    head_.charset = "utf-8";
    head_.content_length = remaining_bytes_;
    head_.headers = CreateHttpResponseHeaders(200);

    data_producer_ = std::make_unique<mojo::StringDataPipeProducer>(
        std::move(pipe.producer_handle));

    file_data_ =
        base::MakeRefCounted<net::IOBuffer>(kDefaultFileSystemUrlPipeSize);
    ReadMoreFileData();
  }

  void ReadMoreFileData() {
    int64_t bytes_to_read = std::min(
        static_cast<int64_t>(kDefaultFileSystemUrlPipeSize), remaining_bytes_);
    if (!bytes_to_read) {
      if (consumer_handle_.is_valid()) {
        // This was an empty file; make sure to call OnReceiveResponse
        // regardless.
        client_->OnReceiveResponse(head_);
      }
      OnFileWritten(MOJO_RESULT_OK);
      return;
    }
    net::CompletionCallback read_callback = base::BindRepeating(
        &FileSystemFileURLLoader::DidReadMoreFileData, base::AsWeakPtr(this));
    const int rv =
        reader_->Read(file_data_.get(), bytes_to_read, read_callback);
    if (rv == net::ERR_IO_PENDING) {
      // async callback will be called.
      return;
    }
    std::move(read_callback).Run(rv);
  }

  void DidReadMoreFileData(int result) {
    if (result <= 0) {
      OnFileWritten(result);
      return;
    }

    if (consumer_handle_.is_valid()) {
      if (byte_range_.first_byte_position() == 0) {
        // Only sniff for mime-type in the first block of the file.
        std::string type_hint;
        GetMimeType(url_, &type_hint);
        SniffMimeType(file_data_->data(), result, url_.ToGURL(), type_hint,
                      net::ForceSniffFileUrlsForHtml::kDisabled,
                      &head_.mime_type);
        head_.did_mime_sniff = true;
      }

      client_->OnReceiveResponse(head_);
      client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
    }
    remaining_bytes_ -= result;
    DCHECK_GE(remaining_bytes_, 0);

    WriteFileData(result);
  }

  void WriteFileData(int bytes_read) {
    data_producer_->Write(
        base::StringPiece(file_data_->data(), bytes_read),
        mojo::StringDataPipeProducer::AsyncWritingMode::
            STRING_STAYS_VALID_UNTIL_COMPLETION,
        base::BindOnce(&FileSystemFileURLLoader::OnFileDataWritten,
                       base::AsWeakPtr(this)));
  }

  void OnFileDataWritten(MojoResult result) {
    if (result != MOJO_RESULT_OK || remaining_bytes_ == 0) {
      OnFileWritten(result);
      return;
    }
    ReadMoreFileData();
  }

  void OnFileWritten(MojoResult result) {
    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();
    file_data_ = nullptr;

    OnClientComplete(result == MOJO_RESULT_OK ? net::OK : net::ERR_FAILED);
  }

  int64_t remaining_bytes_ = 0;
  mojo::ScopedDataPipeConsumerHandle consumer_handle_;
  std::unique_ptr<FileStreamReader> reader_;
  scoped_refptr<net::IOBuffer> file_data_;
  network::ResourceResponseHead head_;
  const network::ResourceRequest original_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemFileURLLoader);
};

// A URLLoaderFactory used for the filesystem:// scheme used when the Network
// Service is enabled.
class FileSystemURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  FileSystemURLLoaderFactory(
      FactoryParams params,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner)
      : params_(std::move(params)), io_task_runner_(io_task_runner) {}

  ~FileSystemURLLoaderFactory() override = default;

  network::mojom::URLLoaderFactoryPtr CreateBinding() {
    network::mojom::URLLoaderFactoryPtr factory;
    bindings_.AddBinding(this, mojo::MakeRequest(&factory));
    return factory;
  }

 private:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DVLOG(1) << "CreateLoaderAndStart: " << request.url;

    const std::string path = request.url.path();

    // If the path ends with a /, we know it's a directory. If the path refers
    // to a directory and gets dispatched to FileSystemFileURLLoader, that class
    // will redirect to FileSystemDirectoryURLLoader.
    if (!path.empty() && path.back() == '/') {
      FileSystemDirectoryURLLoader::CreateAndStart(request, std::move(loader),
                                                   client.PassInterface(),
                                                   params_, io_task_runner_);
      return;
    }

    FileSystemFileURLLoader::CreateAndStart(request, std::move(loader),
                                            client.PassInterface(), params_,
                                            io_task_runner_);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest loader) override {
    bindings_.AddBinding(this, std::move(loader));
  }

  const FactoryParams params_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLLoaderFactory);
};

}  // anonymous namespace

std::unique_ptr<network::mojom::URLLoaderFactory>
CreateFileSystemURLLoaderFactory(
    RenderFrameHost* render_frame_host,
    bool is_navigation,
    scoped_refptr<FileSystemContext> file_system_context,
    const std::string& storage_domain) {
  // Get the RPH ID for security checks for non-navigation resource requests.
  int render_process_host_id = is_navigation
                                   ? ChildProcessHost::kInvalidUniqueID
                                   : render_frame_host->GetProcess()->GetID();

  FactoryParams params = {render_process_host_id,
                          render_frame_host->GetFrameTreeNodeId(),
                          file_system_context, storage_domain};

  return std::make_unique<FileSystemURLLoaderFactory>(
      std::move(params),
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}));
}

}  // namespace content
