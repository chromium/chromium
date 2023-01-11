// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_

#include "base/functional/callback.h"
#include "base/threading/thread_checker.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Helper class to get mojo::PendingRemote<media::mojom::InterfaceFactory>.
// Get() lazily connects to the global Media Service instance.
class MediaInterfaceFactoryHolder {
 public:
  using MediaServiceGetter =
      base::RepeatingCallback<media::mojom::MediaService&()>;
  using FrameServicesGetter = base::RepeatingCallback<
      mojo::PendingRemote<media::mojom::FrameInterfaceFactory>()>;

  // |media_service_getter| will be called from the UI thread.
  MediaInterfaceFactoryHolder(MediaServiceGetter media_service_getter,
                              FrameServicesGetter frame_services_getter);

  MediaInterfaceFactoryHolder(const MediaInterfaceFactoryHolder&) = delete;
  MediaInterfaceFactoryHolder& operator=(const MediaInterfaceFactoryHolder&) =
      delete;

  ~MediaInterfaceFactoryHolder();

  // Gets the MediaService |interface_factory_remote_|. The returned pointer is
  // still owned by this class.
  media::mojom::InterfaceFactory* Get();

 private:
  void ConnectToMediaService();

  MediaServiceGetter media_service_getter_;
  FrameServicesGetter frame_services_getter_;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_remote_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace content
#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERFACE_FACTORY_HOLDER_H_
