// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_MAIN_H_
#define FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_MAIN_H_

#include "fuchsia_web/webengine/web_engine_export.h"

// Main function for the process that implements web::ContextProvider interface.
// Called by WebRunnerMainDelegate when the process is started without --type
// argument.
WEB_ENGINE_EXPORT int ContextProviderMain();

#endif  // FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_MAIN_H_
