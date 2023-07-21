// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/io_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/browser/devtools/devtools_stream_blob.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {
namespace protocol {

IOHandler::IOHandler(DevToolsIOContext* io_context)
    : DevToolsDomainHandler(IO::Metainfo::domainName),
      io_context_(io_context),
      browser_context_(nullptr),
      storage_partition_(nullptr) {}

IOHandler::~IOHandler() = default;

void IOHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<IO::Frontend>(dispatcher->channel());
  IO::Dispatcher::wire(dispatcher, this);
}

void IOHandler::SetRenderer(int process_host_id,
                            RenderFrameHostImpl* frame_host) {
  RenderProcessHost* process_host = RenderProcessHost::FromID(process_host_id);
  if (process_host) {
    browser_context_ = process_host->GetBrowserContext();
    storage_partition_ = process_host->GetStoragePartition();
  } else {
    browser_context_ = nullptr;
    storage_partition_ = nullptr;
  }
}

void IOHandler::Read(
    const std::string& handle,
    Maybe<int> offset,
    Maybe<int> max_size,
    std::unique_ptr<ReadCallback> callback) {
  static const size_t kDefaultChunkSize = 10 * 1024 * 1024;
  static const char kBlobPrefix[] = "blob:";

  scoped_refptr<DevToolsIOContext::Stream> stream =
      io_context_->GetByHandle(handle);
  if (!stream && browser_context_ &&
      StartsWith(handle, kBlobPrefix, base::CompareCase::SENSITIVE)) {
    ChromeBlobStorageContext* blob_context =
        ChromeBlobStorageContext::GetFor(browser_context_);
    std::string uuid = handle.substr(strlen(kBlobPrefix));
    stream = DevToolsStreamBlob::Create(io_context_, blob_context,
                                        storage_partition_, handle, uuid);
  }

  if (!stream) {
    callback->sendFailure(Response::InvalidParams("Invalid stream handle"));
    return;
  }
  if (offset.has_value() && !stream->SupportsSeek()) {
    callback->sendFailure(
        Response::InvalidParams("Read offset is specificed for a stream that "
                                "does not support random access"));
    return;
  }
  int size = max_size.value_or(kDefaultChunkSize);
  if (size <= 0) {
    callback->sendFailure(Response::InvalidParams("Invalid max read size"));
    return;
  }
  stream->Read(offset.value_or(-1), size,
               base::BindOnce(&IOHandler::ReadComplete,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void IOHandler::ReadComplete(std::unique_ptr<ReadCallback> callback,
                             std::unique_ptr<std::string> data,
                             bool base64_encoded,
                             int status) {
  if (status == DevToolsIOContext::Stream::StatusFailure) {
    callback->sendFailure(Response::ServerError("Read failed"));
    return;
  }
  bool eof = status == DevToolsIOContext::Stream::StatusEOF;
  callback->sendSuccess(base64_encoded, std::move(*data), eof);
}

Response IOHandler::Close(const std::string& handle) {
  return io_context_->Close(handle)
             ? Response::Success()
             : Response::InvalidParams("Invalid stream handle");
}

}  // namespace protocol
}  // namespace content
