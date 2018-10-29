// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/cdm/renderer/external_clear_key_key_system_properties.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/simple_connection_filter.h"
#include "content/public/test/test_service.mojom.h"
#include "content/shell/common/power_monitor_test_impl.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

namespace {

// A test service which can be driven by browser tests for various reasons.
class TestRendererServiceImpl : public mojom::TestService {
 public:
  explicit TestRendererServiceImpl(mojom::TestServiceRequest request)
      : binding_(this, std::move(request)) {
    binding_.set_connection_error_handler(base::BindOnce(
        &TestRendererServiceImpl::OnConnectionError, base::Unretained(this)));
  }

  ~TestRendererServiceImpl() override {}

 private:
  void OnConnectionError() { delete this; }

  // mojom::TestService:
  void DoSomething(DoSomethingCallback callback) override {
    // Instead of responding normally, unbind the pipe, write some garbage,
    // and go away.
    const std::string kBadMessage = "This is definitely not a valid response!";
    mojo::ScopedMessagePipeHandle pipe = binding_.Unbind().PassMessagePipe();
    MojoResult rv = mojo::WriteMessageRaw(
        pipe.get(), kBadMessage.data(), kBadMessage.size(), nullptr, 0,
        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(rv, MOJO_RESULT_OK);

    // Deletes this.
    OnConnectionError();
  }

  void DoTerminateProcess(DoTerminateProcessCallback callback) override {
    NOTREACHED();
  }

  void DoCrashImmediately(DoCrashImmediatelyCallback callback) override {
    NOTREACHED();
  }

  void CreateFolder(CreateFolderCallback callback) override { NOTREACHED(); }

  void GetRequestorName(GetRequestorNameCallback callback) override {
    std::move(callback).Run("Not implemented.");
  }

  void CreateSharedBuffer(const std::string& message,
                          CreateSharedBufferCallback callback) override {
    NOTREACHED();
  }

  mojo::Binding<mojom::TestService> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestRendererServiceImpl);
};

void CreateRendererTestService(mojom::TestServiceRequest request) {
  // Owns itself.
  new TestRendererServiceImpl(std::move(request));
}

}  // namespace

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_.reset(new web_cache::WebCacheImpl());

  auto registry = std::make_unique<service_manager::BinderRegistry>();
  registry->AddInterface<mojom::TestService>(
      base::Bind(&CreateRendererTestService),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface<mojom::PowerMonitorTest>(
      base::Bind(&PowerMonitorTestImpl::MakeStrongBinding,
                 base::Passed(std::make_unique<PowerMonitorTestImpl>())),
      base::ThreadTaskRunnerHandle::Get());
  content::ChildThread::Get()
      ->GetServiceManagerConnection()
      ->AddConnectionFilter(
          std::make_unique<SimpleConnectionFilter>(std::move(registry)));
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);
}

bool ShellContentRendererClient::HasErrorPage(int http_status_code) {
  return http_status_code >= 400 && http_status_code < 600;
}

void ShellContentRendererClient::PrepareErrorPage(
    RenderFrame* render_frame,
    const blink::WebURLRequest& failed_request,
    const blink::WebURLError& error,
    std::string* error_html,
    base::string16* error_description) {
  if (error_html && error_html->empty()) {
    *error_html =
        "<head><title>Error</title></head><body>Could not load the requested "
        "resource.<br/>Error code: " +
        base::IntToString(error.reason()) +
        (error.reason() < 0 ? " (" + net::ErrorToString(error.reason()) + ")"
                            : "") +
        "</body>";
  }
}

void ShellContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLRequest& failed_request,
    const GURL& unreachable_url,
    int http_status,
    std::string* error_html,
    base::string16* error_description) {
  if (error_html) {
    *error_html =
        "<head><title>Error</title></head><body>Server returned HTTP status " +
        base::IntToString(http_status) + "</body>";
  }
}

bool ShellContentRendererClient::IsPluginAllowedToUseCompositorAPI(
    const GURL& url) {
#if BUILDFLAG(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

bool ShellContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
#if BUILDFLAG(ENABLE_PLUGINS)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePepperTesting);
#else
  return false;
#endif
}

void ShellContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExposeInternalsForTesting)) {
    blink::WebTestingSupport::InjectInternalsObject(context);
  }
}

#if BUILDFLAG(ENABLE_MOJO_CDM)
void ShellContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {
  if (!base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting))
    return;

  static const char kExternalClearKeyKeySystem[] =
      "org.chromium.externalclearkey";
  key_systems->emplace_back(
      new cdm::ExternalClearKeyProperties(kExternalClearKeyKeySystem));
}
#endif

}  // namespace content
