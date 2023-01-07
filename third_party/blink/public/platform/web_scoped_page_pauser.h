// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// WebScopedPagePauser implements the concept of 'pause' in HTML standard.
// https://html.spec.whatwg.org/C/#pause
// All script execution is suspended while any of WebScopedPagePauser instances
// exists.
class WebScopedPagePauser {
 public:
  BLINK_EXPORT static std::unique_ptr<WebScopedPagePauser> Create();

  WebScopedPagePauser(const WebScopedPagePauser&) = delete;
  WebScopedPagePauser& operator=(const WebScopedPagePauser&) = delete;
  BLINK_EXPORT ~WebScopedPagePauser();

 private:
  WebScopedPagePauser();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
