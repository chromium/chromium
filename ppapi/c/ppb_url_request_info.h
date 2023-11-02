/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From ppb_url_request_info.idl modified Thu May 17 11:28:52 2018. */

#ifndef PPAPI_C_PPB_URL_REQUEST_INFO_H_
#define PPAPI_C_PPB_URL_REQUEST_INFO_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/pp_var.h"

#define PPB_URLREQUESTINFO_INTERFACE_1_0 "PPB_URLRequestInfo;1.0"
#define PPB_URLREQUESTINFO_INTERFACE PPB_URLREQUESTINFO_INTERFACE_1_0

/**
 * @file
 * This file defines the <code>PPB_URLRequestInfo</code> API for creating and
 * manipulating URL requests.
 */


/**
 * @addtogroup Enums
 * @{
 */
/**
 * This enumeration contains properties that can be set on a URL request.
 */
typedef enum {
  /** This corresponds to a string (<code>PP_VARTYPE_STRING</code>). */
  PP_URLREQUESTPROPERTY_URL = 0,
  /**
   * This corresponds to a string (<code>PP_VARTYPE_STRING</code>); either
   * POST or GET. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html">HTTP
   * Methods</a> documentation for further information.
   *
   */
  PP_URLREQUESTPROPERTY_METHOD = 1,
  /**
   * This corresponds to a string (<code>PP_VARTYPE_STRING</code>); \n
   * delimited. Refer to the
   * <a href="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html"Header
   * Field Definitions</a> documentation for further information.
   */
  PP_URLREQUESTPROPERTY_HEADERS = 2,
  /**
   * This corresponds to a <code>PP_Bool</code> (<code>PP_VARTYPE_BOOL</code>;
   * default=<code>PP_FALSE</code>).
   * This property is no longer supported, so attempting to set it will always
   * fail.
   */
  PP_URLREQUESTPROPERTY_STREAMTOFILE = 3,
  /**
   * This corresponds to a <code>PP_Bool</code> (<code>PP_VARTYPE_BOOL</code>;
   * default=<code>PP_TRUE</code>).
   * Set this value to <code>PP_FALSE</code> if you want to use
   * PPB_URLLoader.FollowRedirects() to follow the redirects only after
   * examining redirect headers.
   */
  PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS = 4,
  /**
   * This corresponds to a <code>PP_Bool</code> (<code>PP_VARTYPE_BOOL</code>;
   * default=<code>PP_FALSE</code>).
   * Set this value to <code>PP_TRUE</code> if you want to be able to poll the
   * download progress using PPB_URLLoader.GetDownloadProgress().
   */
  PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS = 5,
  /**
   * This corresponds to a <code>PP_Bool</code>
   * (default=<code>PP_FALSE</code>). Set this value to <code>PP_TRUE</code> if
   * you want to be able to poll the upload progress using
   * PPB_URLLoader.GetUploadProgress().
   */
  PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS = 6,
  /**
   * This corresponds to a string (<code>PP_VARTYPE_STRING)</code> or may be
   * undefined (<code>PP_VARTYPE_UNDEFINED</code>; default).
   * Set it to a string to set a custom referrer (if empty, the referrer header
   * will be omitted), or to undefined to use the default referrer. Only loaders
   * with universal access (only available on trusted implementations) will
   * accept <code>URLRequestInfo</code> objects that try to set a custom
   * referrer; if given to a loader without universal access,
   * <code>PP_ERROR_NOACCESS</code> will result.
   */
  PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL = 7,
  /**
   * This corresponds to a <code>PP_Bool</code> (<code>PP_VARTYPE_BOOL</code>;
   * default=<code>PP_FALSE</code>). Whether cross-origin requests are allowed.
   * Cross-origin requests are made using the CORS (Cross-Origin Resource
   * Sharing) algorithm to check whether the request should be allowed. For the
   * complete CORS algorithm, refer to
   * the <a href="http://www.w3.org/TR/access-control">Cross-Origin Resource
   * Sharing</a> documentation.
   */
  PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS = 8,
  /**
   * This corresponds to a <code>PP_Bool</code> (<code>PP_VARTYPE_BOOL</code>;
   * default=<code>PP_FALSE</code>).
   * Whether HTTP credentials are sent with cross-origin requests. If false,
   * no credentials are sent with the request and cookies are ignored in the
   * response. If the request is not cross-origin, this property is ignored.
   */
  PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS = 9,
  /**
   * This corresponds to a string (<code>PP_VARTYPE_STRING</code>) or may be
   * undefined (<code>PP_VARTYPE_UNDEFINED</code>; default).
   * Set it to a string to set a custom content-transfer-encoding header (if
   * empty, that header will be omitted), or to undefined to use the default
   * (if any). Only loaders with universal access (only available on trusted
   * implementations) will accept <code>URLRequestInfo</code> objects that try
   * to set a custom content transfer encoding; if given to a loader without
   * universal access, <code>PP_ERROR_NOACCESS</code> will result.
   */
  PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING = 10,
  /**
   * This corresponds to an integer (<code>PP_VARTYPE_INT32</code>); default
   * is not defined and is set by the browser, possibly depending on system
   * capabilities. Set it to an integer to set an upper threshold for the
   * prefetched buffer of an asynchronous load. When exceeded, the browser will
   * defer loading until
   * <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> is hit,
   * at which time it will begin prefetching again. When setting this property,
   * <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> must also
   * be set. Behavior is undefined if the former is <= the latter.
   */
  PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD = 11,
  /**
   * This corresponds to an integer (<code>PP_VARTYPE_INT32</code>); default is
   * not defined and is set by the browser to a value appropriate for the
   * default <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD</code>.
   * Set it to an integer to set a lower threshold for the prefetched buffer
   * of an asynchronous load. When reached, the browser will resume loading if
   * If <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERERTHRESHOLD</code> had
   * previously been reached.
   * When setting this property,
   * <code>PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD</code> must also
   * be set. Behavior is undefined if the former is >= the latter.
   */
  PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD = 12,
  /**
   * This corresponds to a string (<code>PP_VARTYPE_STRING</code>) or may be
   * undefined (<code>PP_VARTYPE_UNDEFINED</code>; default). Set it to a string
   * to set a custom user-agent header (if empty, that header will be omitted),
   * or to undefined to use the default. Only loaders with universal access
   * (only available on trusted implementations) will accept
   * <code>URLRequestInfo</code> objects that try to set a custom user agent; if
   * given to a loader without universal access, <code>PP_ERROR_NOACCESS</code>
   * will result.
   */
  PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT = 13
} PP_URLRequestProperty;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLRequestProperty, 4);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * The <code>PPB_URLRequestInfo</code> interface is used to create
 * and handle URL requests. This API is used in conjunction with
 * <code>PPB_URLLoader</code>. Refer to <code>PPB_URLLoader</code> for further
 * information.
 */
