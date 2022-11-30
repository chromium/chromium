// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"

#include <memory>

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

namespace content {

BrowserPpapiHostTest::BrowserPpapiHostTest() : sink_() {
  ppapi_host_ = std::make_unique<BrowserPpapiHostImpl>(
      &sink_, ppapi::PpapiPermissions::AllPermissions(), std::string(),
      base::FilePath(), base::FilePath(), false /* in_process */,
      false /* external_plugin */);
  ppapi_host_->set_plugin_process(base::Process::Current());
}

BrowserPpapiHostTest::~BrowserPpapiHostTest() {}

BrowserPpapiHost* BrowserPpapiHostTest::GetBrowserPpapiHost() {
  return ppapi_host_.get();
}

}  // namespace content
