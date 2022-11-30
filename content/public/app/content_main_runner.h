// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {
struct ContentMainParams;

// This class is responsible for content initialization, running and shutdown.
class CONTENT_EXPORT ContentMainRunner {
 public:
  virtual ~ContentMainRunner() {}

  // Create a new ContentMainRunner object.
  static std::unique_ptr<ContentMainRunner> Create();

  // Initialize all necessary content state.
  virtual int Initialize(ContentMainParams params) = 0;

  // Some platforms (Android) can call Run() multiple times in different modes,
  // use this method to reset the ContentMainParams it will use between runs.
  virtual void ReInitializeParams(ContentMainParams new_params) = 0;

  // Perform the default run logic.
  virtual int Run() = 0;

  // Shut down the content state.
  virtual void Shutdown() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_RUNNER_H_
