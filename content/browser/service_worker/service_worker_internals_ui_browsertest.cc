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
#include "build/chromeos_buildflags.h"
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
        GetActiveWindow(),
        embedded_test_server()->GetURL(kServiceWorkerSetupPage), 1);
    FocusContent(FROM_HERE);
  }

  void NavigateToServiceWorkerInternalUI() {
    ASSERT_TRUE(
        NavigateToURL(GetActiveWindow(), GURL(kServiceWorkerInternalsUrl)));
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

  WebContentsImpl* web_contents() {
    return static_cast<WebContentsImpl*>(GetActiveWindow()->web_contents());
  }

  // Create a new window and navigate to about::blank.
  Shell* CreateNewWindow() {
    SetActiveWindow(CreateBrowser());
    return GetActiveWindow();
  }
  // Tear down the page.
  void TearDownWindow() {
    GetActiveWindow()->Close();
    SetActiveWindow(shell());
  }
  void SetActiveWindow(Shell* window) { active_shell_ = window; }
  Shell* GetActiveWindow() { return active_shell_; }

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
    ASSERT_EQ(FindRegistration(),
              blink::ServiceWorkerStatusCode::kErrorNotFound)
        << "Should not be able to find any Service Worker.";
  }

  testing::AssertionResult SetMutationObserver(std::string target,
                                               std::string expected,
                                               const std::u16string& title) {
    static constexpr char kScript[] = R"(
      const elementToObserve = document.getElementById("serviceworker-list");
      const options = { childList: true, subtree: true };

      const callback = function (mutations, observer) {
        mutations.forEach((mutation) => {
          if (
            mutation.type === "childList" &&
            mutation.target &&
            mutation.target.attributes &&
            mutation.target.attributes.jscontent &&
            RegExp($1+$2+$3).test(mutation.target.attributes.jscontent.value)
          ) {
            mutation.addedNodes.forEach((node) => {
              if (node.data === $4) {
                document.title = $5;
                observer.disconnect();
              }
            });
          }
        });
      };

      const observer = new MutationObserver(callback);
      observer.observe(elementToObserve, options);
   )";
    return ExecJs(
        web_contents()->GetMainFrame(),
        JsReplace(kScript, "^(.this.)?(", target, ")$", expected, title),
        EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  std::string GetServiceWorkerInfoFromInternalUI(int64_t registration_id,
                                                 std::string target) {
    static constexpr char kScript[] = R"(
     var serviceworkers = document.querySelectorAll(
       "div#serviceworker-list > div:not([style='display: none;'])\
             > div:not([class='serviceworker-summary']) > \
             div.serviceworker-registration"
     );

     let result = "not found";
     serviceworkers.forEach((serviceworker) => {
       let target;
       Array.prototype.forEach.call(
         serviceworker.querySelectorAll("*"),
         (node) =>
       {
           if (
             node.attributes.jscontent &&
             RegExp("registration_id").test(node.attributes.jscontent.value) &&
             node.innerText == $1
           ) {
             target = serviceworker;
           }
       });
       if (target) {
         Array.prototype.forEach.call(target.querySelectorAll("span"),
         (node) => {
           if (
             node.attributes.jscontent &&
             RegExp($2+$3+$4).test(node.attributes.jscontent.value)
           ) {
            result = node.innerText;
            if (result === "") result = "missed";
           }
         });
       }
     });
     result;
   )";
    return EvalJs(web_contents()->GetMainFrame(),
                  JsReplace(kScript, base::NumberToString(registration_id),
                            "^(.this.)?(", target, ")$"),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
        .ExtractString();
  }

  enum InfoTag {
    SCOPE,
    STATUS,
    RUNNING_STATUS,
    PROCESS_ID,
  };

  std::string GetServiceWorkerInfo(int info_tag) {
    ServiceWorkerRegistrationInfo registration = GetAllRegistrations().front();
    switch (info_tag) {
      case SCOPE:
        return registration.scope.spec();
      case STATUS:
        switch (registration.active_version.status) {
          case ServiceWorkerVersion::NEW:
            return "NEW";
          case ServiceWorkerVersion::INSTALLING:
            return "INSTALLING";
          case ServiceWorkerVersion::INSTALLED:
            return "INSTALLED";
          case ServiceWorkerVersion::ACTIVATING:
            return "ACTIVATING";
          case ServiceWorkerVersion::ACTIVATED:
            return "ACTIVATED";
          case ServiceWorkerVersion::REDUNDANT:
            return "REDUNDANT";
        }
      case RUNNING_STATUS:
        switch (registration.active_version.running_status) {
          case EmbeddedWorkerStatus::STOPPED:
            return "STOPPED";
          case EmbeddedWorkerStatus::STARTING:
            return "STARTING";
          case EmbeddedWorkerStatus::RUNNING:
            return "RUNNING";
          case EmbeddedWorkerStatus::STOPPING:
            return "STOPPING";
        }
      case PROCESS_ID:
        return base::NumberToString(base::GetProcId(
            RenderProcessHost::FromID(registration.active_version.process_id)
                ->GetProcess()
                .Handle()));
      default:
        return "";
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  Shell* active_shell_ = shell();
};

class WorkerActivatedObserver
    : public ServiceWorkerContextCoreObserver,
      public base::RefCountedThreadSafe<WorkerActivatedObserver> {
 public:
  explicit WorkerActivatedObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}

  WorkerActivatedObserver(const WorkerActivatedObserver&) = delete;
  WorkerActivatedObserver& operator=(const WorkerActivatedObserver&) = delete;

  void Init() { context_->AddObserver(this); }
  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == ServiceWorkerVersion::ACTIVATED) {
      context_->RemoveObserver(this);
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      run_loop_.Quit();
    }
  }
  void Wait() { run_loop_.Run(); }

  int64_t registration_id() { return registration_id_; }
  int64_t version_id() { return version_id_; }

 private:
  friend class base::RefCountedThreadSafe<WorkerActivatedObserver>;
  ~WorkerActivatedObserver() override = default;

  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  base::RunLoop run_loop_;
  raw_ptr<ServiceWorkerContextWrapper> context_;
};

