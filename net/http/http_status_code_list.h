// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values. The following line silences a
// presubmit and Tricium warning that would otherwise be triggered by this:
// no-include-guard-because-multiply-included
// NOLINT(build/header_guard)

// This file contains the list of HTTP status codes. Taken from IANA HTTP Status
// Code Registry.
// http://www.iana.org/assignments/http-status-codes/http-status-codes.xml

#ifndef HTTP_STATUS_ENUM_VALUE
#error "Do #include net/http/http_status_code.h instead of this file directly."
#endif

// Informational 1xx
HTTP_STATUS_ENUM_VALUE(CONTINUE, 100, "Continue")
HTTP_STATUS_ENUM_VALUE(SWITCHING_PROTOCOLS, 101, "Switching Protocols")
HTTP_STATUS_ENUM_VALUE(PROCESSING, 102, "Processing")
HTTP_STATUS_ENUM_VALUE(EARLY_HINTS, 103, "Early Hints")

// Successful 2xx
HTTP_STATUS_ENUM_VALUE(OK, 200, "OK")
HTTP_STATUS_ENUM_VALUE(CREATED, 201, "Created")
HTTP_STATUS_ENUM_VALUE(ACCEPTED, 202, "Accepted")
HTTP_STATUS_ENUM_VALUE(NON_AUTHORITATIVE_INFORMATION,
                       203,
                       "Non-Authoritative Information")
HTTP_STATUS_ENUM_VALUE(NO_CONTENT, 204, "No Content")
HTTP_STATUS_ENUM_VALUE(RESET_CONTENT, 205, "Reset Content")
HTTP_STATUS_ENUM_VALUE(PARTIAL_CONTENT, 206, "Partial Content")
HTTP_STATUS_ENUM_VALUE(MULTI_STATUS, 207, "Multi-Status")
HTTP_STATUS_ENUM_VALUE(ALREADY_REPORTED, 208, "Already Reported")
HTTP_STATUS_ENUM_VALUE(IM_USED, 226, "IM Used")

// Redirection 3xx
HTTP_STATUS_ENUM_VALUE(MULTIPLE_CHOICES, 300, "Multiple Choices")
HTTP_STATUS_ENUM_VALUE(MOVED_PERMANENTLY, 301, "Moved Permanently")
HTTP_STATUS_ENUM_VALUE(FOUND, 302, "Found")
HTTP_STATUS_ENUM_VALUE(SEE_OTHER, 303, "See Other")
HTTP_STATUS_ENUM_VALUE(NOT_MODIFIED, 304, "Not Modified")
HTTP_STATUS_ENUM_VALUE(USE_PROXY, 305, "Use Proxy")
// 306 is no longer used.
HTTP_STATUS_ENUM_VALUE(TEMPORARY_REDIRECT, 307, "Temporary Redirect")
HTTP_STATUS_ENUM_VALUE(PERMANENT_REDIRECT, 308, "Permanent Redirect")

// Client error 4xx
HTTP_STATUS_ENUM_VALUE(BAD_REQUEST, 400, "Bad Request")
HTTP_STATUS_ENUM_VALUE(UNAUTHORIZED, 401, "Unauthorized")
HTTP_STATUS_ENUM_VALUE(PAYMENT_REQUIRED, 402, "Payment Required")
HTTP_STATUS_ENUM_VALUE(FORBIDDEN, 403, "Forbidden")
HTTP_STATUS_ENUM_VALUE(NOT_FOUND, 404, "Not Found")
HTTP_STATUS_ENUM_VALUE(METHOD_NOT_ALLOWED, 405, "Method Not Allowed")
HTTP_STATUS_ENUM_VALUE(NOT_ACCEPTABLE, 406, "Not Acceptable")
HTTP_STATUS_ENUM_VALUE(PROXY_AUTHENTICATION_REQUIRED,
                       407,
                       "Proxy Authentication Required")
