// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/result_catcher.h"

#include "base/run_loop.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"

namespace extensions {

ResultCatcher::ResultCatcher() : browser_context_restriction_(nullptr) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_TEST_PASSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_TEST_FAILED,
                 content::NotificationService::AllSources());
}

ResultCatcher::~ResultCatcher() {
}

bool ResultCatcher::GetNextResult() {
  // Depending on the tests, multiple results can come in from a single call
  // to RunMessageLoop(), so we maintain a queue of results and just pull them
  // off as the test calls this, going to the run loop only when the queue is
  // empty.
  if (results_.empty()) {
    base::RunLoop run_loop;
    quit_closure_ = content::GetDeferredQuitTaskForRunLoop(&run_loop);
    content::RunThisRunLoop(&run_loop);
  }

  if (!results_.empty()) {
    bool ret = results_.front();
    results_.pop_front();
    message_ = messages_.front();
    messages_.pop_front();
    return ret;
  }

  NOTREACHED();
  return false;
}

void ResultCatcher::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  if (browser_context_restriction_ &&
      content::Source<content::BrowserContext>(source).ptr() !=
          browser_context_restriction_) {
    return;
  }

  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_TEST_PASSED:
      VLOG(1) << "Got EXTENSION_TEST_PASSED notification.";
      results_.push_back(true);
      messages_.push_back(std::string());
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
      break;

    case extensions::NOTIFICATION_EXTENSION_TEST_FAILED:
      VLOG(1) << "Got EXTENSION_TEST_FAILED notification.";
      results_.push_back(false);
      messages_.push_back(*(content::Details<std::string>(details).ptr()));
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
      break;

    default:
      NOTREACHED();
  }
}

}  // namespace extensions
