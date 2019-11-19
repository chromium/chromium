// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/test_url_request_interceptor.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/test/url_request/url_request_test_job_backed_by_file.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// This class is needed because URLRequestTestJobBackedByFile always returns a
// -1 HTTP response status code.
class TestURLRequestJob : public URLRequestTestJobBackedByFile {
 public:
  TestURLRequestJob(URLRequest* request,
                    NetworkDelegate* network_delegate,
                    const base::FilePath& file_path,
                    const scoped_refptr<base::TaskRunner>& worker_task_runner)
      : URLRequestTestJobBackedByFile(request,
                                      network_delegate,
                                      file_path,
                                      worker_task_runner) {}

  void GetResponseInfo(HttpResponseInfo* info) override {
    info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");
  }

 private:
  ~TestURLRequestJob() override = default;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJob);
};

}  // namespace

// This class handles the actual URL request interception. It may be constructed
// on any thread, but all other methods are called on the |network_task_runner|
// thread. It is destroyed by the URLRequestFilter singleton.
class TestURLRequestInterceptor::Delegate : public URLRequestInterceptor {
 public:
  Delegate(const std::string& scheme,
           const std::string& hostname,
           const scoped_refptr<base::TaskRunner>& network_task_runner,
           const scoped_refptr<base::TaskRunner>& worker_task_runner)
      : scheme_(scheme),
        hostname_(hostname),
        network_task_runner_(network_task_runner),
        worker_task_runner_(worker_task_runner),
        hit_count_(0) {}
  ~Delegate() override = default;

  void Register() {
    URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        scheme_, hostname_, std::unique_ptr<URLRequestInterceptor>(this));
  }

  static void Unregister(const std::string& scheme,
                         const std::string& hostname) {
    URLRequestFilter::GetInstance()->RemoveHostnameHandler(scheme, hostname);
  }

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname and scheme of |url| must match the corresponding parameters
  // passed as constructor arguments.
  void SetResponse(const GURL& url,
                   const base::FilePath& path,
                   bool ignore_query) {
    DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
    if (ignore_query) {
      ignore_query_responses_[url] = path;
    } else {
      responses_[url] = path;
    }
  }

  // Returns how many requests have been issued that have a stored reply.
  int GetHitCount() const {
    base::AutoLock auto_lock(hit_count_lock_);
    return hit_count_;
  }

 private:
  typedef std::map<GURL, base::FilePath> ResponseMap;

  // When computing matches, this ignores the query parameters of the url.
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
    if (request->url().scheme() != scheme_ ||
        request->url().host() != hostname_) {
      return nullptr;
    }

    auto it = responses_.find(request->url());
    if (it == responses_.end()) {
      // Search for this request's url, ignoring any query parameters.
      GURL url = request->url();
      if (url.has_query()) {
        GURL::Replacements replacements;
        replacements.ClearQuery();
        url = url.ReplaceComponents(replacements);
      }
      it = ignore_query_responses_.find(url);
      if (it == ignore_query_responses_.end())
        return nullptr;
    }
    {
      base::AutoLock auto_lock(hit_count_lock_);
      ++hit_count_;
    }

    return new TestURLRequestJob(
        request, network_delegate, it->second, worker_task_runner_);
  }

  const std::string scheme_;
  const std::string hostname_;

  const scoped_refptr<base::TaskRunner> network_task_runner_;
  const scoped_refptr<base::TaskRunner> worker_task_runner_;

  ResponseMap responses_;
  ResponseMap ignore_query_responses_;

  mutable base::Lock hit_count_lock_;
  mutable int hit_count_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

TestURLRequestInterceptor::TestURLRequestInterceptor(
    const std::string& scheme,
    const std::string& hostname,
    const scoped_refptr<base::TaskRunner>& network_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner)
    : scheme_(scheme),
      hostname_(hostname),
      network_task_runner_(network_task_runner),
      delegate_(new Delegate(scheme,
                             hostname,
                             network_task_runner_,
                             worker_task_runner)) {
  network_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Delegate::Register, base::Unretained(delegate_)));
}

TestURLRequestInterceptor::~TestURLRequestInterceptor() {
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::Unregister, scheme_, hostname_));
}

void TestURLRequestInterceptor::SetResponse(const GURL& url,
                                            const base::FilePath& path) {
  CHECK_EQ(scheme_, url.scheme());
  CHECK_EQ(hostname_, url.host());
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::SetResponse,
                                base::Unretained(delegate_), url, path, false));
}

void TestURLRequestInterceptor::SetResponseIgnoreQuery(
    const GURL& url,
    const base::FilePath& path) {
  CHECK_EQ(scheme_, url.scheme());
  CHECK_EQ(hostname_, url.host());
  network_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Delegate::SetResponse,
                                base::Unretained(delegate_), url, path, true));
}

int TestURLRequestInterceptor::GetHitCount() {
  return delegate_->GetHitCount();
}

LocalHostTestURLRequestInterceptor::LocalHostTestURLRequestInterceptor(
    const scoped_refptr<base::TaskRunner>& network_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner)
    : TestURLRequestInterceptor("http",
                                "localhost",
                                network_task_runner,
                                worker_task_runner) {
}

}  // namespace net