HTTP_STATUS_ENUM_VALUE(REQUEST_TIMEOUT, 408, "Request Timeout")
HTTP_STATUS_ENUM_VALUE(CONFLICT, 409, "Conflict")
HTTP_STATUS_ENUM_VALUE(GONE, 410, "Gone")
HTTP_STATUS_ENUM_VALUE(LENGTH_REQUIRED, 411, "Length Required")
HTTP_STATUS_ENUM_VALUE(PRECONDITION_FAILED, 412, "Precondition Failed")
HTTP_STATUS_ENUM_VALUE(REQUEST_ENTITY_TOO_LARGE,
                       413,
                       "Request Entity Too Large")
HTTP_STATUS_ENUM_VALUE(REQUEST_URI_TOO_LONG, 414, "Request-URI Too Long")
HTTP_STATUS_ENUM_VALUE(UNSUPPORTED_MEDIA_TYPE, 415, "Unsupported Media Type")
HTTP_STATUS_ENUM_VALUE(REQUESTED_RANGE_NOT_SATISFIABLE,
                       416,
                       "Requested Range Not Satisfiable")
HTTP_STATUS_ENUM_VALUE(EXPECTATION_FAILED, 417, "Expectation Failed")
// 418 returned by Cloud Print.
HTTP_STATUS_ENUM_VALUE(INVALID_XPRIVET_TOKEN, 418, "Invalid XPrivet Token")
HTTP_STATUS_ENUM_VALUE(MISDIRECTED_REQUEST, 421, "Misdirected Request")
HTTP_STATUS_ENUM_VALUE(UNPROCESSABLE_CONTENT, 422, "Unprocessable Content")
HTTP_STATUS_ENUM_VALUE(LOCKED, 423, "Locked")
HTTP_STATUS_ENUM_VALUE(FAILED_DEPENDENCY, 424, "Failed Dependency")
HTTP_STATUS_ENUM_VALUE(TOO_EARLY, 425, "Too Early")
HTTP_STATUS_ENUM_VALUE(UPGRADE_REQUIRED, 426, "Upgrade Required")
HTTP_STATUS_ENUM_VALUE(PRECONDITION_REQUIRED, 428, "Precondition Required")
HTTP_STATUS_ENUM_VALUE(TOO_MANY_REQUESTS, 429, "Too Many Requests")
HTTP_STATUS_ENUM_VALUE(REQUEST_HEADER_FIELDS_TOO_LARGE,
                       431,
                       "Request Header Fields Too Large")
HTTP_STATUS_ENUM_VALUE(UNAVAILABLE_FOR_LEGAL_REASONS,
                       451,
                       "Unavailable For Legal Reasons")

// Server error 5xx
HTTP_STATUS_ENUM_VALUE(INTERNAL_SERVER_ERROR, 500, "Internal Server Error")
HTTP_STATUS_ENUM_VALUE(NOT_IMPLEMENTED, 501, "Not Implemented")
HTTP_STATUS_ENUM_VALUE(BAD_GATEWAY, 502, "Bad Gateway")
HTTP_STATUS_ENUM_VALUE(SERVICE_UNAVAILABLE, 503, "Service Unavailable")
HTTP_STATUS_ENUM_VALUE(GATEWAY_TIMEOUT, 504, "Gateway Timeout")
HTTP_STATUS_ENUM_VALUE(VERSION_NOT_SUPPORTED, 505, "HTTP Version Not Supported")
HTTP_STATUS_ENUM_VALUE(VARIANT_ALSO_NEGOTIATES, 506, "Variant Also Negotiates")
HTTP_STATUS_ENUM_VALUE(INSUFFICIENT_STORAGE, 507, "Insufficient Storage")
HTTP_STATUS_ENUM_VALUE(LOOP_DETECTED, 508, "Loop Detected")
HTTP_STATUS_ENUM_VALUE(NOT_EXTENDED_OBSOLETED, 510, "Not Extended (Obsoleted)")
HTTP_STATUS_ENUM_VALUE(NETWORK_AUTHENTICATION_REQUIRED,
                       511,
                       "Network Authentication Required")

// Max value for histograms. Should not be recorded.
HTTP_STATUS_ENUM_VALUE(STATUS_CODE_MAX, 600, "HTTP Status Code Max")
