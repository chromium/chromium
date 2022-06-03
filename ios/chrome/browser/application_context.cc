// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/application_context.h"


namespace {
// Global ApplicationContext instance.
ApplicationContext* g_application_context = nullptr;
}

ApplicationContext* GetApplicationContext() {
  return g_application_context;
}

ApplicationContext::ApplicationContext() {
}

ApplicationContext::~ApplicationContext() {
}

// static
void ApplicationContext::SetApplicationContext(ApplicationContext* context) {
  g_application_context = context;
}
