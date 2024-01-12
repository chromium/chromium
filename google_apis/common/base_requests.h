// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides base classes used to issue HTTP requests for Google
// APIs.

#ifndef GOOGLE_APIS_COMMON_BASE_REQUESTS_H_
#define GOOGLE_APIS_COMMON_BASE_REQUESTS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "google_apis/common/api_error_codes.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace google_apis {

class RequestSender;

using PrepareCallback = base::OnceCallback<void(ApiErrorCode)>;

// Callback used for DownloadFileRequest and ResumeUploadRequestBase.
// |first_chunk| indicates if |content| is from the very beginning of
// the file being downloaded and helps consumers detect if download
// was restarted, for example due to re-authentication.
typedef base::RepeatingCallback<void(int64_t progress, int64_t total)>
    ProgressCallback;

// Callback used to get the content from DownloadFileRequest.
typedef base::RepeatingCallback<void(ApiErrorCode error,
                                     std::unique_ptr<std::string> content,
                                     bool first_chunk)>
    GetContentCallback;

// Most commonly used HTTP request methods.
enum class HttpRequestMethod { kGet, kPost, kPut, kPatch, kDelete };

// Parses JSON passed in |json|. Returns NULL on failure.
std::unique_ptr<base::Value> ParseJson(const std::string& json);

// Maps the error body to reason and logs the error code.
std::optional<std::string> MapJsonErrorToReason(const std::string& error_body);

// Stringifies `HttpRequestMethod` enum value.
std::string HttpRequestMethodToString(HttpRequestMethod method);

//======================= AuthenticatedRequestInterface ======================

// An interface class for implementing a request which requires OAuth2
// authentication.
class AuthenticatedRequestInterface {
 public:
  // Called when re-authentication is required. See Start() for details.
  using ReAuthenticateCallback =
      base::RepeatingCallback<void(AuthenticatedRequestInterface* request)>;

  virtual ~AuthenticatedRequestInterface() {}

  // Starts the request with |access_token|. User-Agent header will be set
  // to |custom_user_agent| if the value is not empty.
  //
  // |callback| is called when re-authentication is needed for a certain
  // number of times (see kMaxReAuthenticateAttemptsPerRequest in .cc).
  // The callback should retry by calling Start() again with a new access
  // token, or just call OnAuthFailed() if a retry is not attempted.
  // |callback| must not be null.
  virtual void Start(const std::string& access_token,
                     const std::string& custom_user_agent,
                     ReAuthenticateCallback callback) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(ApiErrorCode code) = 0;

  // Gets a weak pointer to this request object. Since requests may be
  // deleted when it is canceled by user action, for posting asynchronous tasks
  // on the authentication request object, weak pointers have to be used.
  // TODO(kinaba): crbug.com/134814 use more clean life time management than
  // using weak pointers.
  virtual base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() = 0;

  // Cancels the request. It will invoke the callback object passed in
  // each request's constructor with error code CANCELLED.
  virtual void Cancel() = 0;
};

//============================ UrlFetchRequestBase ===========================

