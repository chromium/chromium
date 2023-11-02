// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/api_error_codes.h"

#include "base/strings/string_number_conversions.h"

namespace google_apis {

std::string ApiErrorCodeToString(ApiErrorCode error) {
  switch (error) {
    case HTTP_SUCCESS:
      return "HTTP_SUCCESS";

    case HTTP_CREATED:
      return "HTTP_CREATED";

    case HTTP_NO_CONTENT:
      return "HTTP_NO_CONTENT";

    case HTTP_FOUND:
      return "HTTP_FOUND";

    case HTTP_NOT_MODIFIED:
      return "HTTP_NOT_MODIFIED";

    case HTTP_RESUME_INCOMPLETE:
      return "HTTP_RESUME_INCOMPLETE";

    case HTTP_BAD_REQUEST:
      return "HTTP_BAD_REQUEST";

    case HTTP_UNAUTHORIZED:
      return "HTTP_UNAUTHORIZED";

    case HTTP_FORBIDDEN:
      return "HTTP_FORBIDDEN";

    case HTTP_NOT_FOUND:
      return "HTTP_NOT_FOUND";

    case HTTP_CONFLICT:
      return "HTTP_CONFLICT";

    case HTTP_GONE:
      return "HTTP_GONE";

    case HTTP_LENGTH_REQUIRED:
      return "HTTP_LENGTH_REQUIRED";

    case HTTP_PRECONDITION:
      return "HTTP_PRECONDITION";

    case HTTP_INTERNAL_SERVER_ERROR:
      return "HTTP_INTERNAL_SERVER_ERROR";

    case HTTP_NOT_IMPLEMENTED:
      return "HTTP_NOT_IMPLEMENTED";

    case HTTP_BAD_GATEWAY:
      return "HTTP_BAD_GATEWAY";

    case HTTP_SERVICE_UNAVAILABLE:
      return "HTTP_SERVICE_UNAVAILABLE";

    case NO_CONNECTION:
      return "NO_CONNECTION";

    case NOT_READY:
      return "NOT_READY";

    case OTHER_ERROR:
      return "OTHER_ERROR";

    case CANCELLED:
      return "CANCELLED";

    case PARSE_ERROR:
      return "PARSE_ERROR";

    case DRIVE_FILE_ERROR:
      return "DRIVE_FILE_ERROR";

    case DRIVE_NO_SPACE:
      return "DRIVE_NO_SPACE";

    case DRIVE_RESPONSE_TOO_LARGE:
      return "DRIVE_RESPONSE_TOO_LARGE";
  }

  return "UNKNOWN_ERROR_" + base::NumberToString(error);
}

bool IsSuccessfulDriveApiErrorCode(ApiErrorCode error) {
  return 200 <= error && error <= 299;
}

bool IsSuccessfulCalendarApiErrorCode(ApiErrorCode error) {
  return error == 200;
}

}  // namespace google_apis
