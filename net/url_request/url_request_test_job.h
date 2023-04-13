// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/load_timing_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {

// This job type is designed to help with simple unit tests. To use, you
// probably want to inherit from it to set up the state you want. Then install
// it as the protocol handler for the "test" scheme.
//
// It will respond to several URLs, which you can retrieve using the test_url*
// getters, which will in turn respond with the corresponding responses returned
// by test_data*. Any other URLs that begin with "test:" will return an error,
// which might also be useful, you can use test_url_error() to retrieve a
// standard one.
//
// You can override the known URLs or the response data by overriding Start().
//
// Optionally, you can also construct test jobs to return a headers and data
// provided to the contstructor in response to any request url.
//
// When a job is created, it gets put on a queue of pending test jobs. To
// process jobs on this queue, use ProcessOnePendingMessage, which will process
// one step of the next job. If the job is incomplete, it will be added to the
// end of the queue.
//
// Optionally, you can also construct test jobs that advance automatically
// without having to call ProcessOnePendingMessage.
class URLRequestTestJob : public URLRequestJob {
 public:
  // Constructs a job to return one of the canned responses depending on the
  // request url.
  explicit URLRequestTestJob(URLRequest* request, bool auto_advance = false);

  // Constructs a job to return the given response regardless of the request
  // url. The headers should include the HTTP status line and use CRLF/LF as the
  // line separator.
  URLRequestTestJob(URLRequest* request,
                    const std::string& response_headers,
                    const std::string& response_data,
                    bool auto_advance);

  ~URLRequestTestJob() override;

  // The canned URLs this handler will respond to without having been
  // explicitly initialized with response headers and data.

  // URL that, by default, automatically advances through each state.  Reads
  // complete synchronously.
  static GURL test_url_1();

  // URLs that, by default, must be manually advanced through each state.
  static GURL test_url_2();
  static GURL test_url_3();
  static GURL test_url_4();

  // URL that, by default, automatically advances through each state.  Reads
  // complete asynchronously. Has same response body as test_url_1(), which is
  // (test_data_1()).
  static GURL test_url_auto_advance_async_reads_1();

  // URL that fails with ERR_INVALID_URL.
  static GURL test_url_error();

  // Redirects to test_url_1().
  static GURL test_url_redirect_to_url_1();

  // Redirects to test_url_2().
  static GURL test_url_redirect_to_url_2();

  // The data that corresponds to each of the URLs above
  static std::string test_data_1();
  static std::string test_data_2();
  static std::string test_data_3();
  static std::string test_data_4();

  // The headers that correspond to each of the URLs above
  static std::string test_headers();

  // The headers for a redirect response
  static std::string test_redirect_headers();

  // The headers for a redirect response to the first test url.
  static std::string test_redirect_to_url_1_headers();

  // The headers for a redirect response to the second test url.
  static std::string test_redirect_to_url_2_headers();

  // The headers for a server error response
  static std::string test_error_headers();

  // Processes one pending message from the stack, returning true if any
  // message was processed, or false if there are no more pending request
  // notifications to send. This is not applicable when using auto_advance.
  static bool ProcessOnePendingMessage();

  // With auto advance enabled, the job will advance thru the stages without
  // the caller having to call ProcessOnePendingMessage. Auto advance depends
  // on having a message loop running. The default is to not auto advance.
  // Should not be altered after the job has started.
  bool auto_advance() { return auto_advance_; }
  void set_auto_advance(bool auto_advance) { auto_advance_ = auto_advance; }

  void set_load_timing_info(const LoadTimingInfo& load_timing_info) {
    load_timing_info_ = load_timing_info;
  }

  RequestPriority priority() const { return priority_; }

  // Job functions
  void SetPriority(RequestPriority priority) override;
  void Start() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  void Kill() override;
  bool GetMimeType(std::string* mime_type) const override;
  void GetResponseInfo(HttpResponseInfo* info) override;
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  int64_t GetTotalReceivedBytes() const override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;

 protected:
  // Override to specify whether the next read done from this job will
  // return IO pending.  This controls whether or not the WAITING state will
  // transition back to WAITING or to DATA_AVAILABLE after an asynchronous
  // read is processed.
  virtual bool NextReadAsync();

  // This is what operation we are going to do next when this job is handled.
  // When the stage is DONE, this job will not be put on the queue.
  enum Stage { WAITING, DATA_AVAILABLE, ALL_DATA, DONE };

  // Call to process the next opeation, usually sending a notification, and
  // advancing the stage if necessary. THIS MAY DELETE THE OBJECT.
  void ProcessNextOperation();

  // Call to move the job along to the next operation.
  void AdvanceJob();

  // Called via InvokeLater to cause callbacks to occur after Start() returns.
  virtual void StartAsync();

  // Assigns |response_headers_| and |response_headers_length_|.
  void SetResponseHeaders(const std::string& response_headers);

  // Copies as much of the response body as will into |buf|, and returns number
  // of bytes written.
  int CopyDataForRead(IOBuffer* buf, int buf_size);

  bool auto_advance_;

  Stage stage_ = WAITING;

  RequestPriority priority_ = DEFAULT_PRIORITY;

  // The data to send, will be set in Start() if not provided in the explicit
  // ctor.
  std::string response_data_;

  // current offset within response_data_
  int offset_ = 0;

  // Holds the buffer for an asynchronous ReadRawData call
  scoped_refptr<IOBuffer> async_buf_;
  int async_buf_size_ = 0;

  LoadTimingInfo load_timing_info_;

 private:
  // The headers the job should return, will be set in Start() if not provided
  // in the explicit ctor.
  scoped_refptr<HttpResponseHeaders> response_headers_;

  // Original size in bytes of the response headers before decoding.
  int response_headers_length_;

  bool async_reads_ = false;

  base::WeakPtrFactory<URLRequestTestJob> weak_factory_{this};
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_TEST_JOB_H_
