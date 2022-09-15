// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STARTUP_DATA_IMPL_H_
#define CONTENT_BROWSER_STARTUP_DATA_IMPL_H_

#include <memory>

#include "content/browser/browser_process_io_thread.h"
#include "content/common/content_export.h"
#include "content/public/common/startup_data.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace content {

// The browser implementation of StartupData.
struct CONTENT_EXPORT StartupDataImpl : public StartupData {
  StartupDataImpl();
  ~StartupDataImpl() override;

  std::unique_ptr<BrowserProcessIOThread> io_thread;
  std::unique_ptr<mojo::core::ScopedIPCSupport> mojo_ipc_support;
};

}  // namespace content

#endif  // CONTENT_BROWSER_STARTUP_DATA_IMPL_H_
