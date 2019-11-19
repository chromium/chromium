// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/services.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/data_decoder/data_decoder_service.h"
#include "services/network/network_service.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"

namespace content {

namespace {

auto RunNetworkService(
    mojo::PendingReceiver<network::mojom::NetworkService> receiver) {
  auto binders = std::make_unique<service_manager::BinderRegistry>();
  GetContentClient()->utility()->RegisterNetworkBinders(binders.get());
  return std::make_unique<network::NetworkService>(
      std::move(binders), std::move(receiver),
      /*delay_initialization_until_set_client=*/true);
}

auto RunDataDecoder(
    mojo::PendingReceiver<data_decoder::mojom::DataDecoderService> receiver) {
  UtilityThread::Get()->EnsureBlinkInitialized();
  return std::make_unique<data_decoder::DataDecoderService>(
      std::move(receiver));
}

auto RunVideoCapture(
    mojo::PendingReceiver<video_capture::mojom::VideoCaptureService> receiver) {
  return std::make_unique<video_capture::VideoCaptureServiceImpl>(
      std::move(receiver), base::ThreadTaskRunnerHandle::Get());
}

mojo::ServiceFactory& GetIOThreadServiceFactory() {
  static base::NoDestructor<mojo::ServiceFactory> factory{
      RunNetworkService,
  };
  return *factory;
}

mojo::ServiceFactory& GetMainThreadServiceFactory() {
  static base::NoDestructor<mojo::ServiceFactory> factory{
      RunDataDecoder,
      RunVideoCapture,
  };
  return *factory;
}

}  // namespace

void HandleServiceRequestOnIOThread(
    mojo::GenericPendingReceiver receiver,
    base::SequencedTaskRunner* main_thread_task_runner) {
  if (GetIOThreadServiceFactory().MaybeRunService(&receiver))
    return;

  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  auto* embedder_factory =
      GetContentClient()->utility()->GetIOThreadServiceFactory();
  if (embedder_factory && embedder_factory->MaybeRunService(&receiver))
    return;

  DCHECK(receiver.is_valid());
  main_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HandleServiceRequestOnMainThread, std::move(receiver)));
}

void HandleServiceRequestOnMainThread(mojo::GenericPendingReceiver receiver) {
  if (GetMainThreadServiceFactory().MaybeRunService(&receiver))
    return;

  // If the request was handled already, we should not reach this point.
  DCHECK(receiver.is_valid());
  auto* embedder_factory =
      GetContentClient()->utility()->GetMainThreadServiceFactory();
  if (embedder_factory && embedder_factory->MaybeRunService(&receiver))
    return;

  DCHECK(receiver.is_valid());
  DLOG(ERROR) << "Unhandled out-of-process service request for "
              << receiver.interface_name().value();
}

}  // namespace content
