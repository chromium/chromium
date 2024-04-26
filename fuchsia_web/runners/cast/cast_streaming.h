// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_STREAMING_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_STREAMING_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

// TODO(crbug.com/40131115): Remove unused methods here once the
// Cast Streaming Receiver component has been implemented.

// Name of the Cast Streaming MessagePort.
extern const char kCastStreamingMessagePortName[];

// Returns true if |application_config| is a cast streaming application.
bool IsAppConfigForCastStreaming(
    const chromium::cast::ApplicationConfig& application_config);

// Returns the proper origin value for the MessagePort for |app_id|.
std::string GetMessagePortOriginForAppId(const std::string& app_id);

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_STREAMING_H_
