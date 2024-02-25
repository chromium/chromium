// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/test_network_interceptor.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace headless {

namespace {

class RedirectLoader : public network::mojom::URLLoader {
 public:
  RedirectLoader(TestNetworkInterceptor::Impl* interceptor_impl,
                 content::URLLoaderInterceptor::RequestParams* request,
                 TestNetworkInterceptor::Response* response,
                 const std::string& url)
      : interceptor_impl_(interceptor_impl),
        receiver_(this, std::move(request->receiver)),
        client_(std::move(request->client)),
        url_request_(request->url_request),
        response_(response),
        url_(request->url_request.url) {
    receiver_.set_disconnect_handler(
        base::BindOnce([](RedirectLoader* self) { delete self; }, this));
    NotifyRedirect(std::move(url));
  }

  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  void NotifyRedirect(const std::string& location) {
    auto redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        url_request_.method, url_request_.url, url_request_.site_for_cookies,
        net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT,
        url_request_.referrer_policy, url_request_.referrer.spec(),
        response_->headers->response_code(), url_.Resolve(location),
        net::RedirectUtil::GetReferrerPolicyHeader(response_->headers.get()),
        false /* insecure_scheme_was_upgraded */, true);
    auto head = network::mojom::URLResponseHead::New();
    head->request_time = base::Time::Now();
    head->response_time = base::Time::Now();
    head->content_length = 0;
    head->encoded_data_length = 0;
    head->headers = response_->headers;
    url_ = redirect_info.new_url;
    method_ = redirect_info.new_method;
    client_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  const raw_ptr<TestNetworkInterceptor::Impl> interceptor_impl_;

  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  network::ResourceRequest url_request_;
  raw_ptr<TestNetworkInterceptor::Response> response_;
  GURL url_;
  std::string method_;
};

}  // namespace

class TestNetworkInterceptor::Impl {
 public:
  explicit Impl(base::WeakPtr<TestNetworkInterceptor> interceptor)
      : interceptor_(std::move(interceptor)) {}

  void InsertResponse(std::string url, Response response) {
    response_map_.emplace(StripFragment(url),
                          std::make_unique<Response>(std::move(response)));
  }

  Response* FindResponse(const std::string& method, const std::string& url) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&TestNetworkInterceptor::LogRequest,
                                  interceptor_, method, url));
    auto it = response_map_.find(StripFragment(url));
    return it == response_map_.end() ? nullptr : it->second.get();
  }

  bool RequestHandler(content::URLLoaderInterceptor::RequestParams* request) {
    Response* response = FindResponse(request->url_request.method,
                                      request->url_request.url.spec());
    if (!response)
      return false;

    std::string location;
    if (response->headers->IsRedirect(&location)) {
      new RedirectLoader(this, request, response, location);
      return true;
    }
    content::URLLoaderInterceptor::WriteResponse(
        response->raw_headers, response->body, request->client.get());
    return true;
  }

 private:
  std::string StripFragment(std::string url) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    return GURL(url).ReplaceComponents(replacements).spec();
  }

  std::map<std::string, std::unique_ptr<Response>> response_map_;
  base::WeakPtr<TestNetworkInterceptor> interceptor_;
};

void RedirectLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers /* unused */,
    const net::HttpRequestHeaders& modified_headers /* unused */,
    const net::HttpRequestHeaders& modified_cors_exempt_headers /* unused */,
    const std::optional<GURL>& new_url) {
  response_ = interceptor_impl_->FindResponse(method_, url_.spec());
  CHECK(response_) << "No content for " << url_.spec();
  std::string location;
  if (response_->headers->IsRedirect(&location)) {
    NotifyRedirect(location);
    return;
  }
  content::URLLoaderInterceptor::WriteResponse(response_->raw_headers,
                                               response_->body, client_.get());
  delete this;
}

TestNetworkInterceptor::Response::Response(const std::string data) {
  static const char kHeaderDelimiter[] = "\r\n\r\n";
  size_t end_of_headers = data.find(kHeaderDelimiter);
  CHECK(end_of_headers != std::string::npos);
  raw_headers = data.substr(0, end_of_headers);
  body = data.substr(end_of_headers + strlen(kHeaderDelimiter));
  headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers));
}

TestNetworkInterceptor::Response::Response(const std::string body,
                                           const std::string& mime_type)
    : raw_headers("HTTP/1.1 200 OK\r\nContent-Type: " + mime_type),
      body(std::move(body)) {
  headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers));
}

TestNetworkInterceptor::Response::Response(const Response& r) = default;
TestNetworkInterceptor::Response::Response(Response&& r) = default;

TestNetworkInterceptor::Response::~Response() {}

TestNetworkInterceptor::TestNetworkInterceptor() {
  impl_ = std::make_unique<Impl>(weak_factory_.GetWeakPtr());
  interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(&TestNetworkInterceptor::Impl::RequestHandler,
                          base::Unretained(impl_.get())));
}

TestNetworkInterceptor::~TestNetworkInterceptor() {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, impl_.release());
  interceptor_.reset();
}

void TestNetworkInterceptor::InsertResponse(std::string url,
                                            Response response) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Impl::InsertResponse, base::Unretained(impl_.get()),
                     std::move(url), std::move(response)));
}

void TestNetworkInterceptor::LogRequest(std::string method, std::string url) {
  urls_requested_.emplace_back(std::move(url));
  methods_requested_.emplace_back(std::move(method));
}

}  // namespace headless
