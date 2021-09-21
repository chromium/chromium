// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

#include <utility>

#include "content/public/common/child_process_host.h"
#include "ipc/ipc_message.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"

namespace content {

WebContents::CreateParams::CreateParams(BrowserContext* context,
                                        base::Location creator_location)
    : CreateParams(context, nullptr, creator_location) {}

WebContents::CreateParams::CreateParams(BrowserContext* context,
                                        scoped_refptr<SiteInstance> site,
                                        base::Location creator_location)
    : browser_context(context),
      site_instance(std::move(site)),
      opener_render_process_id(content::ChildProcessHost::kInvalidUniqueID),
      opener_render_frame_id(MSG_ROUTING_NONE),
      opener_suppressed(false),
      opened_by_another_window(false),
      initially_hidden(false),
      guest_delegate(nullptr),
      context(nullptr),
      renderer_initiated_creation(false),
      desired_renderer_state(kOkayToHaveRendererProcess),
      starting_sandbox_flags(network::mojom::WebSandboxFlags::kNone),
      is_never_visible(false),
      creator_location(creator_location) {}

WebContents::CreateParams::CreateParams(const CreateParams& other) = default;

WebContents::CreateParams::~CreateParams() {
}

}  // namespace content
