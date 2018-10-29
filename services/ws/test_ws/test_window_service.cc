// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_ws/test_window_service.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/host_event_dispatcher.h"
#include "services/ws/host_event_queue.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/test_host_event_dispatcher.h"
#include "services/ws/test_ws/test_gpu_interface_provider.h"
#include "services/ws/window_service.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace ws {
namespace test {

TestWindowService::TestWindowService() = default;

TestWindowService::~TestWindowService() {
  Shutdown(base::NullCallback());
}

void TestWindowService::InitForInProcess(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private,
    std::unique_ptr<GpuInterfaceProvider> gpu_interface_provider) {
  is_in_process_ = true;
  aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>(
      aura::Env::CreateLocalInstanceForInProcess());
  SetupAuraTestHelper(context_factory, context_factory_private);

  gpu_interface_provider_ = std::move(gpu_interface_provider);
}

void TestWindowService::InitForOutOfProcess() {
#if defined(OS_CHROMEOS)
  // Use gpu service only for ChromeOS to run content_browsertests in mash.
  //
  // To use this code path for all platforms, we need to fix the following
  // flaky failure on Win7 bot:
  //   gl_surface_egl.cc:
  //     EGL Driver message (Critical) eglInitialize: No available renderers
  //   gl_initializer_win.cc:
  //     GLSurfaceEGL::InitializeOneOff failed.
  CreateGpuHost();
#else
  gl::GLSurfaceTestSupport::InitializeOneOff();
  CreateAuraTestHelper();
#endif  // defined(OS_CHROMEOS)
}

std::unique_ptr<aura::Window> TestWindowService::NewTopLevel(
    aura::PropertyConverter* property_converter,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::unique_ptr<aura::Window> top_level = std::make_unique<aura::Window>(
      nullptr, aura::client::WINDOW_TYPE_UNKNOWN, aura_test_helper_->GetEnv());
  aura::SetWindowType(top_level.get(), aura::GetWindowTypeFromProperties(
                                           mojo::FlatMapToMap(properties)));
  top_level->Init(ui::LAYER_NOT_DRAWN);
  aura_test_helper_->root_window()->AddChild(top_level.get());
  for (auto property : properties) {
    property_converter->SetPropertyFromTransportValue(
        top_level.get(), property.first, &property.second);
  }
  return top_level;
}

void TestWindowService::RunDragLoop(aura::Window* window,
                                    const ui::OSExchangeData& data,
                                    const gfx::Point& screen_location,
                                    uint32_t drag_operation,
                                    ui::DragDropTypes::DragEventSource source,
                                    DragDropCompletedCallback callback) {
  std::move(callback).Run(drag_drop_client_.StartDragAndDrop(
      data, window->GetRootWindow(), window, screen_location, drag_operation,
      source));
}

void TestWindowService::CancelDragLoop(aura::Window* window) {
  drag_drop_client_.DragCancel();
}

aura::WindowTreeHost* TestWindowService::GetWindowTreeHostForDisplayId(
    int64_t display_id) {
  return aura_test_helper_->host();
}

void TestWindowService::OnStart() {
  CHECK(!started_);
  started_ = true;

  registry_.AddInterface(base::BindRepeating(
      &TestWindowService::BindServiceFactory, base::Unretained(this)));
  registry_.AddInterface(base::BindRepeating(&TestWindowService::BindTestWs,
                                             base::Unretained(this)));

  if (!is_in_process_) {
    DCHECK(!aura_test_helper_);
    InitForOutOfProcess();
  }
}

void TestWindowService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void TestWindowService::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  DCHECK_EQ(name, mojom::kServiceName);

  // Defer CreateService if |aura_test_helper_| is not created.
  if (!aura_test_helper_) {
    DCHECK(!pending_create_service_);

    pending_create_service_ = base::BindOnce(
        &TestWindowService::CreateService, base::Unretained(this),
        std::move(request), name, std::move(pid_receiver));
    return;
  }

  DCHECK(!ui_service_created_);
  ui_service_created_ = true;

  auto window_service = std::make_unique<WindowService>(
      this, std::move(gpu_interface_provider_),
      aura_test_helper_->focus_client(), /*decrement_client_ids=*/false,
      aura_test_helper_->GetEnv());
  test_host_event_dispatcher_ =
      std::make_unique<TestHostEventDispatcher>(aura_test_helper_->host());
  host_event_queue_ = window_service->RegisterHostEventDispatcher(
      aura_test_helper_->host(), test_host_event_dispatcher_.get());
  service_context_ = std::make_unique<service_manager::ServiceContext>(
      std::move(window_service), std::move(request));
  pid_receiver->SetPID(base::GetCurrentProcId());
}

void TestWindowService::OnGpuServiceInitialized() {
  CreateAuraTestHelper();

  if (pending_create_service_)
    std::move(pending_create_service_).Run();
}

void TestWindowService::Shutdown(
    test_ws::mojom::TestWs::ShutdownCallback callback) {
  // WindowService depends upon Screen, which is owned by AuraTestHelper.
  service_context_.reset();

  // |aura_test_helper_| could be null when exiting before fully initialized.
  if (aura_test_helper_) {
    aura::client::SetScreenPositionClient(aura_test_helper_->root_window(),
                                          nullptr);
    // AuraTestHelper expects TearDown() to be called.
    aura_test_helper_->TearDown();
    aura_test_helper_.reset();
  }

  ui::TerminateContextFactoryForTests();

  if (callback)
    std::move(callback).Run();
}

void TestWindowService::BindServiceFactory(
    service_manager::mojom::ServiceFactoryRequest request) {
  service_factory_bindings_.AddBinding(this, std::move(request));
}

void TestWindowService::BindTestWs(test_ws::mojom::TestWsRequest request) {
  test_ws_bindings_.AddBinding(this, std::move(request));
}

void TestWindowService::CreateGpuHost() {
  discardable_shared_memory_manager_ =
      std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();

  gpu_host_ = std::make_unique<gpu_host::GpuHost>(
      this, context()->connector(), discardable_shared_memory_manager_.get());

  gpu_interface_provider_ = std::make_unique<TestGpuInterfaceProvider>(
      gpu_host_.get(), discardable_shared_memory_manager_.get());

  // |aura_test_helper_| is created later in OnGpuServiceInitialized.
}

void TestWindowService::CreateAuraTestHelper() {
  DCHECK(!aura_test_helper_);

  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  ui::InitializeContextFactoryForTests(false /* enable_pixel_output */,
                                       &context_factory,
                                       &context_factory_private);
  aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
  SetupAuraTestHelper(context_factory, context_factory_private);
}

void TestWindowService::SetupAuraTestHelper(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  aura_test_helper_->SetUp(context_factory, context_factory_private);

  aura::client::SetScreenPositionClient(aura_test_helper_->root_window(),
                                        &screen_position_client_);
}

}  // namespace test
}  // namespace ws
