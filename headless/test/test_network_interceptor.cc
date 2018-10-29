// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/test_network_interceptor.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace headless {

using content::BrowserThread;

namespace {

class RedirectLoader : public network::mojom::URLLoader {
 public:
  RedirectLoader(TestNetworkInterceptor::Impl* interceptor_impl,
                 content::URLLoaderInterceptor::RequestParams* request,
                 TestNetworkInterceptor::Response* response,
                 const std::string& url)
      : interceptor_impl_(interceptor_impl),
        binding_(this, std::move(request->request)),
        client_(std::move(request->client)),
        url_request_(request->url_request),
        response_(response),
        url_(request->url_request.url) {
    binding_.set_connection_error_handler(
        base::BindOnce([](RedirectLoader* self) { delete self; }, this));
    NotifyRedirect(std::move(url));
  };

  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;

  void ProceedWithResponse() override { DCHECK(false); }
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

 private:
  void NotifyRedirect(const std::string& location) {
    auto redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        url_request_.method, url_request_.url, url_request_.site_for_cookies,
        net::URLRequest::FirstPartyURLPolicy::
            UPDATE_FIRST_PARTY_URL_ON_REDIRECT,
        url_request_.referrer_policy, url_request_.referrer.spec(),
        response_->headers.get(), response_->headers->response_code(),
        url_.Resolve(location), false /* insecure_scheme_was_upgraded */, true);
    network::ResourceResponseHead head;
    head.request_time = base::Time::Now();
    head.response_time = base::Time::Now();
    head.content_length = 0;
    head.encoded_data_length = 0;
    head.headers = response_->headers;
    url_ = redirect_info.new_url;
    method_ = redirect_info.new_method;
    client_->OnReceiveRedirect(redirect_info, head);
  }

  TestNetworkInterceptor::Impl* const interceptor_impl_;

  mojo::Binding<network::mojom::URLLoader> binding_;
  network::mojom::URLLoaderClientPtr client_;
  network::ResourceRequest url_request_;
  TestNetworkInterceptor::Response* response_;
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
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             base::BindOnce(&TestNetworkInterceptor::LogRequest,
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
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
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
      net::HttpUtil::AssembleRawHeaders(raw_headers.c_str(),
                                        raw_headers.size()));
}

TestNetworkInterceptor::Response::Response(const std::string body,
                                           const std::string& mime_type)
    : raw_headers("HTTP/1.1 200 OK\r\nContent-Type: " + mime_type),
      body(std::move(body)) {
  headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(raw_headers.c_str(),
                                        raw_headers.size()));
}

TestNetworkInterceptor::Response::Response(const Response& r) = default;
TestNetworkInterceptor::Response::Response(Response&& r) = default;

TestNetworkInterceptor::Response::~Response() {}

TestNetworkInterceptor::TestNetworkInterceptor() : weak_factory_(this) {
  impl_.reset(new Impl(weak_factory_.GetWeakPtr()));
  interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindRepeating(&TestNetworkInterceptor::Impl::RequestHandler,
                          base::Unretained(impl_.get())));
}

TestNetworkInterceptor::~TestNetworkInterceptor() {
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, impl_.release());
  interceptor_.reset();
}

void TestNetworkInterceptor::InsertResponse(std::string url,
                                            Response response) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&Impl::InsertResponse, base::Unretained(impl_.get()),
                     std::move(url), std::move(response)));
}

void TestNetworkInterceptor::LogRequest(std::string method, std::string url) {
  urls_requested_.emplace_back(std::move(url));
  methods_requested_.emplace_back(std::move(method));
}

}  // namespace headless
