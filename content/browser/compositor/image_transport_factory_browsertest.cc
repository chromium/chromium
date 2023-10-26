// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/image_transport_factory.h"

#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/compositor.h"

namespace content {
namespace {

using ImageTransportFactoryBrowserTest = ContentBrowserTest;

class MockContextLostObserver : public viz::ContextLostObserver {
 public:
  MOCK_METHOD0(OnContextLost, void());
};

// TODO(crbug.com/394083, crbug.com/1305007, crbug.com/1302879): Flaky on
// ChromeOS, Linux, and Windows.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
#define MAYBE_TestLostContext DISABLED_TestLostContext
#else
#define MAYBE_TestLostContext TestLostContext
#endif
// Checks that upon context loss the observer is notified.
IN_PROC_BROWSER_TEST_F(ImageTransportFactoryBrowserTest,
                       MAYBE_TestLostContext) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();

  // This test doesn't make sense in software compositing mode.
  if (GpuDataManagerImpl::GetInstance()->IsGpuCompositingDisabled())
    return;

  scoped_refptr<viz::RasterContextProvider> context_provider =
      factory->GetContextFactory()->SharedMainThreadRasterContextProvider();

  MockContextLostObserver observer;
  context_provider->AddObserver(&observer);

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnContextLost())
      .WillOnce(testing::Invoke(&run_loop, &base::RunLoop::Quit));

  gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
  ri->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                          GL_INNOCENT_CONTEXT_RESET_ARB);

  // We have to flush to make sure that the client side gets a chance to notice
  // the context is gone.
  ri->Flush();

  run_loop.Run();

  context_provider->RemoveObserver(&observer);

  // Close the channel to the GPU process. This is needed because the GPU
  // channel is down by the time that the network service is flushed, but
  // flushing the network service tries to bring it back up again and there are
  // pending requests causing a DCHECK to hit.
  BrowserGpuChannelHostFactory::instance()->CloseChannel();
}

}  // anonymous namespace
}  // namespace content
