// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
#define NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/base/ip_endpoint.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {

// Changes URLFetcher's Factory for the lifetime of the object.
// Note that this scoper cannot be nested (to make it even harder to misuse).
class ScopedURLFetcherFactory {
 public:
  explicit ScopedURLFetcherFactory(URLFetcherFactory* factory);
  virtual ~ScopedURLFetcherFactory();

 private:
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ScopedURLFetcherFactory);
};

// TestURLFetcher and TestURLFetcherFactory are used for testing consumers of
// URLFetcher. TestURLFetcherFactory is a URLFetcherFactory that creates
// TestURLFetchers. TestURLFetcher::Start is overriden to do nothing. It is
// expected that you'll grab the delegate from the TestURLFetcher and invoke
// the callback method when appropriate. In this way it's easy to mock a
// URLFetcher.
// Typical usage:
//   // TestURLFetcher requires a MessageLoop and an IO thread to release
//   // URLRequestContextGetter in URLFetcher::Core.
//   BrowserTaskEnvironment task_environment_;
//   // Create factory (it automatically sets itself as URLFetcher's factory).
//   TestURLFetcherFactory factory;
//   // Do something that triggers creation of a URLFetcher.
//   ...
//   TestURLFetcher* fetcher = factory.GetFetcherByID(expected_id);
//   DCHECK(fetcher);
//   // Notify delegate with whatever data you want.
//   fetcher->delegate()->OnURLFetchComplete(...);
//   // Make sure consumer of URLFetcher does the right thing.
//   ...
//
// Note: if you don't know when your request objects will be created you
// might want to use the FakeURLFetcher and FakeURLFetcherFactory classes
// below.

class TestURLFetcherFactory;
class TestURLFetcher : public URLFetcher {
 public:
  // Interface for tests to intercept production code classes using URLFetcher.
  // Allows even-driven mock server classes to analyze the correctness of
  // requests / uploads events and forge responses back at the right moment.
  class DelegateForTests {
   public:
    // Callback issued correspondingly to the call to the |Start()| method.
    virtual void OnRequestStart(int fetcher_id) = 0;

    // Callback issued correspondingly to the call to |AppendChunkToUpload|.
    // Uploaded chunks can be retrieved with the |upload_chunks()| getter.
    virtual void OnChunkUpload(int fetcher_id) = 0;

    // Callback issued correspondingly to the destructor.
    virtual void OnRequestEnd(int fetcher_id) = 0;
  };

  TestURLFetcher(int id,
                 const GURL& url,
                 URLFetcherDelegate* d);
  ~TestURLFetcher() override;

  // URLFetcher implementation
  void SetUploadData(const std::string& upload_content_type,
                     const std::string& upload_content) override;
  void SetUploadFilePath(
      const std::string& upload_content_type,
      const base::FilePath& file_path,
      uint64_t range_offset,
      uint64_t range_length,
      scoped_refptr<base::TaskRunner> file_task_runner) override;
  void SetUploadStreamFactory(
      const std::string& upload_content_type,
      const CreateUploadStreamCallback& callback) override;
  void SetChunkedUpload(const std::string& upload_content_type) override;
  // Overriden to cache the chunks uploaded. Caller can read back the uploaded
  // chunks with the upload_chunks() accessor.
  void AppendChunkToUpload(const std::string& data,
                           bool is_last_chunk) override;
  void SetLoadFlags(int load_flags) override;
  int GetLoadFlags() const override;
  void SetAllowCredentials(bool allow_credentials) override {}
  void SetReferrer(const std::string& referrer) override;
  void SetReferrerPolicy(URLRequest::ReferrerPolicy referrer_policy) override;
  void SetExtraRequestHeaders(
      const std::string& extra_request_headers) override;
  void AddExtraRequestHeader(const std::string& header_line) override;
  void SetRequestContext(
      URLRequestContextGetter* request_context_getter) override;
  void SetInitiator(const base::Optional<url::Origin>& initiator) override;
  void SetURLRequestUserData(
      const void* key,
      const CreateDataCallback& create_data_callback) override;
  void SetStopOnRedirect(bool stop_on_redirect) override;
  void SetAutomaticallyRetryOn5xx(bool retry) override;
  void SetMaxRetriesOn5xx(int max_retries) override;
  int GetMaxRetriesOn5xx() const override;
  base::TimeDelta GetBackoffDelay() const override;
  void SetAutomaticallyRetryOnNetworkChanges(int max_retries) override;
  void SaveResponseToFileAtPath(
      const base::FilePath& file_path,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) override;
  void SaveResponseToTemporaryFile(
      scoped_refptr<base::SequencedTaskRunner> file_task_runner) override;
  void SaveResponseWithWriter(
      std::unique_ptr<URLFetcherResponseWriter> response_writer) override;
  HttpResponseHeaders* GetResponseHeaders() const override;
  IPEndPoint GetSocketAddress() const override;
  const ProxyServer& ProxyServerUsed() const override;
  bool WasCached() const override;
  // Only valid when the response was set via SetResponseString().
  int64_t GetReceivedResponseContentLength() const override;
  // Only valid when the response was set via SetResponseString(), or
  // set_was_cached(true) was called.
  int64_t GetTotalReceivedBytes() const override;
  void Start() override;

