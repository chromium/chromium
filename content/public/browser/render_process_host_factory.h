// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_FACTORY_H_

namespace content {
class BrowserContext;
class RenderProcessHost;
class SiteInstance;

// Factory object for RenderProcessHosts. Using this factory allows tests to
// swap out a different one to use a MockRenderProcessHost.
class RenderProcessHostFactory {
 public:
  virtual ~RenderProcessHostFactory() {}
  virtual RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_FACTORY_H_
