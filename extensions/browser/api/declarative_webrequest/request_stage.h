// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGE_H_

namespace extensions {

// The stages of the web request during which a condition could be tested and
// an action could be applied. This is required because for example the response
// headers cannot be tested before a request has been sent. Note that currently
// not all stages are supported in declarative Web Request, only those marked
// as "active" in |kActiveStages| below.
enum RequestStage {
  ON_BEFORE_REQUEST = 1 << 0,
  ON_BEFORE_SEND_HEADERS = 1 << 1,
  ON_SEND_HEADERS = 1 << 2,
  ON_HEADERS_RECEIVED = 1 << 3,
  ON_AUTH_REQUIRED = 1 << 4,
  ON_BEFORE_REDIRECT = 1 << 5,
  ON_RESPONSE_STARTED = 1 << 6,
  ON_COMPLETED = 1 << 7,
  ON_ERROR = 1 << 8
};

// The bitmap with active stages.
extern const unsigned int kActiveStages;

// The highest bit in |kActiveStages|. This allows to iterate over all active
// stages in a "for" loop.
extern const unsigned int kLastActiveStage;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_WEBREQUEST_REQUEST_STAGE_H_
