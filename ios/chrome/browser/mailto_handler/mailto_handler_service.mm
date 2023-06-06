// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mailto_handler/mailto_handler_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MailtoHandlerService::MailtoHandlerService() = default;

MailtoHandlerService::~MailtoHandlerService() = default;

void MailtoHandlerService::HandleMailtoURL(NSURL* url,
                                           base::OnceClosure completion) {
  // TODO(crbug.com/1443722): Remove this implementation once all subclasses
  // provide their own. For now, forward the `url` to `HandleMailtoURL()` which
  // is specialized by each subclass.
  HandleMailtoURL(url);
  std::move(completion).Run();
}

void MailtoHandlerService::HandleMailtoURL(NSURL* url) {
  // TODO(crbug.com/1443722): Remove this implementation once all subclasses
  // overrides have been removed. This implementation is only provided so
  // overrides can be removed, which could not be done if this was pure virtual.
  NOTREACHED();
}
