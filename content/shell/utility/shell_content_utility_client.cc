// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/process/process.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/test/test_service.h"
#include "content/public/test/test_service.mojom.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/test/echo/echo_service.h"

#if defined(OS_CHROMEOS)
// TODO(https://crbug.com/784179): Remove nogncheck.
#include "services/ws/test_ws/test_window_service_factory.h"  // nogncheck
#include "services/ws/test_ws/test_ws.mojom.h"                // nogncheck
#include "ui/base/ui_base_features.h"
#endif

namespace content {

namespace {

class TestUtilityServiceImpl : public mojom::TestService {
 public:
  static void Create(mojom::TestServiceRequest request) {
    mojo::MakeStrongBinding(base::WrapUnique(new TestUtilityServiceImpl),
                            std::move(request));
  }

  // mojom::TestService implementation:
  void DoSomething(DoSomethingCallback callback) override {
    std::move(callback).Run();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    base::Process::TerminateCurrentProcessImmediately(0);
  }

  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override {
    IMMEDIATE_CRASH();
  }

  void CreateFolder(CreateFolderCallback callback) override {
    // Note: This is used to check if the sandbox is disabled or not since
    //       creating a folder is forbidden when it is enabled.
    std::move(callback).Run(base::ScopedTempDir().CreateUniqueTempDir());
  }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    NOTREACHED();
  }

  void CreateSharedBuffer(const std::string& message,
                          CreateSharedBufferCallback callback) override {
    mojo::ScopedSharedBufferHandle buffer =
        mojo::SharedBufferHandle::Create(message.size());
    CHECK(buffer.is_valid());

    mojo::ScopedSharedBufferMapping mapping = buffer->Map(message.size());
    CHECK(mapping);
    std::copy(message.begin(), message.end(),
              reinterpret_cast<char*>(mapping.get()));

    std::move(callback).Run(std::move(buffer));
  }

 private:
  explicit TestUtilityServiceImpl() {}

  DISALLOW_COPY_AND_ASSIGN(TestUtilityServiceImpl);
};

std::unique_ptr<service_manager::Service> CreateTestService() {
  return std::unique_ptr<service_manager::Service>(new TestService);
}

}  // namespace

ShellContentUtilityClient::ShellContentUtilityClient(bool is_browsertest) {
  if (is_browsertest &&
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType) == switches::kUtilityProcess) {
    network_service_test_helper_ = std::make_unique<NetworkServiceTestHelper>();
    audio_service_test_helper_ = std::make_unique<AudioServiceTestHelper>();
  }
}

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::UtilityThreadStarted() {
  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface(base::BindRepeating(&TestUtilityServiceImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface<mojom::PowerMonitorTest>(
      base::BindRepeating(
          &PowerMonitorTestImpl::MakeStrongBinding,
          base::Passed(std::make_unique<PowerMonitorTestImpl>())),
      base::ThreadTaskRunnerHandle::Get());
  content::ChildThread::Get()
      ->GetServiceManagerConnection()
      ->AddConnectionFilter(
          std::make_unique<SimpleConnectionFilter>(std::move(registry)));
}

void ShellContentUtilityClient::RegisterServices(StaticServiceMap* services) {
  {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::BindRepeating(&CreateTestService);
    services->insert(std::make_pair(kTestServiceUrl, info));
  }

  {
    service_manager::EmbeddedServiceInfo info;
    info.factory = base::BindRepeating(&echo::CreateEchoService);
    services->insert(std::make_pair(echo::mojom::kServiceName, info));
  }

#if defined(OS_CHROMEOS)
  if (features::IsMultiProcessMash()) {
    service_manager::EmbeddedServiceInfo info;
    info.factory =
        base::BindRepeating(&ws::test::CreateOutOfProcessWindowService);
    services->insert(std::make_pair(test_ws::mojom::kServiceName, info));
  }
#endif
}

void ShellContentUtilityClient::RegisterNetworkBinders(
    service_manager::BinderRegistry* registry) {
  network_service_test_helper_->RegisterNetworkBinders(registry);
}

void ShellContentUtilityClient::RegisterAudioBinders(
    service_manager::BinderRegistry* registry) {
  audio_service_test_helper_->RegisterAudioBinders(registry);
}

}  // namespace content
