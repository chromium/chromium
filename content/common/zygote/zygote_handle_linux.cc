// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/zygote/zygote_handle.h"

#include "base/command_line.h"
#include "content/common/zygote/zygote_communication_linux.h"
#include "content/common/zygote/zygote_handle_impl_linux.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

// Intentionally leaked.
ZygoteCommunication* g_generic_zygote = nullptr;
ZygoteCommunication* g_unsandboxed_zygote = nullptr;

}  // namespace

ZygoteCommunication* CreateGenericZygote(ZygoteLaunchCallback launch_cb) {
  CHECK(!g_generic_zygote);
  g_generic_zygote =
      new ZygoteCommunication(ZygoteCommunication::ZygoteType::kSandboxed);
  g_generic_zygote->Init(std::move(launch_cb));
  return g_generic_zygote;
}

ZygoteCommunication* GetGenericZygote() {
  CHECK(g_generic_zygote);
  return g_generic_zygote;
}

ZygoteCommunication* CreateUnsandboxedZygote(ZygoteLaunchCallback launch_cb) {
  CHECK(!g_unsandboxed_zygote);
  g_unsandboxed_zygote =
      new ZygoteCommunication(ZygoteCommunication::ZygoteType::kUnsandboxed);
  g_unsandboxed_zygote->Init(std::move(launch_cb));
  return g_unsandboxed_zygote;
}

ZygoteCommunication* GetUnsandboxedZygote() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoUnsandboxedZygote)) {
    CHECK(!g_unsandboxed_zygote);
    return nullptr;
  }
  CHECK(g_unsandboxed_zygote);
  return g_unsandboxed_zygote;
}

}  // namespace content
