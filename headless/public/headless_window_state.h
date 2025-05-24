// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_WINDOW_STATE_H_
#define HEADLESS_PUBLIC_HEADLESS_WINDOW_STATE_H_

#include <optional>
#include <string>
#include <string_view>

namespace headless {

// Represents headless window state. Since headless does not have window
// representation the state is maintained by HeadlessWebContents.
enum class HeadlessWindowState { kNormal, kMinimized, kMaximized, kFullscreen };

// Return string window state representation for use with DevTools Protocol
// methods.
std::string GetProtocolWindowState(HeadlessWindowState window_state);

// Return enum window state representation given the DevTools Protocol string
// representation.
std::optional<HeadlessWindowState> GetWindowStateFromProtocol(
    std::string_view window_state);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_WINDOW_STATE_H_
