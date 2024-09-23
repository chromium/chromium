// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
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
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
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

void ExpectUnregisterResultAndRun(
    blink::ServiceWorkerStatusCode expected_status,
    base::RepeatingClosure continuation,
    blink::ServiceWorkerStatusCode actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class ServiceWorkerObserver : public ServiceWorkerContextCoreObserver {
 public:
  explicit ServiceWorkerObserver(ServiceWorkerContextWrapper* context)
      : context_(context) {}

  void Init() { context_->AddObserver(this); }
  void Wait() { run_loop_.Run(); }

 protected:
  void Quit() {
    context_->RemoveObserver(this);
    run_loop_.Quit();
  }
  raw_ptr<ServiceWorkerContextWrapper> context_;

 private:
  base::RunLoop run_loop_;
};

class SWStateObserver : public ServiceWorkerObserver,
                        public base::RefCountedThreadSafe<SWStateObserver> {
 public:
  explicit SWStateObserver(ServiceWorkerContextWrapper* context,
                           ServiceWorkerVersion::Status target)
      : ServiceWorkerObserver(context), target_(target) {}

  SWStateObserver(const SWStateObserver&) = delete;
  SWStateObserver& operator=(const SWStateObserver&) = delete;

  int64_t RegistrationID() { return registration_id_; }
  int64_t VersionID() { return version_id_; }

 protected:
  using parent = ServiceWorkerObserver;

  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const ServiceWorkerVersion* version =
        parent::context_->GetLiveVersion(version_id);
    if (version->status() == target_) {
      version_id_ = version_id;
      registration_id_ = version->registration_id();
      parent::Quit();
    }
  }

 private:
  friend class base::RefCountedThreadSafe<SWStateObserver>;
  ~SWStateObserver() override = default;

  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
  const ServiceWorkerVersion::Status target_;
};

class SWOnStoppedObserver
    : public ServiceWorkerObserver,
      public base::RefCountedThreadSafe<SWOnStoppedObserver> {
 public:
  explicit SWOnStoppedObserver(ServiceWorkerContextWrapper* context)
      : ServiceWorkerObserver(context) {}

  SWOnStoppedObserver(const SWOnStoppedObserver&) = delete;
  SWOnStoppedObserver& operator=(const SWOnStoppedObserver&) = delete;

 protected:
  using parent = ServiceWorkerObserver;

  // ServiceWorkerContextCoreObserver overrides.
  void OnStopped(int64_t version_id) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const ServiceWorkerVersion* version =
        parent::context_->GetLiveVersion(version_id);
    ASSERT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kStopped);
    parent::Quit();
  }

 private:
  friend class base::RefCountedThreadSafe<SWOnStoppedObserver>;
  ~SWOnStoppedObserver() override = default;
};

class SWOnStartedObserver
    : public ServiceWorkerObserver,
      public base::RefCountedThreadSafe<SWOnStartedObserver> {
 public:
  explicit SWOnStartedObserver(ServiceWorkerContextWrapper* context)
      : ServiceWorkerObserver(context) {}

  SWOnStartedObserver(const SWOnStartedObserver&) = delete;
  SWOnStartedObserver& operator=(const SWOnStartedObserver&) = delete;

 protected:
  using parent = ServiceWorkerObserver;

  // ServiceWorkerContextCoreObserver overrides.
  void OnStarted(int64_t version_id,
                 const GURL& scope,
                 int process_id,
                 const GURL& script_url,
                 const blink::ServiceWorkerToken& token,
                 const blink::StorageKey& key) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const ServiceWorkerVersion* version =
        parent::context_->GetLiveVersion(version_id);
    ASSERT_EQ(version->running_status(), blink::EmbeddedWorkerStatus::kRunning);
    parent::Quit();
  }

 private:
  friend class base::RefCountedThreadSafe<SWOnStartedObserver>;
  ~SWOnStartedObserver() override = default;
};
class SWOnRegistrationDeletedObserver
    : public ServiceWorkerObserver,
      public base::RefCountedThreadSafe<SWOnRegistrationDeletedObserver> {
 public:
  explicit SWOnRegistrationDeletedObserver(ServiceWorkerContextWrapper* context)
      : ServiceWorkerObserver(context) {}

  SWOnRegistrationDeletedObserver(const SWOnRegistrationDeletedObserver&) =
      delete;
  SWOnRegistrationDeletedObserver& operator=(
      const SWOnRegistrationDeletedObserver&) = delete;

  int64_t RegistrationID() { return registration_id_; }

 protected:
  using parent = ServiceWorkerObserver;

  // ServiceWorkerContextCoreObserver overrides.
  void OnRegistrationDeleted(int64_t registration_id,
                             const GURL& scope,
                             const blink::StorageKey& key) override {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    registration_id_ = registration_id;
    parent::Quit();
  }

 private:
  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  friend class base::RefCountedThreadSafe<SWOnRegistrationDeletedObserver>;
  ~SWOnRegistrationDeletedObserver() override = default;
};

}  // namespace

