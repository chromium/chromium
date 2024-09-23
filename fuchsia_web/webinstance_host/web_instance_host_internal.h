// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_INTERNAL_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_INTERNAL_H_

#include <stdint.h>
#include <zircon/types.h>

#include <string>
#include <string_view>
#include <vector>

namespace base {
class CommandLine;
}

namespace fuchsia::web {
class CreateContextParams;
enum class ContextFeatureFlags : uint64_t;
}  // namespace fuchsia::web

// Registers product data for the web_instance Component, ensuring it is
// registered regardless of how the Component is launched and without requiring
// all of its clients to provide the required services (until a better solution
// is available - see crbug.com/1275224). This should only be called once per
// process, and the calling thread must have an async_dispatcher.
void RegisterWebInstanceProductData(std::string_view absolute_component_url);

// File names must not contain directory separators, nor match the special
// current- nor parent-directory filenames.
bool IsValidContentDirectoryName(std::string_view file_name);

// Appends switches and values to `launch_args` based on the contents of
// `params`. Members of `params` not supported by the build will be cleared if
// set.
zx_status_t AppendLaunchArgs(fuchsia::web::CreateContextParams& params,
                             base::CommandLine& launch_args);

// Appends the names of dynamically-provisioned services to `services` based
// on the requested properties.
void AppendDynamicServices(fuchsia::web::ContextFeatureFlags features,
                           bool enable_playready,
                           std::vector<std::string>& services);

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_INTERNAL_H_
