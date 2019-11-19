// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_STREAMS_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_STREAMS_REGISTRY_IMPL_H_

#include <map>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/desktop_streams_registry.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT DesktopStreamsRegistryImpl
    : public DesktopStreamsRegistry {
 public:
  DesktopStreamsRegistryImpl();
  ~DesktopStreamsRegistryImpl() override;

  // Returns the DesktopStreamRegistryImpl singleton.
  static DesktopStreamsRegistryImpl* GetInstance();

  std::string RegisterStream(int render_process_id,
                             int render_frame_id,
                             const url::Origin& origin,
                             const DesktopMediaID& source,
                             const std::string& extension_name,
                             const DesktopStreamRegistryType type) override;

  DesktopMediaID RequestMediaForStreamId(
      const std::string& id,
      int render_process_id,
      int render_frame_id,
      const url::Origin& origin,
      std::string* extension_name,
      const DesktopStreamRegistryType type) override;

 private:
  // Type used to store list of accepted desktop media streams.
  struct ApprovedDesktopMediaStream {
    ApprovedDesktopMediaStream();

    int render_process_id;
    int render_frame_id;
    url::Origin origin;
    DesktopMediaID source;
    std::string extension_name;
    DesktopStreamRegistryType type;
  };
  typedef std::map<std::string, ApprovedDesktopMediaStream> StreamsMap;

  // Helper function that removes an expired stream from the registry.
  void CleanupStream(const std::string& id);

  StreamsMap approved_streams_;

  DISALLOW_COPY_AND_ASSIGN(DesktopStreamsRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_STREAMS_REGISTRY_IMPL_H_
