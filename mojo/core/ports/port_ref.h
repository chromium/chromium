// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_PORT_REF_H_
#define MOJO_CORE_PORTS_PORT_REF_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/core/ports/name.h"

namespace mojo {
namespace core {
namespace ports {

class Port;
class PortLocker;

class COMPONENT_EXPORT(MOJO_CORE_PORTS) PortRef {
 public:
  ~PortRef();
  PortRef();
  PortRef(const PortName& name, scoped_refptr<Port> port);

  PortRef(const PortRef& other);
  PortRef(PortRef&& other);

  PortRef& operator=(const PortRef& other);
  PortRef& operator=(PortRef&& other);

  const PortName& name() const { return name_; }

  bool is_valid() const { return !!port_; }

 private:
  friend class PortLocker;

  Port* port() const { return port_.get(); }

  PortName name_;
  scoped_refptr<Port> port_;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_PORT_REF_H_
