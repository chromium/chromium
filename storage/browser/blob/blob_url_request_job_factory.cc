// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_request_job_factory.h"

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"

namespace storage {

namespace {

int kUserDataKey;  // The value is not important, the addr is a key.

}  // namespace

// static
std::unique_ptr<net::URLRequest> BlobProtocolHandler::CreateBlobRequest(
    std::unique_ptr<BlobDataHandle> blob_data_handle,
    const net::URLRequestContext* request_context,
    net::URLRequest::Delegate* request_delegate) {
  const GURL kBlobUrl("blob://see_user_data/");
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("blob_read", R"(
        semantics {
          sender: "BlobProtocolHandler"
          description:
            "Blobs are used for a variety of use cases, and are basically "
            "immutable blocks of data. See https://chromium.googlesource.com/"
            "chromium/src/+/master/storage/browser/blob/README.md for an "
            "explanation of blobs and their implementation in Chrome. These "
            "can be created by scripts in a website, web platform features, or "
            "internally in the browser."
          trigger:
            "Request for reading the contents of a blob."
          data:
            "A reference to a Blob, File, or CacheStorage entry created from "
            "script, a web platform feature, or browser internals."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented. This is a local data fetch request and has no "
            "network activity."
        })");
  std::unique_ptr<net::URLRequest> request = request_context->CreateRequest(
      kBlobUrl, net::DEFAULT_PRIORITY, request_delegate, traffic_annotation);
  request->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_COOKIES);
  SetRequestedBlobDataHandle(request.get(), std::move(blob_data_handle));
  return request;
}

// static
void BlobProtocolHandler::SetRequestedBlobDataHandle(
    net::URLRequest* request,
    std::unique_ptr<BlobDataHandle> blob_data_handle) {
  request->SetUserData(&kUserDataKey, std::move(blob_data_handle));
}

// static
BlobDataHandle* BlobProtocolHandler::GetRequestBlobDataHandle(
    net::URLRequest* request) {
  return static_cast<BlobDataHandle*>(request->GetUserData(&kUserDataKey));
}

BlobProtocolHandler::BlobProtocolHandler(BlobStorageContext* context) {
  if (context)
    context_ = context->AsWeakPtr();
}

BlobProtocolHandler::~BlobProtocolHandler() = default;

net::URLRequestJob* BlobProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new storage::BlobURLRequestJob(request, network_delegate,
                                        LookupBlobHandle(request));
}

bool BlobProtocolHandler::IsSafeRedirectTarget(const GURL& location) const {
  return false;
}

BlobDataHandle* BlobProtocolHandler::LookupBlobHandle(
    net::URLRequest* request) const {
  BlobDataHandle* blob_data_handle = GetRequestBlobDataHandle(request);
  if (blob_data_handle)
    return blob_data_handle;
  if (!context_.get())
    return nullptr;

  // Support looking up based on uuid, the FeedbackExtensionAPI relies on this.
  // TODO(michaeln): Replace this use case and others like it with a BlobReader
  // impl that does not depend on urlfetching to perform this function.
  const std::string kPrefix("blob:uuid/");
  if (!base::StartsWith(request->url().spec(), kPrefix,
                        base::CompareCase::SENSITIVE))
    return nullptr;
  std::string uuid = request->url().spec().substr(kPrefix.length());
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(uuid);
  BlobDataHandle* handle_ptr = handle.get();
  if (handle) {
    SetRequestedBlobDataHandle(request, std::move(handle));
  }
  return handle_ptr;
}

}  // namespace storage
