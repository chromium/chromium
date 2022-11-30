// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_

#include "ppapi/host/resource_host.h"

namespace content {

class BrowserPpapiHost;

class PepperBrowserFontSingletonHost : public ppapi::host::ResourceHost {
 public:
  PepperBrowserFontSingletonHost(BrowserPpapiHost* host,
                                 PP_Instance instance,
                                 PP_Resource resource);

  PepperBrowserFontSingletonHost(const PepperBrowserFontSingletonHost&) =
      delete;
  PepperBrowserFontSingletonHost& operator=(
      const PepperBrowserFontSingletonHost&) = delete;

  ~PepperBrowserFontSingletonHost() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_BROWSER_FONT_SINGLETON_HOST_H_
