// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_

namespace content {
class RenderFrameHost;

// Test-only helpers.

// Forces RenderFrameHost to be left in pending deletion, so it would not be
// deleted. This is done to ensure that the tests have a way to reliably get a
// RenderFrameHost which is inactive (see RenderFrameHost::IsActive) and
// test that they handle it correctly.
void LeaveInPendingDeletionState(RenderFrameHost* rfh);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_
