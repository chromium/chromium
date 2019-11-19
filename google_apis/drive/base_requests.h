// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides base classes used to issue HTTP requests for Google
// APIs.

#ifndef GOOGLE_APIS_DRIVE_BASE_REQUESTS_H_
#define GOOGLE_APIS_DRIVE_BASE_REQUESTS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace google_apis {

class FileResource;
class RequestSender;

// Content type for multipart body.
enum MultipartType {
  MULTIPART_RELATED,
  MULTIPART_MIXED
};

// Pair of content type and data.
struct ContentTypeAndData {
  std::string type;
  std::string data;
};

typedef base::Callback<void(DriveApiErrorCode)> PrepareCallback;

// Callback used for requests that the server returns FileResource data
// formatted into JSON value.
typedef base::Callback<void(DriveApiErrorCode error,
                            std::unique_ptr<FileResource> entry)>
    FileResourceCallback;

// Callback used for DownloadFileRequest and ResumeUploadRequestBase.
typedef base::Callback<void(int64_t progress, int64_t total)> ProgressCallback;

// Callback used to get the content from DownloadFileRequest.
typedef base::Callback<void(DriveApiErrorCode error,
                            std::unique_ptr<std::string> content)>
    GetContentCallback;

// Parses JSON passed in |json|. Returns NULL on failure.
std::unique_ptr<base::Value> ParseJson(const std::string& json);

// Generate multipart body. If |predetermined_boundary| is not empty, it uses
// the string as boundary. Otherwise it generates random boundary that does not
// conflict with |parts|. If |data_offset| is not nullptr, it stores the
// index of first byte of each part in multipart body.
void GenerateMultipartBody(MultipartType multipart_type,
                           const std::string& predetermined_boundary,
                           const std::vector<ContentTypeAndData>& parts,
                           ContentTypeAndData* output,
                           std::vector<uint64_t>* data_offset);

//======================= AuthenticatedRequestInterface ======================

// An interface class for implementing a request which requires OAuth2
// authentication.
class AuthenticatedRequestInterface {
 public:
  // Called when re-authentication is required. See Start() for details.
  typedef base::Callback<void(AuthenticatedRequestInterface* request)>
      ReAuthenticateCallback;

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
                     const ReAuthenticateCallback& callback) = 0;

  // Invoked when the authentication failed with an error code |code|.
  virtual void OnAuthFailed(DriveApiErrorCode code) = 0;

  // Gets a weak pointer to this request object. Since requests may be
  // deleted when it is canceled by user action, for posting asynchronous tasks
  // on the authentication request object, weak pointers have to be used.
  // TODO(kinaba): crbug.com/134814 use more clean life time management than
  // using weak pointers.
  virtual base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() = 0;

  // Cancels the request. It will invoke the callback object passed in
  // each request's constructor with error code DRIVE_CANCELLED.
  virtual void Cancel() = 0;
};

//============================ UrlFetchRequestBase ===========================

// Base class for requests that are fetching URLs.
class UrlFetchRequestBase : public AuthenticatedRequestInterface,
                            public network::SimpleURLLoaderStreamConsumer {
 public:
  // AuthenticatedRequestInterface overrides.
  void Start(const std::string& access_token,
             const std::string& custom_user_agent,
             const ReAuthenticateCallback& callback) override;
  base::WeakPtr<AuthenticatedRequestInterface> GetWeakPtr() override;
  void Cancel() override;

 protected:
  UrlFetchRequestBase(RequestSender* sender,
                      const ProgressCallback& upload_progress_callback,
                      const ProgressCallback& download_progress_callback);
  ~UrlFetchRequestBase() override;

  // Does async initialization for the request. |Start| calls this method so you
  // don't need to call this before |Start|.
  virtual void Prepare(const PrepareCallback& callback);

  // Gets URL for the request.
  virtual GURL GetURL() const = 0;

  // Returns the request type. A derived class should override this method
  // for a request type other than HTTP GET.
  virtual std::string GetRequestType() const;

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
  virtual void RunCallbackOnPrematureFailure(DriveApiErrorCode code) = 0;

  // Invoked from derived classes when ProcessURLFetchResults() is completed.
  void OnProcessURLFetchResultsComplete();

  // Returns an appropriate DriveApiErrorCode based on the HTTP response code
  // and the status of the URLLoader.
  DriveApiErrorCode GetErrorCode() const;

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
  void OnDownloadProgress(const ProgressCallback& progress_callback,
                          uint64_t current);

  // Called by SimpleURLLoader to report upload progress.
  void OnUploadProgress(const ProgressCallback& progress_callback,
                        uint64_t position,
                        uint64_t total);

  // Called with the result of |WriteFileData|.
  void OnWriteComplete(std::unique_ptr<DownloadData> download_data,
                       base::OnceClosure resume,
                       bool write_success);

  // Called after completion (with output file closed).
  void OnOutputFileClosed(bool success);

  // SimpleURLLoaderStreamConsumer implementation:
  void OnDataReceived(base::StringPiece string_piece,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  // Continues |Start| function after |Prepare|.
  void StartAfterPrepare(const std::string& access_token,
                         const std::string& custom_user_agent,
                         const ReAuthenticateCallback& callback,
                         DriveApiErrorCode code);

  // Called when the SimpleURLLoader first receives a response.
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Invokes callback with |code| and request to delete the request to
  // |sender_|.
  void CompleteRequestWithError(DriveApiErrorCode code);

  // AuthenticatedRequestInterface overrides.
  void OnAuthFailed(DriveApiErrorCode code) override;

  ReAuthenticateCallback re_authenticate_callback_;
  int re_authenticate_count_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  RequestSender* sender_;
  base::Optional<DriveApiErrorCode> error_code_;
  const ProgressCallback upload_progress_callback_;
  const ProgressCallback download_progress_callback_;
  std::unique_ptr<DownloadData> download_data_;
  int64_t response_content_length_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UrlFetchRequestBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UrlFetchRequestBase);
};

