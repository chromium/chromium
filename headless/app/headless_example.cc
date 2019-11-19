// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A small example application showing the use of the C++ Headless Chrome
// library. It navigates to a web site given on the command line, waits for it
// to load and prints out the DOM.
//
// Tip: start reading from the main() function below.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "ui/gfx/geometry/size.h"

#if defined(OS_WIN)
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

// This class contains the main application logic, i.e., waiting for a page to
// load and printing its DOM. Note that browser initialization happens outside
// this class.
class HeadlessExample : public headless::HeadlessWebContents::Observer,
                        public headless::page::Observer {
 public:
  HeadlessExample(headless::HeadlessBrowser* browser,
                  headless::HeadlessWebContents* web_contents);
  ~HeadlessExample() override;

  // headless::HeadlessWebContents::Observer implementation:
  void DevToolsTargetReady() override;

  // headless::page::Observer implementation:
  void OnLoadEventFired(
      const headless::page::LoadEventFiredParams& params) override;

  // Tip: Observe headless::inspector::ExperimentalObserver::OnTargetCrashed to
  // be notified of renderer crashes.

  void OnDomFetched(std::unique_ptr<headless::runtime::EvaluateResult> result);

 private:
  // The headless browser instance. Owned by the headless library. See main().
  headless::HeadlessBrowser* browser_;
  // Our tab. Owned by |browser_|.
  headless::HeadlessWebContents* web_contents_;
  // The DevTools client used to control the tab.
  std::unique_ptr<headless::HeadlessDevToolsClient> devtools_client_;
  // A helper for creating weak pointers to this class.
  base::WeakPtrFactory<HeadlessExample> weak_factory_{this};
};

namespace {
HeadlessExample* g_example;
}

HeadlessExample::HeadlessExample(headless::HeadlessBrowser* browser,
                                 headless::HeadlessWebContents* web_contents)
    : browser_(browser),
      web_contents_(web_contents),
      devtools_client_(headless::HeadlessDevToolsClient::Create()) {
  web_contents_->AddObserver(this);
}

HeadlessExample::~HeadlessExample() {
  // Note that we shut down the browser last, because it owns objects such as
  // the web contents which can no longer be accessed after the browser is gone.
  devtools_client_->GetPage()->RemoveObserver(this);
  web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  web_contents_->RemoveObserver(this);
  browser_->Shutdown();
}

// This method is called when the tab is ready for DevTools inspection.
void HeadlessExample::DevToolsTargetReady() {
  // Attach our DevTools client to the tab so that we can send commands to it
  // and observe events.
  web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());

  // Start observing events from DevTools's page domain. This lets us get
  // notified when the page has finished loading. Note that it is possible
  // the page has already finished loading by now. See
  // HeadlessShell::DevToolTargetReady for how to handle that case correctly.
  devtools_client_->GetPage()->AddObserver(this);
  devtools_client_->GetPage()->Enable();
}

void HeadlessExample::OnLoadEventFired(
    const headless::page::LoadEventFiredParams& params) {
  // The page has now finished loading. Let's grab a snapshot of the DOM by
  // evaluating the innerHTML property on the document element.
  devtools_client_->GetRuntime()->Evaluate(
      "(document.doctype ? new "
      "XMLSerializer().serializeToString(document.doctype) + '\\n' : '') + "
      "document.documentElement.outerHTML",
      base::BindOnce(&HeadlessExample::OnDomFetched,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessExample::OnDomFetched(
    std::unique_ptr<headless::runtime::EvaluateResult> result) {
  // Make sure the evaluation succeeded before reading the result.
  if (result->HasExceptionDetails()) {
    LOG(ERROR) << "Failed to serialize document: "
               << result->GetExceptionDetails()->GetText();
  } else {
    printf("%s\n", result->GetResult()->GetValue()->GetString().c_str());
  }

  // Shut down the browser (see ~HeadlessExample).
  delete g_example;
  g_example = nullptr;
}

// This function is called by the headless library after the browser has been
// initialized. It runs on the UI thread.
void OnHeadlessBrowserStarted(headless::HeadlessBrowser* browser) {
  // In order to open tabs, we first need a browser context. It corresponds to a
  // user profile and contains things like the user's cookies, local storage,
  // cache, etc.
  headless::HeadlessBrowserContext::Builder context_builder =
      browser->CreateBrowserContextBuilder();

  // Here we can set options for the browser context. As an example we enable
  // incognito mode, which makes sure profile data is not written to disk.
  context_builder.SetIncognitoMode(true);

  // Construct the context and set it as the default. The default browser
  // context is used by the Target.createTarget() DevTools command when no other
  // context is given.
  headless::HeadlessBrowserContext* browser_context = context_builder.Build();
  browser->SetDefaultBrowserContext(browser_context);

  // Get the URL from the command line.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty()) {
    LOG(ERROR) << "No URL to load";
    browser->Shutdown();
    return;
  }
  GURL url(args[0]);

  // Open a tab (i.e., HeadlessWebContents) in the newly created browser
  // context.
  headless::HeadlessWebContents::Builder tab_builder(
      browser_context->CreateWebContentsBuilder());

  // We can set options for the opened tab here. In this example we are just
  // setting the initial URL to navigate to.
  tab_builder.SetInitialURL(url);

  // Create an instance of the example app, which will wait for the page to load
  // and print its DOM.
  headless::HeadlessWebContents* web_contents = tab_builder.Build();
  g_example = new HeadlessExample(browser, web_contents);
}

int main(int argc, const char** argv) {
#if !defined(OS_WIN)
  // This function must be the first thing we call to make sure child processes
  // such as the renderer are started properly. The headless library starts
  // child processes by forking and exec'ing the main application.
  headless::RunChildProcessIfNeeded(argc, argv);
#endif

  // Create a headless browser instance. There can be one of these per process
  // and it can only be initialized once.
  headless::HeadlessBrowser::Options::Builder builder(argc, argv);

#if defined(OS_WIN)
  // In windows, you must initialize and set the sandbox, or pass it along
  // if it has already been initialized.
  sandbox::SandboxInterfaceInfo sandbox_info = {0};
  content::InitializeSandboxInfo(&sandbox_info);
  builder.SetSandboxInfo(&sandbox_info);
#endif
  // Here you can customize browser options. As an example we set the window
  // size.
  builder.SetWindowSize(gfx::Size(800, 600));

  // Pass control to the headless library. It will bring up the browser and
  // invoke the given callback on the browser UI thread. Note: if you need to
  // pass more parameters to the callback, you can add them to the Bind() call
  // below.
  return headless::HeadlessBrowserMain(
      builder.Build(), base::BindOnce(&OnHeadlessBrowserStarted));
}
