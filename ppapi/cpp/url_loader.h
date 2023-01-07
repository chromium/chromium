// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_URL_LOADER_H_
#define PPAPI_CPP_URL_LOADER_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API for loading URLs.
namespace pp {

class CompletionCallback;
class InstanceHandle;
class URLRequestInfo;
class URLResponseInfo;

/// URLLoader provides an API for loading URLs.
/// Refer to <code>ppapi/examples/url_loader/streaming.cc</code>
/// for an example of how to use this class.
class URLLoader : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>URLLoader</code> object.
  URLLoader() {}

  /// A constructor used when a <code>PP_Resource</code> is provided as a
  /// return value whose reference count we need to increment.
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// <code>URLLoader</code> resource.
  explicit URLLoader(PP_Resource resource);

  /// A constructor used to allocate a new URLLoader in the browser. The
  /// resulting object will be <code>is_null</code> if the allocation failed.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit URLLoader(const InstanceHandle& instance);

  /// The copy constructor for <code>URLLoader</code>.
  ///
  /// @param other A <code>URLLoader</code> to be copied.
  URLLoader(const URLLoader& other);
  URLLoader& operator=(const URLLoader& other);

  /// This function begins loading the <code>URLRequestInfo</code>.
  /// The operation completes when response headers are received or when an
  /// error occurs.  Use GetResponseInfo() to access the response
  /// headers.
  ///
  /// @param[in] request_info A <code>URLRequestInfo</code> corresponding to a
  /// URLRequestInfo.
  /// @param[in] cc A <code>CompletionCallback</code> to run on asynchronous
  /// completion of Open(). This callback will run when response
  /// headers for the url are received or error occurred. This callback
  /// will only run if Open() returns <code>PP_OK_COMPLETIONPENDING</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t Open(const URLRequestInfo& request_info,
               const CompletionCallback& cc);

  /// This function can be invoked to follow a
  /// redirect after Open() completed on receiving redirect headers.
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to run on asynchronous
  /// completion of FollowRedirect(). This callback will run when response
  /// headers for the redirect url are received or error occurred. This callback
  /// will only run if FollowRedirect() returns
  /// <code>PP_OK_COMPLETIONPENDING</code>.
  ///
  /// @return An int32_t containing an error code from
  /// <code>pp_errors.h</code>.
  int32_t FollowRedirect(const CompletionCallback& cc);

  /// This function returns the current upload progress (which is only
  /// meaningful after Open() has been called). Progress only refers to the
  /// request body and does not include the headers.
  ///
  /// This data is only available if the <code>URLRequestInfo</code> passed to
  /// Open() had the
  /// <code>PP_URLREQUESTPROPERTY_REPORTUPLOADPROGRESS</code> property set to
  /// <code>PP_TRUE</code>.
  ///
  /// @param[in] bytes_sent The number of bytes sent thus far.
  /// @param[in] total_bytes_to_be_sent The total number of bytes to be sent.
  ///
  /// @return true if the upload progress is available, false if it is not
  /// available.
  bool GetUploadProgress(int64_t* bytes_sent,
                         int64_t* total_bytes_to_be_sent) const;

  /// This function returns the current download progress, which is meaningful
  /// after Open() has been called. Progress only refers to the response body
  /// and does not include the headers.
  ///
  /// This data is only available if the <code>URLRequestInfo</code> passed to
  /// Open() had the
  /// <code>PP_URLREQUESTPROPERTY_REPORTDOWNLOADPROGRESS</code> property set to
  /// PP_TRUE.
  ///
  /// @param[in] bytes_received The number of bytes received thus far.
  /// @param[in] total_bytes_to_be_received The total number of bytes to be
  /// received. The total bytes to be received may be unknown, in which case
  /// <code>total_bytes_to_be_received</code> will be set to -1.
  ///
  /// @return true if the download progress is available, false if it is
  /// not available.
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const;

  /// This is a function that returns the current
  /// <code>URLResponseInfo</code> object.
  ///
  /// @return A <code>URLResponseInfo</code> corresponding to the
  /// <code>URLResponseInfo</code> if successful, an <code>is_null</code>
  /// object if the loader is not a valid resource or if Open() has not been
  /// called.
  URLResponseInfo GetResponseInfo() const;

  /// This function is used to read the response body. The size of the buffer
  /// must be large enough to hold the specified number of bytes to read.
  /// This function might perform a partial read.
  ///
  /// @param[in,out] buffer A pointer to the buffer for the response body.
  /// @param[in] bytes_to_read The number of bytes to read.
  /// @param[in] cc A <code>CompletionCallback</code> to run on asynchronous
  /// completion. The callback will run if the bytes (full or partial) are
  /// read or an error occurs asynchronously. This callback will run only if
  /// this function returns <code>PP_OK_COMPLETIONPENDING</code>.
  ///
  /// @return An int32_t containing the number of bytes read or an error code
  /// from <code>pp_errors.h</code>.
  int32_t ReadResponseBody(void* buffer,
                           int32_t bytes_to_read,
                           const CompletionCallback& cc);

  /// This function is used to wait for the response body to be completely
  /// downloaded to the file provided by the GetBodyAsFileRef() in the current
  /// <code>URLResponseInfo</code>. This function is only used if
  /// <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> was set on the
  /// <code>URLRequestInfo</code> passed to Open().
  ///
  /// @param[in] cc A <code>CompletionCallback</code> to run on asynchronous
  /// completion. This callback will run when body is downloaded or an error
  /// occurs after FinishStreamingToFile() returns
  /// <code>PP_OK_COMPLETIONPENDING</code>.
  ///
  /// @return An int32_t containing the number of bytes read or an error code
  /// from <code>pp_errors.h</code>.
  int32_t FinishStreamingToFile(const CompletionCallback& cc);

  /// This function is used to cancel any pending IO and close the URLLoader
  /// object. Any pending callbacks will still run, reporting
  /// <code>PP_ERROR_ABORTED</code> if pending IO was interrupted.  It is NOT
  /// valid to call Open() again after a call to this function.
  ///
  /// <strong>Note:</strong> If the <code>URLLoader</code> object is destroyed
  /// while it is still open, then it will be implicitly closed so you are not
  /// required to call Close().
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_URL_LOADER_H_
