// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_STREAMING_H_
#define FUCHSIA_RUNNERS_CAST_CAST_STREAMING_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "fuchsia/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"

// TODO(crbug.com/1082821): Remove unused methods here once the
// Cast Streaming Receiver component has been implemented.

// URL for the Cast Streaming application.
extern const char kCastStreamingWebUrl[];

// Name of the Cast Streaming MessagePort.
extern const char kCastStreamingMessagePortName[];

// Returns true if |application_config| is a cast streaming application.
bool IsAppConfigForCastStreaming(
    const chromium::cast::ApplicationConfig& application_config);

// Modifies |params| to apply Cast Streaming-specific Context Params.
void ApplyCastStreamingContextParams(fuchsia::web::CreateContextParams* params);

// Returns the proper origin value for the MessagePort for |app_id|.
std::string GetMessagePortOriginForAppId(const std::string& app_id);

#endif  // FUCHSIA_RUNNERS_CAST_CAST_STREAMING_H_