//============================ BatchableDelegate ============================

// Delegate to be used by |SingleBatchableDelegateRequest| and
// |BatchUploadRequest|.
class BatchableDelegate {
 public:
  virtual ~BatchableDelegate() {}

  // See UrlFetchRequestBase.
  virtual GURL GetURL() const = 0;
  virtual std::string GetRequestType() const = 0;
  virtual std::vector<std::string> GetExtraRequestHeaders() const = 0;
  virtual void Prepare(const PrepareCallback& callback) = 0;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) = 0;

  // Notifies result of the request. Usually, it parses the |code| and
  // |response_body|, then notifies the parsed value to client code of the
  // API.  |callback| must be called on completion. The instance must not
  // do anything after calling |callback| since the instance may be deleted in
  // |callback|.
  virtual void NotifyResult(DriveApiErrorCode code,
                            const std::string& response_body,
                            const base::Closure& callback) = 0;

  // Notifies error. Unlike |NotifyResult|, it must report error
  // synchronously. The instance may be deleted just after calling
  // NotifyError.
  virtual void NotifyError(DriveApiErrorCode code) = 0;

  // Notifies progress.
  virtual void NotifyUploadProgress(int64_t current, int64_t total) = 0;
};

//============================ EntryActionRequest ============================

// Callback type for requests that return only error status, like: Delete/Move.
typedef base::Callback<void(DriveApiErrorCode error)> EntryActionCallback;

// This class performs a simple action over a given entry (document/file).
// It is meant to be used for requests that return no JSON blobs.
class EntryActionRequest : public UrlFetchRequestBase {
 public:
  // |callback| is called when the request is finished either by success or by
  // failure. It must not be null.
  EntryActionRequest(RequestSender* sender,
                     const EntryActionCallback& callback);
  ~EntryActionRequest() override;

 protected:
  // Overridden from UrlFetchRequestBase.
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(DriveApiErrorCode code) override;

 private:
  const EntryActionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(EntryActionRequest);
};

//=========================== InitiateUploadRequestBase=======================

// Callback type for DriveServiceInterface::InitiateUpload.
typedef base::Callback<void(DriveApiErrorCode error,
                            const GURL& upload_url)> InitiateUploadCallback;

// This class provides base implementation for performing the request for
// initiating the upload of a file.
// |callback| will be called with the obtained upload URL. The URL will be
// used with requests for resuming the file uploading.
//
// Here's the flow of uploading:
// 1) Get the upload URL with a class inheriting InitiateUploadRequestBase.
// 2) Upload the first 1GB (see kUploadChunkSize in drive_uploader.cc)
//    of the target file to the upload URL
// 3) If there is more data to upload, go to 2).
//
class InitiateUploadRequestBase : public UrlFetchRequestBase {
 protected:
  // |callback| will be called with the upload URL, where upload data is
  // uploaded to with ResumeUploadRequestBase. It must not be null.
  // |content_type| and |content_length| should be the attributes of the
  // uploading file.
  InitiateUploadRequestBase(RequestSender* sender,
                            const InitiateUploadCallback& callback,
                            const std::string& content_type,
                            int64_t content_length);
  ~InitiateUploadRequestBase() override;

  // UrlFetchRequestBase overrides.
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(DriveApiErrorCode code) override;
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  const InitiateUploadCallback callback_;
  const std::string content_type_;
  const int64_t content_length_;

