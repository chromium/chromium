// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_url_loader_client.h"

// This WebURLLoaderClient.cpp, which includes only
// WebURLLoaderClient.h, should be in Source/platform/exported,
// because WebURLLoaderClient is not compiled without this cpp.
// So if we don't have this cpp, we will see unresolved symbol error
// when constructor/destructor's address is required.
