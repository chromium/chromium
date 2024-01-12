// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_HTTP_RESPONSE_H_
#define CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_HTTP_RESPONSE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_connection_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace net {
class HttpByteRange;
}  // namespace net

namespace content {

// Class for configuring and returning http responses for download requests.
// Each instance can handle one http request.
// Lives on embedded test server's IO thread.
class TestDownloadHttpResponse {
 public:
  static const char kTestDownloadHostName[];
  static GURL GetNextURLForDownload();

  // OnPauseHandler can be used to pause the response until the enclosed
  // callback is called. The OnPauseHandler is copyable for Parameters
  // objects to be used repeatedly, though it will only be called once when
  // given to a TestDownloadHttpResponse.
  using OnPauseHandler = base::RepeatingCallback<void(base::OnceClosure)>;

  // Called when an injected error triggers.
  using InjectErrorCallback = base::RepeatingCallback<void(int64_t, int64_t)>;

  struct HttpResponseData {
    HttpResponseData() = default;
    HttpResponseData(int64_t min_offset,
                     int64_t max_offset,
                     const std::string& response,
                     bool is_transient,
                     bool delay_response);
    HttpResponseData(const HttpResponseData& other) = default;
    HttpResponseData& operator=(const HttpResponseData& other) = default;

    // The range for first byte position in range header to use this response.
    int64_t min_offset = -1;
    int64_t max_offset = -1;

    std::string response;

    // Whether the response data is transient and will be invalidated after
    // sending once.
    bool is_transient = false;

    bool delay_response = false;
  };

  struct Parameters {
    // Constructs a Parameters structure using the default constructor, but with
    // the injection of an error which will be triggered at byte offset
    // (filesize / 2). |inject_error_cb| will be called once the response is
    // sent.
    static Parameters WithSingleInterruption(
        const InjectErrorCallback& inject_error_cb);

    // The default constructor initializes the parameters for serving a 100 KB
    // resource with no interruptions. The response contains an ETag and a
    // Last-Modified header and the server supports byte range requests.
    Parameters();

    Parameters(const Parameters&);
    Parameters& operator=(const Parameters&);

    ~Parameters();

    // Clears the errors in injected_errors.
    void ClearInjectedErrors();

    // Sets the response for range request when the starting offset of
    // the request falls into [min_offset, max_offset]. If |is_transient|
    // is true, |response| will only be sent once for the given range. And
    // later responses will be calculated from the parameters. If
    // |is_transient| is false, the |response| will be applied to the
    // given range request forever.
    // if |delay_response| is true, the response body will be sent when
    // SendDelayedResponse() is called.
    void SetResponseForRangeRequest(int64_t min_offset,
                                    int64_t max_offset,
                                    const std::string& response,
                                    bool is_transient = false,
                                    bool delay_response = false);

    // Contents of the ETag header field of the response.  No Etag header is
    // sent if this field is empty.
    std::string etag;

    // Contents of the Last-Modified header field of the response. No
    // Last-Modified header is sent if this field is empty.
    std::string last_modified;

    // The Content-Type of the response. No Content-Type header is sent if this
    // field is empty.
    std::string content_type;

    // The total size of the entity body. If the entire entity is requested,
    // then this would be the same as the value returned in the Content-Length
    // header.
    int64_t size;

    // Seed for the pseudo-random sequence that defines the response body
    // contents. The seed is with GetPatternBytes() to generate the body of the
    // response.
    int pattern_generator_seed;

    // Whether the server can handle partial request.
    // If true, contains a 'Accept-Ranges: bytes' header for HTTP 200
    // response, or contains 'Content-Range' header for HTTP 206 response.
    bool support_byte_ranges;

    // Whether the server supports partial range responses. A server can claim
    // it support byte ranges, but actually doesn't send partial responses. In
    // that case, Set |support_byte_ranges| to true and this variable to false
    // to simulate the case.
    bool support_partial_response;

    // The connection type in the response.
    net::HttpConnectionInfo connection_type = net::HttpConnectionInfo::kUNKNOWN;

    // If specified, return this as the http response to the client.
    // No error injection or range request will be handled for static response.
    std::string static_response;

    // List of responses for range requests.
    std::vector<HttpResponseData> range_request_responses;

