// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_H_
#define IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#import "ios/web/public/test/http_server/response_provider.h"

namespace base {
class FilePath;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
struct HttpRequest;
}
}

namespace web {
class ResponseProvider;
}

namespace web {
namespace test {

// A convience class for wrapping a ResponseProvider so that it can be used
// inside data structures that operate on ref counted objects. This class is a
// ref counted container for a ResponseProvider.
// This object exists for legacy reasons since a large part of the code base
// still uses ResponseProviders that are not ref counted.
class RefCountedResponseProviderWrapper
    : public base::RefCounted<RefCountedResponseProviderWrapper> {
 public:
  // Main constructor.
  explicit RefCountedResponseProviderWrapper(
      std::unique_ptr<ResponseProvider> response_provider);
  // Returns the ResponseProvider that backs this object.
  ResponseProvider* GetResponseProvider() { return response_provider_.get(); }

 private:
  friend class base::RefCounted<RefCountedResponseProviderWrapper>;
  // The ResponseProvider that backs this object.
  std::unique_ptr<ResponseProvider> response_provider_;
  virtual ~RefCountedResponseProviderWrapper();
};

// The HttpServer is an in-process web server that is used to service requests.
// It is a singleton and backed by a EmbeddedTestServer.
// HttpServer can be configured to serve requests by registering
// web::ResponseProviders.
// This class is not thread safe on the whole and only certain methods are
// thread safe.
class HttpServer : public base::RefCountedThreadSafe<HttpServer> {
 public:
  typedef std::vector<std::unique_ptr<ResponseProvider>> ProviderList;

  // Returns the shared HttpServer instance. Thread safe.
  static HttpServer& GetSharedInstance();
  // Returns the shared HttpServer instance and registers the response providers
  // as well. Takes ownership of the response providers. Must be called from the
  // main thread.
  static HttpServer& GetSharedInstanceWithResponseProviders(
      ProviderList response_providers);

  // A convenience method for the longer form of
  // |web::test::HttpServer::GetSharedInstance().MakeUrlForHttpServer|
  static GURL MakeUrl(const std::string& url);
  // Starts the server on the default port 8080. CHECKs if the server can not be
  // started.
  // Must be called from the main thread.
  void StartOrDie(const base::FilePath& files_path);
  // Stops the server and prevents it from accepting new requests.
  // Must be called from the main thread.
  void Stop();
  // Sets the server to hang and return no response.
  void SetSuspend(bool suspended);
  // Returns true if the server is running.
  // Must be called from the main thread.
  bool IsRunning() const;

  // Returns the port that the server is running on. Thread Safe
  NSUInteger GetPort() const;

  // Adds a ResponseProvider. Takes ownership of the ResponseProvider.
  // Note for using URLs inside of the |response_provider|:
  // The HttpServer cannot run on default HTTP port 80, so URLs used in
  // ResponseProviders must be converted at runtime after the HttpServer's port
  // is determined. Please use |MakeUrl| to handle converting URLs.
  // Must be called from the main thread.
  void AddResponseProvider(std::unique_ptr<ResponseProvider> response_provider);
  // Removes the |response_provider|. Must be called from the main thread.
  void RemoveResponseProvider(ResponseProvider* response_provider);
  // Removes all the response providers. Must be called from the main thread.
  void RemoveAllResponseProviders();

 private:
  friend class base::RefCountedThreadSafe<HttpServer>;
  HttpServer();
  ~HttpServer();

  // Sets the port that the server is running on. Thread Safe
  void SetPort(NSUInteger port);

  // Creates a GURL that the server can service based on the |url|
  // passed in.
  // It does not rewrite URLs if the |url| can already be serviced by the
  // server.
  // |url| must be a valid URL. Thread safe.
  GURL MakeUrlForHttpServer(const std::string& url) const;

  // Returns the response provider that can handle the |request|.
  // Note: No more than one response provider can handle the request.
  // Thread safe.
  scoped_refptr<RefCountedResponseProviderWrapper>
  GetResponseProviderForRequest(const web::ResponseProvider::Request& request);

  // Lock for serializing access to |provider_|.
  mutable base::Lock provider_list_lock_;
  // Lock for serializing access to |port_|.
  mutable base::Lock port_lock_;
  // The port that the server is running on. 0 if the server is not running.
  NSUInteger port_;
  // The EmbeddedTestServer backing the HttpServer.
  std::unique_ptr<net::test_server::EmbeddedTestServer> embedded_test_server_;
  // The list of providers to service a request.
  std::vector<scoped_refptr<RefCountedResponseProviderWrapper>> providers_;
  // Returns the response providers for a request.
  ResponseProvider* GetResponseProviderForProviderRequest(
      const web::ResponseProvider::Request& request);
  // Provides a shim between EmbeddedTestServer and ResponseProvider. This
  // handler converts an EmbeddedTestServer request to ResponseProvider
  // request, look up the corresponding providers, and then converts the
  // ResponseProvider response back to EmbeddedTestServer response.
  std::unique_ptr<net::test_server::HttpResponse> GetResponse(
      const net::test_server::HttpRequest& request);
  // Status tracking if the server is hung.
  bool isSuspended;
  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

}  // namespace test
}  // namspace web

#endif  // IOS_WEB_PUBLIC_TEST_HTTP_SERVER_HTTP_SERVER_H_
