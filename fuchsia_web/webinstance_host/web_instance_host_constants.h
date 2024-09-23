// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_CONSTANTS_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_CONSTANTS_H_

namespace switches {

// Launch web_instance in the web_engine_with_webui package rather than the
// standard web_engine package.
// TODO(crbug.com/40248894): Replace the with_webui component with direct
// routing of the resources from web_engine_shell.
inline constexpr char kWithWebui[] = "with-webui";

}  // namespace switches

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_CONSTANTS_H_
