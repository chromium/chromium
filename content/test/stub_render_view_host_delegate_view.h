// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STUB_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
#define CONTENT_TEST_STUB_RENDER_VIEW_HOST_DELEGATE_VIEW_H_

#include "content/browser/renderer_host/render_view_host_delegate_view.h"

namespace content {

class StubRenderViewHostDelegateView : public RenderViewHostDelegateView {
  // The class already implements every method as a stub, but can't be created
  // and destroyed without a subclass.
};

}  // namespace content

#endif  // CONTENT_TEST_STUB_RENDER_VIEW_HOST_DELEGATE_VIEW_H_
