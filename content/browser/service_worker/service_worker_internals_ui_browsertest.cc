// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {
namespace {
const char kServiceWorkerInternalsUrl[] = "chrome://serviceworker-internals";
const char kServiceWorkerSetupPage[] = "/service_worker/empty.html";
const char kServiceWorkerUrl[] = "/service_worker/fetch_event.js";
const char kServiceWorkerScope[] = "/service_worker/";
const std::u16string kCompleteTitle = u"Complete";

void ExpectRegisterResultAndRun(blink::ServiceWorkerStatusCode expected,
                                base::RepeatingClosure continuation,
                                blink::ServiceWorkerStatusCode actual) {
  ASSERT_EQ(expected, actual);
  continuation.Run();
}

void ExpectUnregisterResultAndRun(bool expected,
                                  base::RepeatingClosure continuation,
                                  bool actual) {
  ASSERT_EQ(expected, actual);
  continuation.Run();
}
}  // namespace

static int CountRenderProcessHosts() {
  return RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}
class ServiceWorkerInternalsUIBrowserTest : public ContentBrowserTest {
 public:
  ServiceWorkerInternalsUIBrowserTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    StartServer();
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  void TearDownOnMainThread() override {
    // Flush remote storage control so that all pending callbacks are executed.
    wrapper()
        ->context()
        ->registry()
        ->GetRemoteStorageControl()
        .FlushForTesting();
    content::RunAllTasksUntilIdle();
    wrapper_ = nullptr;
  }

  void StartServer() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    embedded_test_server()->StartAcceptingConnections();
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  blink::ServiceWorkerStatusCode FindRegistration() {
    const GURL& document_url =
        embedded_test_server()->GetURL(kServiceWorkerSetupPage);
    blink::ServiceWorkerStatusCode status;
    base::RunLoop loop;
    wrapper()->FindReadyRegistrationForClientUrl(
        document_url, blink::StorageKey(url::Origin::Create(document_url)),
        base::BindLambdaForTesting(
            [&](blink::ServiceWorkerStatusCode find_status,
                scoped_refptr<ServiceWorkerRegistration> registration) {
              status = find_status;
              if (!registration.get())
                EXPECT_NE(blink::ServiceWorkerStatusCode::kOk, status);
              loop.Quit();
            }));
    loop.Run();
    return status;
  }

  std::vector<ServiceWorkerRegistrationInfo> GetAllRegistrations() {
    return wrapper()->GetAllLiveRegistrationInfo();
  }

  // Navigate to the page to set up a renderer page to embed a worker
  void NavigateToServiceWorkerSetupPage() {
    NavigateToURLBlockUntilNavigationsComplete(
        active_shell_, embedded_test_server()->GetURL(kServiceWorkerSetupPage),
        1);
    FocusContent(FROM_HERE);
  }

  void NavigateToServiceWorkerInternalUI() {
    ASSERT_TRUE(NavigateToURL(active_shell_, GURL(kServiceWorkerInternalsUrl)));
    // Ensure the window has focus after the navigation.
    FocusContent(FROM_HERE);
  }

