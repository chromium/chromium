// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_URL_RESPONSE_INFO_H_
#define PPAPI_CPP_URL_RESPONSE_INFO_H_

#include <stdint.h>

#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the API for examining URL responses.
namespace pp {

class FileRef;

/// URLResponseInfo provides an API for examining URL responses.
class URLResponseInfo : public Resource {
 public:
  /// Default constructor. This constructor creates an <code>is_null</code>
  /// resource.
  URLResponseInfo() {}

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has already been reference counted.
  ///
  /// @param[in] resource A <code>PP_Resource</code> corresponding to a
  /// resource.
  URLResponseInfo(PassRef, PP_Resource resource);

  /// The copy constructor for <code>URLResponseInfo</code>.
  URLResponseInfo(const URLResponseInfo& other);
  URLResponseInfo& operator=(const URLResponseInfo& other);

  /// This function gets a response property.
  ///
  /// @param[in] property A <code>PP_URLResponseProperty</code> identifying the
  /// type of property in the response.
  ///
  /// @return A <code>Var</code> containing the response property value if
  /// successful, <code>is_undefined Var</code> if an input parameter is
  /// invalid.
  Var GetProperty(PP_URLResponseProperty property) const;

  /// This function returns an <code>is_null</code> object, as the
  /// <code>PP_URLREQUESTPROPERTY_STREAMTOFILE</code> is no longer supported.
  /// <code>URLResponseInfo</code> is closed or destroyed.
  ///
  /// @return An <code>is_null</code> object.
  FileRef GetBodyAsFileRef() const;

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_URL</code>
  /// property for the response.
  ///
  /// @return An <code>is_string Var</code> containing the response property
  /// value if successful, <code>is_undefined Var</code> if an input parameter
  /// is invalid.
  Var GetURL() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_URL);
  }

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_REDIRECTURL</code>
  /// property for the response.
  ///
  /// @return An <code>is_string Var</code> containing the response property
  /// value if successful, <code>is_undefined Var</code> if an input parameter
  /// is invalid.
  Var GetRedirectURL() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_REDIRECTURL);
  }

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_REDIRECTMETHOD</code>
  /// property for the response.
  ///
  /// @return An <code>is_string Var</code> containing the response property
  /// value if successful, <code>is_undefined Var</code> if an input parameter
  /// is invalid.
  Var GetRedirectMethod() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_REDIRECTMETHOD);
  }

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_STATUSCODE</code>
  /// property for the response.
  ///
  /// @return A int32_t containing the response property value if successful,
  /// <code>is_undefined Var</code> if an input parameter is invalid.
  int32_t GetStatusCode() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_STATUSCODE).AsInt();
  }

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_STATUSLINE</code>
  /// property for the response.
  ///
  /// @return An <code>is_string Var</code> containing the response property
  /// value if successful, <code>is_undefined Var</code> if an input parameter
  /// is invalid.
  Var GetStatusLine() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_STATUSLINE);
  }

  /// This function gets the <code>PP_URLRESPONSEPROPERTY_HEADERS</code>
  /// property for the response.
  ///
  /// @return An <code>is_string Var</code> containing the response property
  /// value if successful, <code>is_undefined Var</code> if an input parameter
  /// is invalid.
  Var GetHeaders() const {
    return GetProperty(PP_URLRESPONSEPROPERTY_HEADERS);
  }
};

}  // namespace pp

#endif  // PPAPI_CPP_URL_RESPONSE_INFO_H_