// Base class for requests that are fetching URLs.
class UrlFetchRequestBase : public AuthenticatedRequestInterface,
                            public network::SimpleURLLoaderStreamConsumer {
 public:
  UrlFetchRequestBase(const UrlFetchRequestBase&) = delete;
  UrlFetchRequestBase& operator=(const UrlFetchRequestBase&) = delete;

  // AuthenticatedRequestInterface overrides.
  void Start(const std::string& access_token,
             const std::string& custom_user_agent,
             ReAuthenticateCallback callback) override;
  base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() override;
  void Cancel() override;

 protected:
  UrlFetchRequestBase(RequestSender* sender,
                      ProgressCallback upload_progress_callback,
                      ProgressCallback download_progress_callback);
  ~UrlFetchRequestBase() override;

  // Does async initialization for the request. |Start| calls this method so you
  // don't need to call this before |Start|.
  virtual void Prepare(PrepareCallback callback);

  // Gets URL for the request.
  virtual GURL GetURL() const = 0;

  // Returns corresponding ApiErrorCode based on the `reason` and the `code`.
  virtual ApiErrorCode MapReasonToError(ApiErrorCode code,
                                        const std::string& reason) = 0;

  // Checks if the error is a successful error code for this api.
  virtual bool IsSuccessfulErrorCode(ApiErrorCode error) = 0;

  // Returns the request type. A derived class should override this method
  // for a request type other than HTTP GET.
  virtual HttpRequestMethod GetRequestType() const;

  // Returns the extra HTTP headers for the request. A derived class should
  // override this method to specify any extra headers needed for the request.
  virtual std::vector<std::string> GetExtraRequestHeaders() const;

  // Used by a derived class to add any content data to the request.
  // Returns true if |upload_content_type| and |upload_content| are updated
  // with the content type and data for the request.
  // Note that this and GetContentFile() cannot be used together.
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content);

  // Used by a derived class to add content data which is the whole file or
  // a part of the file at |local_file_path|.
  // Returns true if all the arguments are updated for the content being
  // uploaded.
  // Note that this and GetContentData() cannot be used together.
  virtual bool GetContentFile(base::FilePath* local_file_path,
                              int64_t* range_offset,
                              int64_t* range_length,
                              std::string* upload_content_type);

  // Used by a derived class to set an output file path if they want to save
  // the downloaded content to a file at a specific path.
  // Sets |get_content_callback|, which is called when some part of the response
  // is read.
  virtual void GetOutputFilePath(base::FilePath* local_file_path,
                                 GetContentCallback* get_content_callback);

  // Invoked when the request completes without an authentication error.
  // |response_head| may be nullptr if the resource load failed - call
  // GetErrorCode() to determine cause of failure. |response_file| will contain
  // the path to the requested output file. |response_body| will contain the
  // requested resource. If requested to save the response to disk then
  // |response_body| may be truncated and only contain the starting portion
  // of the resource.
  virtual void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) = 0;

  // Invoked by this base class upon an authentication error or cancel by
  // a user request. Must be implemented by a derived class.
  virtual void RunCallbackOnPrematureFailure(ApiErrorCode code) = 0;

  // Invoked from derived classes when ProcessURLFetchResults() is completed.
  void OnProcessURLFetchResultsComplete();

  // Returns an appropriate ApiErrorCode based on the HTTP response code
  // and the status of the URLLoader.
  ApiErrorCode GetErrorCode() const;

  // Returns the net::Error representing the final status of the request.
  // Only valid after the request completes (successful or not).
  int NetError() const;

  // Returns true if called on the thread where the constructor was called.
  bool CalledOnValidThread();

  // Returns the task runner that should be used for blocking tasks.
  base::SequencedTaskRunner* blocking_task_runner() const;

 private:
  // Values for the current in-progress request being handled by the
  // url_loader_.
  struct DownloadData {
    explicit DownloadData(
        scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
    DownloadData(const DownloadData&) = delete;
    DownloadData& operator=(const DownloadData&) = delete;
    ~DownloadData();

    base::File output_file;
    base::FilePath output_file_path;
    GetContentCallback get_content_callback;
    std::string response_body;

   private:
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  };

  // Write the data in |file_data| to disk and call |resume| once complete on
  // a blocking sequence.
  static bool WriteFileData(std::string file_data, DownloadData* download_data);

  // Called by SimpleURLLoader to report download progress.
  void OnDownloadProgress(ProgressCallback progress_callback, uint64_t current);

  // Called by SimpleURLLoader to report upload progress.
  void OnUploadProgress(ProgressCallback progress_callback,
                        uint64_t position,
                        uint64_t total);

  // Called with the result of |WriteFileData|.
  void OnWriteComplete(std::unique_ptr<DownloadData> download_data,
                       base::OnceClosure resume,
                       bool write_success);

  // Called after completion (with output file closed).
  void OnOutputFileClosed(bool success);

  // SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(std::string_view string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  // Continues |Start| function after |Prepare|.
  void StartAfterPrepare(const std::string& access_token,
                         const std::string& custom_user_agent,
                         ReAuthenticateCallback callback,
                         ApiErrorCode code);

  // Called when the SimpleURLLoader first receives a response.
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Invokes callback with |code| and request to delete the request to
  // |sender_|.
  void CompleteRequestWithError(ApiErrorCode code);

  // AuthenticatedRequestInterface overrides.
  void OnAuthFailed(ApiErrorCode code) override;

  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  raw_ptr<RequestSender> sender_;
  std::optional<ApiErrorCode> error_code_;
  const ProgressCallback upload_progress_callback_;
  const ProgressCallback download_progress_callback_;
  std::unique_ptr<DownloadData> download_data_;
  int64_t response_content_length_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UrlFetchRequestBase> weak_ptr_factory_{this};
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_BASE_REQUESTS_H_
