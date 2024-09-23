// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"

#include "base/files/file_path.h"
#include "fuchsia_web/webengine/test/test_data.h"

// TODO(crbug.com/40735380): Remove this and use default after updating tests in
// frame_impl_browser_test_base.cc to use the appropriate base.
FrameImplTestBase::FrameImplTestBase() {
  set_test_server_root(base::FilePath(kTestServerRoot));
}

void FrameImplTestBaseWithServer::SetUpOnMainThread() {
  FrameImplTestBase::SetUpOnMainThread();

  ASSERT_TRUE(test_server_handle_ =
                  embedded_test_server()->StartAndReturnHandle());
}

FrameImplTestBaseWithServer::FrameImplTestBaseWithServer() {
  set_test_server_root(base::FilePath(kTestServerRoot));
}
