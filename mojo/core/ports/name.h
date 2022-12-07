// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_NAME_H_
#define MOJO_CORE_PORTS_NAME_H_

#include <stdint.h>

#include <ostream>
#include <tuple>

#include "base/component_export.h"
#include "base/hash/hash.h"

namespace mojo {
namespace core {
namespace ports {

struct COMPONENT_EXPORT(MOJO_CORE_PORTS) Name {
  constexpr Name(uint64_t v1, uint64_t v2) : v1(v1), v2(v2) {}
  uint64_t v1, v2;
};

inline bool operator==(const Name& a, const Name& b) {
  return a.v1 == b.v1 && a.v2 == b.v2;
}

inline bool operator!=(const Name& a, const Name& b) {
  return !(a == b);
}

inline bool operator<(const Name& a, const Name& b) {
  return std::tie(a.v1, a.v2) < std::tie(b.v1, b.v2);
}

COMPONENT_EXPORT(MOJO_CORE_PORTS)
std::ostream& operator<<(std::ostream& stream, const Name& name);

struct COMPONENT_EXPORT(MOJO_CORE_PORTS) PortName : Name {
  constexpr PortName() : Name(0, 0) {}
  constexpr PortName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

extern COMPONENT_EXPORT(MOJO_CORE_PORTS) const PortName kInvalidPortName;

struct COMPONENT_EXPORT(MOJO_CORE_PORTS) NodeName : Name {
  constexpr NodeName() : Name(0, 0) {}
  constexpr NodeName(uint64_t v1, uint64_t v2) : Name(v1, v2) {}
};

extern COMPONENT_EXPORT(MOJO_CORE_PORTS) const NodeName kInvalidNodeName;

}  // namespace ports
}  // namespace core
}  // namespace mojo

namespace std {

template <>
struct COMPONENT_EXPORT(MOJO_CORE_PORTS) hash<mojo::core::ports::PortName> {
  std::size_t operator()(const mojo::core::ports::PortName& name) const {
    return base::HashInts64(name.v1, name.v2);
  }
};

template <>
struct COMPONENT_EXPORT(MOJO_CORE_PORTS) hash<mojo::core::ports::NodeName> {
  std::size_t operator()(const mojo::core::ports::NodeName& name) const {
    return base::HashInts64(name.v1, name.v2);
  }
};

}  // namespace std

#endif  // MOJO_CORE_PORTS_NAME_H_
