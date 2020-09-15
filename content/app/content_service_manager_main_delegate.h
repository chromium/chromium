// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
#define CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "content/public/app/content_main.h"
#include "services/service_manager/embedder/main_delegate.h"

namespace content {

class ContentMainRunnerImpl;

class ContentServiceManagerMainDelegate : public service_manager::MainDelegate {
 public:
  explicit ContentServiceManagerMainDelegate(const ContentMainParams& params);
  ~ContentServiceManagerMainDelegate() override;

  // service_manager::MainDelegate:
  int Initialize(const InitializeParams& params) override;
  bool IsEmbedderSubprocess() override;
  int RunEmbedderProcess() override;
  void ShutDownEmbedderProcess() override;
  void InitializeMojo(mojo::core::Configuration* config) override;

  // Sets the flag whether to start the Service Manager without starting the
  // full browser.
  void SetStartServiceManagerOnly(bool start_service_manager_only);

 private:
  ContentMainParams content_main_params_;
  std::unique_ptr<ContentMainRunnerImpl> content_main_runner_;

#if defined(OS_ANDROID)
  bool initialized_ = false;
#endif

  // Indicates whether to start the Service Manager without starting the full
  // browser.
  bool start_service_manager_only_ = false;

  DISALLOW_COPY_AND_ASSIGN(ContentServiceManagerMainDelegate);
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
