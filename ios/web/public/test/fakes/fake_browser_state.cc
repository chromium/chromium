// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_browser_state.h"

#include "base/files/file_path.h"
#include "base/task/single_thread_task_runner.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ios/web/test/test_url_constants.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"

namespace web {

namespace {

class TestContextURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestContextURLRequestContextGetter(web::BrowserState* browser_state) {
    job_factory_.SetProtocolHandler(
        kTestWebUIScheme,
        web::URLDataManagerIOSBackend::CreateProtocolHandler(browser_state));
    context_.set_job_factory(&job_factory_);
  }

  net::URLRequestContext* GetURLRequestContext() override { return &context_; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return web::GetIOThreadTaskRunner({});
  }

 private:
  ~TestContextURLRequestContextGetter() override {}

  net::TestURLRequestContext context_;
  net::URLRequestJobFactory job_factory_;
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
  if (!request_context_)
    request_context_ = new TestContextURLRequestContextGetter(this);
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

}  // namespace web
