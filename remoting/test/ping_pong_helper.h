// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_PING_PONG_HELPER_H_
#define REMOTING_TEST_PING_PONG_HELPER_H_

#include <optional>
#include <string>
#include <string_view>

namespace remoting {

// This file contains a set of helper functions for facilitating a ping-pong
// match between two endpoints. A ping-pong exchange is initiated by either of
// the endpoints when they send a "Ping" message with a count N. The receiver
// will then respond with a "Pong" message, which is then replied to with
// another "Ping" message, and so on, until the count reaches a limit.

// Dispatches a ping-pong message to the appropriate handler. Returns the
// payload for a reply if one is needed.
std::optional<std::string> OnPingPongMessageReceived(std::string_view payload);

// Creates the first "Ping" message to start an exchange.
std::string CreatePingMessage(int count);

// Handles a "Ping" message and returns the payload for a "Pong" message.
std::optional<std::string> HandlePing(std::string_view payload);

// Handles a "Pong" message and returns the payload for a "Ping" message if the
// exchange should continue.
std::optional<std::string> HandlePong(std::string_view payload);

// Returns true if the message is a "Ping" message.
bool IsPingMessage(std::string_view payload);

// Returns true if the message is a "Pong" message.
bool IsPongMessage(std::string_view payload);

}  // namespace remoting

#endif  // REMOTING_TEST_PING_PONG_HELPER_H_
