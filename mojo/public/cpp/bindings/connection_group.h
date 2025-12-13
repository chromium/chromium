// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_

#include <atomic>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/connection_group_ref.h"

namespace base {
class SequencedTaskRunner;
}

namespace mojo {

// A ConnectionGroup is used to loosely track groups of related interface
// receivers. Any Receiver or PendingReceiver can reference a single
// ConnectionGroup by holding onto a corresponding ConnectionGroupRef.
//
// Belonging to a connection group is a viral property: if a Receiver belongs to
// a connection group, any PendingReceivers arriving in inbound messages
// automatically inherit a ConnectionGroupRef to the same group. Likewise if a
// PendingReceiver belongs to a group, any Receiver which consumes and binds
// that PendingReceiver inherits its group membership.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ConnectionGroup
    : public base::RefCountedThreadSafe<ConnectionGroup> {
 public:
  // Constructs a new ConnectionGroup and returns an initial ConnectionGroupRef
  // to it. This initial reference does *not* increase the group's ref-count.
  // All other copies of ConnectionGroupRef increase the ref-count. Any time the
  // ref-count is decremented to zero, |callback| is invoked on |task_runner|.
  // If |task_runner| is null (useless except perhaps in tests), |callback| is
  // ignored.
  static ConnectionGroupRef Create(
      base::RepeatingClosure callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ConnectionGroup(const ConnectionGroup&) = delete;
  ConnectionGroup& operator=(const ConnectionGroup&) = delete;

  unsigned int GetNumRefsForTesting() const { return num_refs_; }

 private:
  friend class base::RefCountedThreadSafe<ConnectionGroup>;
  friend class ConnectionGroupRef;

  ConnectionGroup(base::RepeatingClosure callback,
                  scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual ~ConnectionGroup();

  void AddGroupRef();
  void ReleaseGroupRef();
  void SetParentGroup(ConnectionGroupRef parent_group);

  const base::RepeatingClosure notification_callback_;
  const scoped_refptr<base::SequencedTaskRunner> notification_task_runner_;

  // A reference to this group's parent group, if any.
  ConnectionGroupRef parent_group_;

  // We maintain our own ref count because we need to trigger behavior on
  // release, and doing that in conjunction with the RefCountedThreadSafe's own
  // lifetime-controlling ref count is not safely possible.
  std::atomic<unsigned int> num_refs_{0};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CONNECTION_GROUP_H_
