// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_URL_REQUEST_INFO_H_
#define PPAPI_CPP_URL_REQUEST_INFO_H_

#include <stdint.h>

#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for creating and manipulating URL requests.
namespace pp {

class FileRef;
class InstanceHandle;

/// URLRequestInfo provides an API for creating and manipulating URL requests.
class URLRequestInfo : public Resource {
 public:
  /// Default constructor. This constructor creates an
  /// <code>is_null</code> resource.
  URLRequestInfo() {}

  /// A constructor used to allocate a new <code>URLLoader</code> in the
  /// browser. The resulting object will be <code>is_null</code> if the
  /// allocation failed.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit URLRequestInfo(const InstanceHandle& instance);

  /// The copy constructor for <code>URLRequestInfo</code>.
  ///
  /// @param[in] other A <code>URLRequestInfo</code> to be copied.
  URLRequestInfo(const URLRequestInfo& other);

  /// SetProperty() sets a request property. The value of the property must be
  /// the correct type according to the property being set.
  ///
  /// @param[in] property A <code>PP_URLRequestProperty</code> identifying the
  /// property to set.
  /// @param[in] value A <code>Var</code> containing the property value.
  ///
  /// @return true if successful, false if any of the
  /// parameters are invalid.
  bool SetProperty(PP_URLRequestProperty property, const Var& value);

  /// AppendDataToBody() appends data to the request body. A content-length
  /// request header will be automatically generated.
  ///
  /// @param[in] data A pointer to a buffer holding the data.
  /// @param[in] len The length, in bytes, of the data.
  ///
  /// @return true if successful, false if any of the
  /// parameters are invalid.
  bool AppendDataToBody(const void* data, uint32_t len);

  /// AppendFileToBody() is used to append an entire file, to be uploaded, to
  /// the request body. A content-length request header will be automatically
  /// generated.
  ///
  /// @param[in] file_ref A <code>FileRef</code> containing the file
  /// reference.

  /// @param[in] expected_last_modified_time An optional (non-zero) last
  /// modified time stamp used to validate that the file was not modified since
  /// the given time before it was uploaded. The upload will fail with an error
  /// code of <code>PP_ERROR_FILECHANGED</code> if the file has been modified
  /// since the given time. If expected_last_modified_time is 0, then no
  /// validation is performed.
  ///
  /// @return true if successful, false if any of the
  /// parameters are invalid.
  bool AppendFileToBody(const FileRef& file_ref,
                        PP_Time expected_last_modified_time = 0);

  /// AppendFileRangeToBody() is a pointer to a function used to append part or
  /// all of a file, to be uploaded, to the request body. A content-length
  /// request header will be automatically generated.
  ///
  /// @param[in] file_ref A <code>FileRef</code> containing the file
  /// reference.
  /// @param[in] start_offset An optional starting point offset within the
  /// file.
  /// @param[in] length An optional number of bytes of the file to
  /// be included. If the value is -1, then the sub-range to upload extends
  /// to the end of the file.
  /// @param[in] expected_last_modified_time An optional (non-zero) last
  /// modified time stamp used to validate that the file was not modified since
  /// the given time before it was uploaded. The upload will fail with an error
  /// code of <code>PP_ERROR_FILECHANGED</code> if the file has been modified
  /// since the given time. If expected_last_modified_time is 0, then no
  /// validation is performed.
  ///
  /// @return true if successful, false if any of the
  /// parameters are invalid.
  bool AppendFileRangeToBody(const FileRef& file_ref,
                             int64_t start_offset,
                             int64_t length,
                             PP_Time expected_last_modified_time = 0);

  /// SetURL() sets the <code>PP_URLREQUESTPROPERTY_URL</code>
  /// property for the request.
  ///
  /// @param[in] url_string A <code>Var</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetURL(const Var& url_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_URL, url_string);
  }

