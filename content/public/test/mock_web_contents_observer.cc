// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_web_contents_observer.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace content {

MockWebContentsObserver::MockWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
MockWebContentsObserver::~MockWebContentsObserver() = default;

}  // namespace content
