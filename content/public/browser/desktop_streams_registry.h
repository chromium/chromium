// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DESKTOP_STREAMS_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_DESKTOP_STREAMS_REGISTRY_H_

#include <optional>

#include "content/common/content_export.h"

namespace url {
class Origin;
}

namespace content {

enum DesktopStreamRegistryType {
  kRegistryStreamTypeDesktop,
  kRegistryStreamTypeTab
};

struct DesktopMediaID;

// Interface to DesktopStreamsRegistry which is used to store accepted desktop
// media streams for Desktop/Tab Capture API. Single instance of this class is
// created at first time use. This should be called on UI thread.
class CONTENT_EXPORT DesktopStreamsRegistry {
 public:
  virtual ~DesktopStreamsRegistry() {}

  static DesktopStreamsRegistry* GetInstance();

  // Adds new stream to the registry. Called by the implementation of either
  // desktopCapture API or tabCapture API, differentiated by |type|, after user
  // has approved access to |source| for the |origin|. Returns identifier of
  // the new stream.
  // If |restrict_to_render_frame_id| has a value, access to the stream will
  // be restricted to that render frame. If it has no value, any frame in the
  // same origin and process may use the stream.
  virtual std::string RegisterStream(
      int render_process_id,
      std::optional<int> restrict_to_render_frame_id,
      const url::Origin& origin,
      const DesktopMediaID& source,
      const DesktopStreamRegistryType type) = 0;

  // Validates stream identifier specified in getUserMedia(). Returns null
  // DesktopMediaID if the specified |id| is invalid, i.e. wasn't generated
  // using RegisterStream() or if it was generated for a different
  // RenderFrame/origin/type. Otherwise returns ID of the source and removes it
  // from the registry.
  virtual DesktopMediaID RequestMediaForStreamId(
      const std::string& id,
      int render_process_id,
      int render_frame_id,
      const url::Origin& origin,
      const DesktopStreamRegistryType type) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DESKTOP_STREAMS_REGISTRY_H_
