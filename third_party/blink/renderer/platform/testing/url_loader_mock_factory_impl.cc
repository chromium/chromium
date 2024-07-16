// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory_impl.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// static
URLLoaderMockFactory* URLLoaderMockFactory::GetSingletonInstance() {
  DEFINE_STATIC_LOCAL(URLLoaderMockFactoryImpl, s_singleton, (nullptr));
  return &s_singleton;
}

URLLoaderMockFactoryImpl::URLLoaderMockFactoryImpl(
    TestingPlatformSupport* platform)
    : platform_(platform) {}

URLLoaderMockFactoryImpl::~URLLoaderMockFactoryImpl() = default;

std::unique_ptr<URLLoader> URLLoaderMockFactoryImpl::CreateURLLoader() {
  return std::make_unique<URLLoaderMock>(this);
}

void URLLoaderMockFactoryImpl::RegisterURL(const WebURL& url,
                                           const WebURLResponse& response,
                                           const WebString& file_path) {
  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.IsNull() && !file_path.IsEmpty()) {
    response_info.file_path = blink::WebStringToFilePath(file_path);
    DCHECK(base::PathExists(response_info.file_path))
        << response_info.file_path.MaybeAsASCII() << " does not exist.";
  }

  DCHECK(!base::Contains(url_to_response_info_, url));
  url_to_response_info_.Set(url, response_info);
}

void URLLoaderMockFactoryImpl::RegisterErrorURL(const WebURL& url,
                                                const WebURLResponse& response,
                                                const WebURLError& error) {
  DCHECK(!base::Contains(url_to_response_info_, url));
  RegisterURL(url, response, WebString());
  url_to_error_info_.Set(url, error);
}

void URLLoaderMockFactoryImpl::UnregisterURL(const blink::WebURL& url) {
  URLToResponseMap::iterator iter = url_to_response_info_.find(url);
  CHECK(iter != url_to_response_info_.end());
  url_to_response_info_.erase(iter);

  URLToErrorMap::iterator error_iter = url_to_error_info_.find(url);
  if (error_iter != url_to_error_info_.end()) {
    url_to_error_info_.erase(error_iter);
  }
}

void URLLoaderMockFactoryImpl::RegisterURLProtocol(
    const WebString& protocol,
    const WebURLResponse& response,
    const WebString& file_path) {
  DCHECK(protocol.ContainsOnlyASCII());

  ResponseInfo response_info;
  response_info.response = response;
  if (!file_path.IsNull() && !file_path.IsEmpty()) {
    response_info.file_path = blink::WebStringToFilePath(file_path);
    DCHECK(base::PathExists(response_info.file_path))
        << response_info.file_path.MaybeAsASCII() << " does not exist.";
  }

  DCHECK(!base::Contains(protocol_to_response_info_, protocol));
  protocol_to_response_info_.Set(protocol, response_info);
}

void URLLoaderMockFactoryImpl::UnregisterURLProtocol(
    const WebString& protocol) {
  ProtocolToResponseMap::iterator iter =
      protocol_to_response_info_.find(protocol);
  CHECK(iter != protocol_to_response_info_.end());
  protocol_to_response_info_.erase(iter);
}

void URLLoaderMockFactoryImpl::UnregisterAllURLsAndClearMemoryCache() {
  url_to_response_info_.clear();
  url_to_error_info_.clear();
  protocol_to_response_info_.clear();
  if (IsMainThread()) {
    MemoryCache::Get()->EvictResources();
  }
}

void URLLoaderMockFactoryImpl::ServeAsynchronousRequests() {
  // Serving a request might trigger more requests, so we cannot iterate on
  // pending_loaders_ as it might get modified.
  while (!pending_loaders_.empty()) {
    LoaderToRequestMap::iterator iter = pending_loaders_.begin();
    base::WeakPtr<URLLoaderMock> loader(iter->key->GetWeakPtr());
    std::unique_ptr<network::ResourceRequest> request = std::move(iter->value);
    pending_loaders_.erase(loader.get());

    WebURLResponse response;
    std::optional<WebURLError> error;
    scoped_refptr<SharedBuffer> data;
    LoadRequest(WebURL(KURL(request->url)), &response, &error, data);
    // Follow any redirects while the loader is still active.
    while (response.HttpStatusCode() >= 300 &&
           response.HttpStatusCode() < 400) {
      WebURL new_url = loader->ServeRedirect(
          WebString::FromLatin1(request->method), response);
      RunUntilIdle();
      if (!loader || loader->is_cancelled() || loader->is_deferred()) {
        break;
      }
      LoadRequest(new_url, &response, &error, data);
    }
    // Serve the request if the loader is still active.
    if (loader && !loader->is_cancelled() && !loader->is_deferred()) {
      loader->ServeAsynchronousRequest(delegate_, response, data, error);
      RunUntilIdle();
    }
  }
  RunUntilIdle();
}