  void FocusContent(const base::Location& from_here) {
    RenderWidgetHostImpl* host = RenderWidgetHostImpl::From(
        web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
    host->GotFocus();
    host->SetActive(true);

    ASSERT_TRUE(web_contents()->GetRenderWidgetHostView()->HasFocus())
        << "Location: " << from_here.ToString();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(active_shell_->web_contents());
  }

  // Create a new window and navigate to about::blank.
  Shell* CreateNewWindow() {
    active_shell_ = CreateBrowser();
    return active_shell_;
  }

  // Tear down the page.
  void TearDownWindow() {
    active_shell_->Close();
    active_shell_ = shell();
  }

  void MoveToWindow(Shell* window) { active_shell_ = window; }
  void ReloadWindow() { active_shell_->Reload(); }

  // Registers a service worker and then tears down the process it used, for a
  // clean slate going forward.
  void RegisterServiceWorker() {
    NavigateToServiceWorkerSetupPage();
    {
      base::RunLoop run_loop;
      blink::mojom::ServiceWorkerRegistrationOptions options(
          embedded_test_server()->GetURL(kServiceWorkerScope),
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports);
      // Set up the storage key for the service worker
      blink::StorageKey key(url::Origin::Create(options.scope));
      // Register returns when the promise is resolved.
      public_context()->RegisterServiceWorker(
          embedded_test_server()->GetURL(kServiceWorkerUrl), key, options,
          base::BindOnce(&ExpectRegisterResultAndRun,
                         blink::ServiceWorkerStatusCode::kOk,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }
  }

  void UnRegisterServiceWorker() {
    {
      base::RunLoop run_loop;
      blink::StorageKey key(url::Origin::Create(
          embedded_test_server()->GetURL(kServiceWorkerScope)));
      // Unregistering something should return true.
      public_context()->UnregisterServiceWorker(
          embedded_test_server()->GetURL(kServiceWorkerScope), key,
          base::BindOnce(&ExpectUnregisterResultAndRun, true,
                         run_loop.QuitClosure()));
      run_loop.Run();
    }
    EXPECT_EQ(FindRegistration(),
              blink::ServiceWorkerStatusCode::kErrorNotFound)
        << "Should not be able to find any Service Worker.";
  }

  testing::AssertionResult setMutationObserverSWPopulated() {
    static constexpr char kScript[] = R"(
     const elementToObserve = document.getElementById("serviceworker-list");
     const options = { childList: true, subtree: true };
     let placeholder = document.createElement("browserTestResult_SWPopulated");
     document.body.appendChild(placeholder);

     const callback = function (mutations, observer) {
       mutations.forEach((mutation) => {
         if (mutation.type === "childList") {
           mutation.addedNodes.forEach((node) => {
             if (
               node.classList &&
               node.classList.contains("serviceworker-registration")
             ) {
               placeholder.innerText = "done";
               document.title = $1;
               observer.disconnect();
             }
           });
         }
       });
     };

     const observer = new MutationObserver(callback);
     observer.observe(elementToObserve, options);
   )";
    return ExecJs(web_contents()->GetMainFrame(),
                  JsReplace(kScript, kCompleteTitle),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  std::string cleanMutationObserver(std::string mutation_observer) {
    static constexpr char kScript[] = R"(
     const task = $1;
     const result = document.querySelector("browserTestResult_"+task).innerText;
     document.querySelector("browserTestResult_"+task).remove();
     document.title = null;
     result;
   )";
    return EvalJs(web_contents()->GetMainFrame(),
                  JsReplace(kScript, mutation_observer),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                  /*world_id=*/1)
        .ExtractString();
  }

  std::string getServiceWorkerInfoFromInternalUI(
      int64_t registration_id,
      std::string service_worker_info) {
    static constexpr char kScript[] = R"(
     var serviceworkers = document.querySelectorAll(
       "div#serviceworker-list > div:not([style='display: none;'])\
             > div:not([class='serviceworker-summary']) > \
             div.serviceworker-registration"
     );

     let result = "";
     serviceworkers.forEach((serviceworker) => {
       let target;
       Array.prototype.forEach.call(
         serviceworker.querySelectorAll("*"),
         (node) =>
       {
           if (
             node.attributes.jscontent &&
             RegExp("registration_id").test(node.attributes.jscontent.value) &&
             node.innerText === $1
           ) {
             target = serviceworker;
           }
       });
       if (target) {
         Array.prototype.forEach.call(target.querySelectorAll("*"), (node) => {
           if (
             node.attributes.jscontent &&
             RegExp($2+$3+$4).test(node.attributes.jscontent.value)
           ) {
             result = node.innerText;
           }
         });
       }
     });
     result;
   )";
    return EvalJs(web_contents()->GetMainFrame(),
                  JsReplace(kScript, base::NumberToString(registration_id),
                            "^(.this\\.)?(", service_worker_info, ")$"),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
        .ExtractString();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  Shell* active_shell_ = shell();
};

// Tests
IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       NoRegisteredServiceWorker) {
  EXPECT_TRUE(CreateNewWindow());
  EXPECT_EQ(1, CountRenderProcessHosts());

  NavigateToServiceWorkerInternalUI();

  EXPECT_EQ(0, EvalJs(web_contents()->GetMainFrame(),
                      R"(document.querySelectorAll(
                 "div#serviceworker-list > div:not([style='display: none;'])\
                 > div:not([class='serviceworker-summary'])\
                 > div.serviceworker-registration"
               ).length)",
                      EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
                   .ExtractInt());

  TearDownWindow();
  EXPECT_EQ(0, CountRenderProcessHosts());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       RegisteredSWReflectedOnInternalUI) {
  Shell* SWInternalUIWindow = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();
  EXPECT_EQ(1, CountRenderProcessHosts());

  setMutationObserverSWPopulated();

  Shell* SWREgistrationWindow = CreateNewWindow();
  RegisterServiceWorker();
  EXPECT_EQ(2, CountRenderProcessHosts());

  MoveToWindow(SWInternalUIWindow);

  TitleWatcher title_watcher(web_contents(), kCompleteTitle);
  EXPECT_EQ(kCompleteTitle, title_watcher.WaitAndGetTitle());
  EXPECT_EQ("done", cleanMutationObserver("SWPopulated"));

  std::vector<ServiceWorkerRegistrationInfo> registrations =
      GetAllRegistrations();
  EXPECT_EQ(1u, registrations.size())
      << "There should be exactly one registration";
  EXPECT_EQ(registrations[0].scope.spec(),
            getServiceWorkerInfoFromInternalUI(registrations[0].registration_id,
                                               "scope"));
  EXPECT_EQ("ACTIVATED", getServiceWorkerInfoFromInternalUI(
                             registrations[0].registration_id, "status"));
  UnRegisterServiceWorker();
  EXPECT_GE(2, CountRenderProcessHosts()) << "Unregistering doesn't stop the"
                                             "workers eagerly, so their RPHs"
                                             "can still be running.";
  MoveToWindow(SWREgistrationWindow);
  TearDownWindow();

  MoveToWindow(SWInternalUIWindow);
  TearDownWindow();
  EXPECT_GE(1, CountRenderProcessHosts());
}
}  // namespace content
