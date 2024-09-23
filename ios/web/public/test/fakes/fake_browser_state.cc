// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_browser_state.h"

#include "base/files/file_path.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/test/test_url_constants.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"

namespace web {

namespace {

class TestContextURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestContextURLRequestContextGetter(
      web::BrowserState* browser_state,
      std::unique_ptr<net::CookieStore> cookie_store) {
    auto context_builder = net::CreateTestURLRequestContextBuilder();
    if (cookie_store) {
      context_builder->SetCookieStore(std::move(cookie_store));
    }
    context_builder->SetProtocolHandler(
        kTestWebUIScheme,
        web::URLDataManagerIOSBackend::CreateProtocolHandler(browser_state));
    context_ = context_builder->Build();
  }

  net::URLRequestContext* GetURLRequestContext() override {
    return context_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return web::GetIOThreadTaskRunner({});
  }

 private:
  ~TestContextURLRequestContextGetter() override {}

  std::unique_ptr<net::URLRequestContext> context_;
};

}  // namespace

// static
const char FakeBrowserState::kCorsExemptTestHeaderName[] = "ExemptTest";

FakeBrowserState::FakeBrowserState() : is_off_the_record_(false) {}

FakeBrowserState::~FakeBrowserState() {}

bool FakeBrowserState::IsOffTheRecord() const {
  return is_off_the_record_;
}

base::FilePath FakeBrowserState::GetStatePath() const {
  return base::FilePath();
}

net::URLRequestContextGetter* FakeBrowserState::GetRequestContext() {
  if (!request_context_) {
    request_context_ = base::MakeRefCounted<TestContextURLRequestContextGetter>(
        this, std::move(cookie_store_));
  }
  return request_context_.get();
}

void FakeBrowserState::UpdateCorsExemptHeader(
    network::mojom::NetworkContextParams* params) {
  DCHECK(params);
  params->cors_exempt_header_list.push_back(kCorsExemptTestHeaderName);
}

void FakeBrowserState::SetOffTheRecord(bool flag) {
  is_off_the_record_ = flag;
}

void FakeBrowserState::SetCookieStore(
    std::unique_ptr<net::CookieStore> cookie_store) {
  DCHECK(!request_context_);
  cookie_store_ = std::move(cookie_store);
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeBrowserState::GetSharedURLLoaderFactory() {
  return test_shared_url_loader_factory_
             ? test_shared_url_loader_factory_
             : BrowserState::GetSharedURLLoaderFactory();
}

void FakeBrowserState::SetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  test_shared_url_loader_factory_ = std::move(shared_url_loader_factory);
}

const std::string& FakeBrowserState::GetWebKitStorageID() const {
  return storage_uuid_;
}

void FakeBrowserState::SetWebKitStorageID(std::string uuid) {
  storage_uuid_ = uuid;
}

}  // namespace web