void URLLoaderMockFactoryImpl::FillNavigationParamsResponse(
    WebNavigationParams* params) {
  KURL kurl = params->url;
  if (kurl.ProtocolIsData()) {
    ResourceResponse response;
    scoped_refptr<SharedBuffer> buffer;
    int result;
    std::tie(result, response, buffer) =
        network_utils::ParseDataURL(kurl, params->http_method);
    DCHECK(buffer);
    DCHECK_EQ(net::OK, result);
    params->response = WrappedResourceResponse(response);
    params->is_static_data = true;
    params->body_loader =
        StaticDataNavigationBodyLoader::CreateWithData(std::move(buffer));
    return;
  }

  if (delegate_ && delegate_->FillNavigationParamsResponse(params)) {
    return;
  }

  std::optional<WebURLError> error;
  scoped_refptr<SharedBuffer> data;

  size_t redirects = 0;
  LoadRequest(params->url, &params->response, &error, data);
  DCHECK(!error);
  while (params->response.HttpStatusCode() >= 300 &&
         params->response.HttpStatusCode() < 400) {
    WebURL new_url(KURL(params->response.HttpHeaderField("Location")));
    ++redirects;
    params->redirects.reserve(redirects);
    params->redirects.resize(redirects);
    params->redirects[redirects - 1].redirect_response = params->response;
    params->redirects[redirects - 1].new_url = new_url;
    params->redirects[redirects - 1].new_http_method = "GET";
    LoadRequest(new_url, &params->response, &error, data);
    DCHECK(!error);
  }

  params->is_static_data = true;
  params->body_loader =
      StaticDataNavigationBodyLoader::CreateWithData(std::move(data));
}

bool URLLoaderMockFactoryImpl::IsMockedURL(const blink::WebURL& url) {
  std::optional<WebURLError> error;
  ResponseInfo response_info;
  return LookupURL(url, &error, &response_info);
}

void URLLoaderMockFactoryImpl::CancelLoad(URLLoaderMock* loader) {
  pending_loaders_.erase(loader);
}

void URLLoaderMockFactoryImpl::LoadSynchronously(
    std::unique_ptr<network::ResourceRequest> request,
    WebURLResponse* response,
    std::optional<WebURLError>* error,
    scoped_refptr<SharedBuffer>& data,
    int64_t* encoded_data_length) {
  LoadRequest(WebURL(KURL(request->url)), response, error, data);
  *encoded_data_length = data->size();
}

void URLLoaderMockFactoryImpl::LoadAsynchronouly(
    std::unique_ptr<network::ResourceRequest> request,
    URLLoaderMock* loader) {
  DCHECK(!pending_loaders_.Contains(loader));
  pending_loaders_.Set(loader, std::move(request));
}

void URLLoaderMockFactoryImpl::RunUntilIdle() {
  if (platform_) {
    platform_->RunUntilIdle();
  } else {
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  }
}

void URLLoaderMockFactoryImpl::LoadRequest(const WebURL& url,
                                           WebURLResponse* response,
                                           std::optional<WebURLError>* error,
                                           scoped_refptr<SharedBuffer>& data) {
  ResponseInfo response_info;
  if (!LookupURL(url, error, &response_info)) {
    // Non mocked URLs should not have been passed to the default URLLoader.
    NOTREACHED_IN_MIGRATION() << url;
    return;
  }

  if (!*error && !ReadFile(response_info.file_path, data)) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  *response = response_info.response;
}

bool URLLoaderMockFactoryImpl::LookupURL(const WebURL& url,
                                         std::optional<WebURLError>* error,
                                         ResponseInfo* response_info) {
  URLToErrorMap::const_iterator error_iter = url_to_error_info_.find(url);
  if (error_iter != url_to_error_info_.end()) {
    *error = error_iter->value;
  }

  URLToResponseMap::const_iterator iter = url_to_response_info_.find(url);
  if (iter != url_to_response_info_.end()) {
    *response_info = iter->value;
    return true;
  }

  for (const auto& key_value_pair : protocol_to_response_info_) {
    String protocol = key_value_pair.key;
    if (url.ProtocolIs(protocol.Ascii().c_str())) {
      *response_info = key_value_pair.value;
      return true;
    }
  }

  return false;
}

// static
bool URLLoaderMockFactoryImpl::ReadFile(const base::FilePath& file_path,
                                        scoped_refptr<SharedBuffer>& data) {
  // If the path is empty then we return an empty file so tests can simulate
  // requests without needing to actually load files.
  if (file_path.empty()) {
    return true;
  }

  std::string buffer;
  if (!base::ReadFileToString(file_path, &buffer)) {
    return false;
  }

  data = SharedBuffer::Create(buffer.data(), buffer.size());
  return true;
}

}  // namespace blink
