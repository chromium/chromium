// Copyright 2013 The Chromium Authors. All rights reserved.
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

#ifndef HTTP_STATUS
#error "Do #include net/http/http_status_code.h instead of this file directly."
#endif

// Informational 1xx
HTTP_STATUS(CONTINUE, 100, "Continue")
HTTP_STATUS(SWITCHING_PROTOCOLS, 101, "Switching Protocols")
HTTP_STATUS(EARLY_HINTS, 103, "Early Hints")

// Successful 2xx
HTTP_STATUS(OK, 200, "OK")
HTTP_STATUS(CREATED, 201, "Created")
HTTP_STATUS(ACCEPTED, 202, "Accepted")
HTTP_STATUS(NON_AUTHORITATIVE_INFORMATION, 203, "Non-Authoritative Information")
HTTP_STATUS(NO_CONTENT, 204, "No Content")
HTTP_STATUS(RESET_CONTENT, 205, "Reset Content")
HTTP_STATUS(PARTIAL_CONTENT, 206, "Partial Content")

// Redirection 3xx
HTTP_STATUS(MULTIPLE_CHOICES, 300, "Multiple Choices")
HTTP_STATUS(MOVED_PERMANENTLY, 301, "Moved Permanently")
HTTP_STATUS(FOUND, 302, "Found")
HTTP_STATUS(SEE_OTHER, 303, "See Other")
HTTP_STATUS(NOT_MODIFIED, 304, "Not Modified")
HTTP_STATUS(USE_PROXY, 305, "Use Proxy")
// 306 is no longer used.
HTTP_STATUS(TEMPORARY_REDIRECT, 307, "Temporary Redirect")
HTTP_STATUS(PERMANENT_REDIRECT, 308, "Permanent Redirect")

// Client error 4xx
HTTP_STATUS(BAD_REQUEST, 400, "Bad Request")
HTTP_STATUS(UNAUTHORIZED, 401, "Unauthorized")
HTTP_STATUS(PAYMENT_REQUIRED, 402, "Payment Required")
HTTP_STATUS(FORBIDDEN, 403, "Forbidden")
HTTP_STATUS(NOT_FOUND, 404, "Not Found")
HTTP_STATUS(METHOD_NOT_ALLOWED, 405, "Method Not Allowed")
HTTP_STATUS(NOT_ACCEPTABLE, 406, "Not Acceptable")
HTTP_STATUS(PROXY_AUTHENTICATION_REQUIRED, 407, "Proxy Authentication Required")
HTTP_STATUS(REQUEST_TIMEOUT, 408, "Request Timeout")
HTTP_STATUS(CONFLICT, 409, "Conflict")
HTTP_STATUS(GONE, 410, "Gone")
HTTP_STATUS(LENGTH_REQUIRED, 411, "Length Required")
HTTP_STATUS(PRECONDITION_FAILED, 412, "Precondition Failed")
HTTP_STATUS(REQUEST_ENTITY_TOO_LARGE, 413, "Request Entity Too Large")
HTTP_STATUS(REQUEST_URI_TOO_LONG, 414, "Request-URI Too Long")
HTTP_STATUS(UNSUPPORTED_MEDIA_TYPE, 415, "Unsupported Media Type")
HTTP_STATUS(REQUESTED_RANGE_NOT_SATISFIABLE, 416,
            "Requested Range Not Satisfiable")
HTTP_STATUS(EXPECTATION_FAILED, 417, "Expectation Failed")
// 418 returned by Cloud Print.
HTTP_STATUS(INVALID_XPRIVET_TOKEN, 418, "Invalid XPrivet Token")
HTTP_STATUS(TOO_EARLY, 425, "Too Early")
HTTP_STATUS(TOO_MANY_REQUESTS, 429, "Too Many Requests")

// Server error 5xx
HTTP_STATUS(INTERNAL_SERVER_ERROR, 500, "Internal Server Error")
HTTP_STATUS(NOT_IMPLEMENTED, 501, "Not Implemented")
HTTP_STATUS(BAD_GATEWAY, 502, "Bad Gateway")
HTTP_STATUS(SERVICE_UNAVAILABLE, 503, "Service Unavailable")
HTTP_STATUS(GATEWAY_TIMEOUT, 504, "Gateway Timeout")
HTTP_STATUS(VERSION_NOT_SUPPORTED, 505, "HTTP Version Not Supported")
