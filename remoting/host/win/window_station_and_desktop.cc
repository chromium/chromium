// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/window_station_and_desktop.h"

#include <algorithm>

namespace remoting {

WindowStationAndDesktop::WindowStationAndDesktop()
    : desktop_(nullptr), window_station_(nullptr) {}

WindowStationAndDesktop::~WindowStationAndDesktop() {
  SetDesktop(nullptr);
  SetWindowStation(nullptr);
}

void WindowStationAndDesktop::SetDesktop(HDESK desktop) {
  std::swap(desktop_, desktop);
  if (desktop) {
    CloseDesktop(desktop);
  }
}

void WindowStationAndDesktop::SetWindowStation(HWINSTA window_station) {
  std::swap(window_station_, window_station);
  if (window_station) {
    CloseWindowStation(window_station);
  }
}

void WindowStationAndDesktop::Swap(WindowStationAndDesktop& other) {
  std::swap(desktop_, other.desktop_);
  std::swap(window_station_, other.window_station_);
}

}  // namespace remoting