    // Error offsets to be injected. The response will successfully fulfill
    // requests to read up to offset. An attempt to read the byte at offset
    // cause |inject_error_cb| to run.
    //
    // If a read spans the range containing an offset, then the portion of the
    // request preceding the offset will succeed. The next read would start at
    // offset and hence would result in an error.
    //
    // E.g.: injected_errors.push(100);
    //
    //    A network read for 1024 bytes at offset 0 would result in successfully
    //    reading 100 bytes (bytes with offset 0-99). The next read would,
    //    therefore, start at offset 100 and would result in |inject_error_cb|
    //    to get called.
    //
    // Injected errors are processed in the order in which they appear in
    // |injected_errors|. When handling a network request for the range [S,E]
    // (inclusive), all events in |injected_errors| whose offset is less than
    // S will be ignored. The first event remaining will trigger an error once
    // the sequence of reads proceeds to a point where its offset is included
    // in [S,E].
    //
    // This implies that |injected_errors| must be specified in increasing order
    // of offset. I.e. |injected_errors| must be sorted by offset.
    //
    // Errors at relative offset 0 are ignored for a partial request. I.e. If
    // the request is for the byte range 100-200, then an error at offset 100
    // will not trigger. This is done so that non-overlapping continuation
    // attempts don't require resetting parameters to succeed.
    //
    // E.g.: If the caller injects an error at offset 100, then a request for
    // the entire entity will fail after reading 100 bytes (offsets 0 through
    // 99). A subsequent request for byte range "100-" (offsets 100 through EOF)
    // will succeed since the error at offset 100 is ignored.
    //
    // An injected error with -1 offset will immediately abort the response
    // without sending anything to the client, including the response header.
    base::queue<int64_t> injected_errors;

    // Callback to run when an injected error triggers.
    InjectErrorCallback inject_error_cb;

    // If on_pause_handler is valid, it will be invoked once when the
    // |pause_condition| is reached. It is not a OnceCallback to allow
    // Parameters to be reused.
    OnPauseHandler on_pause_handler;

    // Offset of body to pause the response sending. A -1 offset will pause
    // the response before header is sent.
    std::optional<int64_t> pause_offset;
  };

  // Information about completed requests.
  struct CompletedRequest {
    CompletedRequest(const net::test_server::HttpRequest& request);
    ~CompletedRequest();

    // Count of bytes returned in the response body.
    int64_t transferred_byte_count = 0;

    // The request received from the client.
    net::test_server::HttpRequest http_request;
  };

  // Called when response are sent to the client.
  using OnResponseSentCallback =
      base::OnceCallback<void(std::unique_ptr<CompletedRequest>)>;

  // Generate a pseudo random pattern.
  //
  // |seed| is the seed for the pseudo random sequence.  |offset| is the byte
  // offset into the sequence.  |length| is a count of bytes to generate.
  //
  // The pattern has the following properties:
  //
  // * For a given |seed|, the entire sequence of bytes is fixed. Any
  //   subsequence can be generated by specifying the |offset| and |length|.
  //
  // * The sequence is aperiodic (at least for the first 1M bytes).
  //
  // * |seed| is chaotic. Different seeds produce "very different" data. This
  //   means that there's no trivial mapping between sequences generated using
  //   two distinct seeds.
  //
  // These properties make the generated bytes useful for testing partial
  // requests where the response may need to be built using a sequence of
  // partial requests.
  //
  // Note: Don't use this function to generate a cryptographically secure
  // pseudo-random sequence.
  static std::string GetPatternBytes(int seed, int64_t offset, int length);

  // Start responding to http request for |url| with responses based on
  // |parameters|.
  //
  // This method registers the |url| and |parameters| mapping until another
  // call to StartServing() or StartServingStaticResponse() changes it. It
  // can be called on any thread.
  static void StartServing(
      const TestDownloadHttpResponse::Parameters& parameters,
      const GURL& url);

  // Start responding to URLRequests for |url| with a static response
  // containing the headers in |headers|.
  static void StartServingStaticResponse(const std::string& headers,
                                         const GURL& url);

  TestDownloadHttpResponse(const net::test_server::HttpRequest& request,
                           const Parameters& parameters,
                           OnResponseSentCallback on_response_sent_callback);

  TestDownloadHttpResponse(const TestDownloadHttpResponse&) = delete;
  TestDownloadHttpResponse& operator=(const TestDownloadHttpResponse&) = delete;

  ~TestDownloadHttpResponse();

  // Creates a shim HttpResponse object for embedded test server. This life time
  // of the object returned is short.
  std::unique_ptr<net::test_server::HttpResponse> CreateResponseForTestServer();

  // Starts to send the response. |delegate| owns the shim HttpResponse object
  // created by |CreateResponseForTestServer| and will outlive it.
  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate);

  // Runs |delayed_response_callback_| to send the delayed response.
  void SendDelayedResponse();

 private:
  // Parses the request headers.
  void ParseRequestHeader();

  // Sends the response headers.
  void SendResponseHeaders();

  // Called to append the range headers to |response| when validator matches.
  // Returns true if the range headers can be added, or false if the request
  // is invalid.
  bool HandleRangeAssumingValidatorMatch(std::string& response);

