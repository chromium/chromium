// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A thin wrapper around jingle_xmpp::XmppClient that exposes weak pointers
// so that users know when the jingle_xmpp::XmppClient becomes invalid to use
// (not necessarily only at destruction time).

#ifndef JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_
#define JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "third_party/libjingle_xmpp/xmpp/xmppclient.h"

namespace jingle_xmpp {
class TaskParent;
}  // namespace

namespace notifier {

// jingle_xmpp::XmppClient's destructor isn't marked virtual, but it inherits
// from rtc::Task, whose destructor *is* marked virtual, so we
// can safely inherit from it.
class WeakXmppClient : public jingle_xmpp::XmppClient {
 public:
  explicit WeakXmppClient(jingle_xmpp::TaskParent* parent);

  ~WeakXmppClient() override;

  // Returns a weak pointer that is invalidated when the XmppClient
  // becomes invalid to use.
  base::WeakPtr<WeakXmppClient> AsWeakPtr();

  // Invalidates all weak pointers to this object.  (This method is
  // necessary as calling Abort() does not always lead to Stop() being
  // called, so it's not a reliable way to cause an invalidation.)
  void Invalidate();

 protected:
  void Stop() override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // We use our own WeakPtrFactory instead of inheriting from
  // SupportsWeakPtr since we want to invalidate in other places
  // besides the destructor.
  base::WeakPtrFactory<WeakXmppClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WeakXmppClient);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_WEAK_XMPP_CLIENT_H_
