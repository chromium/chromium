// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/public/test/content_browser_test.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
namespace test_server {
class EmbeddedTestServer;
struct HttpRequest;
}  // namespace test_server
}  // namespace net

namespace content {

class SignedExchangeHandlerFactory;

class PrefetchBrowserTestBase : public ContentBrowserTest {
 public:
  struct ResponseEntry {
    ResponseEntry();
    ResponseEntry(
        const std::string& content,
        const std::string& content_types = "text/html",
        const std::vector<std::pair<std::string, std::string>>& headers = {},
        net::HttpStatusCode code = net::HTTP_OK);
    ResponseEntry(const ResponseEntry&) = delete;
    ResponseEntry(ResponseEntry&& other);
    ResponseEntry& operator=(const ResponseEntry&) = delete;
    ResponseEntry& operator=(ResponseEntry&&);
    ~ResponseEntry();

    std::string content;
    std::string content_type;
    std::vector<std::pair<std::string, std::string>> headers;
    net::HttpStatusCode code;
  };

  struct ScopedSignedExchangeHandlerFactory {
    explicit ScopedSignedExchangeHandlerFactory(
        SignedExchangeHandlerFactory* factory);
    ~ScopedSignedExchangeHandlerFactory();
  };

  PrefetchBrowserTestBase();

  PrefetchBrowserTestBase(const PrefetchBrowserTestBase&) = delete;
  PrefetchBrowserTestBase& operator=(const PrefetchBrowserTestBase&) = delete;

  ~PrefetchBrowserTestBase() override;

  void SetUpOnMainThread() override;

 protected:
  class RequestCounter : public base::RefCountedThreadSafe<RequestCounter> {
   public:
    // Create a counter that is to be incremented when |path| on the
    // |test_server| is accessed. |waiter| can be optionally specified that will
    // be run after the counter is incremented. The counter value can be
    // obtained via GetRequestCount(). This class works across threads,
    // GetRequestCount can be called on any threads.
    static scoped_refptr<RequestCounter> CreateAndMonitor(
        net::EmbeddedTestServer* test_server,
        const std::string& path,
        base::RunLoop* waiter = nullptr);
    RequestCounter(const std::string& path, base::RunLoop* waiter);

    RequestCounter(const RequestCounter&) = delete;
    RequestCounter& operator=(const RequestCounter&) = delete;

    int GetRequestCount();

   private:
    friend base::RefCountedThreadSafe<RequestCounter>;
    ~RequestCounter();

    void OnRequest(const net::test_server::HttpRequest& request);

    base::OnceClosure waiter_closure_;
    const std::string path_;
    int request_count_ GUARDED_BY(lock_) = 0;
    base::Lock lock_;
  };

  void RegisterResponse(const std::string& url, ResponseEntry&& entry);

  std::unique_ptr<net::test_server::HttpResponse> ServeResponses(
      const net::test_server::HttpRequest& request);
  void OnPrefetchURLLoaderCalled();

  void RegisterRequestHandler(
      net::test_server::EmbeddedTestServer* test_server);
  void NavigateToURLAndWaitTitle(const GURL& url, const std::string& title);
  void WaitUntilLoaded(const GURL& url);

  int GetPrefetchURLLoaderCallCount();

  // Register a callback to be called just before the `PrefetchURLLoader` is
  // created and started.
  void RegisterPrefetchLoaderCallback(base::OnceClosure callback);

 private:
  std::map<std::string, ResponseEntry> response_map_;

  base::OnceClosure prefetch_loader_callback_;

  int prefetch_url_loader_called_ GUARDED_BY(lock_) = 0;
  base::Lock lock_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_BROWSERTEST_BASE_H_
