// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WINDOW_STATION_AND_DESKTOP_H_
#define REMOTING_HOST_WIN_WINDOW_STATION_AND_DESKTOP_H_

#include <windows.h>

namespace remoting {

// Scoper for a pair of window station and desktop handles. Both handles are
// closed when the object goes out of scope.
class WindowStationAndDesktop {
 public:
  WindowStationAndDesktop();

  WindowStationAndDesktop(const WindowStationAndDesktop&) = delete;
  WindowStationAndDesktop& operator=(const WindowStationAndDesktop&) = delete;

  ~WindowStationAndDesktop();

  HDESK desktop() const { return desktop_; }
  HWINSTA window_station() const { return window_station_; }

  // Sets a new desktop handle closing the owned desktop handle if needed.
  void SetDesktop(HDESK desktop);

  // Sets a new window station handle closing the owned window station handle
  // if needed.
  void SetWindowStation(HWINSTA window_station);

  // Swaps contents with the other WindowStationAndDesktop.
  void Swap(WindowStationAndDesktop& other);

 private:
  HDESK desktop_;
  HWINSTA window_station_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WINDOW_STATION_AND_DESKTOP_H_
