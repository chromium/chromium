// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_GEOMETRY_H_
#define REMOTING_HOST_DESKTOP_GEOMETRY_H_

#include <optional>
#include <vector>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace remoting {
using DesktopScreenId = intptr_t;

const DesktopScreenId kFullDesktopScreenId = -1;

class DesktopResolution {
 public:
  DesktopResolution(gfx::Size dimensions, gfx::Vector2d dpi)
      : dimensions_(dimensions), dpi_(dpi) {}
  const gfx::Size dimensions() const { return dimensions_; }
  const gfx::Vector2d dpi() const { return dpi_; }

 private:
  gfx::Size dimensions_;
  gfx::Vector2d dpi_;
};

class DesktopLayout {
 public:
  DesktopLayout(std::optional<int64_t> screen_id,
                gfx::Rect rect,
                gfx::Vector2d dpi)
      : screen_id_(screen_id), rect_(rect), dpi_(dpi) {}
  DesktopLayout(const DesktopLayout& other);
  DesktopLayout& operator=(const DesktopLayout& other);
  bool operator==(const DesktopLayout& rhs) const;

  std::optional<int64_t> screen_id() const { return screen_id_; }
  void set_screen_id(int64_t id) { screen_id_ = id; }

  const gfx::Rect& rect() const { return rect_; }
  const gfx::Vector2d& dpi() const { return dpi_; }
  // Make this struct API shape like VideoTrackLayout proto in
  // //remoting/proto/control.proto
  int width() const { return rect().width(); }
  int height() const { return rect().height(); }
  int position_x() const { return rect().x(); }
  int position_y() const { return rect().y(); }

 private:
  std::optional<int64_t> screen_id_;
  gfx::Rect rect_;
  gfx::Vector2d dpi_;
};

struct DesktopLayoutSet {
  DesktopLayoutSet();
  DesktopLayoutSet(const DesktopLayoutSet&);
  explicit DesktopLayoutSet(const std::vector<DesktopLayout> layouts);
  DesktopLayoutSet& operator=(const DesktopLayoutSet&);
  ~DesktopLayoutSet();
  bool operator==(const DesktopLayoutSet& rhs) const;

  std::vector<DesktopLayout> layouts;
  std::optional<int64_t> primary_screen_id;
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_GEOMETRY_H_
