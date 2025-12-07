// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_browsertest_base.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/loader/prefetch_url_loader_service_context.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"

namespace content {

PrefetchBrowserTestBase::ResponseEntry::ResponseEntry() = default;

PrefetchBrowserTestBase::ResponseEntry::ResponseEntry(
    const std::string& content,
    const std::string& content_type,
    const std::vector<std::pair<std::string, std::string>>& headers,
    net::HttpStatusCode code)
    : content(content),
      content_type(content_type),
      headers(headers),
      code(code) {}

PrefetchBrowserTestBase::ResponseEntry::ResponseEntry(ResponseEntry&& other) =
    default;

PrefetchBrowserTestBase::ResponseEntry::~ResponseEntry() = default;

PrefetchBrowserTestBase::ResponseEntry& PrefetchBrowserTestBase::ResponseEntry::
operator=(ResponseEntry&& other) = default;

PrefetchBrowserTestBase::ScopedSignedExchangeHandlerFactory::
    ScopedSignedExchangeHandlerFactory(SignedExchangeHandlerFactory* factory) {
  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(factory);
}

PrefetchBrowserTestBase::ScopedSignedExchangeHandlerFactory::
    ~ScopedSignedExchangeHandlerFactory() {
  SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(nullptr);
}

PrefetchBrowserTestBase::PrefetchBrowserTestBase() = default;
PrefetchBrowserTestBase::~PrefetchBrowserTestBase() = default;

void PrefetchBrowserTestBase::SetUpOnMainThread() {
  ContentBrowserTest::SetUpOnMainThread();
  StoragePartitionImpl* partition =
      static_cast<StoragePartitionImpl*>(shell()
                                             ->web_contents()
                                             ->GetBrowserContext()
                                             ->GetDefaultStoragePartition());
  partition->GetSubresourceProxyingURLLoaderService()
      ->prefetch_url_loader_service_context_for_testing()
      .RegisterPrefetchLoaderCallbackForTest(base::BindRepeating(
          &PrefetchBrowserTestBase::OnPrefetchURLLoaderCalled,
          base::Unretained(this)));
}

void PrefetchBrowserTestBase::RegisterResponse(const std::string& url,
                                               ResponseEntry&& entry) {
  response_map_[url] = std::move(entry);
}

std::unique_ptr<net::test_server::HttpResponse>
PrefetchBrowserTestBase::ServeResponses(
    const net::test_server::HttpRequest& request) {
  auto found = response_map_.find(request.relative_url);
  if (found != response_map_.end()) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(found->second.code);
    response->set_content(found->second.content);
    response->set_content_type(found->second.content_type);
    for (const auto& header : found->second.headers) {
      response->AddCustomHeader(header.first, header.second);
    }
    return std::move(response);
  }
  return nullptr;
}

void PrefetchBrowserTestBase::OnPrefetchURLLoaderCalled() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  prefetch_url_loader_called_++;

  if (prefetch_loader_callback_) {
    std::move(prefetch_loader_callback_).Run();
  }
}

int PrefetchBrowserTestBase::GetPrefetchURLLoaderCallCount() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock lock(lock_);
  return prefetch_url_loader_called_;
}

void PrefetchBrowserTestBase::RegisterRequestHandler(
    net::EmbeddedTestServer* test_server) {
  test_server->RegisterRequestHandler(base::BindRepeating(
      &PrefetchBrowserTestBase::ServeResponses, base::Unretained(this)));
}

void PrefetchBrowserTestBase::NavigateToURLAndWaitTitle(
    const GURL& url,
    const std::string& title) {
  // If the current page already has the specified title, then the
  // `WaitAndGetTitle()` call may return to early (for instance, before
  // resources that might change the title have a chance to load. Callers of
  // this method likely want the navigation to complete first, so check for
  // this case beforehand to avoid surprises!
  EXPECT_FALSE(EvalJs(shell()->web_contents(),
                      JsReplace("document.title === $1;", title))
                   .ExtractBool())
      << "Title already matches prior to the navigation";

  std::u16string title16 = base::ASCIIToUTF16(title);
  TitleWatcher title_watcher(shell()->web_contents(), title16);
  // Execute the JavaScript code to trigger the followup navigation from the
  // current page.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), JsReplace("location.href = $1;", url)));
  EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
}

void PrefetchBrowserTestBase::WaitUntilLoaded(const GURL& url) {
  std::string script = R"(
    new Promise((resolve) => {
      const url = $1;
      if (performance.getEntriesByName(url).length > 0) {
        resolve();
        return;
      }
      new PerformanceObserver((list) => {
        if (list.getEntriesByName(url).length > 0) {
          resolve();
        }
      }).observe({ entryTypes: ['resource'] });
    })
  )";

  ASSERT_TRUE(ExecJs(shell()->web_contents(), JsReplace(script, url)));
}

void PrefetchBrowserTestBase::RegisterPrefetchLoaderCallback(
    base::OnceClosure callback) {
  prefetch_loader_callback_ = std::move(callback);
}

// static
scoped_refptr<PrefetchBrowserTestBase::RequestCounter>
PrefetchBrowserTestBase::RequestCounter::CreateAndMonitor(
    net::EmbeddedTestServer* test_server,
    const std::string& path,
    base::RunLoop* waiter) {
  auto counter = base::MakeRefCounted<RequestCounter>(path, waiter);
  test_server->RegisterRequestMonitor(
      base::BindRepeating(&RequestCounter::OnRequest, counter));
  return counter;
}

PrefetchBrowserTestBase::RequestCounter::RequestCounter(const std::string& path,
                                                        base::RunLoop* waiter)
    : waiter_closure_(waiter ? waiter->QuitClosure() : base::OnceClosure()),
      path_(path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PrefetchBrowserTestBase::RequestCounter::~RequestCounter() = default;

int PrefetchBrowserTestBase::RequestCounter::GetRequestCount() {
  base::AutoLock lock(lock_);
  return request_count_;
}

void PrefetchBrowserTestBase::RequestCounter::OnRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != path_) {
    return;
  }
  base::AutoLock lock(lock_);
  ++request_count_;
  if (waiter_closure_) {
    std::move(waiter_closure_).Run();
  }
}

}  // namespace content
