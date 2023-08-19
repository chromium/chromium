// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/web_engine_browser_test.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>

#include <vector>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/run_loop.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_context.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_main_parts.h"
#include "fuchsia_web/webengine/browser/web_engine_content_browser_client.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webengine/web_engine_main_delegate.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

WebEngineBrowserTest::WebEngineBrowserTest() = default;

WebEngineBrowserTest::~WebEngineBrowserTest() = default;

void WebEngineBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  SetUpCommandLine(command_line);
  BrowserTestBase::SetUp();
}

void WebEngineBrowserTest::PreRunTestOnMainThread() {
  zx_status_t status = published_services().Connect(context_.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect fuchsia.web.Context";

  net::test_server::RegisterDefaultHandlers(embedded_test_server());
  if (!test_server_root_.empty()) {
    embedded_test_server()->ServeFilesFromSourceDirectory(test_server_root_);
  }
}

void WebEngineBrowserTest::PostRunTestOnMainThread() {
  // Unbind the Context while the message loops are still alive.
  context_.Unbind();

  // Shutting down the context needs to run connection error handlers
  // etc which normally are what causes the main loop to exit. Since in
  // tests we are not running a main loop indefinitely, we want to let those
  // things run, just as they would in production, before shutting down. This
  // makes the main loop run until breaking the connection completes.
  base::RunLoop().RunUntilIdle();
}

sys::ServiceDirectory& WebEngineBrowserTest::published_services() {
  if (!published_services_) {
    fidl::InterfaceRequest<fuchsia::io::Directory> svc_request;
    published_services_ =
        sys::ServiceDirectory::CreateWithRequest(&svc_request);
    base::ComponentContextForProcess()
        ->outgoing()
        ->GetOrCreateDirectory("svc")
        ->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                    fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                svc_request.TakeChannel());
  }
  return *published_services_;
}

void WebEngineBrowserTest::SetHeadlessInCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchNative(switches::kOzonePlatform,
                                   switches::kHeadless);
  command_line->AppendSwitch(switches::kHeadless);
}

ContextImpl* WebEngineBrowserTest::context_impl() const {
  // The ContentMainDelegate and ContentBrowserClient must already exist,
  // since those are created early on, before test setup or execution.
  auto* browser_client =
      WebEngineMainDelegate::GetInstanceForTest()->browser_client();
  CHECK(browser_client);

  auto* main_parts = browser_client->main_parts_for_test();
  CHECK(main_parts) << "context_impl() called too early in browser startup.";

  auto* context = main_parts->context_for_test();
  CHECK(context) << "context_impl() called before Context connected.";

  return context;
}

std::vector<FrameHostImpl*> WebEngineBrowserTest::frame_host_impls() const {
  // The ContentMainDelegate and ContentBrowserClient must already exist,
  // since those are created early on, before test setup or execution.
  auto* browser_client =
      WebEngineMainDelegate::GetInstanceForTest()->browser_client();
  CHECK(browser_client);

  auto* main_parts = browser_client->main_parts_for_test();
  CHECK(main_parts) << "frame_host_impl() called too early in browser startup.";

  return main_parts->frame_hosts_for_test();
}
