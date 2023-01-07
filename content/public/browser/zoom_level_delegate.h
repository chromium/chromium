// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ZOOM_LEVEL_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ZOOM_LEVEL_DELEGATE_H_

namespace content {

class HostZoomMap;

// An interface to allow the client to initialize the HostZoomMap.
class ZoomLevelDelegate {
 public:
  virtual void InitHostZoomMap(HostZoomMap* host_zoom_map) = 0;
  virtual ~ZoomLevelDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ZOOM_LEVEL_DELEGATE_H_
