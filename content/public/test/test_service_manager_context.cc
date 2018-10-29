// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_service_manager_context.h"

#include "base/task/post_task.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/service_manager_connection.h"

namespace content {

TestServiceManagerContext::TestServiceManagerContext() {
  // Isolate from previous tests that may have already initialized
  // ServiceManagerConnection (e.g. in
  // RenderProcessHostImpl::InitializeChannelProxy()).
  ServiceManagerConnection::DestroyForProcess();
  context_.reset(new ServiceManagerContext(
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));
  ServiceManagerContext::StartBrowserConnection();
}

TestServiceManagerContext::~TestServiceManagerContext() {
  ChildProcessLauncher::ResetRegisteredFilesForTesting();
}

}  // namespace content