class ServiceWorkerInternalsUIBrowserTest : public ContentBrowserTest {
 public:
  ServiceWorkerInternalsUIBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

 protected:
  // void SetUp() override {
  //   ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  //   ContentBrowserTest::SetUp();
  // }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    // StartServer();
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

  // void StartServer() {
  //   DCHECK_CURRENTLY_ON(BrowserThread::UI);
  //   embedded_test_server()->StartAcceptingConnections();
  // }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  blink::ServiceWorkerStatusCode FindRegistration() {
    const GURL& document_url =
        embedded_test_server()->GetURL(kServiceWorkerSetupPage);
    blink::ServiceWorkerStatusCode status;
    base::RunLoop loop;
    wrapper()->FindReadyRegistrationForClientUrl(
        document_url,
        blink::StorageKey::CreateFirstParty(url::Origin::Create(document_url)),
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
  void TearDownWindow(Shell* window) {
    SetActiveWindow(window);
    GetActiveWindow()->Close();
    SetActiveWindow(shell());
  }
  void SetActiveWindow(Shell* window) { active_shell_ = window; }
  Shell* GetActiveWindow() { return active_shell_; }
  void RegisterServiceWorker() {
    NavigateToServiceWorkerSetupPage();
    {
      base::RunLoop run_loop;
      blink::mojom::ServiceWorkerRegistrationOptions options(
          embedded_test_server()->GetURL(kServiceWorkerScope),
          blink::mojom::ScriptType::kClassic,
          blink::mojom::ServiceWorkerUpdateViaCache::kImports);
      // Set up the storage key for the service worker
      const blink::StorageKey key = blink::StorageKey::CreateFirstParty(
          url::Origin::Create(options.scope));
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
      const blink::StorageKey key =
          blink::StorageKey::CreateFirstParty(url::Origin::Create(
              embedded_test_server()->GetURL(kServiceWorkerScope)));
      // Unregistering something should return true.
      public_context()->UnregisterServiceWorker(
          embedded_test_server()->GetURL(kServiceWorkerScope), key,
          base::BindOnce(&ExpectUnregisterResultAndRun,
                         blink::ServiceWorkerStatusCode::kOk,
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
        web_contents()->GetPrimaryMainFrame(),
        JsReplace(kScript, "^(.this.)?(", target, ")$", expected, title),
        EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1);
  }

  int ServiveWorkerCountFromInternalUI() {
    return EvalJs(web_contents()->GetPrimaryMainFrame(),
                  R"(document.querySelectorAll(
                    "div#serviceworker-list \
                    > div:not([style='display: none;']) \
                    > div:not([class='serviceworker-summary']) \
                    > div.serviceworker-registration"
                  ).length)",
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                  /*world_id=*/1)
        .ExtractInt();
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
    return EvalJs(web_contents()->GetPrimaryMainFrame(),
                  JsReplace(kScript, base::NumberToString(registration_id),
                            "^(.this.)?(", target, ")$"),
                  EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1)
        .ExtractString();
  }

  void TriggerServiceWorkerInternalUIOption(int64_t registration_id,
                                            std::string option) {
    static constexpr char kScript[] = R"(
      var serviceworkers = document.querySelectorAll(
        "div#serviceworker-list > div:not([style='display: none;'])\
                  > div:not([class='serviceworker-summary']) > \
                  div.serviceworker-registration"
      );

      serviceworkers.forEach((serviceworker) => {
        Array.prototype.forEach.call(serviceworker.querySelectorAll("*"),
        (node) => {
          if (
            node.attributes.jscontent &&
            RegExp("registration_id").test(node.attributes.jscontent.value) &&
            node.innerText == $1
          ) {
            serviceworker.querySelector("button."+$2).click();
          }
        });
      });
    )";
    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        JsReplace(kScript, base::NumberToString(registration_id), option),
        EXECUTE_SCRIPT_DEFAULT_OPTIONS, /*world_id=*/1));
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
          case blink::EmbeddedWorkerStatus::kStopped:
            return "STOPPED";
          case blink::EmbeddedWorkerStatus::kStarting:
            return "STARTING";
          case blink::EmbeddedWorkerStatus::kRunning:
            return "RUNNING";
          case blink::EmbeddedWorkerStatus::kStopping:
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
  raw_ptr<Shell, DanglingUntriaged> active_shell_ = shell();
  net::EmbeddedTestServer https_server_;
};

