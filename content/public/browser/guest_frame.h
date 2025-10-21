// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GUEST_FRAME_H_
#define CONTENT_PUBLIC_BROWSER_GUEST_FRAME_H_

#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class RenderFrameHost;
class WebContents;

// This class represents the frame of a guest WebContents that is embedded in
// another WebContents. It handles the connection between the guest and the
// embedder.
// TODO(secure-embed): Find a better name. Consider using the SecureEmbed prefix
// to align with SecureEmbedDelegate's naming pattern.
class CONTENT_EXPORT GuestFrame {
 public:
  // Creates a GuestFrame for the given guest WebContents.
  // This will attach the guest to the embedder's frame.
  static std::unique_ptr<GuestFrame> Create(WebContents* guest_web_contents);

  virtual ~GuestFrame() = default;

  // Called by the embedder to provide the surface ID for the guest content.
  virtual void SetLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) = 0;

  // Gets the FrameSinkId of the guest's view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GUEST_FRAME_H_
