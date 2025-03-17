// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_window_state.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "headless/lib/browser/protocol/target_handler.h"

namespace headless {

std::string GetProtocolWindowState(HeadlessWindowState window_state) {
  switch (window_state) {
    case HeadlessWindowState::kNormal:
      return protocol::Target::WindowStateEnum::Normal;
    case HeadlessWindowState::kMinimized:
      return protocol::Target::WindowStateEnum::Minimized;
    case HeadlessWindowState::kMaximized:
      return protocol::Target::WindowStateEnum::Maximized;
    case HeadlessWindowState::kFullscreen:
      return protocol::Target::WindowStateEnum::Fullscreen;
  }
}

std::optional<HeadlessWindowState> GetWindowStateFromProtocol(
    std::string_view window_state) {
  static const base::NoDestructor<
      base::flat_map<std::string_view, HeadlessWindowState>>
      kWindowStateMap({
          {protocol::Target::WindowStateEnum::Normal,
           HeadlessWindowState::kNormal},
          {protocol::Target::WindowStateEnum::Minimized,
           HeadlessWindowState::kMinimized},
          {protocol::Target::WindowStateEnum::Maximized,
           HeadlessWindowState::kMaximized},
          {protocol::Target::WindowStateEnum::Fullscreen,
           HeadlessWindowState::kFullscreen},
      });

  const auto it = kWindowStateMap->find(window_state);
  if (it != kWindowStateMap->cend()) {
    return it->second;
  }

  return std::nullopt;
}

}  // namespace headless
