// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_function_constants.h"

namespace extensions {
namespace function_constants {

// An error thrown when determining the WebContents that sent the request for
// the API call failed. Note: typically, this would only happen if the
// WebContents disappeared after the API call (i.e., the caller is no longer
// alive, such as a tab closing or background page suspending). For this reason,
// the error is not overly helpful. However, it is important that we have a
// specific error message in order to track down any peculiar cases.
const char kCouldNotFindSenderWebContents[] =
    "Could not find sender WebContents.";

}  // namespace function_constants
}  // namespace extensions
