// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_compiler_launcher.h"

#include "base/command_line.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/switches.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom.h"

namespace content {

mojo::Remote<webnn::mojom::WebNNCompilerService> LaunchWebNNCompilerProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ServiceProcessHost::Options options;
  options.WithDisplayName("WebNN Compiler");

  // Only bypass MITIGATION_FORCE_MS_SIGNED_BINS when the browser was launched
  // with --allow-third-party-modules (for testing with non-MS-signed DLLs).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kAllowThirdPartyModules)) {
    options.WithExtraCommandLineSwitches(
        {sandbox::policy::switches::kAllowThirdPartyModules});
  }

  return ServiceProcessHost::Launch<webnn::mojom::WebNNCompilerService>(
      std::move(options));
}

}  // namespace content
