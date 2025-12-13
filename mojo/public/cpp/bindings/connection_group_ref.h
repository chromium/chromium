// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_REF_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_REF_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace mojo {

class ConnectionGroup;

// A single opaque reference to a ConnectionGroup. May be freely moved and
// copied.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ConnectionGroupRef {
 public:
  ConnectionGroupRef();
  ConnectionGroupRef(const ConnectionGroupRef&);
  ConnectionGroupRef(ConnectionGroupRef&&) noexcept;
  ~ConnectionGroupRef();

  ConnectionGroupRef& operator=(const ConnectionGroupRef&);
  ConnectionGroupRef& operator=(ConnectionGroupRef&&) noexcept;

  explicit operator bool() const { return group_ != nullptr; }
  void reset();

  // Returns a real reference to the underlying ConnectionGroup. Should only
  // be used in testing.
  scoped_refptr<ConnectionGroup>& GetGroupForTesting() { return group_; }

  // Returns a weak copy of this ConnectionGroupRef. Does not increase
  // ref-count.
  ConnectionGroupRef WeakCopy() const;

  // Indicates whether the underlying ConnectionGroup has zero strong
  // references. Must ONLY be called from the sequence which owns the
  // primordial weak Ref, since that sequence may increase the ref count at
  // any time and otherwise this accessor would be unreliable.
  bool HasZeroRefs() const;

  // Attaches this group to another group, causing the former to retain a
  // reference to the latter throughout its lifetime.
  void SetParentGroup(ConnectionGroupRef parent_group);

 private:
  friend class ConnectionGroup;

  enum class Type {
    // A weak Ref does not influence the ref-count of its referenced group.
    // Weak references always produce strong references when copied.
    kWeak,

    // A strong Ref influences the ref-count of its reference group.
    kStrong,
  };

  explicit ConnectionGroupRef(scoped_refptr<ConnectionGroup> group);

  Type type_ = Type::kWeak;
  scoped_refptr<ConnectionGroup> group_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_REF_H_
