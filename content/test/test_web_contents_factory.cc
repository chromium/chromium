// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_web_contents_factory.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

namespace content {

TestWebContentsFactory::TestWebContentsFactory()
    : rvh_enabler_(new content::RenderViewHostTestEnabler()) {
}

TestWebContentsFactory::~TestWebContentsFactory() {
  // We explicitly clear the vector to force destruction of any created web
  // contents so that we can properly handle their cleanup (running posted
  // tasks, etc).
  web_contents_.clear();
  // Let any posted tasks for web contents deletion run.
  base::RunLoop().RunUntilIdle();
  rvh_enabler_.reset();
  // Let any posted tasks for RenderProcess/ViewHost deletion run.
  base::RunLoop().RunUntilIdle();
}

WebContents* TestWebContentsFactory::CreateWebContents(
    BrowserContext* context) {
  web_contents_.push_back(
      WebContentsTester::CreateTestWebContents(context, nullptr));
  DCHECK(web_contents_.back());
  return web_contents_.back().get();
}

void TestWebContentsFactory::DestroyWebContents(WebContents* contents) {
  auto it = web_contents_.begin();
  for (; it != web_contents_.end(); ++it) {
    if (it->get() == contents)
      break;
  }
  if (it == web_contents_.end())
    return;
  web_contents_.erase(it);
}

}  // namespace content
