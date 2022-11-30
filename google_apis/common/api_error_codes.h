// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_API_ERROR_CODES_H_
#define GOOGLE_APIS_COMMON_API_ERROR_CODES_H_

#include <string>

namespace google_apis {

// HTTP errors that can be returned by Google API service.
enum ApiErrorCode {
  // 200-599
  // HTTP error codes:
  HTTP_SUCCESS = 200,
  HTTP_CREATED = 201,
  HTTP_NO_CONTENT = 204,
  HTTP_FOUND = 302,
  HTTP_NOT_MODIFIED = 304,
  HTTP_RESUME_INCOMPLETE = 308,
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED = 401,
  HTTP_FORBIDDEN = 403,
  HTTP_NOT_FOUND = 404,
  HTTP_CONFLICT = 409,
  HTTP_GONE = 410,
  HTTP_LENGTH_REQUIRED = 411,
  HTTP_PRECONDITION = 412,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_BAD_GATEWAY = 502,
  HTTP_SERVICE_UNAVAILABLE = 503,

  // 900-999
  // Common API error codes:
  NO_CONNECTION = 900,
  NOT_READY = 901,
  OTHER_ERROR = 902,
  CANCELLED = 903,
  PARSE_ERROR = 904,

  // 1000-1999
  // Drive API error codes:
  DRIVE_FILE_ERROR = 1000,
  DRIVE_NO_SPACE = 1001,
  DRIVE_RESPONSE_TOO_LARGE = 1002,

  // Needed in order to log this in UMA.
  kMaxValue = DRIVE_RESPONSE_TOO_LARGE,
};

// Returns a string representation of ApiErrorCode.
std::string ApiErrorCodeToString(ApiErrorCode error);

// Checks if the error code represents success for drive api.
bool IsSuccessfulDriveApiErrorCode(ApiErrorCode error);

// Checks if the error code represents success for calendar api.
bool IsSuccessfulCalendarApiErrorCode(ApiErrorCode error);

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_API_ERROR_CODES_H_
