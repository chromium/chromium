// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ISOLATION_CONTEXT_H_
#define CONTENT_BROWSER_ISOLATION_CONTEXT_H_

#include "base/types/id_type.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_or_resource_context.h"
#include "content/public/browser/browsing_instance_id.h"

namespace content {

// This class is used to specify the context in which process model decisions
// need to be made.  For example, dynamically added isolated origins only take
// effect in future BrowsingInstances, and this class can be used to specify
// that a process model decision is being made from a specific
// BrowsingInstance, so that only isolated origins that are applicable to that
// BrowsingInstance are used. This object may be used on UI or IO threads.
class CONTENT_EXPORT IsolationContext {
 public:
  // Normal use cases should create an IsolationContext associated with both a
  // BrowsingInstance and a BrowserContext (profile).  The constructor that
  // takes in a BrowserContext* may only be used on the UI thread; when
  // creating this object on the IO thread, the BrowserOrResourceContext
  // version should be used instead.
  IsolationContext(BrowsingInstanceId browsing_instance_id,
                   BrowserContext* browser_context,
                   bool is_guest,
                   bool is_fenced,
                   OriginAgentClusterIsolationState default_isolation_state);
  IsolationContext(BrowsingInstanceId browsing_instance_id,
                   BrowserOrResourceContext browser_or_resource_context,
                   bool is_guest,
                   bool is_fenced,
                   OriginAgentClusterIsolationState default_isolation_state);

  // Also temporarily allow constructing an IsolationContext not associated
  // with a specific BrowsingInstance.  Callers can use this when they don't
  // know the current BrowsingInstance, or aren't associated with one.
  //
  // TODO(alexmos):  This is primarily used in tests, as well as in call sites
  // which do not yet plumb proper BrowsingInstance information.  Once the
  // remaining non-test call sites are removed or updated, this should become a
  // test-only API.
  explicit IsolationContext(BrowserContext* browser_context);

  ~IsolationContext() = default;

  // Returns the BrowsingInstance ID associated with this isolation context.
  // BrowsingInstance IDs are ordered such that BrowsingInstances with lower
  // IDs were created earlier than BrowsingInstances with higher IDs.
  //
  // If this is not specified (i.e., |browsing_instance_id().is_null()| is
  // true), then this IsolationContext isn't restricted to any particular
  // BrowsingInstance.  Asking for isolated origins from an IsolationContext
  // with a null |browsing_instance_id()| will return the latest available
  // isolated origins.
  BrowsingInstanceId browsing_instance_id() const {
    return browsing_instance_id_;
  }

  // Return the BrowserOrResourceContext associated with this IsolationContext.
  // This represents the profile associated with this IsolationContext, and can
  // be used on both UI and IO threads.
  const BrowserOrResourceContext& browser_or_resource_context() const {
    return browser_or_resource_context_;
  }

  // True when the BrowsingInstance associated with this context is used in a
  // <webview> guest.
  bool is_guest() const { return is_guest_; }

  bool is_fenced() const { return is_fenced_; }

  // Returns the default isolation state used in this BrowsingInstance, which is
  // a snapshot of the default isolation within the BrowserContext at the time
  // when this BrowsingInstance was created. Since the BrowserContext's default
  // isolation state can change dynamically, and since it's important that the
  // default isolation state remain consistent within a BrowsingInstance, it's
  // important that all uses in the BrowsingInstance requiring default isolation
  // reference this value.
  const OriginAgentClusterIsolationState& default_isolation_state() const {
    return default_isolation_state_;
  }

 private:
  // When non-null, associates this context with a particular BrowsingInstance.
  const BrowsingInstanceId browsing_instance_id_;

  const BrowserOrResourceContext browser_or_resource_context_;

  // Specifies whether the BrowsingInstance associated with this context is for
  // a <webview> guest.
  const bool is_guest_;

  // Specifies whether the BrowsingInstance associated with this context is for
  // a <fencedframe>.
  const bool is_fenced_;

  // A snapshot of the default OriginAgentClusterIsolationState at the time this
  // IsolationContext was created.
  const OriginAgentClusterIsolationState default_isolation_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ISOLATION_CONTEXT_H_
