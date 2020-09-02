// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_devtools_bindings.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/common/web_test_switches.h"
#include "net/base/filename_util.h"

namespace {

GURL GetInspectedPageURL(const GURL& test_url) {
  std::string spec = test_url.spec();
  std::string test_query_param = "&test=";
  std::string test_script_url =
      spec.substr(spec.find(test_query_param) + test_query_param.length());
  std::string inspected_page_url = test_script_url.replace(
      test_script_url.find("/devtools/"), std::string::npos,
      "/devtools/resources/inspected-page.html");
  return GURL(inspected_page_url);
}

}  // namespace

namespace content {

class WebTestDevToolsBindings::SecondaryObserver : public WebContentsObserver {
 public:
  explicit SecondaryObserver(WebTestDevToolsBindings* bindings)
      : WebContentsObserver(bindings->inspected_contents()),
        bindings_(bindings) {}

  // WebContentsObserver implementation.
  void DocumentAvailableInMainFrame() override {
    if (bindings_)
      bindings_->NavigateDevToolsFrontend();
    bindings_ = nullptr;
  }

 private:
  WebTestDevToolsBindings* bindings_;
  DISALLOW_COPY_AND_ASSIGN(SecondaryObserver);
};

// static.
GURL WebTestDevToolsBindings::MapTestURLIfNeeded(const GURL& test_url,
                                                 bool* is_devtools_test) {
  std::string test_url_string = test_url.spec();
  *is_devtools_test = test_url_string.find("/devtools/") != std::string::npos;
  if (!*is_devtools_test)
    return test_url;

  base::FilePath dir_exe;
  if (!base::PathService::Get(base::DIR_EXE, &dir_exe)) {
    NOTREACHED();
    return GURL();
  }
#if defined(OS_MAC)
  // On Mac, the executable is in
  // out/Release/Content Shell.app/Contents/MacOS/Content Shell.
  // We need to go up 3 directories to get to out/Release.
  dir_exe = dir_exe.AppendASCII("../../..");
#endif
  base::FilePath dev_tools_path;
  bool is_debug_dev_tools = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDebugDevTools);
  // The test runner hosts DevTools resources at this path.
  std::string url_string = "http://localhost:8000/inspector-sources/";
  if (is_debug_dev_tools)
    url_string += "debug/";
  url_string += "integration_test_runner.html?experiments=true";
  if (is_debug_dev_tools)
    url_string += "&debugFrontend=true";
  url_string += "&test=" + test_url_string;
  return GURL(url_string);
}

void WebTestDevToolsBindings::NavigateDevToolsFrontend() {
  NavigationController::LoadURLParams params(frontend_url_);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents()->GetController().LoadURLWithParams(params);
}

void WebTestDevToolsBindings::Attach() {}

WebTestDevToolsBindings::WebTestDevToolsBindings(
    WebContents* devtools_contents,
    WebContents* inspected_contents,
    const GURL& frontend_url)
    : ShellDevToolsBindings(devtools_contents, inspected_contents, nullptr),
      frontend_url_(frontend_url) {
  secondary_observer_ = std::make_unique<SecondaryObserver>(this);
  NavigationController::LoadURLParams params(GetInspectedPageURL(frontend_url));
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  inspected_contents->GetController().LoadURLWithParams(params);
}

WebTestDevToolsBindings::~WebTestDevToolsBindings() {}

void WebTestDevToolsBindings::DocumentAvailableInMainFrame() {
  ShellDevToolsBindings::Attach();
}

}  // namespace content
