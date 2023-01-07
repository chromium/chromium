// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_PRIORITY_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_PRIORITY_CLIENT_H_

namespace content {

#if BUILDFLAG(IS_ANDROID)
enum class ChildProcessImportance;
#endif

// Interface for a client that contributes Priority to a RenderProcessHost.
// Clients can call RenderProcessHost::UpdateClientPriority() when their
// Priority changes.
class RenderProcessHostPriorityClient {
 public:
  // Priority (or on Android, the importance) that a client contributes to a
  // RenderProcessHost. E.g. a RenderProcessHost with a visible client has
  // higher priority / importance than a RenderProcessHost with hidden clients
  // only.
  struct Priority {
    bool is_hidden;
    unsigned int frame_depth;
    bool intersects_viewport;
#if BUILDFLAG(IS_ANDROID)
    ChildProcessImportance importance;
#endif
  };

  virtual Priority GetPriority() = 0;

 protected:
  virtual ~RenderProcessHostPriorityClient() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_PROCESS_HOST_PRIORITY_CLIENT_H_
