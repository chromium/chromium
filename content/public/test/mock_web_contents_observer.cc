// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_web_contents_observer.h"

namespace content {

MockWebContentsObserver::MockWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
MockWebContentsObserver::~MockWebContentsObserver() = default;

}  // namespace content
