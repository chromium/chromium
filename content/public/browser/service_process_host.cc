// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_process_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_client.h"

namespace content {

ServiceProcessHost::Options::Options() = default;

ServiceProcessHost::Options::~Options() = default;

ServiceProcessHost::Options::Options(Options&&) = default;

ServiceProcessHost::Options& ServiceProcessHost::Options::WithSandboxType(
    SandboxType type) {
  sandbox_type = type;
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithDisplayName(
    const std::string& name) {
  display_name = base::UTF8ToUTF16(name);
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithDisplayName(
    const base::string16& name) {
  display_name = name;
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithDisplayName(
    int resource_id) {
  display_name = GetContentClient()->GetLocalizedString(resource_id);
  return *this;
}

ServiceProcessHost::Options& ServiceProcessHost::Options::WithChildFlags(
    int flags) {
  child_flags = flags;
  return *this;
}

ServiceProcessHost::Options&
ServiceProcessHost::Options::WithExtraCommandLineSwitches(
    std::vector<std::string> switches) {
  extra_switches = std::move(switches);
  return *this;
}

ServiceProcessHost::Options ServiceProcessHost::Options::Pass() {
  return std::move(*this);
}

}  // namespace content
