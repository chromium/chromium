// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_COMMON_CAST_STREAMING_H_
#define FUCHSIA_WEB_WEBENGINE_COMMON_CAST_STREAMING_H_

#include <fuchsia/web/cpp/fidl.h>

#include <string_view>

// Returns true if Cast Streaming is enabled for this process.
bool IsCastStreamingEnabled();

// TODO(crbug.com/40131115): Remove these 2 functions below once the Cast
// Streaming Receiver is implemented as a separate component from WebEngine.

// Returns true if |origin| is the Cast Streaming MessagePort origin.
bool IsCastStreamingAppOrigin(std::string_view origin);

// Returns true if |origin| is the Cast Streaming MessagePort origin for a
// video-only receiver.
bool IsCastStreamingVideoOnlyAppOrigin(std::string_view origin);

// Returns true if |message| contains a valid Cast Streaming Message.
bool IsValidCastStreamingMessage(const fuchsia::web::WebMessage& message);

#endif  // FUCHSIA_WEB_WEBENGINE_COMMON_CAST_STREAMING_H_
