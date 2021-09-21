// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile_agent.h"

#include <string>

#include "base/lazy_instance.h"

namespace printing {

namespace {

base::LazyInstance<std::string>::Leaky g_user_agent;

}  // namespace

void SetAgent(const std::string& user_agent) {
  g_user_agent.Get() = user_agent;
}

const std::string& GetAgent() {
  return g_user_agent.Get();
}

}  // namespace printing
