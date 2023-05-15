// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_
#define CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_

#import <Cocoa/Cocoa.h>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

@protocol RenderWidgetHostViewMacDelegate;

namespace remote_cocoa {

// This must return an autoreleased object.
using RenderWidgetHostViewMacDelegateCallback =
    base::OnceCallback<NSObject<RenderWidgetHostViewMacDelegate>*()>;

// Create the NSView for a RenderWidgetHostView or WebContentsView. This is
// called in the app shim process through an interface in remote_cocoa. These
// functions should be moved to remote_cocoa, but currently have dependencies on
// content.
// https://crbug.com/888290
void CONTENT_EXPORT CreateRenderWidgetHostNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle,
    RenderWidgetHostViewMacDelegateCallback
        responder_delegate_creation_callback);

void CONTENT_EXPORT CreateWebContentsNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle);

}  // namespace remote_cocoa

#endif  // CONTENT_PUBLIC_BROWSER_REMOTE_COCOA_H_