// Tests
IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       NoRegisteredServiceWorker) {
  ASSERT_TRUE(CreateNewWindow());

  NavigateToServiceWorkerInternalUI();

  ASSERT_EQ(0, EvalJs(web_contents()->GetMainFrame(),
                      R"(document.querySelectorAll(
                 "div#serviceworker-list > div:not([style='display: none;'])\
                 > div:not([class='serviceworker-summary'])\
                 > div.serviceworker-registration"
               ).length)",
                      EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
                   .ExtractInt());

  TearDownWindow();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       RegisteredSWReflectedOnInternalUI) {
  Shell* sw_internal_ui_window = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();

  const std::u16string kTitle = u"Mutated";
  TitleWatcher title_watcher(web_contents(), kTitle);
  SetMutationObserver("status", "ACTIVATED", kTitle);

  Shell* sw_registration_window = CreateNewWindow();
  // Register and wait for activation.
  auto observer = base::MakeRefCounted<WorkerActivatedObserver>(wrapper());
  observer->Init();
  RegisterServiceWorker();
  observer->Wait();
  int64_t registration_id = observer->registration_id();
  int64_t version_id = observer->version_id();
  ASSERT_EQ(1u, GetAllRegistrations().size())
      << "There should be exactly one registration";

  EXPECT_EQ(kTitle, title_watcher.WaitAndGetTitle());
  SetActiveWindow(sw_internal_ui_window);
  ASSERT_EQ(base::NumberToString(version_id),
            GetServiceWorkerInfoFromInternalUI(registration_id, "version_id"));
  ASSERT_EQ(GetServiceWorkerInfo(SCOPE),
            GetServiceWorkerInfoFromInternalUI(registration_id, "scope"));
  ASSERT_EQ(GetServiceWorkerInfo(STATUS),
            GetServiceWorkerInfoFromInternalUI(registration_id, "status"));
  ASSERT_EQ(
      GetServiceWorkerInfo(RUNNING_STATUS),
      GetServiceWorkerInfoFromInternalUI(registration_id, "running_status"));
  ASSERT_EQ(GetServiceWorkerInfo(PROCESS_ID),
            GetServiceWorkerInfoFromInternalUI(registration_id, "process_id"));
  UnRegisterServiceWorker();

  TearDownWindow();
  SetActiveWindow(sw_registration_window);
  TearDownWindow();
}
}  // namespace content
