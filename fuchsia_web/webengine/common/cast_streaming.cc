// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/common/cast_streaming.h"

#include <string_view>

#include "base/command_line.h"
#include "fuchsia_web/webengine/switches.h"
#include "url/gurl.h"

namespace {

constexpr char kCastStreamingMessagePortOrigin[] = "cast-streaming:receiver";
constexpr char kCastStreamingVideoOnlyMessagePortOrigin[] =
    "cast-streaming:video-only-receiver";

}  // namespace

bool IsCastStreamingEnabled() {
  static bool is_cast_streaming_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCastStreamingReceiver);
  return is_cast_streaming_enabled;
}

bool IsCastStreamingAppOrigin(std::string_view origin) {
  return origin == kCastStreamingMessagePortOrigin ||
         IsCastStreamingVideoOnlyAppOrigin(origin);
}

bool IsCastStreamingVideoOnlyAppOrigin(std::string_view origin) {
  return origin == kCastStreamingVideoOnlyMessagePortOrigin;
}

bool IsValidCastStreamingMessage(const fuchsia::web::WebMessage& message) {
  // |message| should contain exactly one OutgoingTransferrable, with a single
  // MessagePort.
  return message.has_outgoing_transfer() &&
         message.outgoing_transfer().size() == 1u &&
         message.outgoing_transfer()[0].is_message_port();
}
