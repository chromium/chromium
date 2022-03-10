// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_
#define CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/types/id_type.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"

namespace perfetto::protos::pbzero {
class SiteInstanceGroup;
}  // namespace perfetto::protos::pbzero

namespace content {

class SiteInstance;
struct ChildProcessTerminationInfo;

using SiteInstanceGroupId = base::IdType32<class SiteInstanceGroupIdTag>;

// A SiteInstanceGroup represents one view of a browsing context group's frame
// trees within a renderer process. It provides a tuning knob, allowing the
// number of groups to vary (for process allocation and
// painting/input/scheduling decisions) without affecting the number of security
// principals that are tracked with SiteInstances.
//
// Similar to layers composing an image from many colors, a set of
// SiteInstanceGroups compose a web page from many renderer processes. Each
// group represents one renderer process' view of a browsing context group,
// containing both local frames (organized into widgets of contiguous frames)
// and proxies for frames in other groups or processes.
//
// The documents in the local frames of a group are organized into
// SiteInstances, representing an atomic group of similar origin documents that
// can access each other directly. A group contains all the documents of one or
// more SiteInstances, all belonging to the same browsing context group (aka
// BrowsingInstance). Each browsing context group has its own set of
// SiteInstanceGroups.
//
// A SiteInstanceGroup is used for generating painted surfaces, directing input
// events, and facilitating communication between frames in different groups.
// The browser process coordinates activities across groups to produce a full
// web page.
//
// SiteInstanceGroups are refcounted by the SiteInstances using them, allowing
// for flexible policies.  Currently, each SiteInstanceGroup has exactly one
// SiteInstance.  See crbug.com/1195535.
class CONTENT_EXPORT SiteInstanceGroup
    : public base::RefCounted<SiteInstanceGroup>,
      public RenderProcessHostObserver {
 public:
  class CONTENT_EXPORT Observer {
   public:
    // Called when this SiteInstanceGroup transitions to having no active
    // frames, as measured by active_frame_count().
    virtual void ActiveFrameCountIsZero(SiteInstanceGroup* site_instance) {}

    // Called when the renderer process of this SiteInstanceGroup has exited.
    // Note that GetProcess() still returns the same RenderProcessHost instance.
    // You can reinitialize it by a call to SiteInstance::GetProcess()->Init().
    virtual void RenderProcessGone(SiteInstanceGroup* site_instance,
                                   const ChildProcessTerminationInfo& info) {}

    // Called when the RenderProcessHost for this SiteInstanceGroup has been
    // destructed. After this, the underlying `process_` is cleared, and calling
    // SiteInstance::GetProcess() would assign a different RenderProcessHost to
    // this SiteInstanceGroup.
    virtual void RenderProcessHostDestroyed() {}
  };

  SiteInstanceGroup(BrowsingInstanceId browsing_instance_id,
                    RenderProcessHost* process);

  SiteInstanceGroup(const SiteInstanceGroup&) = delete;
  SiteInstanceGroup& operator=(const SiteInstanceGroup&) = delete;

  SiteInstanceGroupId GetId() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Increase the number of active frames in this SiteInstanceGroup. This is
  // increased when a frame is created.
  void IncrementActiveFrameCount();

  // Decrease the number of active frames in this SiteInstanceGroup. This is
  // decreased when a frame is destroyed. Decrementing this to zero will notify
  // observers, and may trigger deletion of proxies.
  void DecrementActiveFrameCount();

  // Get the number of active frames which belong to this SiteInstanceGroup. If
  // there are no active frames left, all frames in this SiteInstanceGroup can
  // be safely discarded.
  size_t active_frame_count() const { return active_frame_count_; }

  // `process_` and `agent_scheduling_group_` have to be set together. See
  // `process_` for more details.
  // TODO(crbug.com/1294045): Remove once `this` has the same lifetime as
  // `process`.
  void SetProcessAndAgentSchedulingGroup(RenderProcessHost* process);

  RenderProcessHost* process() const { return process_; }
  bool has_process() const { return process_ != nullptr; }

  BrowsingInstanceId browsing_instance_id() const {
    return browsing_instance_id_;
  }

  AgentSchedulingGroupHost& agent_scheduling_group() {
    DCHECK(agent_scheduling_group_);
    DCHECK_EQ(agent_scheduling_group_->GetProcess(), process_);
    return *agent_scheduling_group_;
  }
  bool has_agent_scheduling_group() {
    return agent_scheduling_group_ != nullptr;
  }

  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::SiteInstanceGroup> proto)
      const;

 private:
  friend class RefCounted<SiteInstanceGroup>;
  ~SiteInstanceGroup() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // A unique ID for this SiteInstanceGroup.
  SiteInstanceGroupId id_;

  // ID of the BrowsingInstance this SiteInstanceGroup belongs to.
  const BrowsingInstanceId browsing_instance_id_;

  // The number of active frames in this SiteInstanceGroup.
  size_t active_frame_count_ = 0;

  // Current RenderProcessHost that is rendering pages for this
  // SiteInstanceGroup, and AgentSchedulingGroupHost (within the process) this
  // SiteInstanceGroup belongs to. Since AgentSchedulingGroupHost is associated
  // with a specific RenderProcessHost, these *must be* changed together to
  // avoid UAF!
  // The |process_| pointer (and hence the |agent_scheduling_group_| pointer as
  // well) will only change once the RenderProcessHost is destructed. They will
  // still remain the same even if the process crashes, since in that scenario
  // the RenderProcessHost remains the same.
  raw_ptr<RenderProcessHost> process_ = nullptr;
  raw_ptr<AgentSchedulingGroupHost> agent_scheduling_group_ = nullptr;

  base::ObserverList<Observer, true>::Unchecked observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_