// Tests
IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       NoRegisteredServiceWorker) {
  ASSERT_TRUE(CreateNewWindow());
  NavigateToServiceWorkerInternalUI();
  ASSERT_EQ(0, ServiveWorkerCountFromInternalUI());
  TearDownWindow();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       RegisteredSWReflectedOnInternalUI) {
  Shell* sw_internal_ui_window = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();

  // Register and wait for the service worker to populate on the internal UI.
  const std::u16string kTitle = u"SW populated";
  TitleWatcher title_watcher(web_contents(), kTitle);
  SetMutationObserver("status", "ACTIVATED", kTitle);

  Shell* sw_registration_window = CreateNewWindow();
  auto sw_state_observer = base::MakeRefCounted<SWStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  sw_state_observer->Init();
  RegisterServiceWorker();
  sw_state_observer->Wait();
  int64_t registration_id = sw_state_observer->RegistrationID();
  int64_t version_id = sw_state_observer->VersionID();
  ASSERT_EQ(1u, GetAllRegistrations().size())
      << "There should be exactly one registration";

  EXPECT_EQ(kTitle, title_watcher.WaitAndGetTitle());

  GURL top_level_page(embedded_test_server()->GetURL(kServiceWorkerUrl));
  GURL scope(embedded_test_server()->GetURL(kServiceWorkerScope));

  // Assert populated service worker info.
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
  ASSERT_EQ(url::Origin::Create(scope).GetDebugString(),
            GetServiceWorkerInfoFromInternalUI(registration_id, "origin"));
  ASSERT_EQ(
      net::SchemefulSite(url::Origin::Create(top_level_page)).Serialize(),
      GetServiceWorkerInfoFromInternalUI(registration_id, "top_level_site"));
  ASSERT_EQ("SameSite", GetServiceWorkerInfoFromInternalUI(
                            registration_id, "ancestor_chain_bit"));
  ASSERT_EQ("missed",
            GetServiceWorkerInfoFromInternalUI(registration_id, "nonce"));

  // Leave a clean state.
  UnRegisterServiceWorker();
  TearDownWindow(sw_registration_window);
  TearDownWindow(sw_internal_ui_window);
}

// The test is flaky on Mac and Linux. crbug.com/1324856
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_StopStartSWReflectedOnInternalUI \
  DISABLED_StopStartSWReflectedOnInternalUI