  // URL we were created with. Because of how we're using URLFetcher GetURL()
  // always returns an empty URL. Chances are you'll want to use
  // GetOriginalURL() in your tests.
  const GURL& GetOriginalURL() const override;
  const GURL& GetURL() const override;
  const URLRequestStatus& GetStatus() const override;
  int GetResponseCode() const override;
  void ReceivedContentWasMalformed() override;
  // Override response access functions to return fake data.
  bool GetResponseAsString(std::string* out_response_string) const override;
  bool GetResponseAsFilePath(bool take_ownership,
                             base::FilePath* out_response_path) const override;

  void GetExtraRequestHeaders(HttpRequestHeaders* headers) const;

  // Unique ID in our factory.
  int id() const { return id_; }

  // Returns the data uploaded on this URLFetcher.
  const std::string& upload_content_type() const {
    return upload_content_type_;
  }
  const std::string& upload_data() const { return upload_data_; }
  const base::FilePath& upload_file_path() const { return upload_file_path_; }

  // Returns the chunks of data uploaded on this URLFetcher.
  const std::list<std::string>& upload_chunks() const { return chunks_; }

  // Checks whether the last call to |AppendChunkToUpload(...)| was final.
  bool did_receive_last_chunk() const { return did_receive_last_chunk_; }

  // Returns the delegate installed on the URLFetcher.
  URLFetcherDelegate* delegate() const { return delegate_; }

  void set_url(const GURL& url) { fake_url_ = url; }
  void set_status(const URLRequestStatus& status);
  void set_response_code(int response_code) {
    fake_response_code_ = response_code;
  }
  void set_was_fetched_via_proxy(bool flag);
  void set_was_cached(bool flag);
  void set_response_headers(scoped_refptr<HttpResponseHeaders> headers);
  void set_backoff_delay(base::TimeDelta backoff_delay);
  void SetDelegateForTests(DelegateForTests* delegate_for_tests);

  // Set string data.
  void SetResponseString(const std::string& response);

  // Set File data.
  void SetResponseFilePath(const base::FilePath& path);

 private:
  enum ResponseDestinationType {
    STRING,  // Default: In a std::string
    TEMP_FILE  // Write to a temp file
  };

  const int id_;
  const GURL original_url_;
  URLFetcherDelegate* delegate_;
  DelegateForTests* delegate_for_tests_;
  std::string upload_content_type_;
  std::string upload_data_;
  base::FilePath upload_file_path_;
  std::list<std::string> chunks_;
  bool did_receive_last_chunk_;

