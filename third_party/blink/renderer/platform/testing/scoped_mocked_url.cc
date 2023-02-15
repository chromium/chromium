// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace test {

ScopedMockedURL::ScopedMockedURL(const WebURL& url) : url_(url) {}

ScopedMockedURL::~ScopedMockedURL() {
  url_test_helpers::RegisterMockedURLUnregister(url_);
}

ScopedMockedURLLoad::ScopedMockedURLLoad(const WebURL& full_url,
                                         const WebString& file_path,
                                         const WebString& mime_type)
    : ScopedMockedURL(full_url) {
  url_test_helpers::RegisterMockedURLLoad(full_url, file_path, mime_type);
}

}  // namespace test
}  // namespace blink