#else
#define MAYBE_StopStartSWReflectedOnInternalUI StopStartSWReflectedOnInternalUI
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest,
                       MAYBE_StopStartSWReflectedOnInternalUI) {
  Shell* sw_internal_ui_window = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();

  // Register and wait for the service worker to populate on the internal UI.
  const std::u16string kTitle = u"SW populated";
  TitleWatcher title_watcher(web_contents(), kTitle);
  SetMutationObserver("status", "ACTIVATED", kTitle);

  Shell* sw_registration_window = CreateNewWindow();
  auto sw_state_observer = base::MakeRefCounted<SWStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  sw_state_observer->Init();
  RegisterServiceWorker();
  sw_state_observer->Wait();
  int64_t registration_id = sw_state_observer->RegistrationID();
  ASSERT_EQ(1u, GetAllRegistrations().size())
      << "There should be exactly one registration";

  EXPECT_EQ(kTitle, title_watcher.WaitAndGetTitle());

  // Assert running status.
  SetActiveWindow(sw_internal_ui_window);
  ASSERT_EQ("RUNNING", GetServiceWorkerInfoFromInternalUI(registration_id,
                                                          "running_status"));
  // Tests that a stopping service worker is reflected on internal UI.
  const std::u16string kTitle2 = u"SW running_status: STOPPED";
  TitleWatcher title_watcher2(web_contents(), kTitle2);
  SetMutationObserver("running_status", "STOPPED", kTitle2);

  wrapper()->StopAllServiceWorkers(base::DoNothing());

  ASSERT_EQ(kTitle2, title_watcher2.WaitAndGetTitle());

  // Tests that a starting service worker is reflected on internal UI.
  const std::u16string kTitle3 = u"SW running_status: STARTING";
  TitleWatcher title_watcher_3(web_contents(), kTitle3);
  // To avoid premature timeouts and flakiness, the expected `running_status` to
  // be asserted will be `STARTING` instead of `RUNNING`.
  SetMutationObserver("running_status", "STARTING", kTitle3);

  wrapper()->StartActiveServiceWorker(GetAllRegistrations().front().scope,
                                      GetAllRegistrations().front().key,
                                      base::DoNothing());

  ASSERT_EQ(kTitle3, title_watcher_3.WaitAndGetTitle());

  // Leave a clean state.
  UnRegisterServiceWorker();
  TearDownWindow(sw_registration_window);
  TearDownWindow(sw_internal_ui_window);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerInternalsUIBrowserTest, InternalUIOptions) {
  Shell* sw_internal_ui_window = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();

  // Register and wait for the service worker to populate on the internal UI.
  const std::u16string kTitle = u"SW populated";
  TitleWatcher title_watcher(web_contents(), kTitle);
  SetMutationObserver("status", "ACTIVATED", kTitle);

  Shell* sw_registration_window = CreateNewWindow();
  auto sw_state_observer = base::MakeRefCounted<SWStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  sw_state_observer->Init();
  RegisterServiceWorker();
  sw_state_observer->Wait();
  int64_t registration_id = sw_state_observer->RegistrationID();
  ASSERT_EQ(1u, GetAllRegistrations().size())
      << "There should be exactly one registration";

  EXPECT_EQ(kTitle, title_watcher.WaitAndGetTitle());

  // Test the stop option on the service worker internal UI.
  SetActiveWindow(sw_internal_ui_window);
  const std::u16string kTitle2 = u"SW running_status: STOPPED";
  TitleWatcher title_watcher_2(web_contents(), kTitle2);
  SetMutationObserver("running_status", "STOPPED", kTitle2);

  auto sw_on_stopped_observer =
      base::MakeRefCounted<SWOnStoppedObserver>(wrapper());
  sw_on_stopped_observer->Init();
  TriggerServiceWorkerInternalUIOption(registration_id, "stop");
  sw_on_stopped_observer->Wait();

  EXPECT_EQ(kTitle2, title_watcher_2.WaitAndGetTitle());

  // Test the start option on the service worker internal UI.
  SetActiveWindow(sw_internal_ui_window);
  const std::u16string kTitle3 = u"SW running_status: RUNNING";
  TitleWatcher title_watcher_3(web_contents(), kTitle3);
  SetMutationObserver("running_status", "RUNNING", kTitle3);

  auto sw_on_started_observer =
      base::MakeRefCounted<SWOnStartedObserver>(wrapper());
  sw_on_started_observer->Init();
  TriggerServiceWorkerInternalUIOption(registration_id, "start");
  sw_on_started_observer->Wait();

  EXPECT_EQ(kTitle3, title_watcher_3.WaitAndGetTitle());

  // Test the unregister option on the service worker internal UI.
  SetActiveWindow(sw_internal_ui_window);
  auto sw_on_registration_deleted_observer =
      base::MakeRefCounted<SWOnRegistrationDeletedObserver>(wrapper());
  sw_on_registration_deleted_observer->Init();
  TriggerServiceWorkerInternalUIOption(registration_id, "unregister");
  sw_on_registration_deleted_observer->Wait();

  ASSERT_EQ(registration_id,
            sw_on_registration_deleted_observer->RegistrationID());

  // Leave a clean state.
  TearDownWindow(sw_registration_window);
  TearDownWindow(sw_internal_ui_window);
}

