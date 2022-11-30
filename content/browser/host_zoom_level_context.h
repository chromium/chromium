// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HOST_ZOOM_LEVEL_CONTEXT_H_
#define CONTENT_BROWSER_HOST_ZOOM_LEVEL_CONTEXT_H_

#include <memory>

#include "content/browser/host_zoom_map_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/zoom_level_delegate.h"

namespace content {

// This class manages a HostZoomMap and associates it with a ZoomLevelDelegate,
// if one is provided. It also serves to keep the zoom level machinery details
// separate from the owning StoragePartitionImpl. It must be destroyed on the
// UI thread.
class HostZoomLevelContext {
 public:
  explicit HostZoomLevelContext(
      std::unique_ptr<ZoomLevelDelegate> zoom_level_delegate);

  HostZoomLevelContext(const HostZoomLevelContext&) = delete;
  HostZoomLevelContext& operator=(const HostZoomLevelContext&) = delete;

  HostZoomMap* GetHostZoomMap() const { return host_zoom_map_impl_.get(); }
  ZoomLevelDelegate* GetZoomLevelDelegate() const {
    return zoom_level_delegate_.get();
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<HostZoomLevelContext>;

  ~HostZoomLevelContext();

  std::unique_ptr<HostZoomMapImpl> host_zoom_map_impl_;
  // Release the delegate before the HostZoomMap, in case it is carrying
  // any HostZoomMap::Subscription pointers.
  std::unique_ptr<ZoomLevelDelegate> zoom_level_delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HOST_ZOOM_LEVEL_CONTEXT_H_
