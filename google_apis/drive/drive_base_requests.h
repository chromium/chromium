// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides base classes used to issue HTTP requests for Google
// APIs.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_BASE_REQUESTS_H_
#define GOOGLE_APIS_DRIVE_DRIVE_BASE_REQUESTS_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
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
enum class MultipartType { kRelated, kMixed };

// Pair of content type and data.
struct ContentTypeAndData {
  std::string type;
  std::string data;
};

// Callback used for requests that the server returns FileResource data
// formatted into JSON value.
typedef base::OnceCallback<void(ApiErrorCode error,
                                std::unique_ptr<FileResource> entry)>
    FileResourceCallback;

// Generate multipart body. If |predetermined_boundary| is not empty, it uses
// the string as boundary. Otherwise it generates random boundary that does not
// conflict with |parts|. If |data_offset| is not nullptr, it stores the
// index of first byte of each part in multipart body.
void GenerateMultipartBody(MultipartType multipart_type,
                           const std::string& predetermined_boundary,
                           const std::vector<ContentTypeAndData>& parts,
                           ContentTypeAndData* output,
                           std::vector<uint64_t>* data_offset);

//============================ DriveUrlFetchRequestBase =======================

// Base class for Drive requests that are fetching URLs.
class DriveUrlFetchRequestBase : public UrlFetchRequestBase {
 public:
  DriveUrlFetchRequestBase(const DriveUrlFetchRequestBase&) = delete;
  DriveUrlFetchRequestBase& operator=(const DriveUrlFetchRequestBase&) = delete;

 protected:
  DriveUrlFetchRequestBase(RequestSender* sender,
                           ProgressCallback upload_progress_callback,
                           ProgressCallback download_progress_callback);
  ~DriveUrlFetchRequestBase() override;

  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;

  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
};

//============================ BatchableDelegate ============================

// Delegate to be used by |SingleBatchableDelegateRequest| and
// |BatchUploadRequest|.
class BatchableDelegate {
 public:
  virtual ~BatchableDelegate() = default;

  // See UrlFetchRequestBase.
  virtual GURL GetURL() const = 0;
  virtual HttpRequestMethod GetRequestType() const = 0;
  virtual std::vector<std::string> GetExtraRequestHeaders() const = 0;
  virtual void Prepare(PrepareCallback callback) = 0;
  virtual bool GetContentData(std::string* upload_content_type,
                              std::string* upload_content) = 0;

  // Notifies result of the request. Usually, it parses the |code| and
  // |response_body|, then notifies the parsed value to client code of the
  // API.  |callback| must be called on completion. The instance must not
  // do anything after calling |callback| since the instance may be deleted in
  // |callback|.
  virtual void NotifyResult(ApiErrorCode code,
                            const std::string& response_body,
                            base::OnceClosure callback) = 0;

  // Notifies error. Unlike |NotifyResult|, it must report error
  // synchronously. The instance may be deleted just after calling
  // NotifyError.
  virtual void NotifyError(ApiErrorCode code) = 0;

  // Notifies progress.
  virtual void NotifyUploadProgress(int64_t current, int64_t total) = 0;
};

//============================ EntryActionRequest ============================

// Callback type for requests that return only error status, like: Delete/Move.
using EntryActionCallback = base::OnceCallback<void(ApiErrorCode error)>;

// This class performs a simple action over a given entry (document/file).
// It is meant to be used for requests that return no JSON blobs.
class EntryActionRequest : public DriveUrlFetchRequestBase {
 public:
  // |callback| is called when the request is finished either by success or by
  // failure. It must not be null.
  EntryActionRequest(RequestSender* sender, EntryActionCallback callback);
  EntryActionRequest(const EntryActionRequest&) = delete;
  EntryActionRequest& operator=(const EntryActionRequest&) = delete;
  ~EntryActionRequest() override;

 protected:
  // Overridden from UrlFetchRequestBase.
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  EntryActionCallback callback_;
};

//=========================== InitiateUploadRequestBase=======================

// Callback type for DriveServiceInterface::InitiateUpload.
typedef base::OnceCallback<void(ApiErrorCode error, const GURL& upload_url)>
    InitiateUploadCallback;

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
class InitiateUploadRequestBase : public DriveUrlFetchRequestBase {
 public:
  InitiateUploadRequestBase(const InitiateUploadRequestBase&) = delete;
  InitiateUploadRequestBase& operator=(const InitiateUploadRequestBase&) =
      delete;

 protected:
  // |callback| will be called with the upload URL, where upload data is
  // uploaded to with ResumeUploadRequestBase. It must not be null.
  // |content_type| and |content_length| should be the attributes of the
  // uploading file.
  InitiateUploadRequestBase(RequestSender* sender,
                            InitiateUploadCallback callback,
                            const std::string& content_type,
                            int64_t content_length);
  ~InitiateUploadRequestBase() override;

  // UrlFetchRequestBase overrides.
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  InitiateUploadCallback callback_;
  const std::string content_type_;
  const int64_t content_length_;
};

//========================== UploadRangeRequestBase ==========================

