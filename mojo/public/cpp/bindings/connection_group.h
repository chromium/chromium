// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_

#include <atomic>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"

namespace mojo {

// A ConnectionGroup is used to loosely track groups of related interface
// receivers. Any Receiver or PendingReceiver can reference a single
// ConnectionGroup by holding onto a corresponding Ref.
//
// Belonging to a connection group is a viral property: if a Receiver belongs to
// a connection group, any PendingReceivers arriving in inbound messages
// automatically inherit a Ref to the same group. Likewise if a PendingReceiver
// belongs to a group, any Receiver which consumes and binds that
// PendingReceiver inherits its group membership.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ConnectionGroup
    : public base::RefCountedThreadSafe<ConnectionGroup> {
 public:
  // A single opaque reference to a ConnectionGroup. May be freely moved and
  // copied.
  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) Ref {
   public:
    Ref();
    Ref(const Ref&);
    Ref(Ref&&) noexcept;
    ~Ref();

    Ref& operator=(const Ref&);
    Ref& operator=(Ref&&) noexcept;

    explicit operator bool() const { return group_ != nullptr; }
    void reset();

    // Returns a real reference to the underlying ConnectionGroup. Should only
    // be used in testing.
    scoped_refptr<ConnectionGroup> GetGroupForTesting() { return group_; }

    // Returns a weak copy of this Ref. Does not increase ref-count.
    Ref WeakCopy() const;

    // Indicates whether the underlying ConnectionGroup has zero strong
    // references. Must ONLY be called from the sequence which owns the
    // primordial weak Ref, since that sequence may increase the ref count at
    // any time and otherwise this accessor would be unreliable.
    bool HasZeroRefs() const;

    // Attaches this group to another group, causing the former to retain a
    // reference to the latter throughout its lifetime.
    void SetParentGroup(Ref parent_group);

   private:
    friend class ConnectionGroup;

    enum class Type {
      // A weak Ref does not influence the ref-count of its referenced group.
      // Weak references always produce strong references when copied.
      kWeak,

      // A strong Ref influences the ref-count of its reference group.
      kStrong,
    };

    explicit Ref(scoped_refptr<ConnectionGroup> group);

    Type type_ = Type::kWeak;
    scoped_refptr<ConnectionGroup> group_;
  };

  // Constructs a new ConnectionGroup and returns an initial Ref to it. This
  // initial reference does *not* increase the group's ref-count. All other
  // copies of Ref increase the ref-count. Any time the ref-count is decremented
  // to zero, |callback| is invoked on |task_runner|. If |task_runner| is null
  // (useless except perhaps in tests), |callback| is ignored.
  static Ref Create(base::RepeatingClosure callback,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  ConnectionGroup(const ConnectionGroup&) = delete;
  ConnectionGroup& operator=(const ConnectionGroup&) = delete;

  unsigned int GetNumRefsForTesting() const { return num_refs_; }

 private:
  friend class base::RefCountedThreadSafe<ConnectionGroup>;
  friend class Ref;

  ConnectionGroup(base::RepeatingClosure callback,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~ConnectionGroup();

  void AddGroupRef();
  void ReleaseGroupRef();
  void SetParentGroup(Ref parent_group);

  const base::RepeatingClosure notification_callback_;
  const scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;

  // A reference to this group's parent group, if any.
  Ref parent_group_;

  // We maintain our own ref count because we need to trigger behavior on
  // release, and doing that in conjunction with the RefCountedThreadSafe's own
  // lifetime-controlling ref count is not safely possible.
  std::atomic<unsigned int> num_refs_{0};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_
