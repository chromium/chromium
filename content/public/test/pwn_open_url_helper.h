// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_PWN_OPEN_URL_HELPER_H_
#define CONTENT_PUBLIC_TEST_PWN_OPEN_URL_HELPER_H_

#include "ui/base/window_open_disposition.h"

class GURL;

namespace content {

class RenderFrameHost;

// Simulates a (potentially compromised) renderer sending a
// content::mojom::FrameHost::OpenURL IPC for |render_frame_host|. The
// initiator_origin is set to the frame's last-committed origin so that
// VerifyOpenURLParams() accepts the message, and the caller picks the
// disposition and whether the renderer claims a user gesture. This is the
// same primitive a real compromised renderer has via GetFrameHost()->OpenURL().
void PwnOpenURLWithDisposition(RenderFrameHost* render_frame_host,
                               const GURL& url,
                               WindowOpenDisposition disposition,
                               bool user_gesture);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_PWN_OPEN_URL_HELPER_H_
