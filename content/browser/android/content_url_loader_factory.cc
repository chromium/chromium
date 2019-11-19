// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_url_loader_factory.h"

#include <limits>
#include <string>
#include <vector>

#include "base/android/content_uri_utils.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/file_data_source.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"

// TODO(eroman): Add unit-tests for "X-Chrome-intent-type"
//               (see url_request_content_job_unittest.cc).
// TODO(eroman): Remove duplication with file_url_loader_factory.cc (notably
//               Range header parsing).

namespace content {
namespace {

constexpr size_t kDefaultContentUrlPipeSize = 65536;

// Assigns the byte range that has been requested based on the Range header.
// This assumes the simplest form of the Range header using a single range.
// If no byte range was specified, the output range will cover the entire file.
bool GetRequestedByteRange(const network::ResourceRequest& request,
                           uint64_t content_size,
                           uint64_t* first_byte_to_send,
                           uint64_t* total_bytes_to_send) {
  *first_byte_to_send = 0;
  *total_bytes_to_send = content_size;

  std::string range_header;
  std::vector<net::HttpByteRange> ranges;

  if (!request.headers.GetHeader(net::HttpRequestHeaders::kRange,
                                 &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return true;
  }

  // Only handle a simple Range header for a single range.
  if (ranges.size() != 1 || !ranges[0].IsValid() ||
      !ranges[0].ComputeBounds(content_size)) {
    return false;
  }

  net::HttpByteRange byte_range = ranges[0];
  *first_byte_to_send = byte_range.first_byte_position();
  *total_bytes_to_send =
      byte_range.last_byte_position() - *first_byte_to_send + 1;
  return true;
}

// Gets the mimetype for |content_path| either by asking the content provider,
// or by using the special Chrome request header X-Chrome-intent-type.
void GetMimeType(const network::ResourceRequest& request,
                 const base::FilePath& content_path,
                 std::string* out_mime_type) {
  out_mime_type->clear();

  std::string intent_type_header;
  if ((request.resource_type ==
       static_cast<int>(content::ResourceType::kMainFrame)) &&
      request.headers.GetHeader("X-Chrome-intent-type", &intent_type_header)) {
    *out_mime_type = intent_type_header;
  }

  if (out_mime_type->empty())
    *out_mime_type = base::GetContentUriMimeType(content_path);
}

class ContentURLLoader : public network::mojom::URLLoader {
 public:
  static void CreateAndStart(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClient
    // bindings are alive - essentially until either the client gives up or all
    // file data has been sent to it.
    auto* content_url_loader = new ContentURLLoader;
    content_url_loader->Start(request, std::move(loader),
                              std::move(client_remote));
  }

  // network::mojom::URLLoader:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  ContentURLLoader() = default;
  ~ContentURLLoader() override = default;

  void Start(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote) {
    network::ResourceResponseHead head;
    head.request_start = head.response_start = base::TimeTicks::Now();
    receiver_.Bind(std::move(loader));
    receiver_.set_disconnect_handler(base::BindOnce(
        &ContentURLLoader::OnMojoDisconnect, base::Unretained(this)));

    mojo::Remote<network::mojom::URLLoaderClient> client(
        std::move(client_remote));

    DCHECK(request.url.SchemeIs("content"));
    base::FilePath path = base::FilePath(request.url.spec());

    // Get the file length.
    base::File::Info info;
    if (!base::GetFileInfo(path, &info))
      return CompleteWithFailure(std::move(client), net::ERR_FILE_NOT_FOUND);

    uint64_t first_byte_to_send;
    uint64_t total_bytes_to_send;
    if (!GetRequestedByteRange(request, (info.size > 0) ? info.size : 0,
                               &first_byte_to_send, &total_bytes_to_send) ||
        (std::numeric_limits<int64_t>::max() < first_byte_to_send) ||
        (std::numeric_limits<int64_t>::max() < total_bytes_to_send)) {
      return CompleteWithFailure(std::move(client),
                                 net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    }

    mojo::DataPipe pipe(kDefaultContentUrlPipeSize);
    if (!pipe.consumer_handle.is_valid())
      return CompleteWithFailure(std::move(client), net::ERR_FAILED);

    base::File file = base::OpenContentUriForRead(path);
    if (!file.IsValid()) {
      return CompleteWithFailure(
          std::move(client), net::FileErrorToNetError(file.error_details()));
    }

    head.content_length = total_bytes_to_send;
    total_bytes_written_ = total_bytes_to_send;

    // Set the mimetype of the response.
    GetMimeType(request, path, &head.mime_type);
    if (head.mime_type.empty() ||
        head.mime_type == "application/octet-stream") {
      // When a bundled exchanges file is downloaded with
      // "content-type: application/webbundle;v=b1" header, Chrome saves it as
      // "application/webbundle" MIME type. The MIME type is stored to Android's
      // DownloadManager. If the file is opened from a URI which is under
      // DownloadManager's control and the ContentProvider can get the MIME
      // type, |head.mime_type| is set to "application/webbundle". But otherwise
      // ContentResolver.getType() returns null or the default type
      // [1]"application/octet-stream" even if the file extension is ".wbn".
      // (eg: opening the file from "Internal Storage")
      // This is because the Media type of BundledHTTPExchanges isn't registered
      // to the IANA registry (https://www.iana.org/assignments/media-types/),
      // and it is not listed in the mime.types files [2][3] which was referd by
      // MimeTypeMap.getMimeTypeFromExtension().
      // So we set the MIME type if the file extension is ".wbn" by calling
      // bundled_exchanges_utils::GetBundledExchangesFileMimeTypeFromFile().
      // [1]
      // https://android.googlesource.com/platform/frameworks/base/+/1b817f6/core/java/android/content/ContentResolver.java#481
      // [2] https://android.googlesource.com/platform/external/mime-support/
      // [3]
      // https://android.googlesource.com/platform/libcore/+/master/luni/src/main/java/libcore/net/android.mime.types
      bundled_exchanges_utils::GetBundledExchangesFileMimeTypeFromFile(
          path, &head.mime_type);
    }

    if (!head.mime_type.empty()) {
      head.headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
      head.headers->AddHeader(
          base::StringPrintf("%s: %s", net::HttpRequestHeaders::kContentType,
                             head.mime_type.c_str()));
    }

    client->OnReceiveResponse(head);
    client->OnStartLoadingResponseBody(std::move(pipe.consumer_handle));
    client_ = std::move(client);

    if (total_bytes_to_send == 0) {
      // There's no more data, so we're already done.
      OnFileWritten(MOJO_RESULT_OK);
      return;
    }

    // In case of a range request, seek to the appropriate position before
    // sending the remaining bytes asynchronously. Under normal conditions
    // (i.e., no range request) this Seek is effectively a no-op.
    auto data_source = std::make_unique<mojo::FileDataSource>(std::move(file));
    data_source->SetRange(first_byte_to_send,
                          first_byte_to_send + total_bytes_to_send);

    data_producer_ = std::make_unique<mojo::DataPipeProducer>(
        std::move(pipe.producer_handle));
    data_producer_->Write(std::move(data_source),
                          base::BindOnce(&ContentURLLoader::OnFileWritten,
                                         base::Unretained(this)));
  }

  void CompleteWithFailure(mojo::Remote<network::mojom::URLLoaderClient> client,
                           net::Error net_error) {
    client->OnComplete(network::URLLoaderCompletionStatus(net_error));
    MaybeDeleteSelf();
  }

  void OnMojoDisconnect() {
    receiver_.reset();
    MaybeDeleteSelf();
  }

  void MaybeDeleteSelf() {
    if (!receiver_.is_bound() && !client_.is_bound())
      delete this;
  }

  void OnFileWritten(MojoResult result) {
    // All the data has been written now. Close the data pipe. The consumer will
    // be notified that there will be no more data to read from now.
    data_producer_.reset();

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

  // In case of successful loads, this holds the total of bytes written.
  // It is used to set some of the URLLoaderCompletionStatus data passed back
  // to the URLLoaderClients (eg SimpleURLLoader).
  size_t total_bytes_written_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ContentURLLoader);
};

}  // namespace

ContentURLLoaderFactory::ContentURLLoaderFactory(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

ContentURLLoaderFactory::~ContentURLLoaderFactory() = default;

void ContentURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ContentURLLoader::CreateAndStart, request,
                                std::move(loader), std::move(client)));
}

void ContentURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) {
  receivers_.Add(this, std::move(loader));
}

}  // namespace content
