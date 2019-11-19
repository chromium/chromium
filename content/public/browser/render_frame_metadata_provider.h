// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_METADATA_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_METADATA_PROVIDER_H_

#include "base/macros.h"
#include "cc/trees/render_frame_metadata.h"
#include "content/common/content_export.h"

namespace content {

// Notifies all Observer of the submission of CompositorFrames which cause a
// change in RenderFrameMetadata.
//
// When ReportAllFrameSubmissionsForTesting(true) is called, this will be
// notified of all frame submissions.
//
// An Observer is provided, so that multiple sources can all observe the
// metadata for a given RenderWidgetHost.
class CONTENT_EXPORT RenderFrameMetadataProvider {
 public:
  // Observer which is notified when RenderFrameMetadata has changed. This is
  // also notified of all frame submissions if
  // RenderFrameMetadataProvider::ReportAllFrameSubmissionsForTesting(true) has
  // been called.
  class Observer {
   public:
    virtual ~Observer() {}

    virtual void OnRenderFrameMetadataChangedBeforeActivation(
        const cc::RenderFrameMetadata& metadata) = 0;
    virtual void OnRenderFrameMetadataChangedAfterActivation() = 0;
    virtual void OnRenderFrameSubmission() = 0;

    // Called to indicate that the viz::LocalSurfaceId within the
    // RenderFrameMetadata has changed. Note that this is called as
    // soon as |metadata| arrives and does not wait for the frame token
    // to pass in Viz.
    virtual void OnLocalSurfaceIdChanged(
        const cc::RenderFrameMetadata& metadata) = 0;
  };

  RenderFrameMetadataProvider() = default;
  virtual ~RenderFrameMetadataProvider() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual const cc::RenderFrameMetadata& LastRenderFrameMetadata() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderFrameMetadataProvider);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_METADATA_PROVIDER_H_
