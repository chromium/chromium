// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/ping_pong_helper.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "remoting/base/internal_headers.h"

namespace remoting {

namespace {

constexpr char kPing[] = "Ping";
constexpr char kPong[] = "Pong";

}  // namespace

std::optional<std::string> OnPingPongMessageReceived(std::string_view payload) {
  std::vector<std::string> parts = base::SplitString(
      payload, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.empty()) {
    return std::nullopt;
  }
  if (parts[0] == kPing) {
    return HandlePing(payload);
  }
  if (parts[0] == kPong) {
    return HandlePong(payload);
  }
  return std::nullopt;
}

std::string CreatePingMessage(int count) {
  return std::string(kPing) + ":" + base::NumberToString(count);
}

std::optional<std::string> HandlePing(std::string_view payload) {
  std::vector<std::string> parts = base::SplitString(
      payload, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2) {
    LOG(WARNING) << "Invalid Ping message format: " << payload;
    return std::nullopt;
  }

  return std::string(kPong) + ":" + parts[1];
}

std::optional<std::string> HandlePong(std::string_view payload) {
  std::vector<std::string> parts = base::SplitString(
      payload, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2) {
    LOG(WARNING) << "Invalid Pong message format: " << payload;
    return std::nullopt;
  }

  int current_count = 0;
  if (!base::StringToInt(parts[1], &current_count)) {
    LOG(WARNING) << "Invalid number in Pong message: " << parts[1];
    return std::nullopt;
  }

  if (current_count < 10) {
    return std::string(kPing) + ":" + base::NumberToString(current_count + 1);
  }

  return std::nullopt;
}

bool IsPingMessage(std::string_view payload) {
  return base::SplitString(payload, ":", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)[0] == kPing;
}

bool IsPongMessage(std::string_view payload) {
  return base::SplitString(payload, ":", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_ALL)[0] == kPong;
}

}  // namespace remoting