class ServiceWorkerInternalsUIBrowserTestWithStoragePartitioning
    : public ServiceWorkerInternalsUIBrowserTest {
 public:
  ServiceWorkerInternalsUIBrowserTestWithStoragePartitioning() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerInternalsUIBrowserTestWithStoragePartitioning,
    RegisteredSWReflectedOnInternalUI) {
  https_server()->ServeFilesFromSourceDirectory(GetTestDataFilePath());
  https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(https_server()->Start());

  Shell* sw_internal_ui_window = CreateNewWindow();
  NavigateToServiceWorkerInternalUI();

  // Register and wait for the service worker to populate on the internal UI.
  const std::u16string kTitle = u"SW populated";
  TitleWatcher title_watcher(web_contents(), kTitle);
  SetMutationObserver("status", "ACTIVATED", kTitle);

  auto sw_state_observer = base::MakeRefCounted<SWStateObserver>(
      wrapper(), ServiceWorkerVersion::ACTIVATED);
  sw_state_observer->Init();

  GURL top_level_page(
      https_server()->GetURL("a.test", kServiceWorkerSetupPage));
  GURL scope(https_server()->GetURL("b.test", kServiceWorkerScope));
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        scope, blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    // Set up the storage key for the service worker
    blink::StorageKey key = blink::StorageKey::Create(
        url::Origin::Create(options.scope),
        net::SchemefulSite(url::Origin::Create(top_level_page)),
        blink::mojom::AncestorChainBit::kCrossSite);
    // Register returns when the promise is resolved.
    public_context()->RegisterServiceWorker(
        https_server()->GetURL("b.test", kServiceWorkerUrl), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  sw_state_observer->Wait();
  int64_t registration_id = sw_state_observer->RegistrationID();

  EXPECT_EQ(kTitle, title_watcher.WaitAndGetTitle());

  // Assert populated service worker info.
  SetActiveWindow(sw_internal_ui_window);
  ASSERT_EQ(scope.spec(),
            GetServiceWorkerInfoFromInternalUI(registration_id, "scope"));

  ASSERT_EQ(url::Origin::Create(scope).GetDebugString(),
            GetServiceWorkerInfoFromInternalUI(registration_id, "origin"));
  ASSERT_EQ(
      net::SchemefulSite(url::Origin::Create(top_level_page)).Serialize(),
      GetServiceWorkerInfoFromInternalUI(registration_id, "top_level_site"));
  ASSERT_EQ("CrossSite", GetServiceWorkerInfoFromInternalUI(
                             registration_id, "ancestor_chain_bit"));
  ASSERT_EQ("missed",
            GetServiceWorkerInfoFromInternalUI(registration_id, "nonce"));
}

}  // namespace content