  // User can use set_* methods to provide values returned by getters.
  // Setting the real values is not possible, because the real class
  // has no setters. The data is a private member of a class defined
  // in a .cc file, so we can't get at it with friendship.
  int fake_load_flags_;
  GURL fake_url_;
  URLRequestStatus fake_status_;
  int fake_response_code_;
  ResponseDestinationType fake_response_destination_;
  std::string fake_response_string_;
  base::FilePath fake_response_file_path_;
  bool write_response_file_;
  ProxyServer fake_proxy_server_;
  bool fake_was_cached_;
  int64_t fake_response_bytes_;
  scoped_refptr<HttpResponseHeaders> fake_response_headers_;
  HttpRequestHeaders fake_extra_request_headers_;
  int fake_max_retries_;
  base::TimeDelta fake_backoff_delay_;
  std::unique_ptr<URLFetcherResponseWriter> response_writer_;

  DISALLOW_COPY_AND_ASSIGN(TestURLFetcher);
};

// The FakeURLFetcher and FakeURLFetcherFactory classes are similar to the
// ones above but don't require you to know when exactly the URLFetcher objects
// will be created.
//
// These classes let you set pre-baked HTTP responses for particular URLs.
// E.g., if the user requests http://a.com/ then respond with an HTTP/500.
//
// We assume that the thread that is calling Start() on the URLFetcher object
// has a message loop running.

// FakeURLFetcher can be used to create a URLFetcher that will emit a fake
// response when started. This class can be used in place of an actual
// URLFetcher.
//
// Example usage:
//  FakeURLFetcher fake_fetcher("http://a.com", some_delegate,
//                              "<html><body>hello world</body></html>",
//                              HTTP_OK);
//
// // Will schedule a call to some_delegate->OnURLFetchComplete(&fake_fetcher).
// fake_fetcher.Start();
class FakeURLFetcher : public TestURLFetcher {
 public:
  // Normal URL fetcher constructor but also takes in a pre-baked response.
  FakeURLFetcher(const GURL& url,
                 URLFetcherDelegate* d,
                 const std::string& response_data,
                 HttpStatusCode response_code,
                 URLRequestStatus::Status status);

  // Start the request.  This will call the given delegate asynchronously
  // with the pre-baked response as parameter.
  void Start() override;

  const GURL& GetURL() const override;

  ~FakeURLFetcher() override;

 private:
  // This is the method which actually calls the delegate that is passed in the
  // constructor.
  void RunDelegate();

  int64_t response_bytes_;
  base::WeakPtrFactory<FakeURLFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcher);
};


