// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_RESOLUTION_MOCK_PROXY_RESOLVER_H_
#define NET_PROXY_RESOLUTION_MOCK_PROXY_RESOLVER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/proxy_resolution/proxy_resolver.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "url/gurl.h"

namespace net {

// Asynchronous mock proxy resolver. All requests complete asynchronously,
// user must call Job::CompleteNow() on a pending request to signal it.
class MockAsyncProxyResolver : public ProxyResolver {
 public:
  class Job {
   public:
    Job(MockAsyncProxyResolver* resolver,
        const GURL& url,
        ProxyInfo* results,
        CompletionOnceCallback callback);

    const GURL& url() const { return url_; }
    ProxyInfo* results() const { return results_; }
    MockAsyncProxyResolver* Resolver() const { return resolver_; }

    void CompleteNow(int rv);

    ~Job();

   private:
    raw_ptr<MockAsyncProxyResolver> resolver_;
    const GURL url_;
    raw_ptr<ProxyInfo, DanglingUntriaged> results_;
    CompletionOnceCallback callback_;
  };

  class RequestImpl : public ProxyResolver::Request {
   public:
    explicit RequestImpl(std::unique_ptr<Job> job);

    ~RequestImpl() override;

    LoadState GetLoadState() override;

   private:
    std::unique_ptr<Job> job_;
  };

  MockAsyncProxyResolver();
  ~MockAsyncProxyResolver() override;

  // ProxyResolver implementation.
  int GetProxyForURL(const GURL& url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& /*net_log*/) override;
  const std::vector<raw_ptr<Job, VectorExperimental>>& pending_jobs() const {
    return pending_jobs_;
  }

  const std::vector<std::unique_ptr<Job>>& cancelled_jobs() const {
    return cancelled_jobs_;
  }

  void AddCancelledJob(std::unique_ptr<Job> job);
  void RemovePendingJob(Job* job);

 private:
  std::vector<raw_ptr<Job, VectorExperimental>> pending_jobs_;
  std::vector<std::unique_ptr<Job>> cancelled_jobs_;
};

// Asynchronous mock proxy resolver factory . All requests complete
// asynchronously; the user must call Request::CompleteNow() on a pending
// request to signal it.
class MockAsyncProxyResolverFactory : public ProxyResolverFactory {
 public:
  class Request;
  using RequestsList = std::vector<scoped_refptr<Request>>;

  explicit MockAsyncProxyResolverFactory(bool resolvers_expect_pac_bytes);
  ~MockAsyncProxyResolverFactory() override;

  int CreateProxyResolver(
      const scoped_refptr<PacFileData>& pac_script,
      std::unique_ptr<ProxyResolver>* resolver,
      CompletionOnceCallback callback,
      std::unique_ptr<ProxyResolverFactory::Request>* request) override;

  const RequestsList& pending_requests() const { return pending_requests_; }

  const RequestsList& cancelled_requests() const { return cancelled_requests_; }

  void RemovePendingRequest(Request* request);

 private:
  class Job;
  RequestsList pending_requests_;
  RequestsList cancelled_requests_;
};

class MockAsyncProxyResolverFactory::Request
    : public base::RefCounted<Request> {
 public:
  Request(MockAsyncProxyResolverFactory* factory,
          const scoped_refptr<PacFileData>& script_data,
          std::unique_ptr<ProxyResolver>* resolver,
          CompletionOnceCallback callback);

  const scoped_refptr<PacFileData>& script_data() const { return script_data_; }

  // Completes this request. A ForwardingProxyResolver that forwards to
  // |resolver| will be returned to the requester. |resolver| must not be
  // null and must remain as long as the resolver returned by this request
  // remains in use.
  void CompleteNowWithForwarder(int rv, ProxyResolver* resolver);

  void CompleteNow(int rv, std::unique_ptr<ProxyResolver> resolver);

 private:
  friend class base::RefCounted<Request>;
  friend class MockAsyncProxyResolverFactory;
  friend class MockAsyncProxyResolverFactory::Job;

  ~Request();

  void FactoryDestroyed();

  raw_ptr<MockAsyncProxyResolverFactory> factory_;
  const scoped_refptr<PacFileData> script_data_;
  raw_ptr<std::unique_ptr<ProxyResolver>> resolver_;
  CompletionOnceCallback callback_;
};

// ForwardingProxyResolver forwards all requests to |impl|. |impl| must remain
// so long as this remains in use.
class ForwardingProxyResolver : public ProxyResolver {
 public:
  explicit ForwardingProxyResolver(ProxyResolver* impl);

  ForwardingProxyResolver(const ForwardingProxyResolver&) = delete;
  ForwardingProxyResolver& operator=(const ForwardingProxyResolver&) = delete;

  // ProxyResolver overrides.
  int GetProxyForURL(const GURL& query_url,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     ProxyInfo* results,
                     CompletionOnceCallback callback,
                     std::unique_ptr<Request>* request,
                     const NetLogWithSource& net_log) override;

 private:
  raw_ptr<ProxyResolver> impl_;
};

}  // namespace net

#endif  // NET_PROXY_RESOLUTION_MOCK_PROXY_RESOLVER_H_