  DISALLOW_COPY_AND_ASSIGN(InitiateUploadRequestBase);
};

//========================== UploadRangeRequestBase ==========================

// Struct for response to ResumeUpload and GetUploadStatus.
struct UploadRangeResponse {
  UploadRangeResponse();
  UploadRangeResponse(DriveApiErrorCode code,
                      int64_t start_position_received,
                      int64_t end_position_received);
  ~UploadRangeResponse();

  DriveApiErrorCode code;
  // The values of "Range" header returned from the server. The values are
  // used to continue uploading more data. These are set to -1 if an upload
  // is complete.
  // |start_position_received| is inclusive and |end_position_received| is
  // exclusive to follow the common C++ manner, although the response from
  // the server has "Range" header in inclusive format at both sides.
  int64_t start_position_received;
  int64_t end_position_received;
};

// Base class for a URL fetch request expecting the response containing the
// current uploading range. This class processes the response containing
// "Range" header and invoke OnRangeRequestComplete.
class UploadRangeRequestBase : public UrlFetchRequestBase {
 protected:
  // |upload_url| is the URL of where to upload the file to.
  UploadRangeRequestBase(RequestSender* sender,
                         const GURL& upload_url,
                         const ProgressCallback& upload_progress_callback);
  ~UploadRangeRequestBase() override;

  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  std::string GetRequestType() const override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(DriveApiErrorCode code) override;

  // This method will be called when the request is done, regardless of
  // whether it is succeeded or failed.
  //
  // 1) If there is more data to upload, |code| of |response| is set to
  // HTTP_RESUME_INCOMPLETE, and positions are set appropriately. Also, |value|
  // will be set to NULL.
  // 2) If the upload is complete, |code| is set to HTTP_CREATED for a new file
  // or HTTP_SUCCESS for an existing file. Positions are set to -1, and |value|
  // is set to a parsed JSON value representing the uploaded file.
  // 3) If a premature failure is found, |code| is set to a value representing
  // the situation. Positions are set to 0, and |value| is set to NULL.
  //
  // See also the comments for UploadRangeResponse.
  // Note: Subclasses should have responsibility to run some callback
  // in this method to notify the finish status to its clients (or ignore it
  // under its responsibility).
  virtual void OnRangeRequestComplete(const UploadRangeResponse& response,
                                      std::unique_ptr<base::Value> value) = 0;

 private:
  // Called when ParseJson() is completed.
  void OnDataParsed(DriveApiErrorCode code, std::unique_ptr<base::Value> value);

  const GURL upload_url_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UploadRangeRequestBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UploadRangeRequestBase);
};

//========================== ResumeUploadRequestBase =========================

// This class performs the request for resuming the upload of a file.
// More specifically, this request uploads a chunk of data carried in |buf|
// of ResumeUploadResponseBase. This class is designed to share the
// implementation of upload resuming between GData WAPI and Drive API v2.
// The subclasses should implement OnRangeRequestComplete inherited by
// UploadRangeRequestBase, because the type of the response should be
// different (although the format in the server response is JSON).
class ResumeUploadRequestBase : public UploadRangeRequestBase {
 protected:
  // |start_position| is the start of range of contents currently stored in
  // |buf|. |end_position| is the end of range of contents currently stared in
  // |buf|. This is exclusive. For instance, if you are to upload the first
  // 500 bytes of data, |start_position| is 0 and |end_position| is 500.
  // |content_length| and |content_type| are the length and type of the
  // file content to be uploaded respectively.
  // |buf| holds current content to be uploaded.
  // See also UploadRangeRequestBase's comment for remaining parameters
  // meaning.
  ResumeUploadRequestBase(RequestSender* sender,
                          const GURL& upload_location,
                          int64_t start_position,
                          int64_t end_position,
                          int64_t content_length,
                          const std::string& content_type,
                          const base::FilePath& local_file_path,
                          const ProgressCallback& progress_callback);
  ~ResumeUploadRequestBase() override;

  // UrlFetchRequestBase overrides.
  std::vector<std::string> GetExtraRequestHeaders() const override;
  bool GetContentFile(base::FilePath* local_file_path,
                      int64_t* range_offset,
                      int64_t* range_length,
                      std::string* upload_content_type) override;

 private:
  // The parameters for the request. See ResumeUploadParams for the details.
  const int64_t start_position_;
  const int64_t end_position_;
  const int64_t content_length_;
  const std::string content_type_;
  const base::FilePath local_file_path_;

  DISALLOW_COPY_AND_ASSIGN(ResumeUploadRequestBase);
};

