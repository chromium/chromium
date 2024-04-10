// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_geometry.h"

namespace remoting {

bool DesktopLayout::operator==(const DesktopLayout& rhs) const = default;
DesktopLayout::DesktopLayout(const DesktopLayout& other) = default;
DesktopLayout& DesktopLayout::operator=(const DesktopLayout& other) = default;

DesktopLayoutSet::DesktopLayoutSet() = default;
DesktopLayoutSet::DesktopLayoutSet(const DesktopLayoutSet&) = default;
DesktopLayoutSet::DesktopLayoutSet(const std::vector<DesktopLayout> layouts)
    : layouts(layouts) {}
DesktopLayoutSet& DesktopLayoutSet::operator=(const DesktopLayoutSet&) =
    default;
DesktopLayoutSet::~DesktopLayoutSet() = default;
bool DesktopLayoutSet::operator==(const DesktopLayoutSet& rhs) const = default;

}  // namespace remoting
