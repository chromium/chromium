// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/testing/mock_web_associated_url_loader.h"

#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"

namespace blink {

MockWebAssociatedURLLoader::MockWebAssociatedURLLoader() = default;

MockWebAssociatedURLLoader::~MockWebAssociatedURLLoader() = default;

}  // namespace blink
