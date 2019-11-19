// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/http_server/http_server.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/net/url_test_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Converts a net::test_server::HttpRequest (received from the
// EmbeddedTestServer servlet) to a request object that the ResponseProvider
// expects.
web::ResponseProvider::Request
ResponseProviderRequestFromEmbeddedTestServerRequest(
    const net::test_server::HttpRequest& request) {
  net::HttpRequestHeaders headers;
  for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
    headers.SetHeader(it->first, it->second);
  }
  return web::ResponseProvider::Request(request.GetURL(), request.method_string,
                                        request.content, headers);
}

}  // namespace

namespace web {
namespace test {

RefCountedResponseProviderWrapper::RefCountedResponseProviderWrapper(
    std::unique_ptr<ResponseProvider> response_provider)
    : response_provider_(std::move(response_provider)) {}

RefCountedResponseProviderWrapper::~RefCountedResponseProviderWrapper() {}

// static
HttpServer& HttpServer::GetSharedInstance() {
  static web::test::HttpServer* shared_instance = nullptr;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    shared_instance = new HttpServer();
    shared_instance->AddRef();
  });
  return *shared_instance;
}

// static
HttpServer& HttpServer::GetSharedInstanceWithResponseProviders(
    ProviderList response_providers) {
  DCHECK([NSThread isMainThread]);
  HttpServer& server = HttpServer::GetSharedInstance();
  // Use non-const reference as the response_provider ownership is transferred.
  for (std::unique_ptr<ResponseProvider>& provider : response_providers)
    server.AddResponseProvider(std::move(provider));
  return server;
}

std::unique_ptr<net::test_server::HttpResponse> HttpServer::GetResponse(
    const net::test_server::HttpRequest& request) {
  if (isSuspended) {
    return std::make_unique<net::test_server::HungResponse>();
  }
  ResponseProvider::Request provider_request =
      ResponseProviderRequestFromEmbeddedTestServerRequest(request);
  ResponseProvider* response_provider =
      GetResponseProviderForProviderRequest(provider_request);

  if (!response_provider) {
    return nullptr;
  }
  return response_provider->GetEmbeddedTestServerResponse(provider_request);
}

ResponseProvider* HttpServer::GetResponseProviderForProviderRequest(
    const web::ResponseProvider::Request& request) {
  auto response_provider = GetResponseProviderForRequest(request);
  if (!response_provider) {
    return nullptr;
  }
  return response_provider->GetResponseProvider();
}

HttpServer::HttpServer() : port_(0) {}

HttpServer::~HttpServer() {}

void HttpServer::StartOrDie(const base::FilePath& files_path) {
  DCHECK([NSThread isMainThread]);

  // Registers request handler which serves files from the http test files
  // directory. The current tests calls full path relative to DIR_SOURCE_ROOT.
  // Registers the DIR_SOURCE_ROOT to avoid massive test changes.
  embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>();
  embedded_test_server_->ServeFilesFromDirectory(files_path);
  embedded_test_server_->RegisterDefaultHandler(
      base::Bind(&HttpServer::GetResponse, this));

  if (embedded_test_server_->Start()) {
    SetPort((NSUInteger)embedded_test_server_->port());
  }
  isSuspended = NO;
  CHECK(IsRunning());
}

void HttpServer::Stop() {
  DCHECK([NSThread isMainThread]);
  DCHECK(IsRunning()) << "Cannot stop an already stopped server.";
  RemoveAllResponseProviders();
  // TODO(crbug.com/711723): Re-write the Stop() function when shutting down
  // server works for iOS.
  embedded_test_server_.release();
  SetPort(0);
}

void HttpServer::SetSuspend(bool suspended) {
  isSuspended = suspended;
}

bool HttpServer::IsRunning() const {
  DCHECK([NSThread isMainThread]);
  if (embedded_test_server_ == nullptr) {
    return false;
  }
  return embedded_test_server_->Started();
}

NSUInteger HttpServer::GetPort() const {
  base::AutoLock autolock(port_lock_);
  return port_;
}

// static
GURL HttpServer::MakeUrl(const std::string& url) {
  return HttpServer::GetSharedInstance().MakeUrlForHttpServer(url);
}

GURL HttpServer::MakeUrlForHttpServer(const std::string& url) const {
  GURL result(url);
  DCHECK(result.is_valid());
  return embedded_test_server_->GetURL(
      "/" + net::GetContentAndFragmentForUrl(result));
}

scoped_refptr<RefCountedResponseProviderWrapper>
HttpServer::GetResponseProviderForRequest(
    const web::ResponseProvider::Request& request) {
  base::AutoLock autolock(provider_list_lock_);
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in HTTPServer.
  base::ScopedAllowCrossThreadRefCountAccess
      allow_cross_thread_ref_count_access;
  scoped_refptr<RefCountedResponseProviderWrapper> result;
  for (const auto& ref_counted_response_provider : providers_) {
    ResponseProvider* response_provider =
        ref_counted_response_provider.get()->GetResponseProvider();
    if (response_provider->CanHandleRequest(request)) {
      DCHECK(!result)
          << "No more than one response provider can handle the same request.";
      result = ref_counted_response_provider;
    }
  }
  return result;
}

void HttpServer::AddResponseProvider(
    std::unique_ptr<ResponseProvider> response_provider) {
  DCHECK([NSThread isMainThread]);
  DCHECK(IsRunning()) << "Can add a response provider only when the server is "
                      << "running.";
  base::AutoLock autolock(provider_list_lock_);
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in HTTPServer.
  base::ScopedAllowCrossThreadRefCountAccess
      allow_cross_thread_ref_count_access;
  scoped_refptr<RefCountedResponseProviderWrapper>
      ref_counted_response_provider(
          new RefCountedResponseProviderWrapper(std::move(response_provider)));
  providers_.push_back(ref_counted_response_provider);
}

void HttpServer::RemoveResponseProvider(ResponseProvider* response_provider) {
  DCHECK([NSThread isMainThread]);
  base::AutoLock autolock(provider_list_lock_);
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in HTTPServer.
  base::ScopedAllowCrossThreadRefCountAccess
      allow_cross_thread_ref_count_access;
  auto found_iter = providers_.end();
  for (auto it = providers_.begin(); it != providers_.end(); ++it) {
    if ((*it)->GetResponseProvider() == response_provider) {
      found_iter = it;
      break;
    }
  }
  if (found_iter != providers_.end()) {
    providers_.erase(found_iter);
  }
}

void HttpServer::RemoveAllResponseProviders() {
  DCHECK([NSThread isMainThread]);
  base::AutoLock autolock(provider_list_lock_);
  // Relax the cross-thread access restriction to non-thread-safe RefCount.
  // The lock above protects non-thread-safe RefCount in HTTPServer.
  base::ScopedAllowCrossThreadRefCountAccess
      allow_cross_thread_ref_count_access;
  providers_.clear();
}

void HttpServer::SetPort(NSUInteger port) {
  base::AutoLock autolock(port_lock_);
  port_ = port;
}

}  // namespace test
}  // namespace web