// FakeURLFetcherFactory is a factory for FakeURLFetcher objects. When
// instantiated, it sets itself up as the default URLFetcherFactory. Fake
// responses for given URLs can be set using SetFakeResponse.
//
// This class is not thread-safe.  You should not call SetFakeResponse or
// ClearFakeResponse at the same time you call CreateURLFetcher.  However, it is
// OK to start URLFetcher objects while setting or clearing fake responses
// since already created URLFetcher objects will not be affected by any changes
// made to the fake responses (once a URLFetcher object is created you cannot
// change its fake response).
//
// Example usage:
//  FakeURLFetcherFactory factory;
//
//  // You know that class SomeService will request http://a.com/success and you
//  // want to respond with a simple html page and an HTTP/200 code.
//  factory.SetFakeResponse("http://a.com/success",
//                          "<html><body>hello world</body></html>",
//                          HTTP_OK,
//                          URLRequestStatus::SUCCESS);
//  // You know that class SomeService will request url http://a.com/servererror
//  // and you want to test the service class by returning a server error.
//  factory.SetFakeResponse("http://a.com/servererror",
//                          "",
//                          HTTP_INTERNAL_SERVER_ERROR,
//                          URLRequestStatus::SUCCESS);
//  // You know that class SomeService will request url http://a.com/autherror
//  // and you want to test the service class by returning a specific error
//  // code, say, a HTTP/401 error.
//  factory.SetFakeResponse("http://a.com/autherror",
//                          "some_response",
//                          HTTP_UNAUTHORIZED,
//                          URLRequestStatus::SUCCESS);
//
//  // You know that class SomeService will request url http://a.com/failure
//  // and you want to test the service class by returning a failure in the
//  // network layer.
//  factory.SetFakeResponse("http://a.com/failure",
//                          "",
//                          HTTP_INTERNAL_SERVER_ERROR,
//                          URLRequestStatus::FAILURE);
//
//  SomeService service;
//  service.Run();  // Will eventually request these three URLs.
class FakeURLFetcherFactory : public URLFetcherFactory,
                              public ScopedURLFetcherFactory {
 public:
  // Parameters to FakeURLFetcherCreator: url, delegate, response_data,
  //                                      response_code
  // |url| URL for instantiated FakeURLFetcher
  // |delegate| Delegate for FakeURLFetcher
  // |response_data| response data for FakeURLFetcher
  // |response_code| response code for FakeURLFetcher
  // |status| URL fetch status for FakeURLFetcher
  // These arguments should by default be used in instantiating FakeURLFetcher
  // like so:
  // new FakeURLFetcher(url, delegate, response_data, response_code, status)
  typedef base::Callback<std::unique_ptr<FakeURLFetcher>(
      const GURL&,
      URLFetcherDelegate*,
      const std::string&,
      HttpStatusCode,
      URLRequestStatus::Status)>
      FakeURLFetcherCreator;

  // |default_factory|, which can be NULL, is a URLFetcherFactory that
  // will be used to construct a URLFetcher in case the URL being created
  // has no pre-baked response. If it is NULL, a URLFetcherImpl will be
  // created in this case.
  explicit FakeURLFetcherFactory(URLFetcherFactory* default_factory);

  // |default_factory|, which can be NULL, is a URLFetcherFactory that
  // will be used to construct a URLFetcher in case the URL being created
  // has no pre-baked response. If it is NULL, a URLFetcherImpl will be
  // created in this case.
  // |creator| is a callback that returns will be called to create a
  // FakeURLFetcher if a response is found to a given URL. It can be
  // set to MakeFakeURLFetcher.
  FakeURLFetcherFactory(URLFetcherFactory* default_factory,
                        const FakeURLFetcherCreator& creator);

  ~FakeURLFetcherFactory() override;

  // If no fake response is set for the given URL this method will delegate the
  // call to |default_factory_| if it is not NULL, or return NULL if it is
  // NULL.
  // Otherwise, it will return a URLFetcher object which will respond with the
  // pre-baked response that the client has set by calling SetFakeResponse().
  std::unique_ptr<URLFetcher> CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcherDelegate* d,
      NetworkTrafficAnnotationTag traffic_annotation) override;

  // Sets the fake response for a given URL. The |response_data| may be empty.
  // The |response_code| may be any HttpStatusCode. For instance, HTTP_OK will
  // return an HTTP/200 and HTTP_INTERNAL_SERVER_ERROR will return an HTTP/500.
  // The |status| argument may be any URLRequestStatus::Status value. Typically,
  // requests that return a valid HttpStatusCode have the SUCCESS status, while
  // requests that indicate a failure to connect to the server have the FAILED
  // status.
  void SetFakeResponse(const GURL& url,
                       const std::string& response_data,
                       HttpStatusCode response_code,
                       URLRequestStatus::Status status);

  // Clear all the fake responses that were previously set via
  // SetFakeResponse().
  void ClearFakeResponses();

 private:
  struct FakeURLResponse {
    std::string response_data;
    HttpStatusCode response_code;
    URLRequestStatus::Status status;
  };
  typedef std::map<GURL, FakeURLResponse> FakeResponseMap;

  const FakeURLFetcherCreator creator_;
  FakeResponseMap fake_responses_;
  URLFetcherFactory* const default_factory_;

  static std::unique_ptr<FakeURLFetcher> DefaultFakeURLFetcherCreator(
      const GURL& url,
      URLFetcherDelegate* delegate,
      const std::string& response_data,
      HttpStatusCode response_code,
      URLRequestStatus::Status status);
  DISALLOW_COPY_AND_ASSIGN(FakeURLFetcherFactory);
};

}  // namespace net

#endif  // NET_URL_REQUEST_TEST_URL_FETCHER_FACTORY_H_
