// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/port_ref.h"

#include "mojo/core/ports/port.h"

namespace mojo {
namespace core {
namespace ports {

PortRef::~PortRef() = default;

PortRef::PortRef() = default;

PortRef::PortRef(const PortName& name, scoped_refptr<Port> port)
    : name_(name), port_(std::move(port)) {}

PortRef::PortRef(const PortRef& other) = default;

PortRef::PortRef(PortRef&& other) = default;

PortRef& PortRef::operator=(const PortRef& other) = default;

PortRef& PortRef::operator=(PortRef&& other) = default;

}  // namespace ports
}  // namespace core
}  // namespace mojo
