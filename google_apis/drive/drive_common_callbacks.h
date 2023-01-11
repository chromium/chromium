// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains callback types used for communicating with the Drive
// server via WAPI (Documents List API) and Drive API.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_
#define GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_

#include "base/functional/callback.h"
#include "google_apis/common/api_error_codes.h"

namespace google_apis {

class AboutResource;

// Callback used for getting AboutResource.
typedef base::OnceCallback<void(ApiErrorCode error,
                                std::unique_ptr<AboutResource> about_resource)>
    AboutResourceCallback;

// Closure for canceling a certain request. Each request-issuing method returns
// this type of closure. If it is called during the request is in-flight, the
// callback passed with the request is invoked with CANCELLED. If the
// request is already finished, nothing happens.
typedef base::OnceClosure CancelCallbackOnce;
typedef base::RepeatingClosure CancelCallbackRepeating;

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_COMMON_CALLBACKS_H_
