// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_

#include <memory>

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace ui {
class ContextFactory;
}

namespace content {

// This class provides the interface for creating the support for the
// cross-process image transport, both for creating the shared surface handle
// (destination surface for the GPU process) and the transport client (logic for
// using that surface as a texture). The factory is a process-wide singleton.
class CONTENT_EXPORT ImageTransportFactory {
 public:
  virtual ~ImageTransportFactory() {}

  // Sets the global transport factory.
  static void SetFactory(std::unique_ptr<ImageTransportFactory> factory);

  // Terminates the global transport factory.
  static void Terminate();

  // Gets the factory instance.
  static ImageTransportFactory* GetInstance();

  // Disable GPU compositing. Will do nothing if GPU compositing is already
  // disabled.
  virtual void DisableGpuCompositing() = 0;

  // Gets the image transport factory as a context factory for the compositor.
  virtual ui::ContextFactory* GetContextFactory() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_IMAGE_TRANSPORT_FACTORY_H_