//======================== GetUploadStatusRequestBase ========================

// This class performs the request for getting the current upload status
// of a file.
// This request calls OnRangeRequestComplete() with:
// - HTTP_RESUME_INCOMPLETE and the range of previously uploaded data,
//   if a file has been partially uploaded. |value| is not used.
// - HTTP_SUCCESS or HTTP_CREATED (up to the upload mode) and |value|
//   for the uploaded data, if a file has been completely uploaded.
// See also UploadRangeRequestBase.
class GetUploadStatusRequestBase : public UploadRangeRequestBase {
 public:
  // |content_length| is the whole data size to be uploaded.
  // See also UploadRangeRequestBase's constructor comment for other
  // parameters.
  GetUploadStatusRequestBase(RequestSender* sender,
                             const GURL& upload_url,
                             int64_t content_length);
  ~GetUploadStatusRequestBase() override;

 protected:
  // UrlFetchRequestBase overrides.
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  const int64_t content_length_;

  DISALLOW_COPY_AND_ASSIGN(GetUploadStatusRequestBase);
};

//=========================== MultipartUploadRequestBase=======================

// This class provides base implementation for performing the request for
// uploading a file by multipart body.
class MultipartUploadRequestBase : public BatchableDelegate {
 public:
  // Set boundary. Only tests can use this method.
  void SetBoundaryForTesting(const std::string& boundary);

 protected:
  // |callback| will be called with the file resource.upload URL.
  // |content_type| and |content_length| should be the attributes of the
  // uploading file. Other parameters are optional and can be empty or null
  // depending on Upload URL provided by the subclasses.
  MultipartUploadRequestBase(base::SequencedTaskRunner* blocking_task_runner,
                             const std::string& metadata_json,
                             const std::string& content_type,
                             int64_t content_length,
                             const base::FilePath& local_file_path,
                             const FileResourceCallback& callback,
                             const ProgressCallback& progress_callback);
  ~MultipartUploadRequestBase() override;

  // BatchableDelegate.
  std::vector<std::string> GetExtraRequestHeaders() const override;
  void Prepare(const PrepareCallback& callback) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void NotifyResult(DriveApiErrorCode code,
                    const std::string& body,
                    const base::Closure& callback) override;
  void NotifyError(DriveApiErrorCode code) override;
  void NotifyUploadProgress(int64_t current, int64_t total) override;
  // Parses the response value and invokes |callback_| with |FileResource|.
  void OnDataParsed(DriveApiErrorCode code,
                    const base::Closure& callback,
                    std::unique_ptr<base::Value> value);

 private:
  // Continues to rest part of |Start| method after determining boundary string
  // of multipart/related.
  void OnPrepareUploadContent(const PrepareCallback& callback,
                              std::string* upload_content_type,
                              std::string* upload_content_data,
                              bool result);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  const std::string metadata_json_;
  const std::string content_type_;
  const base::FilePath local_path_;
  const FileResourceCallback callback_;
  const ProgressCallback progress_callback_;

  // Boundary of multipart body.
  std::string boundary_;

  // Upload content of multipart body.
  std::string upload_content_type_;
  std::string upload_content_data_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MultipartUploadRequestBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MultipartUploadRequestBase);
};

//============================ DownloadFileRequest ===========================

// Callback type for receiving the completion of DownloadFileRequest.
typedef base::Callback<void(DriveApiErrorCode error,
                            const base::FilePath& temp_file)>
    DownloadActionCallback;

// This is a base class for performing the request for downloading a file.
class DownloadFileRequestBase : public UrlFetchRequestBase {
 public:
  // download_action_callback:
  //   This callback is called when the download is complete. Must not be null.
  //
  // get_content_callback:
  //   This callback is called when some part of the content is
  //   read. Used to read the download content progressively. May be null.
  //
  // progress_callback:
  //   This callback is called for periodically reporting the number of bytes
  //   downloaded so far. May be null.
  //
  // download_url:
  //   Specifies the target file to download.
  //
  // output_file_path:
  //   Specifies the file path to save the downloaded file.
  //
  DownloadFileRequestBase(
      RequestSender* sender,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback,
      const ProgressCallback& progress_callback,
      const GURL& download_url,
      const base::FilePath& output_file_path);
  ~DownloadFileRequestBase() override;

 protected:
  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  void GetOutputFilePath(base::FilePath* local_file_path,
                         GetContentCallback* get_content_callback) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(DriveApiErrorCode code) override;

 private:
  const DownloadActionCallback download_action_callback_;
  const GetContentCallback get_content_callback_;
  const GURL download_url_;
  const base::FilePath output_file_path_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileRequestBase);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_BASE_REQUESTS_H_
