// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// WebScopedPagePauser implements the concept of 'pause' in HTML standard.
// https://html.spec.whatwg.org/C/#pause
// All script execution is suspended while any of WebScopedPagePauser instances
// exists.
class WebScopedPagePauser {
 public:
  BLINK_EXPORT static std::unique_ptr<WebScopedPagePauser> Create();

  BLINK_EXPORT ~WebScopedPagePauser();

 private:
  WebScopedPagePauser();

  DISALLOW_COPY_AND_ASSIGN(WebScopedPagePauser);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCOPED_PAGE_PAUSER_H_
