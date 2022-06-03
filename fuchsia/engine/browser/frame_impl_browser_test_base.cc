// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/frame_impl_browser_test_base.h"

#include "base/files/file_path.h"
#include "fuchsia/engine/test/test_data.h"

// TODO(crbug.com/1155378): Remove this and use default after updating tests in
// frame_impl_browser_test_base.cc to use the appropriate base.
FrameImplTestBase::FrameImplTestBase() {
  set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
}

void FrameImplTestBaseWithServer::SetUpOnMainThread() {
  FrameImplTestBase::SetUpOnMainThread();

  ASSERT_TRUE(test_server_handle_ =
                  embedded_test_server()->StartAndReturnHandle());
}

FrameImplTestBaseWithServer::FrameImplTestBaseWithServer() {
  set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
}
