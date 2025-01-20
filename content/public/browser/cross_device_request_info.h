// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CROSS_DEVICE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_CROSS_DEVICE_REQUEST_INFO_H_

#include "base/values.h"
#include "url/origin.h"

namespace content::digital_credentials::cross_device {

// A struct that contains the information needed for the cross-device digital
// credentials request.
struct RequestInfo {
  // The type of request to be sent via the cross-device flow.
  enum class RequestType { kGet, kCreate };

  // The type of the request
  RequestType request_type;

  // The origin of the requesting page (a.k.a the verifier). This is required to
  // be sent to the CredMan API.
  url::Origin rp_origin;

  // The request that was sent from the verifier as would be found in place of
  // "$1" in the following
  // Javascript:  `navigator.credentials.foo({digital: $1});`.
  base::Value request;
};

}  // namespace content::digital_credentials::cross_device

#endif  // CONTENT_PUBLIC_BROWSER_CROSS_DEVICE_REQUEST_INFO_H_