// Struct for response to ResumeUpload and GetUploadStatus.
struct UploadRangeResponse {
  UploadRangeResponse();
  UploadRangeResponse(ApiErrorCode code,
                      int64_t start_position_received,
                      int64_t end_position_received);
  ~UploadRangeResponse();

  ApiErrorCode code = HTTP_SUCCESS;
  // The values of "Range" header returned from the server. The values are
  // used to continue uploading more data. These are set to -1 if an upload
  // is complete.
  // |start_position_received| is inclusive and |end_position_received| is
  // exclusive to follow the common C++ manner, although the response from
  // the server has "Range" header in inclusive format at both sides.
  int64_t start_position_received = 0;
  int64_t end_position_received = 0;
};

// Base class for a URL fetch request expecting the response containing the
// current uploading range. This class processes the response containing
// "Range" header and invoke OnRangeRequestComplete.
class UploadRangeRequestBase : public DriveUrlFetchRequestBase {
 public:
  UploadRangeRequestBase(const UploadRangeRequestBase&) = delete;
  UploadRangeRequestBase& operator=(const UploadRangeRequestBase&) = delete;

 protected:
  // |upload_url| is the URL of where to upload the file to.
  UploadRangeRequestBase(RequestSender* sender,
                         const GURL& upload_url,
                         ProgressCallback upload_progress_callback);
  ~UploadRangeRequestBase() override;

  // UrlFetchRequestBase overrides.
  GURL GetURL() const override;
  HttpRequestMethod GetRequestType() const override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

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
  void OnDataParsed(ApiErrorCode code, std::unique_ptr<base::Value> value);

  const GURL upload_url_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UploadRangeRequestBase> weak_ptr_factory_{this};
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
 public:
  ResumeUploadRequestBase(const ResumeUploadRequestBase&) = delete;
  ResumeUploadRequestBase& operator=(const ResumeUploadRequestBase&) = delete;

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
                          ProgressCallback progress_callback);
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
  GetUploadStatusRequestBase(const GetUploadStatusRequestBase&) = delete;
  GetUploadStatusRequestBase& operator=(const GetUploadStatusRequestBase&) =
      delete;
  ~GetUploadStatusRequestBase() override;

 protected:
  // UrlFetchRequestBase overrides.
  std::vector<std::string> GetExtraRequestHeaders() const override;

 private:
  const int64_t content_length_;
};

//=========================== MultipartUploadRequestBase=======================

// This class provides base implementation for performing the request for
// uploading a file by multipart body.
class MultipartUploadRequestBase : public BatchableDelegate {
 public:
  MultipartUploadRequestBase(const MultipartUploadRequestBase&) = delete;
  MultipartUploadRequestBase& operator=(const MultipartUploadRequestBase&) =
      delete;

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
                             FileResourceCallback callback,
                             ProgressCallback progress_callback);
  ~MultipartUploadRequestBase() override;

  // BatchableDelegate.
  std::vector<std::string> GetExtraRequestHeaders() const override;
  void Prepare(PrepareCallback callback) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void NotifyResult(ApiErrorCode code,
                    const std::string& body,
                    base::OnceClosure callback) override;
  void NotifyError(ApiErrorCode code) override;
  void NotifyUploadProgress(int64_t current, int64_t total) override;
  // Parses the response value and invokes |callback_| with |FileResource|.
  void OnDataParsed(ApiErrorCode code,
                    base::OnceClosure callback,
                    std::unique_ptr<base::Value> value);

 private:
  // Continues to rest part of |Start| method after determining boundary string
  // of multipart/related.
  void OnPrepareUploadContent(PrepareCallback callback,
                              std::string* upload_content_type,
                              std::string* upload_content_data,
                              bool result);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  const std::string metadata_json_;
  const std::string content_type_;
  const base::FilePath local_path_;
  FileResourceCallback callback_;
  const ProgressCallback progress_callback_;

  // Boundary of multipart body.
  std::string boundary_;

  // Upload content of multipart body.
  std::string upload_content_type_;
  std::string upload_content_data_;

  THREAD_CHECKER(thread_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<MultipartUploadRequestBase> weak_ptr_factory_{this};
};

//============================ DownloadFileRequest ===========================

// Callback type for receiving the completion of DownloadFileRequest.
typedef base::OnceCallback<void(ApiErrorCode error,
                                const base::FilePath& temp_file)>
    DownloadActionCallback;

// This is a base class for performing the request for downloading a file.
class DownloadFileRequestBase : public DriveUrlFetchRequestBase {
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
  DownloadFileRequestBase(RequestSender* sender,
                          DownloadActionCallback download_action_callback,
                          const GetContentCallback& get_content_callback,
                          ProgressCallback progress_callback,
                          const GURL& download_url,
                          const base::FilePath& output_file_path);
  DownloadFileRequestBase(const DownloadFileRequestBase&) = delete;
  DownloadFileRequestBase& operator=(const DownloadFileRequestBase&) = delete;
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
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  DownloadActionCallback download_action_callback_;
  const GetContentCallback get_content_callback_;
  const GURL download_url_;
  const base::FilePath output_file_path_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_BASE_REQUESTS_H_