  /// SetMethod() sets the <code>PP_URLREQUESTPROPERTY_METHOD</code>
  /// (corresponding to a string of type <code>PP_VARTYPE_STRING</code>)
  /// property for the request. This string is either a POST or GET. Refer to
  /// the <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html">HTTP
  /// Methods</a> documentation for further information.
  ///
  /// @param[in] method_string A <code>Var</code> containing the property
  /// value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetMethod(const Var& method_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_METHOD, method_string);
  }

  /// SetHeaders() sets the <code>PP_URLREQUESTPROPERTY_HEADERS</code>
  /// (corresponding to a <code>\n</code> delimited string of type
  /// <code>PP_VARTYPE_STRING</code>) property for the request.
  /// Refer to the
  /// <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html"Header
  /// Field Definitions</a> documentation for further information.
  ///
  /// @param[in] headers_string A <code>Var</code> containing the property
  /// value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetHeaders(const Var& headers_string) {
    return SetProperty(PP_URLREQUESTPROPERTY_HEADERS, headers_string);
  }

  /// SetStreamToFile() sets the
  /// <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> (corresponding
  /// to a bool of type <code>PP_VARTYPE_BOOL</code>). The property is no longer
  /// supported, so this always returns false.
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ///
  /// @return false.
  bool SetStreamToFile(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_STREAMTOFILE, enable);
  }

  /// SetFollowRedirects() sets the
  /// <code>PP_URLREQUESTPROPERTY_FOLLOWREDIRECT</code> (corresponding
  /// to a bool of type <code>PP_VARTYPE_BOOL</code>). The default of the
  /// property is true. Set this value to false if you want to use
  /// URLLoader::FollowRedirects() to follow the redirects only after examining
  /// redirect headers.
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetFollowRedirects(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS, enable);
  }

  /// SetRecordDownloadProgress() sets the
  /// <code>PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGESS</code>
  /// (corresponding to a bool of type <code>PP_VARTYPE_BOOL</code>). The
  /// default of the property is false. Set this value to true if you want to
  /// be able to poll the download progress using
  /// URLLoader::GetDownloadProgress().
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ////
  /// @return true if successful, false if the parameter is invalid.
  bool SetRecordDownloadProgress(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS, enable);
  }

  /// SetRecordUploadProgress() sets the
  /// <code>PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS</code>
  /// (corresponding to a bool of type <code>PP_VARTYPE_BOOL</code>). The
  /// default of the property is false. Set this value to true if you want to
  /// be able to poll the upload progress using URLLoader::GetUploadProgress().
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetRecordUploadProgress(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS, enable);
  }

  /// SetCustomReferrerURL() sets the
  /// <code>PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL</code>
  /// (corresponding to a string of type <code>PP_VARTYPE_STRING</code> or
  /// might be set to undefined as <code>PP_VARTYPE_UNDEFINED</code>). Set it
  /// to a string to set a custom referrer (if empty, the referrer header will
  /// be omitted), or to undefined to use the default referrer. Only loaders
  /// with universal access (only available on trusted implementations) will
  /// accept <code>URLRequestInfo</code> objects that try to set a custom
  /// referrer; if given to a loader without universal access,
  /// <code>PP_ERROR_BADARGUMENT</code> will result.
  ///
  /// @param[in] url A <code>Var</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetCustomReferrerURL(const Var& url) {
    return SetProperty(PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL, url);
  }

  /// SetAllowCrossOriginRequests() sets the
  /// <code>PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS</code>
  /// (corresponding to a bool of type <code>PP_VARTYPE_BOOL</code>). The
  /// default of the property is false. Whether cross-origin requests are
  /// allowed. Cross-origin requests are made using the CORS (Cross-Origin
  /// Resource Sharing) algorithm to check whether the request should be
  /// allowed. For the complete CORS algorithm, refer to the
  /// <a href="http://www.w3.org/TR/access-control">Cross-Origin Resource
  /// Sharing</a> documentation.
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetAllowCrossOriginRequests(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS, enable);
  }

  /// SetAllowCredentials() sets the
  /// <code>PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS</code>
  /// (corresponding to a bool of type <code>PP_VARTYPE_BOOL</code>). The
  /// default of the property is false. Whether HTTP credentials are sent with
  /// cross-origin requests. If false, no credentials are sent with the request
  /// and cookies are ignored in the response. If the request is not
  /// cross-origin, this property is ignored.
  ///
  /// @param[in] enable A <code>bool</code> containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetAllowCredentials(bool enable) {
    return SetProperty(PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS, enable);
  }

  /// SetCustomContentTransferEncoding() sets the
  /// <code>PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING</code>
  /// (corresponding to a string of type <code>PP_VARTYPE_STRING</code> or
  /// might be set to undefined as <code>PP_VARTYPE_UNDEFINED</code>). Set it
  /// to a string to set a custom content-transfer-encoding header (if empty,
  /// that header will be omitted), or to undefined to use the default (if
  /// any). Only loaders with universal access (only available on trusted
  /// implementations) will accept <code>URLRequestInfo</code> objects that try
  /// to set a custom content transfer encoding; if given to a loader without
  /// universal access, <code>PP_ERROR_BADARGUMENT</code> will result.
  ///
  /// @param[in] content_transfer_encoding A <code>Var</code> containing the
  /// property value. To use the default content transfer encoding, set
  /// <code>content_transfer_encoding</code> to an undefined <code>Var</code>.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetCustomContentTransferEncoding(const Var& content_transfer_encoding) {
    return SetProperty(PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING,
                       content_transfer_encoding);
  }

  /// SetPrefetchBufferUpperThreshold() sets the
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD</code>
  /// (corresponding to a integer of type <code>PP_VARTYPE_INT32</code>). The
  /// default is not defined and is set by the browser possibly depending on
  /// system capabilities. Set it to an integer to set an upper threshold for
  /// the prefetched buffer of an asynchronous load. When exceeded, the browser
  /// will defer loading until
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> is hit,
  /// at which time it will begin prefetching again. When setting this
  /// property,
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> must
  /// also be set. Behavior is undefined if the former is <= the latter.
  ///
  /// @param[in] size An int32_t containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetPrefetchBufferUpperThreshold(int32_t size) {
    return SetProperty(PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD,
                       size);
  }

  /// SetPrefetchBufferLowerThreshold() sets the
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD</code>
  /// (corresponding to a integer of type <code>PP_VARTYPE_INT32</code>). The
  /// default is not defined and is set by the browser to a value appropriate
  /// for the default
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD</code>.
  /// Set it to an integer to set a lower threshold for the prefetched buffer
  /// of an asynchronous load. When reached, the browser will resume loading if
  /// If <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> had
  /// previously been reached.
  /// When setting this property,
  /// <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD</code> must also
  /// be set. Behavior is undefined if the former is >= the latter.
  ///
  /// @param[in] size An int32_t containing the property value.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetPrefetchBufferLowerThreshold(int32_t size) {
    return SetProperty(PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD,
                       size);
  }

  /// SetCustomUserAgent() sets the
  /// <code>PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT</code> (corresponding to a
  /// string of type <code>PP_VARTYPE_STRING</code> or might be set to undefined
  /// as <code>PP_VARTYPE_UNDEFINED</code>). Set it to a string to set a custom
  /// user-agent header (if empty, that header will be omitted), or to undefined
  /// to use the default. Only loaders with universal access (only available on
  /// trusted implementations) will accept <code>URLRequestInfo</code> objects
  /// that try to set a custom user agent; if given to a loader without
  /// universal access, <code>PP_ERROR_BADARGUMENT</code> will result.
  ///
  /// @param[in] user_agent A <code>Var</code> containing the property value. To
  /// use the default user agent, set <code>user_agent</code> to an undefined
  /// <code>Var</code>.
  ///
  /// @return true if successful, false if the parameter is invalid.
  bool SetCustomUserAgent(const Var& user_agent) {
    return SetProperty(PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT, user_agent);
  }
};

}  // namespace pp

#endif  // PPAPI_CPP_URL_REQUEST_INFO_H_