struct PPB_URLRequestInfo_1_0 {
  /**
   * Create() creates a new <code>URLRequestInfo</code> object.
   *
   * @param[in] instance A <code>PP_Instance</code> identifying one instance
   * of a module.
   *
   * @return A <code>PP_Resource</code> identifying the
   * <code>URLRequestInfo</code> if successful, 0 if the instance is invalid.
   */
  PP_Resource (*Create)(PP_Instance instance);
  /**
   * IsURLRequestInfo() determines if a resource is a
   * <code>URLRequestInfo</code>.
   *
   * @param[in] resource A <code>PP_Resource</code> corresponding to a
   * <code>URLRequestInfo</code>.
   *
   * @return <code>PP_TRUE</code> if the resource is a
   * <code>URLRequestInfo</code>, <code>PP_FALSE</code> if the resource is
   * invalid or some type other than <code>URLRequestInfo</code>.
   */
  PP_Bool (*IsURLRequestInfo)(PP_Resource resource);
  /**
   * SetProperty() sets a request property. The value of the property must be
   * the correct type according to the property being set.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLRequestInfo</code>.
   * @param[in] property A <code>PP_URLRequestProperty</code> identifying the
   * property to set.
   * @param[in] value A <code>PP_Var</code> containing the property value.
   *
   * @return <code>PP_TRUE</code> if successful, <code>PP_FALSE</code> if any
   * of the parameters are invalid.
   */
  PP_Bool (*SetProperty)(PP_Resource request,
                         PP_URLRequestProperty property,
                         struct PP_Var value);
  /**
   * AppendDataToBody() appends data to the request body. A Content-Length
   * request header will be automatically generated.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLRequestInfo</code>.
   * @param[in] data A pointer to a buffer holding the data.
   * @param[in] len The length, in bytes, of the data.
   *
   * @return <code>PP_TRUE</code> if successful, <code>PP_FALSE</code> if any
   * of the parameters are invalid.
   *
   *
   */
  PP_Bool (*AppendDataToBody)(PP_Resource request,
                              const void* data,
                              uint32_t len);
  /**
   * AppendFileToBody() appends a file, to be uploaded, to the request body.
   * A content-length request header will be automatically generated.
   *
   * @param[in] request A <code>PP_Resource</code> corresponding to a
   * <code>URLRequestInfo</code>.
   * @param[in] file_ref A <code>PP_Resource</code> corresponding to a file
   * reference.
   * @param[in] start_offset An optional starting point offset within the
   * file.
   * @param[in] number_of_bytes An optional number of bytes of the file to
   * be included. If <code>number_of_bytes</code> is -1, then the sub-range
   * to upload extends to the end of the file.
   * @param[in] expected_last_modified_time An optional (non-zero) last
   * modified time stamp used to validate that the file was not modified since
   * the given time before it was uploaded. The upload will fail with an error
   * code of <code>PP_ERROR_FILECHANGED</code> if the file has been modified
   * since the given time. If <code>expected_last_modified_time</code> is 0,
   * then no validation is performed.
   *
   * @return <code>PP_TRUE</code> if successful, <code>PP_FALSE</code> if any
   * of the parameters are invalid.
   */
  PP_Bool (*AppendFileToBody)(PP_Resource request,
                              PP_Resource file_ref,
                              int64_t start_offset,
                              int64_t number_of_bytes,
                              PP_Time expected_last_modified_time);
};

typedef struct PPB_URLRequestInfo_1_0 PPB_URLRequestInfo;
/**
 * @}
 */

#endif  /* PPAPI_C_PPB_URL_REQUEST_INFO_H_ */

