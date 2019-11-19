// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_
#define CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace remote_cocoa {

// Create the NSView for a RenderWidgetHostView or WebContentsView. This is
// called in the app shim process through an interface in remote_cocoa. These
// functions should be moved to remote_cocoa, but currently have dependencies on
// content.
// https://crbug.com/888290
void CONTENT_EXPORT CreateRenderWidgetHostNSView(
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle);

void CONTENT_EXPORT CreateWebContentsNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle);

}  // namespace remote_cocoa

#endif  // CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_
