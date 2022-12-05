// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
#define CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_

#include "content/common/content_export.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include <vector>

#include "content/public/common/webplugininfo.h"
#endif

namespace base {
class CommandLine;
class FilePath;
}

namespace sandbox {
class SandboxCompiler;
namespace mojom {
enum class Sandbox;
}  // namespace mojom
}  // namespace sandbox

namespace content {

// This populates the sandbox parameters in the client for the given
// |sandbox_type|. Some parameters may be extracted from the |command_line|.
CONTENT_EXPORT void SetupSandboxParameters(
    sandbox::mojom::Sandbox sandbox_type,
    const base::CommandLine& command_line,
#if BUILDFLAG(ENABLE_PPAPI)
    const std::vector<content::WebPluginInfo>& plugins,
#endif
    sandbox::SandboxCompiler* compiler);

// Expands the SandboxType::kNetwork policy to allow reading files from
// the specified |path|, which stores TLS certificates used by the browser
// test web servers.
CONTENT_EXPORT void SetNetworkTestCertsDirectoryForTesting(
    const base::FilePath& path);

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOX_PARAMETERS_MAC_H_
