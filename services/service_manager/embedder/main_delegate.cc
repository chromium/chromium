// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/main_delegate.h"

namespace service_manager {

MainDelegate::MainDelegate() = default;

MainDelegate::~MainDelegate() = default;

bool MainDelegate::IsEmbedderSubprocess() {
  return false;
}

int MainDelegate::RunEmbedderProcess() {
  return 0;
}

void MainDelegate::ShutDownEmbedderProcess() {}

void MainDelegate::InitializeMojo(mojo::core::Configuration* config) {}

}  // namespace service_manager
