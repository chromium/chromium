// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/mock_bus.h"

#include "base/location.h"

namespace dbus {

MockBus::MockBus(const Bus::Options& options) : Bus(options) {
}

MockBus::~MockBus() = default;

}  // namespace dbus
