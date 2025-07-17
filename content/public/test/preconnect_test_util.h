// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PRECONNECT_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_PRECONNECT_TEST_UTIL_H_

#include "content/public/browser/preconnect_request.h"

namespace content {

bool operator==(const PreconnectRequest& lhs, const PreconnectRequest& rhs);

}

#endif  // CONTENT_PUBLIC_TEST_PRECONNECT_TEST_UTIL_H_