  // Adds headers that describe the entity (Content-Type, ETag, Last-Modified).
  std::string GetCommonEntityHeaders();

  // Gets the default response headers to send.
  std::string GetDefaultResponseHeaders();

  // Gets response header and body for range request starts from
  // |first_byte_position|, and whether to delay sending the response.
  // Returns false if no specific responses are found.
  bool GetResponseForRangeRequest(std::string* output, bool* delay_response);

  // Gets response body chunk based on random seed in |parameters_|.
  std::string GetResponseChunk(const net::HttpByteRange& buffer_range);

  // Pause or interrupt with injected errors.
  bool ShouldAbortImmediately() const;
  bool ShouldPauseImmediately() const;
  bool HandlePause(const net::HttpByteRange& buffer_range);
  bool HandleInjectedError(const net::HttpByteRange& buffer_range);

  // Used to pause the response sending.
  bool ShouldPause(const net::HttpByteRange& buffer_range) const;
  void PauseResponsesAndWaitForResumption();

  // Send part of the response entity body in small buffers, each chunk will be
  // send in one IOBuffer from embedded test server.
  // Will pause or throw error based on configuration in |paramters_|.
  void SendResponseBodyChunk();
  void SendBodyChunkInternal(const net::HttpByteRange& buffer_range,
                             base::OnceClosure next);
  base::OnceClosure SendNextBodyChunkClosure();

  // Generate CompletedRequest as result.
  void GenerateResult();
  base::OnceClosure GenerateResultClosure();

  // The parsed range of the HTTP request. The last byte position can be larger
  // than the file size.
  net::HttpByteRange request_range_;

  // The range of the HTTP response sent from test server, computed based on
  // range request header and |parameters_|. The last byte position may be
  // adjusted to the end of file size.
  net::HttpByteRange range_;

  // The offset of response bytes sent, will send data according to |range_|.
  int64_t response_sent_offset_;

  // Parameters associated with this response.
  Parameters parameters_;

  // Request received from the client.
  net::test_server::HttpRequest request_;

  // The delegate responsible for sending bytes and finishing the connection
  base::WeakPtr<net::test_server::HttpResponseDelegate> response_delegate_ =
      nullptr;

  // The number of bytes transferred.
  int64_t transferred_bytes_;

  // Callback to run when the response is sent.
  OnResponseSentCallback on_response_sent_callback_;

  // If |delay_response| is true in SetResponseForRangeRequest(), this
  // callback will be used to send the delayed response when
  // SendDelayedResponse() is called.
  base::OnceClosure delayed_response_callback_;

  base::WeakPtrFactory<TestDownloadHttpResponse> weak_ptr_factory_{this};
};

// Class for creating and monitoring the completed response from the server.
// Lives on UI thread.
//
// Example usage with TestDownloadHttpResponse:
//
//   test_response_handler->RegisterToTestServer(embedded_test_server());
//   EXPECT_TRUE(embedded_test_server()->Start());
//   GURL url = embedded_test_server()->GetURL("/random-url");
//
//   content::TestDownloadHttpResponse::Parameters parameters;
//   // Tweak the |parameters| here.
//   content::TestDownloadHttpResponse::StartServing(parameters, url);

class TestDownloadResponseHandler {
 public:
  std::unique_ptr<net::test_server::HttpResponse> HandleTestDownloadRequest(
      TestDownloadHttpResponse::OnResponseSentCallback callback,
      const net::test_server::HttpRequest& request);

  TestDownloadResponseHandler();

  TestDownloadResponseHandler(const TestDownloadResponseHandler&) = delete;
  TestDownloadResponseHandler& operator=(const TestDownloadResponseHandler&) =
      delete;

  ~TestDownloadResponseHandler();

  // Register to the embedded test |server|.
  void RegisterToTestServer(net::test_server::EmbeddedTestServer* server);

  void OnRequestCompleted(
      std::unique_ptr<TestDownloadHttpResponse::CompletedRequest> request);

  // Wait for a certain number of requests to complete.
  void WaitUntilCompletion(size_t request_count);

  // Called to dispatch all delayed responses if there are any.
  void DispatchDelayedResponses();

  using CompletedRequests =
      std::vector<std::unique_ptr<TestDownloadHttpResponse::CompletedRequest>>;
  CompletedRequests const& completed_requests() { return completed_requests_; }

 private:
  // Lives on on embedded test server's IO thread.
  std::vector<std::unique_ptr<TestDownloadHttpResponse>> responses_;

  CompletedRequests completed_requests_;
  size_t request_count_ = 0u;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> server_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_DOWNLOAD_HTTP_RESPONSE_H_
