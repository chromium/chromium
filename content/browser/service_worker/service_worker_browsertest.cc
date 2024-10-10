// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/process_lock.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/common/content_constants_internal.h"
#include "content/common/features.h"
#include "content/common/service_worker/race_network_request_write_buffer_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cors_origin_pattern_setter.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "media/media_buildflags.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_status_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "storage/browser/blob/blob_handle.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-test-utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "third_party/re2/src/re2/re2.h"

using blink::mojom::CacheStorageError;

namespace content {

namespace {
using MainResourceLoadCompletedUkmEntry =
    ukm::builders::ServiceWorker_MainResourceLoadCompleted;

// V8ScriptRunner::setCacheTimeStamp() stores 16 byte data (marker + tag +
// timestamp).
const int kV8CacheTimeStampDataSize =
    sizeof(uint32_t) + sizeof(uint32_t) + sizeof(double);

void ExpectRegisterResultAndRun(blink::ServiceWorkerStatusCode expected,
                                base::RepeatingClosure continuation,
                                blink::ServiceWorkerStatusCode actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

void ExpectUnregisterResultAndRun(
    blink::ServiceWorkerStatusCode expected_status,
    base::RepeatingClosure continuation,
    blink::ServiceWorkerStatusCode actual_status) {
  EXPECT_EQ(expected_status, actual_status);
  continuation.Run();
}

class WorkerStateObserver : public ServiceWorkerContextCoreObserver {
 public:
  WorkerStateObserver(scoped_refptr<ServiceWorkerContextWrapper> context,
                      ServiceWorkerVersion::Status target)
      : context_(std::move(context)), target_(target) {
    observation_.Observe(context_.get());
  }

  WorkerStateObserver(const WorkerStateObserver&) = delete;
  WorkerStateObserver& operator=(const WorkerStateObserver&) = delete;

  ~WorkerStateObserver() override = default;

  // ServiceWorkerContextCoreObserver overrides.
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status) override {
    const ServiceWorkerVersion* version = context_->GetLiveVersion(version_id);
    if (version->status() == target_) {
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
  int64_t registration_id_ = blink::mojom::kInvalidServiceWorkerRegistrationId;
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  base::RunLoop run_loop_;
  scoped_refptr<ServiceWorkerContextWrapper> context_;
  const ServiceWorkerVersion::Status target_;
  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation_{this};
};

class WorkerClientDestroyedObserver : public ServiceWorkerContextCoreObserver {
 public:
  explicit WorkerClientDestroyedObserver(ServiceWorkerContextWrapper* context) {
    // `context` must outlive this observer.
    scoped_observation_.Observe(context);
  }
  ~WorkerClientDestroyedObserver() override = default;

  void WaitUntilDestroyed() { run_loop_.Run(); }

  // ServiceWorkerContextCoreObserver overrides.
  void OnClientDestroyed(ukm::SourceId client_source_id,
                         const GURL& url,
                         blink::mojom::ServiceWorkerClientType type) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      scoped_observation_{this};
};

std::unique_ptr<net::test_server::HttpResponse> VerifySaveDataHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/service_worker/generated_sw.js")
    return nullptr;
  auto it = request.headers.find("Save-Data");
  EXPECT_NE(request.headers.end(), it);
  EXPECT_EQ("on", it->second);

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content_type("text/javascript");
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse>
VerifySaveDataNotInAccessControlRequestHeader(
    const net::test_server::HttpRequest& request) {
  if (request.method == net::test_server::METHOD_OPTIONS) {
    // 'Save-Data' is not added to the CORS preflight request.
    auto it = request.headers.find("Save-Data");
    EXPECT_EQ(request.headers.end(), it);
  } else {
    // 'Save-Data' is added to the actual request, as expected.
    auto it = request.headers.find("Save-Data");
    EXPECT_NE(request.headers.end(), it);
    EXPECT_EQ("on", it->second);
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  if (request.method == net::test_server::METHOD_OPTIONS) {
    // Access-Control-Request-Headers should contain 'X-Custom-Header' and not
    // contain 'Save-Data'.
    auto acrh_iter = request.headers.find("Access-Control-Request-Headers");
    EXPECT_NE(request.headers.end(), acrh_iter);
    EXPECT_NE(std::string::npos, acrh_iter->second.find("x-custom-header"));
    EXPECT_EQ(std::string::npos, acrh_iter->second.find("save-data"));
    http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                   acrh_iter->second);
    http_response->AddCustomHeader("Access-Control-Allow-Methods", "GET");
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else {
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    http_response->set_content("PASS");
  }
  return std::move(http_response);
}

void CountScriptResources(ServiceWorkerContextWrapper* wrapper,
                          const GURL& scope,
                          int* num_resources) {
  *num_resources = -1;

  std::vector<ServiceWorkerRegistrationInfo> infos =
      wrapper->GetAllLiveRegistrationInfo();
  if (infos.empty())
    return;

  int version_id;
  size_t index = infos.size() - 1;
  if (infos[index].installing_version.version_id !=
      blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[index].installing_version.version_id;
  else if (infos[index].waiting_version.version_id !=
           blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[1].waiting_version.version_id;
  else if (infos[index].active_version.version_id !=
           blink::mojom::kInvalidServiceWorkerVersionId)
    version_id = infos[index].active_version.version_id;
  else
    return;

  ServiceWorkerVersion* version = wrapper->GetLiveVersion(version_id);
  *num_resources = static_cast<int>(version->script_cache_map()->size());
}

void StoreString(std::string* result,
                 base::OnceClosure callback,
                 base::Value value) {
  if (result && value.is_string())
    *result = value.GetString();
  std::move(callback).Run();
}

int GetInt(const base::Value::Dict& dict, std::string_view key) {
  std::optional<int> out = dict.FindInt(key);
  EXPECT_TRUE(out.has_value());
  return out.value_or(0);
}

std::string GetString(const base::Value::Dict& dict, std::string_view key) {
  const std::string* out = dict.FindString(key);
  EXPECT_TRUE(out);
  return out ? *out : std::string();
}

bool GetBoolean(const base::Value::Dict& dict, std::string_view key) {
  std::optional<bool> out = dict.FindBool(key);
  EXPECT_TRUE(out.has_value());
  return out.value_or(false);
}

bool CheckHeader(const base::Value::Dict& dict,
                 std::string_view header_name,
                 std::string_view header_value) {
  const base::Value::List* headers = dict.FindList("headers");
  if (!headers) {
    ADD_FAILURE();
    return false;
  }

  for (const auto& header : *headers) {
    if (!header.is_list()) {
      ADD_FAILURE();
      return false;
    }
    const base::Value::List& name_value_pair = header.GetList();
    if (name_value_pair.size() != 2u) {
      ADD_FAILURE();
      return false;
    }
    const std::string* name = name_value_pair[0].GetIfString();
    if (!name) {
      ADD_FAILURE();
      return false;
    }

    const std::string* value = name_value_pair[1].GetIfString();
    if (!value) {
      ADD_FAILURE();
      return false;
    }

    if (*name == header_name && *value == header_value)
      return true;
  }
  return false;
}

bool HasHeader(const base::Value::Dict& dict, std::string_view header_name) {
  const base::Value::List* headers = dict.FindList("headers");
  if (!headers) {
    ADD_FAILURE();
    return false;
  }

  for (const auto& header : *headers) {
    if (!header.is_list()) {
      ADD_FAILURE();
      return false;
    }
    const base::Value::List& name_value_pair = header.GetList();
    if (name_value_pair.size() != 2u) {
      ADD_FAILURE();
      return false;
    }
    const std::string* name = name_value_pair[0].GetIfString();
    if (!name) {
      ADD_FAILURE();
      return false;
    }

    if (*name == header_name)
      return true;
  }
  return false;
}

const char kNavigationPreloadNetworkError[] =
    "NetworkError: The service worker navigation preload request failed due to "
    "a network error. This may have been an actual network error, or caused by "
    "the browser simulating offline to see if the page works offline: "
    "see https://w3c.github.io/manifest/#installability-signals";

void CheckPageIsMarkedSecure(
    Shell* shell,
    scoped_refptr<net::X509Certificate> expected_certificate) {
  NavigationEntry* entry =
      shell->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_TRUE(expected_certificate->EqualsExcludingChain(
      entry->GetSSL().certificate.get()));
  EXPECT_FALSE(net::IsCertStatusError(entry->GetSSL().cert_status));
}

}  // namespace

class ServiceWorkerBrowserTest : public ContentBrowserTest {
 protected:
  ServiceWorkerBrowserTest() = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SetServiceWorkerContextWrapper();
    ShellContentBrowserClient::Get()
        ->browser_context()
        ->set_client_hints_controller_delegate(
            &client_hints_controller_delegate_);
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

  void SetServiceWorkerContextWrapper() {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    wrapper_ = static_cast<ServiceWorkerContextWrapper*>(
        partition->GetServiceWorkerContext());
  }

  // Starts the test server and navigates the renderer to an empty page. Call
  // this after adding all request handlers to the test server. Adding handlers
  // after the test server has started is not allowed.
  void StartServerAndNavigateToSetup() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    embedded_test_server()->StartAcceptingConnections();

    // Navigate to the page to set up a renderer page (where we can embed
    // a worker).
    NavigateToURLBlockUntilNavigationsComplete(
        shell(), embedded_test_server()->GetURL("/service_worker/empty.html"),
        1);
  }

  ServiceWorkerContextWrapper* wrapper() { return wrapper_.get(); }
  ServiceWorkerContext* public_context() { return wrapper(); }

  blink::ServiceWorkerStatusCode FindRegistration(const GURL& document_url) {
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

 private:
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  MockClientHintsControllerDelegate client_hints_controller_delegate_{
      content::GetShellUserAgentMetadata()};
};

class MockContentBrowserClient : public ContentBrowserTestContentBrowserClient {
 public:
  MockContentBrowserClient() : data_saver_enabled_(false) {}

  ~MockContentBrowserClient() override = default;

  void set_data_saver_enabled(bool enabled) { data_saver_enabled_ = enabled; }

  // ContentBrowserClient overrides:
  bool IsDataSaverEnabled(BrowserContext* context) override {
    return data_saver_enabled_;
  }

  void OverrideWebkitPrefs(WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override {
    prefs->data_saver_enabled = data_saver_enabled_;
  }

 private:
  bool data_saver_enabled_;
};

// An observer that waits for the service worker to be running.
class WorkerRunningStatusObserver : public ServiceWorkerContextObserver {
 public:
  explicit WorkerRunningStatusObserver(ServiceWorkerContext* context) {
    scoped_context_observation_.Observe(context);
  }

  WorkerRunningStatusObserver(const WorkerRunningStatusObserver&) = delete;
  WorkerRunningStatusObserver& operator=(const WorkerRunningStatusObserver&) =
      delete;

  ~WorkerRunningStatusObserver() override = default;

  int64_t version_id() { return version_id_; }

  void WaitUntilRunning() {
    if (version_id_ == blink::mojom::kInvalidServiceWorkerVersionId)
      run_loop_.Run();
  }

  void OnVersionStartedRunning(
      int64_t version_id,
      const ServiceWorkerRunningInfo& running_info) override {
    version_id_ = version_id;

    if (run_loop_.running())
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<ServiceWorkerContext, ServiceWorkerContextObserver>
      scoped_context_observation_{this};
  int64_t version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;
};

// Tests the |top_frame_origin| and |request_initiator| on the main resource and
// subresource requests from service workers, in order to ensure proper handling
// by the SplitCache. See https://crbug.com/918868.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, RequestOrigin) {
  embedded_test_server()->StartAcceptingConnections();

  // To make things tricky about |top_frame_origin|, this test navigates to a
  // page on |embedded_test_server()| which has a cross-origin iframe that
  // registers the service worker.
  net::EmbeddedTestServer cross_origin_server;
  cross_origin_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(cross_origin_server.Start());

  // There are three requests to test:
  // 1) The request for the worker itself ("request_origin_worker.js").
  // 2) importScripts("empty.js") from the service worker.
  // 3) fetch("empty.html") from the service worker.
  std::map<GURL, bool /* is_main_script */> expected_request_urls = {
      {cross_origin_server.GetURL("/service_worker/request_origin_worker.js"),
       true},
      {cross_origin_server.GetURL("/service_worker/empty.js"), false},
      {cross_origin_server.GetURL("/service_worker/empty.html"), false}};

  base::RunLoop request_origin_expectation_waiter;
  URLLoaderInterceptor request_listener(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it != expected_request_urls.end()) {
          if (it->second) {
            // The main script is loaded from the browser process. In that case,
            // `originated_from_service_worker` is set to false and the
            // `trusted_params` is available.
            EXPECT_FALSE(params->url_request.originated_from_service_worker);
            EXPECT_TRUE(
                params->url_request.trusted_params.has_value() &&
                !params->url_request.trusted_params->isolation_info.IsEmpty());
          } else {
            EXPECT_TRUE(params->url_request.originated_from_service_worker);
            EXPECT_FALSE(
                params->url_request.trusted_params.has_value() &&
                !params->url_request.trusted_params->isolation_info.IsEmpty());
          }
          EXPECT_TRUE(params->url_request.request_initiator.has_value());
          EXPECT_EQ(params->url_request.request_initiator->GetURL(),
                    cross_origin_server.base_url());
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty())
          request_origin_expectation_waiter.Quit();
        return false;
      }));

  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL(
          "/service_worker/one_subframe.html?subframe_url=" +
          cross_origin_server
              .GetURL("/service_worker/create_service_worker.html")
              .spec()),
      1);
  RenderFrameHost* subframe_rfh = FrameMatchingPredicate(
      shell()->web_contents()->GetPrimaryPage(),
      base::BindRepeating(&FrameMatchesName, "subframe_name"));
  DCHECK(subframe_rfh);

  EXPECT_EQ("DONE",
            EvalJs(subframe_rfh, "register('request_origin_worker.js');"));

  request_origin_expectation_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, FetchPageWithSaveData) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/handle_fetch.html";
  const char kWorkerUrl[] = "/service_worker/add_save_data_to_title.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  shell()->web_contents()->OnWebPreferencesChanged();
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  const std::u16string title1 = u"save-data=on";
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Tests that when data saver is enabled and a cross-origin fetch by a webpage
// is intercepted by a serviceworker, and the serviceworker does a fetch, the
// preflight request does not have save-data in Access-Control-Request-Headers.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, CrossOriginFetchWithSaveData) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/fetch_cross_origin.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  net::EmbeddedTestServer cross_origin_server;
  cross_origin_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  cross_origin_server.RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataNotInAccessControlRequestHeader));
  ASSERT_TRUE(cross_origin_server.Start());

  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  shell()->web_contents()->OnWebPreferencesChanged();
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  const std::u16string title = u"PASS";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(base::StringPrintf(
                   "%s?%s", kPageUrl,
                   cross_origin_server.GetURL("/cross_origin_request.html")
                       .spec()
                       .c_str()))));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       FetchPageWithSaveDataPassThroughOnFetch) {
  const char kPageUrl[] = "/service_worker/pass_through_fetch.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_pass_through.js";
  MockContentBrowserClient content_browser_client;
  content_browser_client.set_data_saver_enabled(true);
  shell()->web_contents()->OnWebPreferencesChanged();
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);

  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&VerifySaveDataHeaderInRequest));
  StartServerAndNavigateToSetup();

  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(kPageUrl), 1);

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, Reload) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/reload.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_reload.js";
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  const std::u16string title1 = u"reload=false";
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());

  const std::u16string title2 = u"reload=true";
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

// Test when the renderer requests termination because the service worker is
// idle, and the browser ignores the request because DevTools is attached. The
// renderer should continue processing events on the service worker instead of
// waiting for termination or an event from the browser. Regression test for
// https://crbug.com/878667.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, IdleTimerWithDevTools) {
  StartServerAndNavigateToSetup();

  // Register a service worker.
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  const GURL scope =
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html");
  const GURL worker_url = embedded_test_server()->GetURL(
      "/service_worker/fetch_event_respond_with_fetch.js");

  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kNone);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      worker_url, key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  // Navigate to a new page and request a sub resource. This should succeed
  // normally.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html")));
  EXPECT_EQ("Echo", EvalJs(shell(), "fetch_from_page('/echo');"));

  // Simulate to attach DevTools.
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  version->SetDevToolsAttached(true);

  // Set the idle timer delay to zero for making the service worker
  // idle immediately. This may cause infinite loop of IPCs when no
  // event was queued in the renderer because a callback of
  // RequestTermination() is called and it triggers another
  // RequestTermination() immediately. However, this is unusual
  // situation happening only in testing so it's acceptable.
  // In production code, WakeUp() as the result of
  // RequestTermination() doesn't happen when the idle timer delay is
  // set to zero. Instead, activating a new worker will be triggered.
  version->endpoint()->SetIdleDelay(base::Seconds(0));

  // Trigger another sub resource request. The sub resource request will
  // directly go to the worker thread and be queued because the worker is
  // idle. However, the browser process notifies the renderer to let it continue
  // to work because DevTools is attached, and it'll result in running all
  // queued events.
  EXPECT_EQ("Echo", EvalJs(shell(), "fetch_from_page('/echo');"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPSServiceWorkerIsMarkedAsSecure) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/in-scope";
  const char kWorkerUrl[] = "/service_worker/worker_script";
  const char kWorkerScript[] = R"(
      self.addEventListener('fetch', e => {
        e.respondWith(new Response('<title>Title</title>', {
          headers: {'Content-Type': 'text/html'}
        }));
      });
      // Version: %d)";

  // Register a handler which serves different script on each request. The
  // service worker returns a page titled by "Title" via Blob.
  base::Lock service_worker_served_count_lock;
  int service_worker_served_count = 0;
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        // Note this callback runs on a background thread.
        if (request.relative_url != kWorkerUrl)
          return nullptr;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/javascript");
        base::AutoLock lock(service_worker_served_count_lock);
        response->set_content(
            base::StringPrintf(kWorkerScript, ++service_worker_served_count));
        return response;
      }));
  ASSERT_TRUE(https_server.Start());

  // 1st attempt: install a service worker and open the controlled page.
  {
    // Register a service worker which controls |kPageUrl|.
    WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
    blink::mojom::ServiceWorkerRegistrationOptions options(
        https_server.GetURL(kPageUrl), blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        https_server.GetURL(kWorkerUrl), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
    observer.Wait();
    {
      base::AutoLock lock(service_worker_served_count_lock);
      EXPECT_EQ(1, service_worker_served_count);
    }

    // Wait until the page is appropriately served by the service worker.
    const std::u16string title = u"Title";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), https_server.GetURL(kPageUrl)));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

    // The page should be marked as secure.
    CheckPageIsMarkedSecure(shell(), https_server.GetCertificate());
  }

  // Navigate away from the page so that the worker has no controllee.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  // 2nd attempt: update the service worker and open the controlled page again.
  {
    // Update the service worker.
    WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
    GURL url = https_server.GetURL(kPageUrl);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
    wrapper()->UpdateRegistration(url, key);
    observer.Wait();

    // Wait until the page is appropriately served by the service worker.
    const std::u16string title = u"Title";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

    // The page should be marked as secure.
    CheckPageIsMarkedSecure(shell(), https_server.GetCertificate());
  }

  shell()->Close();

  base::RunLoop run_loop;
  GURL url = https_server.GetURL(kPageUrl);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
  public_context()->UnregisterServiceWorker(
      url, key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       ResponseFromHTTPServiceWorkerIsNotMarkedAsSecure) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/fetch_event_blob.html";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kPageUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  const std::u16string title = u"Title";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  EXPECT_FALSE(entry->GetSSL().certificate);

  shell()->Close();

  base::RunLoop run_loop;
  public_context()->UnregisterServiceWorker(
      embedded_test_server()->GetURL(kPageUrl), key,
      base::BindOnce(&ExpectUnregisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, ImportsBustMemcache) {
  StartServerAndNavigateToSetup();
  const char kScopeUrl[] = "/service_worker/imports_bust_memcache_scope/";
  const char kPageUrl[] = "/service_worker/imports_bust_memcache.html";
  const std::u16string kOKTitle(u"OK");
  const std::u16string kFailTitle(u"FAIL");

  TitleWatcher title_watcher(shell()->web_contents(), kOKTitle);
  title_watcher.AlsoWaitForTitle(kFailTitle);
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  std::u16string title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(kOKTitle, title);

  // Verify the number of resources in the implicit script cache is correct.
  const int kExpectedNumResources = 2;
  int num_resources = 0;
  CountScriptResources(wrapper(), embedded_test_server()->GetURL(kScopeUrl),
                       &num_resources);
  EXPECT_EQ(kExpectedNumResources, num_resources);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, GetRunningServiceWorkerInfos) {
  StartServerAndNavigateToSetup();
  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/fetch_event.js"),
            running_info.script_url);
  // `infos` is not populated synchronously so the worker may have started
  // before `kActivated`, but it will at least be `kActivating` at this point.
  EXPECT_TRUE(content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
                      kActivating == running_info.version_status ||
              content::ServiceWorkerRunningInfo::ServiceWorkerVersionStatus::
                      kActivated == running_info.version_status);
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
      running_info.render_process_id);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, StartWorkerWhileInstalling) {
  StartServerAndNavigateToSetup();
  const char kWorkerUrl[] = "/service_worker/while_true_in_install_worker.js";
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::INSTALLING);
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL(kWorkerUrl),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  public_context()->RegisterServiceWorker(
      embedded_test_server()->GetURL(kWorkerUrl), key, options,
      base::BindOnce(&ExpectRegisterResultAndRun,
                     blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
  observer.Wait();

  base::RunLoop run_loop;
  GURL full_url = embedded_test_server()->GetURL(kWorkerUrl);
  wrapper()->StartActiveServiceWorker(
      full_url,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(full_url)),
      base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
        EXPECT_EQ(status, blink::ServiceWorkerStatusCode::kErrorNotFound);
        run_loop.Quit();
      }));
  run_loop.Run();
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX)
// http://crbug.com/1347684
#define MAYBE_DispatchFetchEventToStoppedWorkerSynchronously \
  DISABLED_DispatchFetchEventToStoppedWorkerSynchronously
#else
#define MAYBE_DispatchFetchEventToStoppedWorkerSynchronously \
  DispatchFetchEventToStoppedWorkerSynchronously
#endif
// Make sure that a fetch event is dispatched to a stopped worker in the task
// which calls ServiceWorkerFetchDispatcher::Run().
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       MAYBE_DispatchFetchEventToStoppedWorkerSynchronously) {
  // Setup the server so that the test doesn't crash when tearing down.
  StartServerAndNavigateToSetup();

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  {
    base::RunLoop loop;
    version->StopWorker(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  }

  bool is_prepare_callback_called = false;
  base::RunLoop fetch_loop;
  blink::ServiceWorkerStatusCode fetch_status;
  ServiceWorkerFetchDispatcher::FetchEventResult fetch_result;
  blink::mojom::FetchAPIResponsePtr fetch_response;

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = embedded_test_server()->GetURL("/service_worker/in-scope");
  request->method = "GET";
  request->is_main_resource_load = true;
  auto dispatcher = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(request), network::mojom::RequestDestination::kDocument,
      /*client_id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*resulting_client_id=*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(), version,
      base::BindLambdaForTesting([&]() { is_prepare_callback_called = true; }),
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              ServiceWorkerFetchDispatcher::FetchEventResult result,
              blink::mojom::FetchAPIResponsePtr response,
              blink::mojom::ServiceWorkerStreamHandlePtr,
              blink::mojom::ServiceWorkerFetchEventTimingPtr,
              scoped_refptr<ServiceWorkerVersion>) {
            fetch_status = status;
            fetch_result = result;
            fetch_response = std::move(response);
            fetch_loop.Quit();
          }));

  // DispatchFetchEvent is called synchronously with dispatcher->Run() even if
  // the worker is stopped.
  dispatcher->Run();
  EXPECT_TRUE(is_prepare_callback_called);
  EXPECT_FALSE(fetch_response);

  // Check if the fetch event is handled by fetch_event.js correctly.
  fetch_loop.Run();
  ASSERT_TRUE(fetch_response);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, fetch_status);
  EXPECT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
            fetch_result);
  EXPECT_EQ(301, fetch_response->status_code);
}

// Check if a fetch event can be failed without crashing if starting a service
// worker fails. This is a regression test for https://crbug.com/1106977.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       DispatchFetchEventToBrokenWorker) {
  // Setup the server so that the test doesn't crash when tearing down.
  StartServerAndNavigateToSetup();

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event.js');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  {
    base::RunLoop loop;
    version->StopWorker(loop.QuitClosure());
    loop.Run();
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  }

  // Set a non-existent resource to the version.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources;
  resources.push_back(storage::mojom::ServiceWorkerResourceRecord::New(
      123456789, version->script_url(), 100, /*sha256_checksum=*/""));
  version->script_cache_map()->resource_map_.clear();
  version->script_cache_map()->SetResources(resources);

  bool is_prepare_callback_called = false;
  base::RunLoop fetch_loop;
  blink::ServiceWorkerStatusCode fetch_status;
  ServiceWorkerFetchDispatcher::FetchEventResult fetch_result;

  auto request = blink::mojom::FetchAPIRequest::New();
  request->url = embedded_test_server()->GetURL("/service_worker/in-scope");
  request->method = "GET";
  request->is_main_resource_load = true;
  auto dispatcher = std::make_unique<ServiceWorkerFetchDispatcher>(
      std::move(request), network::mojom::RequestDestination::kDocument,
      /*client_id=*/base::Uuid::GenerateRandomV4().AsLowercaseString(),
      /*resulting_client_id==*/
      base::Uuid::GenerateRandomV4().AsLowercaseString(), version,
      base::BindLambdaForTesting([&]() { is_prepare_callback_called = true; }),
      base::BindLambdaForTesting(
          [&](blink::ServiceWorkerStatusCode status,
              ServiceWorkerFetchDispatcher::FetchEventResult result,
              blink::mojom::FetchAPIResponsePtr response,
              blink::mojom::ServiceWorkerStreamHandlePtr,
              blink::mojom::ServiceWorkerFetchEventTimingPtr,
              scoped_refptr<ServiceWorkerVersion>) {
            fetch_status = status;
            fetch_result = result;
            fetch_loop.Quit();
          }));

  // DispatchFetchEvent is called synchronously with dispatcher->Run() even if
  // the worker is stopped.
  dispatcher->Run();
  EXPECT_TRUE(is_prepare_callback_called);

  // Check if the fetch event fails due to error of reading the resource.
  fetch_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorDiskCache, fetch_status);
  EXPECT_EQ(ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback,
            fetch_result);

  // Make sure that no crash happens in the remaining tasks.
  base::RunLoop().RunUntilIdle();
}

class UserAgentServiceWorkerBrowserTest : public ServiceWorkerBrowserTest {
 public:
  std::string GetExpectedUserAgent() const {
    return ShellContentBrowserClient::Get()->GetUserAgent();
  }

  void CheckUserAgentString(const std::string& user_agent_value) {
    // A regular expression that matches Chrome/{major_version}.{minor_version}
    // in the User-Agent string, where the {minor_version} is captured.
    static constexpr char kChromeVersionRegex[] =
        "Chrome/[0-9]+\\.([0-9]+\\.[0-9]+\\.[0-9]+)";
    // The minor version in the reduced UA string is always "0.0.0".
    static constexpr char kReducedMinorVersion[] = "0.0.0";

    std::string minor_version;
    EXPECT_TRUE(re2::RE2::PartialMatch(user_agent_value, kChromeVersionRegex,
                                       &minor_version));

    EXPECT_EQ(minor_version, kReducedMinorVersion);
  }
};

IN_PROC_BROWSER_TEST_F(UserAgentServiceWorkerBrowserTest, NavigatorUserAgent) {
  embedded_test_server()->StartAcceptingConnections();

  // The URL that was used to test user-agent.
  static constexpr char kOriginUrl[] = "https://127.0.0.1:44444";

  const GURL main_page_url(
      base::StrCat({kOriginUrl, "/create_service_worker.html"}));
  const GURL service_worker_url(base::StrCat({kOriginUrl, "/user_agent.js"}));

  std::map<GURL, int /* number_of_invocations */> expected_request_urls = {
      {main_page_url, 2}, {service_worker_url, 1}};

  base::RunLoop run_loop;
  URLLoaderInterceptor service_worker_loader(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it == expected_request_urls.end())
          return false;

        std::string path = "content/test/data/service_worker";
        path.append(std::string(params->url_request.url.path_piece()));

        std::string headers = "HTTP/1.1 200 OK\n";
        base::StrAppend(
            &headers,
            {"Content-Type: text/",
             base::EndsWith(params->url_request.url.path_piece(), ".js")
                 ? "javascript"
                 : "html",
             "\n"});

        URLLoaderInterceptor::WriteResponse(
            path, params->client.get(), &headers, std::optional<net::SSLInfo>(),
            params->url_request.url);

        if (--it->second == 0)
          expected_request_urls.erase(it);
        if (expected_request_urls.empty())
          run_loop.Quit();
        return true;
      }));

  // Navigate to the page that has the scripts for registering service workers.
  NavigateToURLBlockUntilNavigationsComplete(shell(), main_page_url, 1);
  // Register a service worker that responds to requests with
  // navigator.userAgent.
  EXPECT_EQ(
      "DONE",
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
             base::StrCat({"register('", service_worker_url.spec(), "');"})));
  // Reload the page so that the service worker handles the request.
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // Fetch the response containing the result of navigator.userAgent from the
  // service worker.
  const std::string navigator_user_agent =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
             "fetch('./user_agent_sw').then(response => response.text())")
          .ExtractString();

  EXPECT_EQ(GetExpectedUserAgent(), navigator_user_agent);
  CheckUserAgentString(navigator_user_agent);

  run_loop.Run();
}

// Regression test for https://crbug.com/1077916.
// Update the service worker by registering a worker with different script url.
// This test makes sure the worker can handle the fetch event using CacheStorage
// API.
// TODO(crbug.com/40695132): flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest,
                       DISABLED_UpdateOnScriptUrlChange) {
  StartServerAndNavigateToSetup();
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));

  // Register a service worker.
  EXPECT_EQ(
      "DONE",
      EvalJs(
          shell(),
          "registerWithoutAwaitingReady('fetch_event.js', './empty.html');"));
  {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_FALSE(infos.empty());
    const ServiceWorkerRunningInfo& running_info = infos.rbegin()->second;
    EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/fetch_event.js"),
              running_info.script_url);
  }

  // Update the service worker by changing the script url.
  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
  EXPECT_EQ("DONE", EvalJs(shell(),
                           "registerWithoutAwaitingReady('fetch_event_response_"
                           "via_cache.js', './empty.html');"));

  {
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_FALSE(infos.empty());
    const ServiceWorkerRunningInfo& running_info = infos.rbegin()->second;
    EXPECT_EQ(embedded_test_server()->GetURL(
                  "/service_worker/fetch_event_response_via_cache.js"),
              running_info.script_url);
  }
  observer.Wait();

  // Navigation should succeed.
  const std::u16string title = u"ServiceWorker test - empty page";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/empty.html")));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// TODO(crbug.com/40514526): ServiceWorkerNavigationPreloadTest should be
// converted to WPT.
class ServiceWorkerNavigationPreloadTest : public ServiceWorkerBrowserTest {
 public:
  using self = ServiceWorkerNavigationPreloadTest;

  ~ServiceWorkerNavigationPreloadTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ServiceWorkerBrowserTest::SetUpOnMainThread();
  }

 protected:
  static const std::string kNavigationPreloadHeaderName;
  static const std::string kEnableNavigationPreloadScript;
  static const std::string kPreloadResponseTestScript;

  static bool HasNavigationPreloadHeader(
      const net::test_server::HttpRequest& request) {
    return request.headers.find(kNavigationPreloadHeaderName) !=
           request.headers.end();
  }

  static std::string GetNavigationPreloadHeader(
      const net::test_server::HttpRequest& request) {
    DCHECK(HasNavigationPreloadHeader(request));
    return request.headers.find(kNavigationPreloadHeaderName)->second;
  }

  void SetupForNavigationPreloadTest(const GURL& scope,
                                     const GURL& worker_url) {
    WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);

    blink::mojom::ServiceWorkerRegistrationOptions options(
        scope, blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        worker_url, key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
    observer.Wait();
  }

  std::string LoadNavigationPreloadTestPage(const GURL& page_url,
                                            const GURL& worker_url,
                                            const char* expected_result) {
    RegisterMonitorRequestHandler();
    StartServerAndNavigateToSetup();
    SetupForNavigationPreloadTest(page_url, worker_url);

    const std::u16string title = u"PASS";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    title_watcher.AlsoWaitForTitle(u"ERROR");
    title_watcher.AlsoWaitForTitle(u"REJECTED");
    title_watcher.AlsoWaitForTitle(u"RESOLVED");
    EXPECT_TRUE(NavigateToURL(shell(), page_url));
    EXPECT_EQ(base::ASCIIToUTF16(expected_result),
              title_watcher.WaitAndGetTitle());
    return GetTextContent();
  }

  void RegisterMonitorRequestHandler() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ServiceWorkerNavigationPreloadTest::MonitorRequestHandler,
        base::Unretained(this)));
  }

  void RegisterStaticFile(const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ServiceWorkerNavigationPreloadTest::StaticRequestHandler,
        base::Unretained(this), relative_url, content, content_type));
  }

  void RegisterCustomResponse(const std::string& relative_url,
                              const net::HttpStatusCode code,
                              const std::optional<std::string>& reason,
                              const base::StringPairs& headers,
                              const std::string& content) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&self::CustomRequestHandler, base::Unretained(this),
                            relative_url, code, reason, headers, content));
  }

  void RegisterKeepSearchRedirect(const std::string& relative_url,
                                  const std::string& redirect_location) {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ServiceWorkerNavigationPreloadTest::KeepSearchRedirectHandler,
        base::Unretained(this), relative_url, redirect_location));
  }

  int GetRequestCount(const std::string& relative_url) const {
    const auto& it = request_log_.find(relative_url);
    if (it == request_log_.end())
      return 0;
    return it->second.size();
  }

  std::string GetTextContent() {
    base::RunLoop run_loop;
    std::string text_content;
    shell()->web_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"document.body.textContent;",
        base::BindOnce(&StoreString, &text_content, run_loop.QuitClosure()),
        ISOLATED_WORLD_ID_GLOBAL);
    run_loop.Run();
    return text_content;
  }

  std::map<std::string, std::vector<net::test_server::HttpRequest>>
      request_log_;

 private:
  class CustomResponse : public net::test_server::HttpResponse {
   public:
    explicit CustomResponse(const net::HttpStatusCode code,
                            const std::optional<std::string>& reason,
                            const base::StringPairs& headers,
                            const std::string& content)
        : code_(code), reason_(reason), headers_(headers), content_(content) {}

    CustomResponse(const CustomResponse&) = delete;
    CustomResponse& operator=(const CustomResponse&) = delete;

    ~CustomResponse() override {}

    void SendResponse(base::WeakPtr<net::test_server::HttpResponseDelegate>
                          delegate) override {
      delegate->SendHeadersContentAndFinish(
          code_, reason_.value_or(net::GetHttpReasonPhrase(code_)), headers_,
          content_);
    }

   private:
    net::HttpStatusCode code_;
    std::optional<std::string> reason_;
    base::StringPairs headers_;
    std::string content_;
  };

  std::unique_ptr<net::test_server::HttpResponse> StaticRequestHandler(
      const std::string& relative_url,
      const std::string& content,
      const std::string& content_type,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return nullptr;
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        std::make_unique<net::test_server::BasicHttpResponse>());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(content);
    http_response->set_content_type(content_type);
    return std::move(http_response);
  }

  std::unique_ptr<net::test_server::HttpResponse> CustomRequestHandler(
      const std::string& relative_url,
      const net::HttpStatusCode code,
      const std::optional<std::string>& reason,
      const base::StringPairs& headers,
      const std::string& content,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return nullptr;
    return std::make_unique<CustomResponse>(code, reason, headers, content);
  }

  std::unique_ptr<net::test_server::HttpResponse> KeepSearchRedirectHandler(
      const std::string& relative_url,
      const std::string& redirect_location,
      const net::test_server::HttpRequest& request) const {
    const size_t query_position = request.relative_url.find('?');
    if (request.relative_url.substr(0, query_position) != relative_url)
      return nullptr;
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse());
    response->set_code(net::HTTP_PERMANENT_REDIRECT);
    response->AddCustomHeader(
        "Location",
        query_position == std::string::npos
            ? redirect_location
            : redirect_location + request.relative_url.substr(query_position));
    return std::move(response);
  }

  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    request_log_[request.relative_url].push_back(request);
  }
};

const std::string
    ServiceWorkerNavigationPreloadTest::kNavigationPreloadHeaderName(
        "Service-Worker-Navigation-Preload");

const std::string
    ServiceWorkerNavigationPreloadTest::kEnableNavigationPreloadScript(
        "self.addEventListener('activate', event => {\n"
        "    event.waitUntil(self.registration.navigationPreload.enable());\n"
        "  });\n");

const std::string
    ServiceWorkerNavigationPreloadTest::kPreloadResponseTestScript =
        "var preload_resolve;\n"
        "var preload_promise = new Promise(r => { preload_resolve = r; });\n"
        "self.addEventListener('fetch', event => {\n"
        "    event.waitUntil(event.preloadResponse.then(\n"
        "        r => {\n"
        "          if (!r) {\n"
        "            preload_resolve(\n"
        "                {result: 'RESOLVED', \n"
        "                 info: 'Resolved with ' + r + '.'});\n"
        "            return;\n"
        "          }\n"
        "          var info = {};\n"
        "          info.type = r.type;\n"
        "          info.url = r.url;\n"
        "          info.status = r.status;\n"
        "          info.ok = r.ok;\n"
        "          info.statusText = r.statusText;\n"
        "          info.headers = [];\n"
        "          r.headers.forEach(\n"
        "              (v, n) => { info.headers.push([n,v]); });\n"
        "          preload_resolve({result: 'RESOLVED',\n"
        "                           info: JSON.stringify(info)}); },\n"
        "        e => { preload_resolve({result: 'REJECTED',\n"
        "                                info: e.toString()}); }));\n"
        "    event.respondWith(\n"
        "        new Response(\n"
        "            '<title>WAITING</title><script>\\n' +\n"
        "            'navigator.serviceWorker.onmessage = e => {\\n' +\n"
        "            '    var div = document.createElement(\\'div\\');\\n' +\n"
        "            '    div.appendChild(' +\n"
        "            '        document.createTextNode(e.data.info));\\n' +\n"
        "            '    document.body.appendChild(div);\\n' +\n"
        "            '    document.title = e.data.result;\\n' +\n"
        "            '  };\\n' +\n"
        "            'navigator.serviceWorker.controller.postMessage(\\n' +\n"
        "            '    null);\\n' +\n"
        "            '</script>',"
        "            {headers: [['content-type', 'text/html']]}));\n"
        "  });\n"
        "self.addEventListener('message', event => {\n"
        "    event.waitUntil(\n"
        "        preload_promise.then(\n"
        "            result => event.source.postMessage(result)));\n"
        "  });";

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NetworkFallback) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript = kEnableNavigationPreloadScript +
                              "self.addEventListener('fetch', event => {\n"
                              "    // Do nothing.\n"
                              "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request can be sent one, two, or three times.
  // - A navigation preload request may be sent. But it is possible that the
  //   navigation preload request is canceled before reaching the server.
  // - A fallback request must be sent since respondWith wasn't used.
  // - A second fallback request can be sent because the HttpCache may get
  //   confused when there are two concurrent requests (navigation preload and
  //   fallback) and one of them is cancelled (navigation preload). It restarts
  //   the ongoing request, possibly triggering another network request (see
  //   https://crbug.com/876911).
  const int request_count = GetRequestCount(kPageUrl);
  EXPECT_TRUE(request_count == 1 || request_count == 2 || request_count == 3)
      << request_count;

  // There should be at least one fallback request.
  int fallback_count = 0;
  const auto& requests = request_log_[kPageUrl];
  for (int i = 0; i < request_count; i++) {
    if (!HasNavigationPreloadHeader(requests[i]))
      fallback_count++;
  }
  EXPECT_GT(fallback_count, 0);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, SetHeaderValue) {
  const std::string kPageUrl = "/service_worker/navigation_preload.html";
  const std::string kWorkerUrl = "/service_worker/navigation_preload.js";
  const std::string kPage = "<title>FROM_SERVER</title>";
  const std::string kScript =
      "function createResponse(title, body) {\n"
      "  return new Response('<title>' + title + '</title>' + body,\n"
      "                      {headers: [['content-type', 'text/html']]})\n"
      "}\n"
      "self.addEventListener('fetch', event => {\n"
      "    if (event.request.url.indexOf('?enable') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.enable()\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('ENABLED', res)));\n"
      "    } else if (event.request.url.indexOf('?change') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.setHeaderValue('Hello')\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('CHANGED', res)));\n"
      "    } else if (event.request.url.indexOf('?disable') != -1) {\n"
      "      event.respondWith(\n"
      "          self.registration.navigationPreload.disable()\n"
      "            .then(_ => event.preloadResponse)\n"
      "            .then(res => createResponse('DISABLED', res)));\n"
      "    } else if (event.request.url.indexOf('?test') != -1) {\n"
      "      event.respondWith(event.preloadResponse.then(res =>\n"
      "          createResponse('TEST', res)));\n"
      "    }\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(page_url, worker_url);

  const std::string kPageUrl1 = kPageUrl + "?enable";
  const std::u16string title1 = u"ENABLED";
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  title_watcher1.AlsoWaitForTitle(u"FROM_SERVER");
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl1)));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  // When the navigation started, the navigation preload was not enabled yet.
  EXPECT_EQ("undefined", GetTextContent());
  ASSERT_EQ(0, GetRequestCount(kPageUrl1));

  const std::string kPageUrl2 = kPageUrl + "?change";
  const std::u16string title2 = u"CHANGED";
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  title_watcher2.AlsoWaitForTitle(u"FROM_SERVER");
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl2)));
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());
  // When the navigation started, the navigation preload was enabled, but the
  // header was not changed yet.
  EXPECT_EQ("[object Response]", GetTextContent());
  ASSERT_EQ(1, GetRequestCount(kPageUrl2));
  ASSERT_TRUE(HasNavigationPreloadHeader(request_log_[kPageUrl2][0]));
  EXPECT_EQ("true", GetNavigationPreloadHeader(request_log_[kPageUrl2][0]));

  const std::string kPageUrl3 = kPageUrl + "?disable";
  const std::u16string title3 = u"DISABLED";
  TitleWatcher title_watcher3(shell()->web_contents(), title3);
  title_watcher3.AlsoWaitForTitle(u"FROM_SERVER");
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl3)));
  EXPECT_EQ(title3, title_watcher3.WaitAndGetTitle());
  // When the navigation started, the navigation preload was not disabled yet.
  EXPECT_EQ("[object Response]", GetTextContent());
  ASSERT_EQ(1, GetRequestCount(kPageUrl3));
  ASSERT_TRUE(HasNavigationPreloadHeader(request_log_[kPageUrl3][0]));
  EXPECT_EQ("Hello", GetNavigationPreloadHeader(request_log_[kPageUrl3][0]));

  const std::string kPageUrl4 = kPageUrl + "?test";
  const std::u16string title4 = u"TEST";
  TitleWatcher title_watcher4(shell()->web_contents(), title4);
  title_watcher4.AlsoWaitForTitle(u"FROM_SERVER");
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl4)));
  EXPECT_EQ(title4, title_watcher4.WaitAndGetTitle());
  // When the navigation started, the navigation preload must be disabled.
  EXPECT_EQ("undefined", GetTextContent());
  ASSERT_EQ(0, GetRequestCount(kPageUrl4));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       RespondWithNavigationPreload) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    if (!event.preloadResponse) {\n"
      "      event.respondWith(\n"
      "          new Response('<title>ERROR</title>',"
      "                       {headers: [['content-type', 'text/html']]}));\n"
      "      return;\n"
      "    }\n"
      "    event.respondWith(event.preloadResponse);\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // the navigation preload response
  ASSERT_EQ(1, GetRequestCount(kPageUrl));
  EXPECT_EQ("true",
            request_log_[kPageUrl][0].headers[kNavigationPreloadHeaderName]);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, GetResponseText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>PASS</title>Hello world.";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ("Hello world.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // "Hello world".
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseCloneText) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.clone())\n"
      "          .then(response => response.text())\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       GetLargeResponseReadableStream) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  std::string title = "<title>PASS</title>";
  // A large body that exceeds the default size of a mojo::DataPipe.
  constexpr size_t kBodySize = 128 * 1024;
  // Randomly generate the body data
  int index = 0;
  std::string body;
  for (size_t i = 0; i < kBodySize; ++i) {
    body += static_cast<char>(index + 'a');
    index = (37 * index + 11) % 26;
  }
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "function drain(reader) {\n"
      "  var data = [];\n"
      "  var decoder = new TextDecoder();\n"
      "  function nextChunk(chunk) {\n"
      "    if (chunk.done)\n"
      "      return data.join('');\n"
      "    data.push(decoder.decode(chunk.value));\n"
      "    return reader.read().then(nextChunk);\n"
      "  }\n"
      "  return reader.read().then(nextChunk);\n"
      "}\n"
      "self.addEventListener('fetch', event => {\n"
      "    event.respondWith(\n"
      "        event.preloadResponse\n"
      "          .then(response => response.body.getReader())\n"
      "          .then(reader => drain(reader))\n"
      "          .then(text =>\n"
      "                  new Response(\n"
      "                      text,\n"
      "                      {headers: [['content-type', 'text/html']]})));\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, title + body, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  EXPECT_EQ(body, LoadNavigationPreloadTestPage(page_url, worker_url, "PASS"));

  // The page request must be sent only once, since the worker responded with
  // a synthetic Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NetworkError) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(page_url, worker_url);

  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  const std::u16string title = u"REJECTED";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  title_watcher.AlsoWaitForTitle(u"RESOLVED");
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(kNavigationPreloadNetworkError, GetTextContent());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       PreloadHeadersSimple) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  std::optional<base::Value> result = base::JSONReader::Read(
      LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  base::Value::Dict* dict = result->GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ("basic", GetString(*dict, "type"));
  EXPECT_EQ(page_url, GURL(GetString(*dict, "url")));
  EXPECT_EQ(200, GetInt(*dict, "status"));
  EXPECT_TRUE(GetBoolean(*dict, "ok"));
  EXPECT_EQ("OK", GetString(*dict, "statusText"));
  EXPECT_TRUE(CheckHeader(*dict, "content-type", "text/html"));
  EXPECT_TRUE(CheckHeader(*dict, "content-length",
                          base::NumberToString(sizeof(kPage) - 1)));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest, NotEnabled) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] = "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kPreloadResponseTestScript, "text/javascript");

  EXPECT_EQ("Resolved with undefined.",
            LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must not be sent, since the worker responded with a
  // generated Response and the navigation preload isn't enabled.
  EXPECT_EQ(0, GetRequestCount(kPageUrl));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       PreloadHeadersCustom) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const base::StringPairs kPageResponseHeaders = {
      {"Connection", "close"},        {"Content-Length", "32"},
      {"Content-Type", "text/html"},  {"Custom-Header", "pen pineapple"},
      {"Custom-Header", "apple pen"}, {"Set-Cookie", "COOKIE1"},
      {"Set-Cookie2", "COOKIE2"},
  };
  const char kPageResonseContent[] = "<title>ERROR</title>Hello world.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, net::HTTP_CREATED, "HELLOWORLD",
                         kPageResponseHeaders, kPageResonseContent);
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  std::optional<base::Value> result = base::JSONReader::Read(
      LoadNavigationPreloadTestPage(page_url, worker_url, "RESOLVED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  base::Value::Dict* dict = result->GetIfDict();
  ASSERT_TRUE(dict);
  EXPECT_EQ("basic", GetString(*dict, "type"));
  EXPECT_EQ(page_url, GURL(GetString(*dict, "url")));
  EXPECT_EQ(201, GetInt(*dict, "status"));
  EXPECT_TRUE(GetBoolean(*dict, "ok"));
  EXPECT_EQ("HELLOWORLD", GetString(*dict, "statusText"));
  EXPECT_TRUE(CheckHeader(*dict, "content-type", "text/html"));
  EXPECT_TRUE(CheckHeader(*dict, "content-length", "32"));
  EXPECT_TRUE(CheckHeader(*dict, "custom-header", "pen pineapple, apple pen"));
  // The forbidden response headers (Set-Cookie, Set-Cookie2) must be removed.
  EXPECT_FALSE(HasHeader(*dict, "set-cookie"));
  EXPECT_FALSE(HasHeader(*dict, "set-cookie2"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       InvalidRedirect_MultiLocation) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kRedirectedPageUrl1[] =
      "/service_worker/navigation_preload_redirected1.html";
  const char kRedirectedPageUrl2[] =
      "/service_worker/navigation_preload_redirected2.html";
  const base::StringPairs kPageResponseHeaders = {
      // "HTTP/1.1 302 Found\r\n"
      {"Connection", "close"},
      {"Location", "/service_worker/navigation_preload_redirected1.html"},
      {"Location", "/service_worker/navigation_preload_redirected2.html"},
  };
  const char kRedirectedPage[] = "<title>ERROR</title>Redirected page.";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, net::HTTP_FOUND, "FOUND",
                         kPageResponseHeaders, "");
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");
  RegisterStaticFile(kRedirectedPageUrl1, kRedirectedPage, "text/html");

  // According to the spec, multiple Location headers is not an error. So the
  // preloadResponse must be resolved with an opaque redirect response.
  // But Chrome treats multiple Location headers as an error (crbug.com/98895).
  EXPECT_EQ(kNavigationPreloadNetworkError,
            LoadNavigationPreloadTestPage(page_url, worker_url, "REJECTED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
  // The redirected request must not be sent.
  EXPECT_EQ(0, GetRequestCount(kRedirectedPageUrl1));
  EXPECT_EQ(0, GetRequestCount(kRedirectedPageUrl2));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       InvalidRedirect_InvalidLocation) {
  const char kPageUrl[] = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const base::StringPairs kPageResponseHeaders = {
      // "HTTP/1.1 302 Found\r\n"
      {"Connection", "close"},
      {"Location", "http://"},
  };
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterCustomResponse(kPageUrl, net::HTTP_FOUND, "FOUND",
                         kPageResponseHeaders, "");
  RegisterStaticFile(
      kWorkerUrl, kEnableNavigationPreloadScript + kPreloadResponseTestScript,
      "text/javascript");

  // TODO(horo): According to the spec, even if the location URL is invalid, the
  // preloadResponse must be resolve with an opaque redirect response. But
  // currently Chrome handles the invalid location URL in the browser process as
  // an error. crbug.com/707185
  EXPECT_EQ(kNavigationPreloadNetworkError,
            LoadNavigationPreloadTestPage(page_url, worker_url, "REJECTED"));

  // The page request must be sent only once, since the worker responded with
  // a generated Response.
  EXPECT_EQ(1, GetRequestCount(kPageUrl));
}

// Tests responding with the navigation preload response when the navigation
// occurred after a redirect.
IN_PROC_BROWSER_TEST_F(ServiceWorkerNavigationPreloadTest,
                       RedirectAndRespondWithNavigationPreload) {
  const std::string kPageUrl = "/service_worker/navigation_preload.html";
  const char kWorkerUrl[] = "/service_worker/navigation_preload.js";
  const char kPage[] =
      "<title></title>\n"
      "<script>document.title = document.location.search;</script>";
  const std::string kScript =
      kEnableNavigationPreloadScript +
      "self.addEventListener('fetch', event => {\n"
      "    if (event.request.url.indexOf('navigation_preload.html') == -1)\n"
      "      return; // For in scope redirection.\n"
      "    event.respondWith(event.preloadResponse);\n"
      "  });";
  const GURL page_url = embedded_test_server()->GetURL(kPageUrl);
  const GURL worker_url = embedded_test_server()->GetURL(kWorkerUrl);
  RegisterStaticFile(kPageUrl, kPage, "text/html");
  RegisterStaticFile(kWorkerUrl, kScript, "text/javascript");

  // Register redirects to the target URL. The service worker responds to the
  // target URL with the navigation preload response.
  const char kRedirectPageUrl[] = "/redirect";
  const char kInScopeRedirectPageUrl[] = "/service_worker/redirect";
  RegisterKeepSearchRedirect(kRedirectPageUrl, page_url.spec());
  RegisterKeepSearchRedirect(kInScopeRedirectPageUrl, page_url.spec());

  RegisterMonitorRequestHandler();
  StartServerAndNavigateToSetup();
  SetupForNavigationPreloadTest(
      embedded_test_server()->GetURL("/service_worker/"), worker_url);

  const GURL redirect_page_url =
      embedded_test_server()->GetURL(kRedirectPageUrl).Resolve("?1");
  const GURL in_scope_redirect_page_url =
      embedded_test_server()->GetURL(kInScopeRedirectPageUrl).Resolve("?2");
  const GURL cross_origin_redirect_page_url =
      embedded_test_server()->GetURL("a.com", kRedirectPageUrl).Resolve("?3");

  // Navigate to a same-origin, out of scope URL that redirects to the target
  // URL. The navigation preload request should be the single request to the
  // target URL.
  const std::u16string title1 = u"?1";
  TitleWatcher title_watcher1(shell()->web_contents(), title1);
  GURL expected_commit_url1(embedded_test_server()->GetURL(kPageUrl + "?1"));
  EXPECT_TRUE(NavigateToURL(shell(), redirect_page_url, expected_commit_url1));
  EXPECT_EQ(title1, title_watcher1.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?1"));

  // Navigate to a same-origin, in-scope URL that redirects to the target URL.
  // The navigation preload request should be the single request to the target
  // URL.
  const std::u16string title2 = u"?2";
  TitleWatcher title_watcher2(shell()->web_contents(), title2);
  GURL expected_commit_url2(embedded_test_server()->GetURL(kPageUrl + "?2"));
  EXPECT_TRUE(
      NavigateToURL(shell(), in_scope_redirect_page_url, expected_commit_url2));
  EXPECT_EQ(title2, title_watcher2.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?2"));

  // Navigate to a cross-origin URL that redirects to the target URL. The
  // navigation preload request should be the single request to the target URL.
  const std::u16string title3 = u"?3";
  TitleWatcher title_watcher3(shell()->web_contents(), title3);
  GURL expected_commit_url3(embedded_test_server()->GetURL(kPageUrl + "?3"));
  EXPECT_TRUE(NavigateToURL(shell(), cross_origin_redirect_page_url,
                            expected_commit_url3));
  EXPECT_EQ(title3, title_watcher3.WaitAndGetTitle());
  EXPECT_EQ(1, GetRequestCount(kPageUrl + "?3"));
}

static int CountRenderProcessHosts() {
  return RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, Registration) {
  StartServerAndNavigateToSetup();
  // Close the only window to be sure we're not re-using its RenderProcessHost.
  shell()->Close();
  EXPECT_EQ(0, CountRenderProcessHosts());

  const char kWorkerUrl[] = "/service_worker/fetch_event.js";
  const char kScope[] = "/service_worker/";

  // Unregistering nothing should return false.
  {
    base::RunLoop run_loop;
    GURL url = embedded_test_server()->GetURL("/");
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
    public_context()->UnregisterServiceWorker(
        embedded_test_server()->GetURL("/"), key,
        base::BindOnce(&ExpectUnregisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kErrorNotFound,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // If we use a worker URL that doesn't exist, registration fails.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL("/does/not/exist"), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kErrorNetwork,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(0, CountRenderProcessHosts());

  // Register returns when the promise would be resolved.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kWorkerUrl), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(1, CountRenderProcessHosts());

  // Registering again should succeed, although the algo still
  // might not be complete.
  {
    base::RunLoop run_loop;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kScope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(kWorkerUrl), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  // The registration algo might not be far enough along to have
  // stored the registration data, so it may not be findable
  // at this point.

  // Unregistering something should return true.
  {
    base::RunLoop run_loop;
    GURL url = embedded_test_server()->GetURL(kScope);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
    public_context()->UnregisterServiceWorker(
        url, key,
        base::BindOnce(&ExpectUnregisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk,
                       run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_GE(1, CountRenderProcessHosts()) << "Unregistering doesn't stop the "
                                             "workers eagerly, so their RPHs "
                                             "can still be running.";

  // Should not be able to find it.
  EXPECT_EQ(FindRegistration(
                embedded_test_server()->GetURL("/service_worker/empty.html")),
            blink::ServiceWorkerStatusCode::kErrorNotFound);
}

enum class ServiceWorkerScriptImportType { kImportScripts, kStaticImport };

struct ServiceWorkerScriptChecksumInfo {
  GURL script_url;
  std::string sha256_checksum;
  std::string updated_sha256_checksum;
};

class ServiceWorkerSha256ScriptChecksumBrowserTest
    : public ServiceWorkerBrowserTest,
      public testing::WithParamInterface<
          std::tuple<ServiceWorkerScriptImportType, bool, bool>> {
 public:
  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    // Set a custom request handler for Sha256ScriptChecksum test.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&ServiceWorkerSha256ScriptChecksumBrowserTest::
                                HandleRequestForSha256ScriptChecksumTest,
                            base::Unretained(this)));
  }

  ServiceWorkerScriptChecksumInfo GetMainScript() {
    switch (ScriptImportType()) {
      case ServiceWorkerScriptImportType::kImportScripts:
        return ServiceWorkerScriptChecksumInfo{
            /*script_url=*/embedded_test_server()->GetURL(
                "/service_worker/import-scripts.js"),
            /*sha256_checksum=*/
            "959CEA1003BF6A06F745F232016C305D1916F90F3266D2BBA708904BB2008A4E",
            /*updated_sha256_checksum=*/
            "2AF06211016CC22679C87ADAEDB463CC5FCEA22FC045D30529A9246C52CB1647"};
      case ServiceWorkerScriptImportType::kStaticImport:
        return ServiceWorkerScriptChecksumInfo{
            /*script_url=*/embedded_test_server()->GetURL(
                "/service_worker/static-import.js"),
            /*sha256_checksum=*/
            "8CB1783CB9FB030FA8DB2F3C6B59728146F8AEF6CBF1754DB5CA1B48B482ACF6",
            /*updated_sha256_checksum=*/
            "FDEB1854E767FFCDC5F07BCBCAAEB4A6C36F136A9EAB9DC71E087FECA76AE267"};
    }
  }

  ServiceWorkerScriptChecksumInfo GetImportedScript() {
    switch (ScriptImportType()) {
      case ServiceWorkerScriptImportType::kImportScripts:
        return ServiceWorkerScriptChecksumInfo{
            /*script_url=*/embedded_test_server()->GetURL(
                "/service_worker/imported-by-import-scripts.js"),
            /*sha256_checksum=*/
            "4DE6400BEABB272D7FB0180E59F997808056978FEF5CD5B1A74D6ED83B43136C",
            /*updated_sha256_checksum=*/
            "F1630A5236D9DF2243943398C209CE00A0FB75360AEF463E798742B7F73FA17B"};
      case ServiceWorkerScriptImportType::kStaticImport:
        return ServiceWorkerScriptChecksumInfo{
            /*script_url=*/embedded_test_server()->GetURL(
                "/service_worker/imported-by-static-import.js"),
            /*sha256_checksum=*/
            "CD6B3DE93DD4BB48243940705156EB3938A737BE494C59F1904EF19D06A06AC3",
            /*updated_sha256_checksum=*/
            "87488A632AA5065D1AB196EDE8681090BB4F9F8727075E6FE464F7ED63E2561B"};
    }
  }

  std::string GetExpectedAggregatedSha256ScriptChecksum(
      bool before_script_update) {
    switch (ScriptImportType()) {
      case ServiceWorkerScriptImportType::kImportScripts:
        if (before_script_update) {
          return "B80961F1D38367CA45F57571740EA2ED4C0972BFEF1CF3A01A2B3BD93CED6"
                 "C98";
        }
        if (IsMainScriptChanged() && IsImportedScriptChanged()) {
          return "8555E49C806BD38482D2A7FDC85735D6AE5C371BBE5A7260193F85499A393"
                 "064";
        } else if (IsMainScriptChanged() && !IsImportedScriptChanged()) {
          return "0B406A99B993938713E07AAE4AF9C2C3BB8837819C4D972097C6291C19355"
                 "B44";
        } else if (!IsMainScriptChanged() && IsImportedScriptChanged()) {
          return "03DCAF85CA3E2B73158B9C43FAC7086BAA6AE9B83B503E389F4323660F58D"
                 "D09";
        }
        NOTREACHED_IN_MIGRATION();
        return "";
      case ServiceWorkerScriptImportType::kStaticImport:
        if (before_script_update) {
          return "0ACE52F5894454C80C36A4C6D49F0B33DC177A69AE79F785C008FE63BDECB"
                 "385";
        }
        if (IsMainScriptChanged() && IsImportedScriptChanged()) {
          return "A929D91AD2BEBCE571ECDE1E5C2289A06F4C603A9BB8CD04720A8120012BE"
                 "331";
        } else if (IsMainScriptChanged() && !IsImportedScriptChanged()) {
          return "D58555C45E466DD977C8D9F24D633D629DBE35BE79A0B5BBF3BA5D3D50D48"
                 "BD3";
        } else if (!IsMainScriptChanged() && IsImportedScriptChanged()) {
          return "3E6C3E5F3C40F87B69D1913FD7201760BD3C8824026837D4BFB4828811F60"
                 "3D7";
        }
        NOTREACHED_IN_MIGRATION();
        return "";
    }
  }

  ServiceWorkerScriptImportType ScriptImportType() {
    return std::get<0>(GetParam());
  }
  bool IsMainScriptChanged() { return std::get<1>(GetParam()); }
  bool IsImportedScriptChanged() { return std::get<2>(GetParam()); }

 private:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleRequestForSha256ScriptChecksumTest(
      const net::test_server::HttpRequest& request) {
    const GURL absolute_url =
        embedded_test_server()->GetURL(request.relative_url);

    std::string updated_content;

    if (absolute_url == GetMainScript().script_url) {
      switch (ScriptImportType()) {
        case ServiceWorkerScriptImportType::kImportScripts:
          updated_content = "importScripts('imported-by-import-scripts.js');";
          break;
        case ServiceWorkerScriptImportType::kStaticImport:
          updated_content =
              "import * as module from './imported-by-static-import.js';";
          break;
      }
      // Add a counter to the response content that is different every request
      // to the script so that a service worker will detect it as a script
      // update, and for the check if the sha256 checksum is updated or not.
      // Increment the counter only when we should update the script.
      updated_content +=
          " var counter = " +
          base::NumberToString(request_counter_for_main_script_) + ";";
      if (IsMainScriptChanged()) {
        request_counter_for_main_script_++;
      }
    }
    if (absolute_url == GetImportedScript().script_url) {
      switch (ScriptImportType()) {
        case ServiceWorkerScriptImportType::kImportScripts:
          updated_content = "var imported_by_import_scripts;";
          break;
        case ServiceWorkerScriptImportType::kStaticImport:
          updated_content = "var imported_by_static_import;";
          break;
      }
      updated_content +=
          "var counter = " +
          base::NumberToString(request_counter_for_imported_script_) + ";";
      if (IsImportedScriptChanged()) {
        request_counter_for_imported_script_++;
      }
    }

    if (updated_content.empty()) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(updated_content);
    http_response->set_content_type("text/javascript");
    http_response->AddCustomHeader("Service-Worker-Allowed", "/");

    return http_response;
  }

  int64_t request_counter_for_main_script_ = 0;
  int64_t request_counter_for_imported_script_ = 0;
};

IN_PROC_BROWSER_TEST_P(ServiceWorkerSha256ScriptChecksumBrowserTest,
                       Sha256ScriptChecksum) {
  StartServerAndNavigateToSetup();

  const ServiceWorkerScriptChecksumInfo main_script = GetMainScript();
  const ServiceWorkerScriptChecksumInfo imported_script = GetImportedScript();

  // Start the ServiceWorker.
  WorkerRunningStatusObserver observer1(public_context());
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));

  std::string js_script;
  switch (ScriptImportType()) {
    case ServiceWorkerScriptImportType::kImportScripts:
      js_script = "register('" + main_script.script_url.spec() + "')";
      break;
    case ServiceWorkerScriptImportType::kStaticImport:
      js_script =
          "register('" + main_script.script_url.spec() + "', null, 'module')";
      break;
  }
  EXPECT_EQ("DONE",
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), js_script));

  observer1.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(version->script_url(), main_script.script_url);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Validate checksums for each script, and ServiceWorkerVersion's one.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> resources =
      version->script_cache_map()->GetResources();
  std::set<std::string> expected_checksums{main_script.sha256_checksum,
                                           imported_script.sha256_checksum};
  EXPECT_EQ(expected_checksums.size(), resources.size());
  for (auto& resource : resources) {
    EXPECT_TRUE(expected_checksums.find(resource->sha256_checksum.value()) !=
                expected_checksums.end());
  }
  EXPECT_EQ(
      GetExpectedAggregatedSha256ScriptChecksum(/*before_script_update=*/true),
      version->sha256_script_checksum());

  // Update the ServiceWorker. This test is only needed for the when either main
  // or imported script has changes.
  if (!IsMainScriptChanged() && !IsImportedScriptChanged()) {
    return;
  }

  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ("DONE",
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), js_script));
  WorkerRunningStatusObserver observer2(public_context());
  const GURL scope(embedded_test_server()->GetURL("/service_worker"));
  wrapper()->SkipWaitingWorker(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)));
  observer2.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> updated_version =
      wrapper()->GetLiveVersion(observer2.version_id());
  EXPECT_EQ(updated_version->script_url(), main_script.script_url);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
            updated_version->running_status());

  // Validate updated checksums for each script, and ServiceWorkerVersion's one.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
      updated_resources = updated_version->script_cache_map()->GetResources();

  std::set<std::string> updated_expected_checksums{
      IsMainScriptChanged() ? main_script.updated_sha256_checksum
                            : main_script.sha256_checksum,
      IsImportedScriptChanged() ? imported_script.updated_sha256_checksum
                                : imported_script.sha256_checksum};
  EXPECT_EQ(updated_expected_checksums.size(), updated_resources.size());
  for (auto& resource : updated_resources) {
    EXPECT_TRUE(
        updated_expected_checksums.find(resource->sha256_checksum.value()) !=
        updated_expected_checksums.end());
  }
  EXPECT_EQ(
      GetExpectedAggregatedSha256ScriptChecksum(/*before_script_update=*/false),
      updated_version->sha256_script_checksum());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerSha256ScriptChecksumBrowserTest,
    testing::Combine(
        testing::Values(ServiceWorkerScriptImportType::kImportScripts,
                        ServiceWorkerScriptImportType::kStaticImport),
        testing::Bool(),
        testing::Bool()));

class CacheStorageSideDataSizeChecker
    : public base::RefCounted<CacheStorageSideDataSizeChecker> {
 public:
  static int GetSize(storage::mojom::CacheStorageControl* cache_storage_control,
                     const GURL& origin,
                     const std::string& cache_name,
                     const GURL& url) {
    mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage_remote;
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
    network::DocumentIsolationPolicy document_isolation_policy;
    cache_storage_control->AddReceiver(
        cross_origin_embedder_policy, mojo::NullRemote(),
        document_isolation_policy,
        storage::BucketLocator::ForDefaultBucket(
            blink::StorageKey::CreateFirstParty(url::Origin::Create(origin))),
        storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_remote.InitWithNewPipeAndPassReceiver());

    auto checker = base::MakeRefCounted<CacheStorageSideDataSizeChecker>(
        std::move(cache_storage_remote), cache_name, url);
    return checker->GetSizeImpl();
  }

  CacheStorageSideDataSizeChecker(
      mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
      const std::string& cache_name,
      const GURL& url)
      : cache_storage_(std::move(cache_storage)),
        cache_name_(cache_name),
        url_(url) {}

  CacheStorageSideDataSizeChecker(const CacheStorageSideDataSizeChecker&) =
      delete;
  CacheStorageSideDataSizeChecker& operator=(
      const CacheStorageSideDataSizeChecker&) = delete;

 private:
  friend class base::RefCounted<CacheStorageSideDataSizeChecker>;

  virtual ~CacheStorageSideDataSizeChecker() = default;

  int GetSizeImpl() {
    int result = 0;
    base::RunLoop loop;
    cache_storage_->Open(
        base::UTF8ToUTF16(cache_name_), /*trace_id=*/0,
        base::BindOnce(
            &CacheStorageSideDataSizeChecker::OnCacheStorageOpenCallback, this,
            &result, loop.QuitClosure()));
    loop.Run();
    return result;
  }

  void OnCacheStorageOpenCallback(int* result,
                                  base::OnceClosure continuation,
                                  blink::mojom::OpenResultPtr open_result) {
    ASSERT_TRUE(open_result->is_cache());

    auto scoped_request = blink::mojom::FetchAPIRequest::New();
    scoped_request->url = url_;

    // Preserve lifetime of this remote across the Match call.
    cache_storage_cache_.emplace(std::move(open_result->get_cache()));

    (*cache_storage_cache_)
        ->Match(std::move(scoped_request),
                blink::mojom::CacheQueryOptions::New(),
                /*in_related_fetch_event=*/false,
                /*in_range_fetch_event=*/false, /*trace_id=*/0,
                base::BindOnce(&CacheStorageSideDataSizeChecker::
                                   OnCacheStorageCacheMatchCallback,
                               this, result, std::move(continuation)));
  }

  void OnCacheStorageCacheMatchCallback(
      int* result,
      base::OnceClosure continuation,
      blink::mojom::MatchResultPtr match_result) {
    if (match_result->is_status()) {
      ASSERT_EQ(match_result->get_status(), CacheStorageError::kErrorNotFound);
      *result = 0;
      std::move(continuation).Run();
      return;
    }
    ASSERT_TRUE(match_result->is_response());

    auto& response = match_result->get_response();
    ASSERT_TRUE(response->side_data_blob);

    auto blob_handle = base::MakeRefCounted<storage::BlobHandle>(
        std::move(response->side_data_blob->blob));
    blob_handle->get()->ReadSideData(base::BindOnce(
        [](scoped_refptr<storage::BlobHandle> blob_handle, int* result,
           base::OnceClosure continuation,
           const std::optional<mojo_base::BigBuffer> data) {
          *result = data ? data->size() : 0;
          std::move(continuation).Run();
        },
        blob_handle, result, std::move(continuation)));
  }

  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  std::optional<mojo::AssociatedRemote<blink::mojom::CacheStorageCache>>
      cache_storage_cache_;
  const std::string cache_name_;
  const GURL url_;
};

class ServiceWorkerV8CodeCacheForCacheStorageTest
    : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerV8CodeCacheForCacheStorageTest() = default;

  ServiceWorkerV8CodeCacheForCacheStorageTest(
      const ServiceWorkerV8CodeCacheForCacheStorageTest&) = delete;
  ServiceWorkerV8CodeCacheForCacheStorageTest& operator=(
      const ServiceWorkerV8CodeCacheForCacheStorageTest&) = delete;

  ~ServiceWorkerV8CodeCacheForCacheStorageTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

 protected:
  virtual std::string GetWorkerURL() { return kWorkerUrl; }

  void RegisterAndActivateServiceWorker() {
    WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(kPageUrl),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(GetWorkerURL()), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
    observer.Wait();
  }

  void NavigateToTestPageWithoutWaiting() {
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL(kPageUrl)));
  }

  void NavigateToTestPage() {
    const std::u16string title = u"Title was changed by the script.";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToTestPageWithoutWaiting();
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  void WaitUntilSideDataSizeIs(int expected_size) {
    while (true) {
      if (GetSideDataSize() == expected_size)
        return;
    }
  }

  void WaitUntilSideDataSizeIsBiggerThan(int minimum_size) {
    while (true) {
      if (GetSideDataSize() > minimum_size)
        return;
    }
  }

 private:
  static const char kPageUrl[];
  static const char kWorkerUrl[];
  static const char kScriptUrl[];

  int GetSideDataSize() {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    return CacheStorageSideDataSizeChecker::GetSize(
        partition->GetCacheStorageControl(), embedded_test_server()->base_url(),
        std::string("cache_name"), embedded_test_server()->GetURL(kScriptUrl));
  }
};

const char ServiceWorkerV8CodeCacheForCacheStorageTest::kPageUrl[] =
    "/service_worker/v8_cache_test.html";
const char ServiceWorkerV8CodeCacheForCacheStorageTest::kWorkerUrl[] =
    "/service_worker/fetch_event_response_via_cache.js";
const char ServiceWorkerV8CodeCacheForCacheStorageTest::kScriptUrl[] =
    "/service_worker/v8_cache_test.js";

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CodeCacheForCacheStorageTest,
                       V8CacheOnCacheStorage) {
  RegisterAndActivateServiceWorker();

  // First load: fetch_event_response_via_cache.js returns |cloned_response|.
  // The V8 code cache should not be stored in CacheStorage.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);

  // Second load: The V8 code cache should be stored in CacheStorage. It must
  // have size greater than 16 bytes.
  NavigateToTestPage();
  WaitUntilSideDataSizeIsBiggerThan(kV8CacheTimeStampDataSize);
}

class ServiceWorkerV8CodeCacheForCacheStorageNoneTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerV8CodeCacheForCacheStorageNoneTest() = default;

  ServiceWorkerV8CodeCacheForCacheStorageNoneTest(
      const ServiceWorkerV8CodeCacheForCacheStorageNoneTest&) = delete;
  ServiceWorkerV8CodeCacheForCacheStorageNoneTest& operator=(
      const ServiceWorkerV8CodeCacheForCacheStorageNoneTest&) = delete;

  ~ServiceWorkerV8CodeCacheForCacheStorageNoneTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kV8CacheOptions, "none");
  }
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerV8CodeCacheForCacheStorageNoneTest,
                       V8CacheOnCacheStorage) {
  RegisterAndActivateServiceWorker();

  // First load.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);

  // Second load: The V8 code cache must not be stored even after the second
  // load when --v8-cache-options=none is set.
  NavigateToTestPage();
  WaitUntilSideDataSizeIs(0);
}

namespace {

class CacheStorageControlForBadOrigin
    : public storage::mojom::CacheStorageControl {
 public:
  void AddReceiver(
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter_remote,
      const network::DocumentIsolationPolicy& document_isolation_policy,
      const storage::BucketLocator& bucket,
      storage::mojom::CacheStorageOwner owner,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override {
    // The CodeCacheHostImpl should not try to add a receiver if the StorageKey
    // is bad.
    NOTREACHED_IN_MIGRATION();
  }
  void AddObserver(mojo::PendingRemote<storage::mojom::CacheStorageObserver>
                       observer) override {
    NOTREACHED_IN_MIGRATION();
  }
  void ApplyPolicyUpdates(std::vector<storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override {
    NOTREACHED_IN_MIGRATION();
  }
};

}  // namespace

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  std::string GetWorkerURL() override {
    return "/service_worker/install_event_caches_script.js";
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventTest,
    FullCodeCacheGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should have been generated when the script was
  // stored in the install event.
  WaitUntilSideDataSizeIsBiggerThan(kV8CacheTimeStampDataSize);
}

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "CacheStorageCodeCacheHint");
  }

  std::string GetWorkerURL() override {
    return "/service_worker/install_event_caches_script_with_hint.js";
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventDisabledByHintTest,
    FullCodeCacheNotGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should not be generated when the script was
  // stored in the install event and the header hint disables code cache.
  WaitUntilSideDataSizeIs(0);
}

class ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest
    : public ServiceWorkerV8CodeCacheForCacheStorageTest {
 public:
  ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ServiceWorkerV8CodeCacheForCacheStorageTest::SetUpOnMainThread();
  }

  std::string GetWorkerURL() override {
    GURL cross_origin_script = embedded_test_server()->GetURL(
        "bar.com", "/service_worker/v8_cache_test.js");
    return "/service_worker/"
           "install_event_caches_no_cors_script.js?script_url=" +
           cross_origin_script.spec();
  }
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerCacheStorageFullCodeCacheFromInstallEventOpaqueResponseTest,
    FullCodeCacheGenerated) {
  RegisterAndActivateServiceWorker();
  // The full code cache should not be generated when the script is an opaque
  // response.
  WaitUntilSideDataSizeIs(0);
}

// ServiceWorkerDisableWebSecurityTests check the behavior when the web security
// is disabled. If '--disable-web-security' flag is set, we don't check the
// origin equality in Blink. So the Service Worker related APIs should succeed
// even if it is thouching other origin Service Workers.
class ServiceWorkerDisableWebSecurityTest : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerDisableWebSecurityTest() = default;

  ServiceWorkerDisableWebSecurityTest(
      const ServiceWorkerDisableWebSecurityTest&) = delete;
  ServiceWorkerDisableWebSecurityTest& operator=(
      const ServiceWorkerDisableWebSecurityTest&) = delete;

  ~ServiceWorkerDisableWebSecurityTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  void SetUpOnMainThread() override {
    cross_origin_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(cross_origin_server_.Start());
    ServiceWorkerBrowserTest::SetUpOnMainThread();
  }

  void RegisterServiceWorkerOnCrossOriginServer(const std::string& scope,
                                                const std::string& script) {
    WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);
    blink::mojom::ServiceWorkerRegistrationOptions options(
        cross_origin_server_.GetURL(scope), blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        cross_origin_server_.GetURL(script), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
    observer.Wait();
  }

  void RunTestWithCrossOriginURL(const std::string& test_page,
                                 const std::string& cross_origin_url) {
    const std::u16string title = u"PASS";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     test_page + "?" +
                     cross_origin_server_.GetURL(cross_origin_url).spec())));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  // Similar to `RunTestWithCrossOriginURL`, but it does not assume that the
  // child and parent frame are same-process. It is assumed that any
  // communication between child/parent will be done via postMessage.
  // `test_script` carries the service worker operations to test. This function
  // assumes that `test_script` postMessages "PASS" to the parent if the
  // test succeeds.
  void RunTestWithCrossOriginURL_MaybeCrossProcess(
      const std::string& test_page,
      const std::string& cross_origin_url,
      const std::string& test_script) {
    //  Run test with cross-origin URL.
    {
      const std::u16string title = u"LOADED";
      TitleWatcher title_watcher(shell()->web_contents(), title);
      EXPECT_TRUE(NavigateToURL(
          shell(), embedded_test_server()->GetURL(
                       test_page + "?" +
                       cross_origin_server_.GetURL(cross_origin_url).spec())));
      EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
    }

    // Verify the parent and child frames are cross-origin.
    auto* parent_frame = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame());
    ASSERT_EQ(1u, parent_frame->child_count());
    RenderFrameHostImpl* child_frame =
        parent_frame->child_at(0)->current_frame_host();
    // This comparison needs to be done here, as the parent doesn't have access
    // to the cross-origin child's location` object.
    EXPECT_NE(EvalJs(parent_frame, "location.origin").ExtractString(),
              EvalJs(child_frame, "location.origin").ExtractString());

    // Verify `test_script` on service worker.
    {
      const std::u16string title = u"PASS";
      TitleWatcher title_watcher(shell()->web_contents(), title);
      EXPECT_TRUE(ExecJs(parent_frame,
                         "onmessage = msg => { document.title = msg.data; };"));
      EXPECT_TRUE(ExecJs(child_frame, test_script));
      EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
    }
  }

 private:
  net::EmbeddedTestServer cross_origin_server_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest,
                       GetRegistrationNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] =
      "/service_worker/disable_web_security_get_registration.html";
  const char kScopeUrl[] = "/service_worker/";
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android, http://crbug.com/1141870.
#define MAYBE_RegisterNoCrash DISABLED_RegisterNoCrash
#else
#define MAYBE_RegisterNoCrash RegisterNoCrash
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest,
                       MAYBE_RegisterNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] = "/service_worker/disable_web_security_register.html";
  const char kScopeUrl[] = "/service_worker/";
  RunTestWithCrossOriginURL(kPageUrl, kScopeUrl);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest, UnregisterNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] =
      "/service_worker/disable_web_security_cross_origin.html";
  const char kScopeUrl[] = "/service_worker/scope/";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  RegisterServiceWorkerOnCrossOriginServer(kScopeUrl, kWorkerUrl);

  std::string test_script =
      R"( navigator.serviceWorker.ready
            .then(reg => reg.unregister())
            .then(_ => { parent.postMessage("PASS", "*"); }); )";
  RunTestWithCrossOriginURL_MaybeCrossProcess(kPageUrl, kScopeUrl, test_script);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerDisableWebSecurityTest, UpdateNoCrash) {
  StartServerAndNavigateToSetup();
  const char kPageUrl[] =
      "/service_worker/disable_web_security_cross_origin.html";
  const char kScopeUrl[] = "/service_worker/scope/";
  const char kWorkerUrl[] = "/service_worker/fetch_event_blob.js";
  RegisterServiceWorkerOnCrossOriginServer(kScopeUrl, kWorkerUrl);

  std::string test_script =
      R"( navigator.serviceWorker.ready
            .then(reg => reg.update())
            .then(_ => { parent.postMessage("PASS", "*"); }); )";
  RunTestWithCrossOriginURL_MaybeCrossProcess(kPageUrl, kScopeUrl, test_script);
}

class HeaderInjectingThrottle : public blink::URLLoaderThrottle {
 public:
  HeaderInjectingThrottle() = default;

  HeaderInjectingThrottle(const HeaderInjectingThrottle&) = delete;
  HeaderInjectingThrottle& operator=(const HeaderInjectingThrottle&) = delete;

  ~HeaderInjectingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    GURL url = request->url;
    if (url.query().find("PlzRedirect") != std::string::npos) {
      GURL::Replacements replacements;
      replacements.SetQueryStr("DidRedirect");
      request->url = url.ReplaceComponents(replacements);
      return;
    }

    request->headers.SetHeader("x-injected", "injected value");
  }
};

class ThrottlingContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  ThrottlingContentBrowserClient() = default;

  ThrottlingContentBrowserClient(const ThrottlingContentBrowserClient&) =
      delete;
  ThrottlingContentBrowserClient& operator=(
      const ThrottlingContentBrowserClient&) = delete;

  ~ThrottlingContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      FrameTreeNodeId frame_tree_node_id,
      std::optional<int64_t> navigation_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    auto throttle = std::make_unique<HeaderInjectingThrottle>();
    throttles.push_back(std::move(throttle));
    return throttles;
  }
};

class ServiceWorkerURLLoaderThrottleTest : public ServiceWorkerBrowserTest {
 public:
  ~ServiceWorkerURLLoaderThrottleTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    net::test_server::RegisterDefaultHandlers(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ServiceWorkerBrowserTest::TearDownOnMainThread();
  }

  void RegisterServiceWorker(const std::string& worker_url) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_url + "');"));
  }

  void RegisterServiceWorkerWithScope(const std::string& worker_url,
                                      const std::string& scope) {
    EXPECT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_url + "', '" +
                                          scope + "');"));
  }
};

// Test that the throttles can inject headers during navigation that are
// observable inside the service worker's fetch event.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       FetchEventForNavigationHasThrottledRequest) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;

  // Register the service worker.
  RegisterServiceWorker("/service_worker/echo_request_headers.js");

  // Perform a navigation. Add "?dump_headers" to tell the service worker to
  // respond with the request headers.
  GURL url =
      embedded_test_server()->GetURL("/service_worker/empty.html?dump_headers");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Extract the headers.
  EvalJsResult result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                               "document.body.textContent");
  ASSERT_TRUE(result.error.empty());
  std::optional<base::Value> parsed_result =
      base::JSONReader::Read(result.ExtractString());
  ASSERT_TRUE(parsed_result);
  base::Value::Dict* dict = parsed_result->GetIfDict();
  ASSERT_TRUE(dict);

  // Default headers are present.
  EXPECT_TRUE(CheckHeader(*dict, "accept",
                          std::string(kFrameAcceptHeaderValue) +
                              std::string(kAcceptHeaderSignedExchangeSuffix)));

  // Injected headers are present.
  EXPECT_TRUE(CheckHeader(*dict, "x-injected", "injected value"));
}

// Test that redirects by throttles occur before service worker interception.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       RedirectOccursBeforeFetchEvent) {
  // Add a throttle which performs a redirect.
  ThrottlingContentBrowserClient content_browser_client;

  // Register the service worker.
  RegisterServiceWorker("/service_worker/fetch_event_pass_through.js");

  // Perform a navigation. Add "?PlzRedirect" to tell the throttle to
  // redirect to another URL.
  GURL url =
      embedded_test_server()->GetURL("/service_worker/empty.html?PlzRedirect");
  GURL redirect_url =
      embedded_test_server()->GetURL("/service_worker/empty.html?DidRedirect");
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 1);
  EXPECT_EQ(redirect_url, shell()->web_contents()->GetLastCommittedURL());

  // This script asks the service worker what fetch events it saw.
  const std::string script = R"(
      (async () => {
        const saw_message = new Promise(resolve => {
          navigator.serviceWorker.onmessage = event => {
            resolve(event.data);
          };
        });
        const registration = await navigator.serviceWorker.ready;
        registration.active.postMessage('');
        return await saw_message;
      })();
  )";

  // Ensure the service worker did not see a fetch event for the PlzRedirect
  // URL, since throttles should have redirected before interception.
  base::Value::List list;
  list.Append(redirect_url.spec());
  EXPECT_EQ(base::Value(std::move(list)),
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script));
}

// Test that the headers injected by throttles during navigation are
// present in the network request in the case of network fallback.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerURLLoaderThrottleTest,
    NavigationHasThrottledRequestHeadersAfterNetworkFallback) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;

  // Register the service worker. Use "/" scope so the "/echoheader" default
  // handler of EmbeddedTestServer is in-scope.
  RegisterServiceWorkerWithScope("/service_worker/fetch_event_pass_through.js",
                                 "/");

  // Perform a navigation. Use "/echoheader" which echoes the given header.
  GURL url = embedded_test_server()->GetURL("/echoheader?x-injected");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  // Check that there is a controller to check that the test is really testing
  // service worker network fallback.
  EXPECT_EQ(true, EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                         "!!navigator.serviceWorker.controller"));

  // The injected header should be present.
  EXPECT_EQ("injected value",
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.textContent"));
}

// Test that the headers injected by throttles during navigation are
// present in the navigation preload request.
IN_PROC_BROWSER_TEST_F(ServiceWorkerURLLoaderThrottleTest,
                       NavigationPreloadHasThrottledRequestHeaders) {
  // Add a throttle which injects a header.
  ThrottlingContentBrowserClient content_browser_client;

  // Register the service worker. Use "/" scope so the "/echoheader" default
  // handler of EmbeddedTestServer is in-scope.
  RegisterServiceWorkerWithScope("/service_worker/navigation_preload_worker.js",
                                 "/");

  // Perform a navigation. Use "/echoheader" which echoes the given header. The
  // server responds to the navigation preload request with this echoed
  // response, and the service worker responds with the navigation preload
  // response.
  //
  // Also test that "Service-Worker-Navigation-Preload" is present to verify
  // we are testing the navigation preload request.
  GURL url = embedded_test_server()->GetURL(
      "/echoheader?Service-Worker-Navigation-Preload&x-injected");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  EXPECT_EQ("true\ninjected value",
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   "document.body.textContent"));
}

// Test fixture to support validating throttling from within an installing
// service worker.
class ServiceWorkerThrottlingTest : public ServiceWorkerBrowserTest {
 protected:
  ServiceWorkerThrottlingTest() {
    // Configure the field trial param to trigger throttling after
    // there are only 2 outstanding requests from an installiner
    // service worker.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kThrottleInstallingServiceWorker, {{"limit", "2"}});
  }

  void RegisterServiceWorkerAndWaitForState(
      const std::string& script_url,
      const std::string& scope,
      ServiceWorkerVersion::Status state) {
    WorkerStateObserver observer(wrapper(), state);
    blink::mojom::ServiceWorkerRegistrationOptions options(
        embedded_test_server()->GetURL(scope),
        blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kImports);
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
    public_context()->RegisterServiceWorker(
        embedded_test_server()->GetURL(script_url), key, options,
        base::BindOnce(&ExpectRegisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk, base::DoNothing()));
    observer.Wait();
  }

  int GetBlockingResponseCount() { return blocking_response_list_.size(); }

  void StopBlocking() {
    std::vector<scoped_refptr<BlockingResponse>> list;
    {
      base::AutoLock auto_lock(lock_);
      should_block_ = false;
      list = std::move(blocking_response_list_);
    }
    for (const auto& response : list) {
      response->StopBlocking();
    }
  }

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    // Configure the EmbeddedTestServer to use our custom request handler
    // to return blocking responses where appropriate.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ServiceWorkerThrottlingTest::HandleRequest, base::Unretained(this)));
    StartServerAndNavigateToSetup();
  }

 private:
  // An object representing an http response that blocks returning its status
  // code until the test tells it to proceed.
  class BlockingResponse : public base::RefCountedThreadSafe<BlockingResponse> {
   public:
    // We must return ownership of a net::test_server::HttpResponse from
    // HandleRequest(), but we also want to track the Response in our test
    // so that we can unblock the response.  In addition, the EmbeddedTestServer
    // deletes its HttpResponse after calling SendResponse().  Therefore, we
    // use an inner class to return to EmbeddedTestServer and hold the
    // outer BlockingResponse alive in the test itself.  The inner class simply
    // forwards the SendResponse() method to the outer class.
    class Inner : public net::test_server::HttpResponse {
     public:
      explicit Inner(base::WeakPtr<BlockingResponse> owner)
          : owner_(std::move(owner)) {}

      ~Inner() override = default;
      void SendResponse(base::WeakPtr<net::test_server::HttpResponseDelegate>
                            delegate) override {
        if (owner_)
          owner_->SendResponse(delegate);
      }

     private:
      base::WeakPtr<BlockingResponse> owner_;
    };

    BlockingResponse()
        : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

    // Mint an HttpResponse suitable for returning to the EmbeddedTestServer
    // that will forward to this BlockingResponse.
    std::unique_ptr<net::test_server::HttpResponse> GetResponse() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      return std::make_unique<Inner>(weak_factory_.GetWeakPtr());
    }

    // Called by the EmbeddedTestServer via our inner class.  The callbacks
    // are stored and invoked later when we've been told to unblock.
    void SendResponse(
        base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      delegate_ = delegate;
      if (should_block_) {
        blocking_ = true;
        return;
      }
      CompleteResponseOnTaskRunner();
    }

    // Called by the test when we want to unblock this response.
    void StopBlocking() {
      // Called on the main thread by the test.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&BlockingResponse::StopBlockingOnTaskRunner, this));
    }

   private:
    friend class base::RefCountedThreadSafe<BlockingResponse>;
    ~BlockingResponse() = default;

    void StopBlockingOnTaskRunner() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      should_block_ = false;
      if (!blocking_)
        return;
      blocking_ = false;
      CompleteResponseOnTaskRunner();
    }

    void CompleteResponseOnTaskRunner() {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      const base::StringPairs kPageHeaders = {
          // "HTTP/1.1 200 HELLOWORLD\r\n"
          {"Connection", "close"},
          {"Content-Length", "32"},
          {"Content-Type", "text/html"},
          {"Cache-Control", "no-store"},
      };
      const char kPageContents[] = "<title>ERROR</title>Hello world.";
      if (delegate_) {
        delegate_->SendHeadersContentAndFinish(net::HTTP_OK, "HELLOWORLD",
                                               kPageHeaders, kPageContents);
      }
    }

    // Accessed on any thread.
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    // All other members only accessed on |task_runner_| sequence.
    base::WeakPtr<net::test_server::HttpResponseDelegate> delegate_ = nullptr;
    bool should_block_ = true;
    bool blocking_ = false;
    base::WeakPtrFactory<BlockingResponse> weak_factory_{this};
  };

  // Return a blocking response to the EmbeddedTestServer for any
  // request where there is a search param named "block".
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(lock_);
    if (!should_block_ ||
        request.GetURL().query().find("block") == std::string::npos) {
      return nullptr;
    }
    auto response = base::MakeRefCounted<BlockingResponse>();
    blocking_response_list_.push_back(std::move(response));
    return blocking_response_list_.back()->GetResponse();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::Lock lock_;
  // Accessed from multiple threads, but protected by |lock_|.
  std::vector<scoped_refptr<BlockingResponse>> blocking_response_list_;
  // Accessed from multiple threads, but protected by |lock_|.
  bool should_block_ = true;
  base::WeakPtrFactory<ServiceWorkerThrottlingTest> weak_factory_{this};
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerThrottlingTest, ThrottleInstalling) {
  // Register a service worker that loads 3 resources in its install
  // handler.  The test server will cause these loads to block which
  // should trigger throttling on the third request.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_blocking_sw.js",
      "/service_worker/throttling_blocking", ServiceWorkerVersion::INSTALLING);

  // Register a second service worker that also loads 3 resources in
  // its install handler.  The test server will not block these loads
  // and the worker should progress to the activated state.
  //
  // This second service worker is used to wait for the first worker
  // to potentially request its resources.  By the time the second worker
  // activates the first worker should have requested its resources and
  // triggered throttling.  This avoids the need for an arbitrary timeout.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_non_blocking_sw.js",
      "/service_worker/throttling_non_blocking",
      ServiceWorkerVersion::ACTIVATED);

  // If throttling worked correctly then there should only be 2 outstanding
  // requests blocked by the test server.
  EXPECT_EQ(2, GetBlockingResponseCount());

  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);

  // Stop blocking the resources loaded by the first service worker.
  StopBlocking();

  // Verify that throttling correctly notes when resources can load and
  // the first service worker fully activates.
  observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerThrottlingTest,
                       ThrottleInstallingWithCacheAddAll) {
  // Register a service worker that loads 3 resources in its install
  // handler via cache.addAll().  The test server will cause these loads
  // to block which should trigger throttling on the third request.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_blocking_cache_addall_sw.js",
      "/service_worker/throttling_blocking_cache_addall",
      ServiceWorkerVersion::INSTALLING);

  // Register a second service worker that also loads 3 resources in
  // its install handler using cache.addAll().  The test server will not
  // block these loads and the worker should progress to the activated state.
  //
  // This second service worker is used to wait for the first worker
  // to potentially request its resources.  By the time the second worker
  // activates the first worker should have requested its resources and
  // triggered throttling.  This avoids the need for an arbitrary timeout.
  RegisterServiceWorkerAndWaitForState(
      "/service_worker/throttling_non_blocking_cache_addall_sw.js",
      "/service_worker/throttling_non_blocking_cache_addall",
      ServiceWorkerVersion::ACTIVATED);

  // If throttling worked correctly then there should only be 2 outstanding
  // requests blocked by the test server.
  EXPECT_EQ(2, GetBlockingResponseCount());

  WorkerStateObserver observer(wrapper(), ServiceWorkerVersion::ACTIVATED);

  // Stop blocking the resources loaded by the first service worker.
  StopBlocking();

  // Verify that throttling correctly notes when resources can load and
  // the first service worker fully activates.
  observer.Wait();
}

// The following tests verify that different values of cross-origin isolation
// enforce the expected process assignments. The page starting the ServiceWorker
// can have COOP+COEP, making it cross-origin isolated, and the ServiceWorker
// itself can have COEP on its main script making it cross-origin isolated. If
// cross-origin isolation status of the page and the script are different, the
// ServiceWorker should be put out of process. It should be put in process
// otherwise.
class ServiceWorkerCrossOriginIsolatedBrowserTest
    : public ServiceWorkerBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  static bool IsPageCrossOriginIsolated() { return std::get<0>(GetParam()); }
  static bool IsServiceWorkerCrossOriginIsolated() {
    return std::get<1>(GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(ServiceWorkerCrossOriginIsolatedBrowserTest,
                       FreshInstall) {
  StartServerAndNavigateToSetup();

  std::string page_path =
      IsPageCrossOriginIsolated()
          ? "/service_worker/create_service_worker_from_isolated.html"
          : "/service_worker/create_service_worker.html";
  std::string worker_path =
      IsServiceWorkerCrossOriginIsolated() ? "empty_isolated.js" : "empty.js";

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(page_path)));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_path + "');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/" + worker_path),
            running_info.script_url);

  bool is_in_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID() ==
      running_info.render_process_id;
  if (!IsPageCrossOriginIsolated() && !IsServiceWorkerCrossOriginIsolated())
    EXPECT_TRUE(is_in_process);
  if (!IsPageCrossOriginIsolated() && IsServiceWorkerCrossOriginIsolated())
    EXPECT_FALSE(is_in_process);

  if (IsPageCrossOriginIsolated() && !IsServiceWorkerCrossOriginIsolated())
    EXPECT_FALSE(is_in_process);
  if (IsPageCrossOriginIsolated() && IsServiceWorkerCrossOriginIsolated())
    EXPECT_TRUE(is_in_process);

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          running_info.render_process_id);
  EXPECT_EQ(IsServiceWorkerCrossOriginIsolated(),
            process_lock.GetWebExposedIsolationInfo().is_isolated());
}

#if BUILDFLAG(IS_ANDROID)
// Flaky on Android, http://crbug.com/1335344.
#define MAYBE_PostInstallRun DISABLED_PostInstallRun
#else
#define MAYBE_PostInstallRun PostInstallRun
#endif
IN_PROC_BROWSER_TEST_P(ServiceWorkerCrossOriginIsolatedBrowserTest,
                       MAYBE_PostInstallRun) {
  StartServerAndNavigateToSetup();

  std::string page_path =
      IsPageCrossOriginIsolated()
          ? "/service_worker/create_service_worker_from_isolated.html"
          : "/service_worker/create_service_worker.html";
  std::string worker_path =
      IsServiceWorkerCrossOriginIsolated() ? "empty_isolated.js" : "empty.js";

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(page_path)));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('" + worker_path + "');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Restart the service worker. The goal is to simulate the launch of an
  // already installed ServiceWorker.
  StopServiceWorker(version.get());
  EXPECT_EQ(StartServiceWorker(version.get()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Wait until the running status is updated.
  base::RunLoop().RunUntilIdle();

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/" + worker_path),
            running_info.script_url);

  bool is_in_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID() ==
      running_info.render_process_id;
  bool should_be_in_process =
      IsPageCrossOriginIsolated() == IsServiceWorkerCrossOriginIsolated();
  EXPECT_EQ(is_in_process, should_be_in_process);

  ProcessLock process_lock =
      ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
          running_info.render_process_id);
  EXPECT_EQ(IsServiceWorkerCrossOriginIsolated(),
            process_lock.GetWebExposedIsolationInfo().is_isolated());
}

// The following tests verify that the page starting the Serviceworker is always
// in the same process as the worker, even when it sets COOP.
class ServiceWorkerCoopBrowserTest : public ServiceWorkerBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  static bool IsCoopEnabledOnMainPage() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ServiceWorkerCoopBrowserTest, FreshInstall) {
  StartServerAndNavigateToSetup();

  std::string page_path =
      IsCoopEnabledOnMainPage()
          ? "/service_worker/create_service_worker_from_coop.html"
          : "/service_worker/create_service_worker.html";

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(page_path)));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/empty.js"),
            running_info.script_url);

  bool is_in_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID() ==
      running_info.render_process_id;
  EXPECT_TRUE(is_in_process);
}

// Sometimes disabled via the macros above
// ServiceWorkerCrossOriginIsolatedBrowserTest.PostInstallRun, as the tests
// flake for the same root cause.
IN_PROC_BROWSER_TEST_P(ServiceWorkerCoopBrowserTest, MAYBE_PostInstallRun) {
  StartServerAndNavigateToSetup();

  std::string page_path =
      IsCoopEnabledOnMainPage()
          ? "/service_worker/create_service_worker_from_coop.html"
          : "/service_worker/create_service_worker.html";

  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(page_path)));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js');"));
  observer.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Restart the service worker. The goal is to simulate the launch of an
  // already installed ServiceWorker.
  StopServiceWorker(version.get());
  EXPECT_EQ(StartServiceWorker(version.get()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Wait until the running status is updated.
  base::RunLoop().RunUntilIdle();

  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(embedded_test_server()->GetURL("/service_worker/empty.js"),
            running_info.script_url);

  bool is_in_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID() ==
      running_info.render_process_id;
  EXPECT_TRUE(is_in_process);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ServiceWorkerCrossOriginIsolatedBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
INSTANTIATE_TEST_SUITE_P(All, ServiceWorkerCoopBrowserTest, testing::Bool());

// Tests with BackForwardCache enabled.
class ServiceWorkerBackForwardCacheAndKeepActiveFreezingBrowserTest
    : public ServiceWorkerBrowserTest {
 protected:
  ServiceWorkerBackForwardCacheAndKeepActiveFreezingBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCache,
              {{"process_binding_strength", "NORMAL"}}}},
            /*ignore_outstanding_network_request=*/false),
        GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }

  const std::string kTryToTriggerEvictionScript = R"(
    window.addEventListener('freeze', () => {
      setTimeout(() => {
        console.log('script that might cause eviction');
      }, 0);
    })
  )";

  const std::string kPostMessageScript = R"(
    new Promise((resolve, reject) => {
      navigator.serviceWorker.addEventListener('message', (event) => {
        resolve(event.data);
      });
      navigator.serviceWorker.controller.postMessage(
        "postMessage from the page");
    });
  )";

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a service worker that shares a renderer process with a
// back-forward cached page and an active page still runs normally.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerBackForwardCacheAndKeepActiveFreezingBrowserTest,
    ShareProcessWithBackForwardCachedPageAndLivePage) {
  StartServerAndNavigateToSetup();

  GURL url_1(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  GURL url_2(embedded_test_server()->GetURL("/service_worker/empty.html"));
  GURL service_worker_url(
      embedded_test_server()->GetURL("/service_worker/hello.js"));

  // 1) Navigate to |url_1|, and register a service worker for it.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();

  {
    WorkerRunningStatusObserver observer(public_context());
    // Register service worker in the current page. This will run a new service
    // worker.
    EXPECT_EQ("DONE", EvalJs(rfh_1, "register('hello.js');"));
    observer.WaitUntilRunning();
  }

  {
    // Assert that there's only 1 service worker running.
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_EQ(1u, infos.size());

    // The service worker shares the process with the page that requested it.
    const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
    EXPECT_EQ(service_worker_url, running_info.script_url);
    EXPECT_EQ(rfh_1->GetProcess()->GetID(), running_info.render_process_id);
  }

  // Reload the page so that it would use the service worker.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  rfh_1 = current_frame_host();

  // Fetch something from the service worker.
  EXPECT_EQ(
      "hello from the service worker\n",
      EvalJs(rfh_1, "fetch('./hello_sw').then(response => response.text())"));

  // Send message to the service worker, and expect a reply.
  EXPECT_EQ("postMessage from the service worker",
            EvalJs(rfh_1, kPostMessageScript));

  // When the page is about to be frozen before getting into the back-forward
  // cache, set a timeout that will run script and cause the page to be evicted
  // from the back-forward cache if the task queues are not properly frozen.
  EXPECT_TRUE(ExecJs(rfh_1, kTryToTriggerEvictionScript));

  // 2) Navigate to |url_2|, which is in-scope of the service worker.
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  RenderFrameHostImpl* rfh_2 = current_frame_host();

  // |rfh_1| and |rfh_2| uses the same renderer process.
  EXPECT_EQ(rfh_1->GetProcess(), rfh_2->GetProcess());

  {
    // Assert that there's still only 1 service worker running.
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_EQ(1u, infos.size());

    // The service worker also shares the same process as |rfh_2|.
    const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
    EXPECT_EQ(service_worker_url, running_info.script_url);
    EXPECT_EQ(rfh_2->GetProcess()->GetID(), running_info.render_process_id);
  }

  // Fetch something from the service worker.
  EXPECT_EQ(
      "hello from the service worker\n",
      EvalJs(rfh_2, "fetch('./hello_sw').then(response => response.text())"));

  // Send message to the service worker, and expect a reply.
  EXPECT_EQ("postMessage from the service worker",
            EvalJs(rfh_2, kPostMessageScript));

  // This test passes if the service worker still runs and responds correctly,
  // and |rfh_1| stays in the back-forward cache, and we're able to restore it
  // from the back-forward cache when we go back.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(rfh_1, current_frame_host());
}

// Tests that a service worker that shares a renderer process with a
// back-forward cached page and no active pages still runs normally.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerBackForwardCacheAndKeepActiveFreezingBrowserTest,
    ShareProcessWithBackForwardCachedPageOnly) {
  StartServerAndNavigateToSetup();

  GURL url_1(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  GURL url_2(embedded_test_server()->GetURL("/service_worker/empty.html"));
  GURL webui_url(std::string(kChromeUIScheme) + "://" +
                 std::string(kChromeUIGpuHost));
  GURL service_worker_url(
      embedded_test_server()->GetURL("/service_worker/hello.js"));

  // 1) Navigate to |url_1|, and register a service worker for it.
  EXPECT_TRUE(NavigateToURL(shell(), url_1));
  RenderFrameHostImpl* rfh_1 = current_frame_host();

  {
    WorkerRunningStatusObserver observer(public_context());
    // Register service worker in the current page. This will run a new service
    // worker.
    EXPECT_EQ("DONE", EvalJs(rfh_1, "register('hello.js');"));
    observer.WaitUntilRunning();
  }

  {
    // Assert that there's only 1 service worker running.
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_EQ(1u, infos.size());

    // The service worker shares the process with the page that requested it.
    const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
    EXPECT_EQ(service_worker_url, running_info.script_url);
    EXPECT_EQ(rfh_1->GetProcess()->GetID(), running_info.render_process_id);
  }

  // Reload the page so that it would use the service worker.
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  rfh_1 = current_frame_host();

  // Fetch something from the service worker.
  EXPECT_EQ(
      "hello from the service worker\n",
      EvalJs(rfh_1, "fetch('./hello_sw').then(response => response.text())"));

  // Send message to the service worker, and expect a reply.
  EXPECT_EQ("postMessage from the service worker",
            EvalJs(rfh_1, kPostMessageScript));

  // When the page is about to be frozen before getting into the back-forward
  // cache, set a timeout that will run script and cause the page to be evicted
  // from the back-forward cache if the task queues are not properly frozen.
  EXPECT_TRUE(ExecJs(rfh_1, kTryToTriggerEvictionScript));

  // 2) Navigate to a WebUI page that will use a different process than |rfh_1|.
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  // The previous page will get into the back-forward cache. At this point, the
  // service worker does not share a process with any active pages.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  EXPECT_NE(rfh_1->GetProcess(), current_frame_host()->GetProcess());

  // 3) Open a new tab and navigate it to |url_2|, which is in-scope of the
  // service worker.
  Shell* new_shell = Shell::CreateNewWindow(
      web_contents()->GetController().GetBrowserContext(), GURL(), nullptr,
      gfx::Size());
  EXPECT_TRUE(NavigateToURL(new_shell, url_2));
  RenderFrameHostImpl* rfh_2 = static_cast<RenderFrameHostImpl*>(
      new_shell->web_contents()->GetPrimaryMainFrame());

  // |rfh_1| and |rfh_2| are in different renderer processes because they are
  // in different tabs.
  EXPECT_NE(rfh_1->GetProcess(), rfh_2->GetProcess());

  {
    // Assert that there's only 1 service worker running.
    const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
        public_context()->GetRunningServiceWorkerInfos();
    ASSERT_EQ(1u, infos.size());

    // The service worker is in a different process than |rfh_2| (it's still
    // in |rfh_1|'s process).
    const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
    EXPECT_EQ(service_worker_url, running_info.script_url);
    EXPECT_NE(rfh_2->GetProcess()->GetID(), running_info.render_process_id);
    EXPECT_EQ(rfh_1->GetProcess()->GetID(), running_info.render_process_id);
  }

  // Fetch something from the service worker.
  EXPECT_EQ(
      "hello from the service worker\n",
      EvalJs(rfh_2, "fetch('./hello_sw').then(response => response.text())"));

  // Send message to the service worker, and expect a reply.
  EXPECT_EQ("postMessage from the service worker",
            EvalJs(rfh_2, kPostMessageScript));

  // This test passes if the service worker still runs and responds correctly,
  // and |rfh_1| stays in the back-forward cache, and we're able to restore it
  // from the back-forward cache when we go back.
  EXPECT_TRUE(rfh_1->IsInBackForwardCache());
  web_contents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  EXPECT_EQ(rfh_1, current_frame_host());
}

// Tests with BackForwardCache enabled.
class ServiceWorkerBackForwardCacheBrowserTest
    : public ServiceWorkerBrowserTest {
 protected:
  ServiceWorkerBackForwardCacheBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kBackForwardCache, {});
  }

  RenderFrameHostImpl* current_frame_host() {
    return static_cast<WebContentsImpl*>(shell()->web_contents())
        ->GetPrimaryFrameTree()
        .root()
        ->current_frame_host();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Fails on Android. https://crbug.com/1216619
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_EvictionOfBackForwardCacheWithMultipleServiceWorkers \
  DISABLED_EvictionOfBackForwardCacheWithMultipleServiceWorkers
#else
#define MAYBE_EvictionOfBackForwardCacheWithMultipleServiceWorkers \
  EvictionOfBackForwardCacheWithMultipleServiceWorkers
#endif

// Regression test for https://crbug.com/1212618.
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerBackForwardCacheBrowserTest,
    MAYBE_EvictionOfBackForwardCacheWithMultipleServiceWorkers) {
  StartServerAndNavigateToSetup();

  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  auto rfh = RenderFrameHostImplWrapper(current_frame_host());

  int first_worker_version_id;

  {
    // Register the first service worker.
    WorkerRunningStatusObserver observer(public_context());
    EXPECT_EQ(
        "DONE",
        EvalJs(current_frame_host(),
               "register('skip_waiting_and_clients_claim_worker.js', '/');"));
    observer.WaitUntilRunning();
    first_worker_version_id = observer.version_id();
  }

  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/service_worker/"
                                              "create_service_worker.html?1")));
  EXPECT_TRUE(rfh->IsInBackForwardCache());

  {
    // Register the second service worker. `rfh` and the current frame host
    // will be controlled by this new service worker. It doesn't await for
    // navigator.serviceWorker.ready.
    WorkerRunningStatusObserver observer(public_context());
    EXPECT_EQ("DONE",
              EvalJs(current_frame_host(),
                     "registerWithoutAwaitingReady('clients_claim_worker.js', "
                     "'/service_worker/');"));
    observer.WaitUntilRunning();
  }

  {
    // `update()` invokes ServiceWorkerContainerHost::UpdateController() which
    // should updates controllees for the first service worker version and the
    // second service worker version. It will cause the BFCache eviction and
    // which causes the ServiceWorkerContainerHost to be destroyed.
    WorkerClientDestroyedObserver observer(wrapper());
    EXPECT_EQ("DONE", EvalJs(current_frame_host(), "update('/');"));
    observer.WaitUntilDestroyed();
  }

  // Try to evict back forward cached controllees in the first service worker
  // version. Since the ServiceWorkerContainerHost has been destroyed, it should
  // have been removed from the first service worker's controllee map. If it
  // hasn't, then calling version->EvictBackForwardCacheControllees() will do a
  // UAF.
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(first_worker_version_id);
  version->EvictBackForwardCachedControllees(
      BackForwardCacheMetrics::NotRestoredReason::kUnknown);

  {
    base::RunLoop loop;
    GURL url = embedded_test_server()->GetURL("/");
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(url));
    public_context()->UnregisterServiceWorker(
        url, key,
        base::BindOnce(&ExpectUnregisterResultAndRun,
                       blink::ServiceWorkerStatusCode::kOk,
                       loop.QuitClosure()));
    loop.Run();
  }
}

class ServiceWorkerFencedFrameBrowserTest : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerFencedFrameBrowserTest() = default;
  ~ServiceWorkerFencedFrameBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

  test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_helper_;
  }

 private:
  test::FencedFrameTestHelper fenced_frame_helper_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerFencedFrameBrowserTest,
                       AncestorFrameTypeIsStoredInServiceWorker) {
  WorkerRunningStatusObserver observer(public_context());

  ASSERT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/service_worker/fenced_frame.html");

  RenderFrameHost* fenced_frame = fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), kFencedFrameUrl);

  // Register the service worker.
  ASSERT_EQ("ok - service worker registered",
            EvalJs(fenced_frame, "RegisterServiceWorker()"));
  observer.WaitUntilRunning();

  // Call backgroundFetch.fetch from the registered service worker, not from
  // the fenced frame. This will be blocked if the worker is registered in the
  // fenced frame
  constexpr char kExpectedError[] =
      "Failed to execute 'fetch' on 'BackgroundFetchManager': "
      "backgroundFetch is not allowed in fenced frames.";
  ASSERT_EQ(kExpectedError,
            EvalJs(fenced_frame, "backgroundFetchFromServiceWorker()"));

  // Stop service worker to save registration data to storage.
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Call backgroundFetch.fetch from the registered service worker again.
  // This ensures if restored data in the service worker keeps the info if it
  // was registered in the fenced frame or not, and the info is used when it
  // became active again.
  ASSERT_EQ(kExpectedError,
            EvalJs(fenced_frame, "backgroundFetchFromServiceWorker()"));
}

class ServiceWorkerFencedFrameProcessAllocationBrowserTest
    : public ServiceWorkerFencedFrameBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerFencedFrameProcessAllocationBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kIsolateFencedFrames,
                                              GetParam());
  }
  ~ServiceWorkerFencedFrameProcessAllocationBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ServiceWorkerFencedFrameProcessAllocationBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "WithFencedFrameProcessIsolation"
                                      : "WithoutFencedFrameProcessIsolation";
                         });

IN_PROC_BROWSER_TEST_P(ServiceWorkerFencedFrameProcessAllocationBrowserTest,
                       ServiceWorkerIsInFencedFrameProcess) {
  WorkerRunningStatusObserver observer(public_context());

  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/service_worker/fenced_frame.html");

  RenderFrameHost* fenced_frame = fenced_frame_test_helper().CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), kFencedFrameUrl);

  // Register the service worker.
  ASSERT_EQ("ok - service worker registered",
            EvalJs(fenced_frame, "RegisterServiceWorker()"));
  observer.WaitUntilRunning();

  // Assert that there's only 1 service worker running.
  const base::flat_map<int64_t, ServiceWorkerRunningInfo>& infos =
      public_context()->GetRunningServiceWorkerInfos();
  ASSERT_EQ(1u, infos.size());

  // Assert that |is_fenced()| for the worker's SiteInfo is true if process
  // isolation is enabled for fenced frames.
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  auto* site_instance = static_cast<SiteInstanceImpl*>(
      wrapper()->process_manager()->GetSiteInstanceForWorker(
          version->embedded_worker()->embedded_worker_id()));
  EXPECT_EQ(version->ancestor_frame_type(),
            blink::mojom::AncestorFrameType::kFencedFrame);
  EXPECT_EQ(site_instance->GetSiteInfo().is_fenced(), GetParam());

  // The service worker shares the process with the page that requested it.
  const ServiceWorkerRunningInfo& running_info = infos.begin()->second;
  EXPECT_EQ(fenced_frame->GetProcess()->GetID(),
            running_info.render_process_id);
}

class ServiceWorkerBrowserTestWithStoragePartitioning
    : public base::test::WithFeatureOverride,
      public ServiceWorkerBrowserTest {
 public:
  // Dedicated worker clients only exist with PlzDedicatedWorker enabled, so
  // turn on that flag.
  ServiceWorkerBrowserTestWithStoragePartitioning()
      : base::test::WithFeatureOverride(
            net::features::kThirdPartyStoragePartitioning),
        scoped_feature_list_(blink::features::kPlzDedicatedWorker) {}
  bool ThirdPartyStoragePartitioningEnabled() const {
    return IsParamFeatureEnabled();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  std::vector<GURL> GetClientURLsForStorageKey(const blink::StorageKey& key) {
    std::vector<GURL> urls;
    for (auto it = wrapper()
                       ->context()
                       ->service_worker_client_owner()
                       .GetServiceWorkerClients(
                           key, /*include_reserved_clients=*/true,
                           /*include_back_forward_cached_clients=*/false);
         !it.IsAtEnd(); ++it) {
      urls.push_back(it->url());
    }
    return urls;
  }

  void RunTestWithWorkers(const std::string& worker_attribute) {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a{" + worker_attribute +
                     "}(b(a{" + worker_attribute + "}))"));
    GURL main_worker_url(embedded_test_server()->GetURL(
        "a.com", "/workers/empty.js?a{" + worker_attribute + "}(b(a{" +
                     worker_attribute + "}))"));
    GURL child_url(embedded_test_server()->GetURL(
        "b.com", "/cross_site_iframe_factory.html?b(a%7B" + worker_attribute +
                     "%7D())"));
    GURL grandchild_url(embedded_test_server()->GetURL(
        "a.com",
        "/cross_site_iframe_factory.html?a%7B" + worker_attribute + "%7D()"));
    GURL grandchild_worker_url(embedded_test_server()->GetURL(
        "a.com", "/workers/empty.js?a{" + worker_attribute + "}()"));
    ASSERT_TRUE(NavigateToURL(shell(), main_url));

    RenderFrameHostImpl* root_rfh = web_contents()->GetPrimaryMainFrame();

    // Check root document setup. The StorageKey at the root should be the same
    // regardless of if `kThirdPartyStoragePartitioning` is enabled.
    auto root_storage_key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(main_url));
    EXPECT_EQ(root_storage_key, root_rfh->GetStorageKey());

    if (ThirdPartyStoragePartitioningEnabled()) {
      // With storage partitioning enabled, the three different frames should
      // each have a different storage key when no host permissions are set.
      EXPECT_THAT(GetClientURLsForStorageKey(root_storage_key),
                  testing::UnorderedElementsAre(main_url, main_worker_url));
      EXPECT_THAT(GetClientURLsForStorageKey(blink::StorageKey::Create(
                      url::Origin::Create(child_url),
                      net::SchemefulSite(root_rfh->GetLastCommittedOrigin()),
                      blink::mojom::AncestorChainBit::kCrossSite)),
                  testing::UnorderedElementsAre(child_url));
      EXPECT_THAT(
          GetClientURLsForStorageKey(blink::StorageKey::Create(
              url::Origin::Create(grandchild_url),
              net::SchemefulSite(root_rfh->GetLastCommittedOrigin()),
              blink::mojom::AncestorChainBit::kCrossSite)),
          testing::UnorderedElementsAre(grandchild_url, grandchild_worker_url));
    } else {
      // With storage partitioning disabled, main frame and grand child should
      // use the same storage key.
      EXPECT_THAT(
          GetClientURLsForStorageKey(root_storage_key),
          testing::UnorderedElementsAre(main_url, main_worker_url,
                                        grandchild_url, grandchild_worker_url));
      EXPECT_THAT(
          GetClientURLsForStorageKey(blink::StorageKey::CreateFirstParty(
              url::Origin::Create(child_url))),
          testing::UnorderedElementsAre(child_url));
    }

    // Give host permissions for b.com (child_rfh) to a.com (root_rfh).
    {
      std::vector<network::mojom::CorsOriginPatternPtr> patterns;
      base::RunLoop run_loop;
      patterns.push_back(network::mojom::CorsOriginPattern::New(
          "http", "b.com", 0,
          network::mojom::CorsDomainMatchMode::kAllowSubdomains,
          network::mojom::CorsPortMatchMode::kAllowAnyPort,
          network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority));
      CorsOriginPatternSetter::Set(
          root_rfh->GetBrowserContext(), root_rfh->GetLastCommittedOrigin(),
          std::move(patterns), {}, run_loop.QuitClosure());
      run_loop.Run();
    }
    // Navigate main host to re-calculate StorageKey calculation.
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    root_rfh = web_contents()->GetPrimaryMainFrame();

    // root_rfh's storage key should not have changed.
    EXPECT_EQ(root_storage_key, root_rfh->GetStorageKey());

    if (ThirdPartyStoragePartitioningEnabled()) {
      EXPECT_THAT(GetClientURLsForStorageKey(root_storage_key),
                  testing::UnorderedElementsAre(main_url, main_worker_url));
      // With storage partitioning enabled, the child frame should now have a
      // top level StorageKey because it is the direct child of the root
      // document and the root has host permissions to it.
      EXPECT_THAT(GetClientURLsForStorageKey(blink::StorageKey::Create(
                      url::Origin::Create(child_url),
                      net::SchemefulSite(url::Origin::Create(child_url)),
                      blink::mojom::AncestorChainBit::kSameSite)),
                  testing::UnorderedElementsAre(child_url));
      // Similarly the grandchild document should now use the child document's
      // origin as the top level site.
      EXPECT_THAT(
          GetClientURLsForStorageKey(blink::StorageKey::Create(
              url::Origin::Create(grandchild_url),
              net::SchemefulSite(url::Origin::Create(child_url)),
              blink::mojom::AncestorChainBit::kCrossSite)),
          testing::UnorderedElementsAre(grandchild_url, grandchild_worker_url));
    } else {
      // With storage partitioning disabled, main frame and grand child should
      // use the same storage key, and generally storage keys are only dependent
      // on the origin.
      EXPECT_THAT(
          GetClientURLsForStorageKey(root_storage_key),
          testing::UnorderedElementsAre(main_url, main_worker_url,
                                        grandchild_url, grandchild_worker_url));
      EXPECT_THAT(
          GetClientURLsForStorageKey(blink::StorageKey::CreateFirstParty(
              url::Origin::Create(child_url))),
          testing::UnorderedElementsAre(child_url));
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ServiceWorkerBrowserTestWithStoragePartitioning);

// http://crbug.com/1385779
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_StorageKeyWithHostPermissionsWithDedicatedWorkers \
  DISABLED_StorageKeyWithHostPermissionsWithDedicatedWorkers
#else
#define MAYBE_StorageKeyWithHostPermissionsWithDedicatedWorkers \
  StorageKeyWithHostPermissionsWithDedicatedWorkers
#endif
IN_PROC_BROWSER_TEST_P(
    ServiceWorkerBrowserTestWithStoragePartitioning,
    MAYBE_StorageKeyWithHostPermissionsWithDedicatedWorkers) {
  RunTestWithWorkers("with-worker");
}

// Android does not have Shared Workers, so skip the shared worker test.
#if !BUILDFLAG(IS_ANDROID)
// http://crbug.com/1385779
#if BUILDFLAG(IS_MAC)
#define MAYBE_StorageKeyWithHostPermissionsWithSharedWorkers \
  DISABLED_StorageKeyWithHostPermissionsWithSharedWorkers
#else
#define MAYBE_StorageKeyWithHostPermissionsWithSharedWorkers \
  StorageKeyWithHostPermissionsWithSharedWorkers
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_P(ServiceWorkerBrowserTestWithStoragePartitioning,
                       MAYBE_StorageKeyWithHostPermissionsWithSharedWorkers) {
  RunTestWithWorkers("with-shared-worker");
}
#endif  // !BUILDFLAG(IS_ANDROID)

enum class SpeculativeStartupNavigationType {
  kBrowserInitiatedNavigation,
  kRendererInitiatedNavigation
};

// This is a test class to verify an optimization to speculatively start a
// service worker for navigation before the "beforeunload" event.
class ServiceWorkerSpeculativeStartupBrowserTest
    : public ServiceWorkerBrowserTest,
      public testing::WithParamInterface<SpeculativeStartupNavigationType> {
 public:
  ServiceWorkerSpeculativeStartupBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kSpeculativeServiceWorkerStartup);
  }
  ~ServiceWorkerSpeculativeStartupBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerSpeculativeStartupBrowserTest,
    testing::Values(
        SpeculativeStartupNavigationType::kBrowserInitiatedNavigation,
        SpeculativeStartupNavigationType::kRendererInitiatedNavigation));

IN_PROC_BROWSER_TEST_P(ServiceWorkerSpeculativeStartupBrowserTest,
                       NavigationWillBeCanceledByBeforeUnload) {
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE",
            EvalJs(GetPrimaryMainFrame(), "register('fetch_event.js');"));
  observer1.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Cancel the next navigation with beforeunload.
  EXPECT_TRUE(
      ExecJs(GetPrimaryMainFrame(), "window.onbeforeunload = () => 'x';"));
  EXPECT_TRUE(web_contents()->NeedToFireBeforeUnloadOrUnloadEvents());
  PrepContentsForBeforeUnloadTest(web_contents());
  SetShouldProceedOnBeforeUnload(shell(),
                                 /*proceed=*/true,
                                 /*success=*/false);

  // Confirm that the service worker speculatively started even when the
  // navigation was canceled.
  WorkerRunningStatusObserver observer2(public_context());
  AppModalDialogWaiter dialog_waiter(shell());
  switch (GetParam()) {
    case SpeculativeStartupNavigationType::kBrowserInitiatedNavigation:
      shell()->LoadURL(in_scope_url);
      break;
    case SpeculativeStartupNavigationType::kRendererInitiatedNavigation:
      EXPECT_TRUE(ExecJs(shell(), JsReplace("location = $1", in_scope_url)));
      break;
  }
  dialog_waiter.Wait();
  EXPECT_TRUE(dialog_waiter.WasDialogRequestedCallbackCalled());
  observer2.WaitUntilRunning();
  EXPECT_EQ(
      blink::EmbeddedWorkerStatus::kRunning,
      wrapper()->GetLiveVersion(observer2.version_id())->running_status());
  histogram_tester().ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::NAVIGATION_HINT), 1);
  histogram_tester().ExpectBucketCount(
      "ServiceWorker.StartWorker.StatusByPurpose_NAVIGATION_HINT",
      static_cast<int>(blink::ServiceWorkerStatusCode::kOk), 1);
}

class ServiceWorkerSpeculativeStartupWithoutParamBrowserTest
    : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerSpeculativeStartupWithoutParamBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kSpeculativeServiceWorkerStartup);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/1440062.
IN_PROC_BROWSER_TEST_F(ServiceWorkerSpeculativeStartupWithoutParamBrowserTest,
                       NavigatingToAboutSrcdocDoesNotCrash) {
  StartServerAndNavigateToSetup();
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(NavigateToURL(shell(), GURL("about:srcdoc")));
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::NAVIGATION_HINT), 0);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, WarmUpAndStartServiceWorker) {
  base::HistogramTester histogram_tester;
  StartServerAndNavigateToSetup();
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           "register('fetch_event_respond_with_fetch.js');"));
  observer1.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(1, version->embedded_worker()->restart_count());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->timeout_timer_.IsRunning());
  EXPECT_FALSE(version->embedded_worker()->pause_initializing_global_scope());

  // Warm-up ServiceWorker. The script should be loaded without evaluating the
  // script.
  EXPECT_FALSE(version->IsWarmedUp());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            WarmUpServiceWorker(version.get()));
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());
  EXPECT_EQ(EmbeddedWorkerInstance::StartingPhase::SCRIPT_LOADED,
            version->embedded_worker()->starting_phase());
  EXPECT_TRUE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_TRUE(version->timeout_timer_.IsRunning());
  const int restart_count_on_warm_up =
      version->embedded_worker()->restart_count();
  EXPECT_EQ(2, restart_count_on_warm_up);
  base::TimeTicks warm_up_start_time = version->start_time_;

  // 2nd ServiceWorker warm-up doesn't change anything except `start_time_`.
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            WarmUpServiceWorker(version.get()));
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());
  EXPECT_EQ(EmbeddedWorkerInstance::StartingPhase::SCRIPT_LOADED,
            version->embedded_worker()->starting_phase());
  EXPECT_TRUE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_TRUE(version->timeout_timer_.IsRunning());
  EXPECT_EQ(restart_count_on_warm_up,
            version->embedded_worker()->restart_count());
  // The 2nd ServiceWorker warm-up reset `start_time_` to be more recent time.
  EXPECT_LT(warm_up_start_time, version->start_time_);

  // Navigate to Service Worker controlled page.
  WorkerRunningStatusObserver observer2(public_context());
  shell()->LoadURL(in_scope_url);
  observer2.WaitUntilRunning();

  // The restart_count doesn't change because there is a warmed-up service
  // worker.
  EXPECT_EQ(restart_count_on_warm_up,
            version->embedded_worker()->restart_count());
  EXPECT_FALSE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME), 1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.StartWorker.StatusByPurpose_FETCH_MAIN_FRAME",
      static_cast<int>(blink::ServiceWorkerStatusCode::kOk), 1);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, WarmUpWorkerAndTimeout) {
  base::HistogramTester histogram_tester;
  StartServerAndNavigateToSetup();
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           "register('fetch_event_respond_with_fetch.js');"));
  observer1.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(1, version->embedded_worker()->restart_count());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->timeout_timer_.IsRunning());

  // Warm-up ServiceWorker. The script should be loaded without evaluating the
  // script.
  EXPECT_FALSE(version->IsWarmedUp());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            WarmUpServiceWorker(version.get()));
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());
  EXPECT_EQ(EmbeddedWorkerInstance::StartingPhase::SCRIPT_LOADED,
            version->embedded_worker()->starting_phase());
  EXPECT_TRUE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_EQ(2, version->embedded_worker()->restart_count());

  // Simulate timeout.
  EXPECT_TRUE(version->timeout_timer_.IsRunning());
  version->start_time_ =
      base::TimeTicks::Now() -
      blink::features::kSpeculativeServiceWorkerWarmUpDuration.Get() -
      base::Minutes(1);
  version->timeout_timer_.user_task().Run();
  while (version->running_status() != blink::EmbeddedWorkerStatus::kStopped) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_EQ(2, version->embedded_worker()->restart_count());

  // Navigate to Service Worker controlled page.
  WorkerRunningStatusObserver observer2(public_context());
  shell()->LoadURL(in_scope_url);
  observer2.WaitUntilRunning();

  EXPECT_EQ(3, version->embedded_worker()->restart_count());
  EXPECT_FALSE(version->embedded_worker()->pause_initializing_global_scope());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME), 1);
  histogram_tester.ExpectBucketCount(
      "ServiceWorker.StartWorker.StatusByPurpose_FETCH_MAIN_FRAME",
      static_cast<int>(blink::ServiceWorkerStatusCode::kOk), 1);
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, WarmUpWorkerTwice) {
  StartServerAndNavigateToSetup();
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           "register('fetch_event_respond_with_fetch.js');"));
  observer1.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(1, version->embedded_worker()->restart_count());

  // Stop ServiceWorker
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->timeout_timer_.IsRunning());

  // Warm-up ServiceWorker.
  EXPECT_FALSE(version->IsWarmedUp());
  EXPECT_TRUE(WarmUpServiceWorker(*public_context(), in_scope_url));
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());

  // Stop ServiceWorker
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->timeout_timer_.IsRunning());

  // Warm-up ServiceWorker again.
  EXPECT_FALSE(version->IsWarmedUp());
  EXPECT_TRUE(WarmUpServiceWorker(*public_context(), in_scope_url));
  EXPECT_TRUE(version->IsWarmedUp());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());

  // Stop ServiceWorker
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_FALSE(version->timeout_timer_.IsRunning());
}

// This is a test class to verify an optimization to speculatively
// warm-up a service worker.
class ServiceWorkerWarmUpBrowserTestBase : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerWarmUpBrowserTestBase() = default;
  ~ServiceWorkerWarmUpBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  scoped_refptr<ServiceWorkerVersion> RegisterServiceWorker(const GURL& url,
                                                            const GURL& scope) {
    // Register a service worker.
    WorkerRunningStatusObserver observer1(public_context());
    EXPECT_TRUE(NavigateToURL(shell(), url));
    const std::string_view script = R"(
      (async () => {
        await navigator.serviceWorker.register('fetch_event.js', {scope: $1});
        await navigator.serviceWorker.ready;
        return 'DONE';
      })();
    )";
    EXPECT_EQ("DONE", EvalJs(GetPrimaryMainFrame(), JsReplace(script, scope)));
    observer1.WaitUntilRunning();

    scoped_refptr<ServiceWorkerVersion> version =
        wrapper()->GetLiveVersion(observer1.version_id());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

    // Stop the current running service worker.
    StopServiceWorker(version.get());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
    return version;
  }

  void AddAnchor(const std::string& id, const GURL& url) {
    const std::string_view script = R"(
      const a = document.createElement('a');
      a.id = $1;
      a.href = $2;
      a.innerText = $1;
      document.body.appendChild(a);
    )";
    EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(), JsReplace(script, id, url)));
    base::RunLoop().RunUntilIdle();
  }
};

class ServiceWorkerWarmUpOnIdleTimeoutBrowserTest
    : public ServiceWorkerWarmUpBrowserTestBase {
 public:
  ServiceWorkerWarmUpOnIdleTimeoutBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSpeculativeServiceWorkerWarmUp,
          {{blink::features::kSpeculativeServiceWorkerWarmUpOnIdleTimeout.name,
            "true"}}}},
        {features::kSpeculativeServiceWorkerStartup});
  }
  ~ServiceWorkerWarmUpOnIdleTimeoutBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerWarmUpOnIdleTimeoutBrowserTest,
                       DoNotWarmUpOnStopWithoutIdleTimeout) {
  base::HistogramTester histogram_tester;
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           "register('fetch_event_respond_with_fetch.js');"));
  observer1.WaitUntilRunning();

  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(1, version->embedded_worker()->restart_count());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Check if the service worker doesn't warm-up automatically.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerWarmUpOnIdleTimeoutBrowserTest,
                       WarmUpOnIdleTimeout) {
  base::HistogramTester histogram_tester;
  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           "register('fetch_event_respond_with_fetch.js');"));
  observer1.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Ask the service worker to trigger idle timeout.
  version->TriggerIdleTerminationAsap();

  // Automatically warm-up service worker after idle timeout.
  base::RunLoop run_loop;
  while (!version->IsWarmedUp()) {
    run_loop.RunUntilIdle();
  }

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version->running_status());
  EXPECT_EQ(EmbeddedWorkerInstance::StartingPhase::SCRIPT_LOADED,
            version->embedded_worker()->starting_phase());
  EXPECT_TRUE(version->embedded_worker()->pause_initializing_global_scope());
}

// Pointer triggered ServiceWorkerWarmUp is not currently available on Android.
#if !BUILDFLAG(IS_ANDROID)

struct ServiceWorkerWarmUpByPointerBrowserTestParam {
  bool enable_warm_up_by_pointerover;
  bool enable_warm_up_by_pointerdown;
};

// This is a test class to verify an optimization to speculatively
// warm-up a service worker by pointer.
class ServiceWorkerWarmUpByPointerBrowserTest
    : public ServiceWorkerWarmUpBrowserTestBase,
      public testing::WithParamInterface<
          ServiceWorkerWarmUpByPointerBrowserTestParam> {
 public:
  ServiceWorkerWarmUpByPointerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kSpeculativeServiceWorkerWarmUp,
          {
              {blink::features::kSpeculativeServiceWorkerWarmUpMaxCount.name,
               "10"},
              {blink::features::kSpeculativeServiceWorkerWarmUpOnPointerover
                   .name,
               GetParam().enable_warm_up_by_pointerover ? "true" : "false"},
              {blink::features::kSpeculativeServiceWorkerWarmUpOnPointerdown
                   .name,
               GetParam().enable_warm_up_by_pointerdown ? "true" : "false"},
          }}},
        {features::kSpeculativeServiceWorkerStartup});
  }
  ~ServiceWorkerWarmUpByPointerBrowserTest() override = default;

  void SimulateMouseEventAndWait(blink::WebInputEvent::Type type,
                                 blink::WebMouseEvent::Button button,
                                 const gfx::Point& point) {
    base::RunLoop().RunUntilIdle();
    InputEventAckWaiter waiter(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost(), type);
    SimulateMouseEvent(web_contents(), type, button, point);
    waiter.Wait();
    RunUntilInputProcessed(static_cast<WebContentsImpl*>(web_contents())
                               ->GetRenderWidgetHostWithPageFocus());
  }

  void SimulateMouseMoveWithElementIdAndWait(const std::string& id) {
    gfx::Point point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), id));
    SimulateMouseEventAndWait(blink::WebMouseEvent::Type::kMouseMove,
                              blink::WebMouseEvent::Button::kNoButton, point);
  }

  void SimulateMouseDownWithElementIdAndWait(const std::string& id) {
    gfx::Point point = gfx::ToFlooredPoint(
        GetCenterCoordinatesOfElementWithId(web_contents(), id));
    SimulateMouseEventAndWait(blink::WebMouseEvent::Type::kMouseDown,
                              blink::WebMouseEvent::Button::kLeft, point);
  }

  void WaitForWarmedUp(const ServiceWorkerVersion& version) {
    base::RunLoop run_loop;
    while (!version.IsWarmedUp()) {
      run_loop.RunUntilIdle();
    }
  }

  void RunUntilInputProcessed(RenderWidgetHost* host) {
    base::RunLoop run_loop;
    RenderWidgetHostImpl::From(host)->WaitForInputProcessed(
        run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

const ServiceWorkerWarmUpByPointerBrowserTestParam
    kServiceWorkerWarmUpByPointerBrowserTestParams[] = {
        {
            .enable_warm_up_by_pointerover = true,
            .enable_warm_up_by_pointerdown = false,
        },
        {
            .enable_warm_up_by_pointerover = false,
            .enable_warm_up_by_pointerdown = true,
        },
        {
            .enable_warm_up_by_pointerover = true,
            .enable_warm_up_by_pointerdown = true,
        },
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerWarmUpByPointerBrowserTest,
    testing::ValuesIn(kServiceWorkerWarmUpByPointerBrowserTestParams));

IN_PROC_BROWSER_TEST_P(ServiceWorkerWarmUpByPointerBrowserTest,
                       PointeroverOrPointerdownWillWarmUpServiceWorker) {
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));

  scoped_refptr<ServiceWorkerVersion> version =
      RegisterServiceWorker(in_scope_url, in_scope_url);

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  AddAnchor("in_scope_url", in_scope_url);
  AddAnchor("out_scope_url", out_scope_url);

  // To ensure that the pointerover event is triggered, move the pointer away
  // from the anchor area.
  SimulateMouseEventAndWait(blink::WebMouseEvent::Type::kMouseMove,
                            blink::WebMouseEvent::Button::kNoButton,
                            gfx::Point(0, 0));

  SimulateMouseMoveWithElementIdAndWait("out_scope_url");
  SimulateMouseMoveWithElementIdAndWait("in_scope_url");

  if (GetParam().enable_warm_up_by_pointerdown) {
    SimulateMouseDownWithElementIdAndWait("in_scope_url");
  }

  WaitForWarmedUp(*version);
}

#endif  // !BUILDFLAG(IS_ANDROID)

class ServiceWorkerSkipEmptyFetchHandlerBrowserTest
    : public ServiceWorkerBrowserTest {
 public:
  ServiceWorkerSkipEmptyFetchHandlerBrowserTest() {
    ServiceWorkerControlleeRequestHandler::
        SetStartServiceWorkerForEmptyFetchHandlerDurationForTesting(0);
  }
  ~ServiceWorkerSkipEmptyFetchHandlerBrowserTest() override = default;

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    StartServerAndNavigateToSetup();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerSkipEmptyFetchHandlerBrowserTest,
                       HasNotSkippedMetrics) {
  base::HistogramTester tester;

  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ(
      "DONE",
      EvalJs(GetPrimaryMainFrame(),
             "register('/service_worker/fetch_event_respond_with_fetch.js')"));
  observer.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Conduct a main resource load.
  EXPECT_TRUE(NavigateToURL(shell(), in_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  tester.ExpectUniqueSample("ServiceWorker.FetchHandler.SkipReason",
                            ServiceWorkerControlleeRequestHandler::
                                FetchHandlerSkipReason::kNotSkipped,
                            1);
  tester.ExpectUniqueSample(
      "ServiceWorker.FetchHandler."
      "TypeAtContinueWithActivatedVersion",
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable, 1);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// TODO(crbug.com/332989700): Disabled due to flakiness on Mac and Linux.
#define MAYBE_HasSkippedForEmptyFetchHandlerMetrics \
  DISABLED_HasSkippedForEmptyFetchHandlerMetrics
#else
#define MAYBE_HasSkippedForEmptyFetchHandlerMetrics \
  HasSkippedForEmptyFetchHandlerMetrics
#endif
IN_PROC_BROWSER_TEST_F(ServiceWorkerSkipEmptyFetchHandlerBrowserTest,
                       MAYBE_HasSkippedForEmptyFetchHandlerMetrics) {
  base::HistogramTester tester;

  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));
  const GURL out_scope_url(embedded_test_server()->GetURL("/empty.html"));
  const GURL in_scope_url(
      embedded_test_server()->GetURL("/service_worker/empty.html"));

  // Register a service worker.
  WorkerRunningStatusObserver observer1(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  EXPECT_EQ("DONE", EvalJs(GetPrimaryMainFrame(),
                           "register('/service_worker/empty_fetch_event.js')"));
  observer1.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Stop the current running service worker.
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Navigate away from the service worker's scope.
  EXPECT_TRUE(NavigateToURL(shell(), out_scope_url));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Conduct a main resource load.
  WorkerRunningStatusObserver observer2(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), in_scope_url));

  // The service worker is started while the fetch handler is skipped.
  observer2.WaitUntilRunning();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  tester.ExpectUniqueSample(
      "ServiceWorker.FetchHandler.SkipReason",
      ServiceWorkerControlleeRequestHandler::FetchHandlerSkipReason::
          kSkippedForEmptyFetchHandler,
      1);

  tester.ExpectUniqueSample(
      "ServiceWorker.FetchHandler."
      "TypeAtContinueWithActivatedVersion",
      ServiceWorkerVersion::FetchHandlerType::kEmptyFetchHandler, 1);
}

// Browser test for the Static Routing API's `race-network-and-fetch-handler`
// source.
class ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest
    : public ServiceWorkerBrowserTest {
 public:
  static constexpr char kSwScriptUrl[] =
      "/service_worker/static_router_race_match_all.js";

  ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest()
      : https_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
        1024);
  }
  ~ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest()
      override = default;

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  scoped_refptr<ServiceWorkerVersion> SetupAndRegisterServiceWorker() {
    scoped_refptr<ServiceWorkerVersion> version =
        SetupAndRegisterServiceWorkerInternal(kSwScriptUrl);

    // Remove any UKMs recorded during setup
    test_ukm_recorder().Purge();
    return version;
  }

  scoped_refptr<ServiceWorkerVersion>
  SetupAndRegisterServiceWorkerWithHTTPSServer() {
    return RegisterRaceNetowrkRequestServiceWorker(https_server(),
                                                   kSwScriptUrl);
  }

  EvalJsResult GetInnerText() {
    // This script asks the service worker what fetch events it saw.
    return EvalJs(GetPrimaryMainFrame(), "document.body.innerText;");
  }

  int GetRequestCount(const std::string& relative_url) const {
    const auto& it = request_log_.find(relative_url);
    if (it == request_log_.end()) {
      return 0;
    }
    return it->second.size();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return *test_ukm_recorder_;
  }

 protected:
  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    RegisterRequestMonitorForRequestCount(embedded_test_server());
    RegisterRequestMonitorForRequestCount(https_server());
    RegisterRequestHandlerForSlowResponsePage(embedded_test_server());
    RegisterRequestHandlerForSlowResponsePage(https_server());
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  scoped_refptr<ServiceWorkerVersion> SetupAndRegisterServiceWorkerInternal(
      const std::string& script_url) {
    StartServerAndNavigateToSetup();
    return RegisterRaceNetowrkRequestServiceWorker(embedded_test_server(),
                                                   script_url);
  }

 private:
  void RegisterRequestHandlerForSlowResponsePage(
      net::EmbeddedTestServer* test_server) {
    test_server->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (!base::Contains(request.GetURL().path(),
                              "/service_worker/mock_response") &&
              !base::Contains(request.GetURL().path(),
                              "/service_worker/no_race")) {
            return nullptr;
          }

          if (base::Contains(request.GetURL().query(), "server_close_socket")) {
            return std::make_unique<net::test_server::RawHttpResponse>("", "");
          }

          const bool is_slow =
              base::Contains(request.GetURL().query(), "server_slow");
          auto http_response =
              is_slow ? std::make_unique<net::test_server::DelayedHttpResponse>(
                            base::Seconds(2))
                      : std::make_unique<net::test_server::BasicHttpResponse>();

          const char kQueryForRedirect[] = "server_redirect";
          if (base::Contains(request.GetURL().query(), kQueryForRedirect)) {
            http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);

            const int pos = request.GetURL().query().find(kQueryForRedirect);
            const int len = strlen(kQueryForRedirect);
            const std::string new_query =
                request.GetURL().query().erase(pos, len);

            http_response->AddCustomHeader(
                "Location", request.GetURL().path() + "?" + new_query);
            return http_response;
          }

          if (!base::Contains(request.GetURL().query(),
                              "server_unknown_mime_type")) {
            http_response->set_content_type("text/plain");
          }

          if (base::Contains(request.GetURL().query(), "server_large_data")) {
            // The data pipe buffer size created for the RaceNetworkRequest test
            // is 1024 byte. Set large data to overflow the buffer.
            http_response->set_content(std::string(1024 * 3, 'A'));
            http_response->set_code(net::HTTP_OK);
            http_response->AddCustomHeader(
                "X-Response-From", "race-network-request-with-large-data");
            return http_response;
          }

          if (base::Contains(request.GetURL().query(), "server_notfound")) {
            http_response->set_code(net::HTTP_NOT_FOUND);
            http_response->set_content(
                "[ServiceWorkerRaceNetworkRequest] Not found");
            return http_response;
          }

          http_response->set_code(net::HTTP_OK);
          http_response->set_content(is_slow
                                         ? "[ServiceWorkerRaceNetworkRequest] "
                                           "Slow response from the network"
                                         : "[ServiceWorkerRaceNetworkRequest] "
                                           "Response from the network");
          return http_response;
        }));
  }
  void RegisterRequestMonitorForRequestCount(
      net::EmbeddedTestServer* test_server) {
    test_server->RegisterRequestMonitor(base::BindRepeating(
        &ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest::
            MonitorRequestHandler,
        base::Unretained(this)));
  }
  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    request_log_[request.relative_url].push_back(request);
  }

  scoped_refptr<ServiceWorkerVersion> RegisterRaceNetowrkRequestServiceWorker(
      net::EmbeddedTestServer* test_server,
      const std::string& script_url) {
    const GURL create_service_worker_url(
        test_server->GetURL("/service_worker/create_service_worker.html"));

    // Register a service worker.
    WorkerRunningStatusObserver observer1(public_context());
    EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
    EXPECT_EQ("DONE", EvalJs(GetPrimaryMainFrame(),
                             base::StrCat({"register('", script_url, "')"})));
    observer1.WaitUntilRunning();
    scoped_refptr<ServiceWorkerVersion> version =
        wrapper()->GetLiveVersion(observer1.version_id());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

    // Stop the current running service worker.
    StopServiceWorker(version.get());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

    return version;
  }

  std::map<std::string, std::vector<net::test_server::HttpRequest>>
      request_log_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  // Capture the response head.
  const GURL test_url = embedded_test_server()->GetURL(
      "/service_worker/mock_response?sw_slow&sw_respond");

  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());

  // ServiceWorker will respond after the delay, so we expect the response from
  // the network request initiated by the RaceNetworkRequest mode comes first.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());

  // Check the response header. "X-Response-From: fetch-handler" is returned
  // when the result from the fetch handler is used.
  EXPECT_NE("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kRace));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kNetwork));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_PassThrough) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  // Capture the response head.
  const GURL test_url = embedded_test_server()->GetURL(
      "/service_worker/mock_response?sw_slow&sw_pass_through");

  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());

  // ServiceWorker will respond after the delay, so we expect the response from
  // the network request initiated by the RaceNetworkRequest mode comes first.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());

  // Check the response header. "X-Response-From: fetch-handler" is returned
  // when the result from the fetch handler is used.
  EXPECT_NE("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));

  // Dispatch another request that returns a response from the fetch handler,
  // which should ensure the first fetch handler execution has finished.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/no_race?sw_respond').then(response "
                   "=> response.text())"));

  // Check the network error count happened inside the fetch handler.
  const std::string script = R"(
    new Promise((resolve, reject) => {
      navigator.serviceWorker.addEventListener('message', (event) => {
        resolve(event.data.length);
      });
      navigator.serviceWorker.controller.postMessage('errors');
    });
  )";

  EXPECT_EQ(0, EvalJs(GetPrimaryMainFrame(), script));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_PassThrough) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();

  // The network request wins, fetch() will be executed with some delay.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/"
                   "mock_response?sw_slow&sw_pass_through').then(response => "
                   "response.text())"));

  // Dispatch another request that returns a response from the fetch handler,
  // which should ensure the first fetch handler execution has finished.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/no_race?sw_respond').then(response "
                   "=> response.text())"));

  // Check the network error count happened inside the fetch handler.
  const std::string script = R"(
    new Promise((resolve, reject) => {
      navigator.serviceWorker.addEventListener('message', (event) => {
        resolve(event.data.length);
      });
      navigator.serviceWorker.controller.postMessage('errors');
    });
  )";

  EXPECT_EQ(0, EvalJs(GetPrimaryMainFrame(), script));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_MarkedAsSecure) {
  // Register the ServiceWorker and navigate to the in scope URL.
  StartServerAndNavigateToSetup();
  ASSERT_TRUE(https_server()->Start());
  SetupAndRegisterServiceWorkerWithHTTPSServer();

  // Capture the response head.
  const GURL test_url = https_server()->GetURL(
      "/service_worker/mock_response?sw_slow&sw_respond");

  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());

  // ServiceWorker will respond after the delay, so we expect the response from
  // the network request initiated by the RaceNetworkRequest mode comes first.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());

  // The page should be marked as secure.
  CheckPageIsMarkedSecure(shell(), https_server()->GetCertificate());
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_MimeTypeSniffed) {
  // Register the ServiceWorker and navigate to the in scope URL.
  StartServerAndNavigateToSetup();

  {
    const GURL test_url = embedded_test_server()->GetURL(
        "/service_worker/mock_response?sw_slow&sw_respond");
    NavigationHandleObserver observer1(web_contents(), test_url);
    NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
    EXPECT_TRUE(observer1.has_committed());
    // Check MIME type as axpected.
    EXPECT_EQ(shell()->web_contents()->GetContentsMimeType(), "text/plain");
  }
  {
    // server_unknown_mime_type doesn't content-type from server.
    const GURL test_url = embedded_test_server()->GetURL(
        "/service_worker/"
        "mock_response?sw_slow&sw_respond&server_unknown_mime_type");
    NavigationHandleObserver observer2(web_contents(), test_url);
    NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
    EXPECT_TRUE(observer2.has_committed());
    // RaceNetworkRequset enables kURLLoadOptionSniffMimeType in URLLoader
    // options, so the mime type is sniffed from the response body.
    EXPECT_EQ(shell()->web_contents()->GetContentsMimeType(), "text/plain");
  }

  // ServiceWorker will respond after the delay, so we expect the response from
  // the network request initiated by the RaceNetworkRequest mode comes first.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_Fetch_No_Respond) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL("/service_worker/mock_response?sw_slow"),
      1);

  // ServiceWorker will respond after the delay, so we expect the response from
  // the network request initiated by the RaceNetworkRequest mode comes first.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());
}
// TODO(crbug.com/40074498) Add tests for
// kURLLoadOptionSendSSLInfoForCertificateError

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_NotFound_FetchHandler_Respond) {
  SetupAndRegisterServiceWorker();

  // Network request is faster, but the response is not found.
  // If the fetch handler respondWith a meaningful response (i.e. 200 response
  // from the cache API), then expect the response from the fetch handler.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL(
          "/service_worker/"
          "mock_response?server_notfound&sw_slow&sw_respond"),
      1);
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_NotFound_FetchHandler_NotRespond) {
  SetupAndRegisterServiceWorker();

  // If the fallback request is not found. Then expect 404.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(),
      embedded_test_server()->GetURL("/service_worker/"
                                     "mock_response?server_notfound"),
      1);
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Not found", GetInnerText());
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_NetworkRequest_Wins_FetchHandler_Fallback \
  DISABLED_NetworkRequest_Wins_FetchHandler_Fallback
#else
#define MAYBE_NetworkRequest_Wins_FetchHandler_Fallback \
  NetworkRequest_Wins_FetchHandler_Fallback
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_NetworkRequest_Wins_FetchHandler_Fallback) {
  // If RaceNetworkRequest comes first, there is a network error, and the fetch
  // handler result is a fallback. In this case the response from
  // RaceNetworkRequest is not used, because we need to support the case when
  // the fetch handler returns a meaningful response e.g. offline page.
  //
  // This test works in the following steps.
  // 1. Start RaceNetworkRequest.
  // 2. Start service worker, and trigger fetch handler that fallback to
  //    network.
  // 3. Get a network error during RaceNetworkRequest.
  // 4. Start fallback network request, neither RaceNetworkRequest nor the fetch
  //    handler is involved.
  // 5. Get the response from the fallback network request.
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?server_close_socket&sw_fallback&sw_slow";
  EXPECT_FALSE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  // Request count should be 2 (RaceNetworkRequest + fallback request).
  EXPECT_EQ(2, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_FetchHandler_Fallback_LargeData) {
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?sw_fallback&sw_slow&server_large_data";
  NavigationHandleObserver observer(
      web_contents(), embedded_test_server()->GetURL(relative_url));
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ(1, GetRequestCount(relative_url));
  EXPECT_EQ("race-network-request-with-large-data",
            observer.GetNormalizedResponseHeader("X-Response-From"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    NetworkRequest_Wins_Post) {
  SetupAndRegisterServiceWorker();
  const std::string action = "/service_worker/mock_response?sw_slow&sw_respond";
  EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(),
                     "document.body.innerHTML = '<form action=\"" + action +
                         "\" method=\"POST\"><button "
                         "type=\"submit\">submit</button></form>'"));

  TestNavigationObserver observer(web_contents());
  EXPECT_TRUE(
      ExecJs(GetPrimaryMainFrame(), "document.querySelector('form').submit()"));
  observer.Wait();

  // RaceNetworkRequest only supports GET method. So the fetch handler is always
  // involved for the navigation via POST.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_NetworkRequest_Wins_Redirect DISABLED_NetworkRequest_Wins_Redirect
#else
#define MAYBE_NetworkRequest_Wins_Redirect NetworkRequest_Wins_Redirect
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_NetworkRequest_Wins_Redirect) {
  SetupAndRegisterServiceWorker();
  const std::string path =
      "/service_worker/mock_response?server_redirect&sw_slow&sw_respond";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&sw_slow&sw_respond";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(path), 1);
  // When a redirect happens, RaceNetworkRequest doesn't handle the final
  // response, and forward the response to the fetch handler.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());

  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a cached
  // resource. RaceNetworkRequest is not triggered.
  EXPECT_EQ(0, GetRequestCount(path_after_redirect));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_NetworkRequest_Wins_Redirect_PassThrough \
  DISABLED_NetworkRequest_Wins_Redirect_PassThrough
#else
#define MAYBE_NetworkRequest_Wins_Redirect_PassThrough \
  NetworkRequest_Wins_Redirect_PassThrough
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_NetworkRequest_Wins_Redirect_PassThrough) {
  SetupAndRegisterServiceWorker();
  const std::string path =
      "/service_worker/mock_response?server_redirect&sw_slow&sw_pass_through";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&sw_slow&sw_pass_through";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(path), 1);
  // The final response is actually from the fetch handler but the fetch handler
  // just passes thorough to the network. So the expected output is "from
  // the network".
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());

  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a pass through
  // fetch request.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    FetchHandler_Wins) {
  SetupAndRegisterServiceWorker();
  // Need to navigate to the page with slow response.
  const GURL slow_url = embedded_test_server()->GetURL(
      "/service_worker/mock_response?server_slow&sw_respond");

  NavigationHandleObserver observer(web_contents(), slow_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), slow_url, 1);
  EXPECT_TRUE(observer.has_committed());
  // RaceNetworkRequest takes long time, but the fetch handler should respond
  // from the cache.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());

  // Check the response header. "X-Response-From: fetch-handler" is returned
  // when the result from the fetch handler is used.
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kRace));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kFetchEvent));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    FetchHandler_Wins_Fallback) {
  SetupAndRegisterServiceWorker();
  // Fetch handler will fallback. This case the response from RaceNetworkRequest
  // is returned as a final response.
  const std::string relative_url =
      "/service_worker/mock_response?server_slow&sw_fallback";
  const GURL slow_url = embedded_test_server()->GetURL(relative_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), slow_url, 1);
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            GetInnerText());
  // Request count should be 1 (RaceNetworkRequest). No additional request to
  // the server.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // TODO(crbug.com/40258805) Ensure if the network error result is from
  // RaceNetworkRequest. The current code only tests if the network error
  // happens.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(NavigateToURL(shell(), slow_url));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    FetchHandler_Wins_NotFound) {
  SetupAndRegisterServiceWorker();
  const GURL slow_url = embedded_test_server()->GetURL(
      "/service_worker/mock_response?server_slow&server_notfound&sw_fallback");

  // Fetch handler is fallback but the response is 404. In this case
  // RaceNetworkRequest is not involved with the navigation.
  EXPECT_TRUE(NavigateToURL(shell(), slow_url));
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Not found", GetInnerText());
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_FetchHandler_Wins_Redirect DISABLED_FetchHandler_Wins_Redirect
#else
#define MAYBE_FetchHandler_Wins_Redirect FetchHandler_Wins_Redirect
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_FetchHandler_Wins_Redirect) {
  SetupAndRegisterServiceWorker();
  const std::string path =
      "/service_worker/mock_response?server_redirect&server_slow&sw_respond";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&server_slow&sw_respond";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(path), 1);
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a cached
  // resource. RaceNetworkRequest is not triggered.
  EXPECT_EQ(0, GetRequestCount(path_after_redirect));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_FetchHandler_Wins_Redirect_PassThrough \
  DISABLED_FetchHandler_Wins_Redirect_PassThrough
#else
#define MAYBE_FetchHandler_Wins_Redirect_PassThrough \
  FetchHandler_Wins_Redirect_PassThrough
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_FetchHandler_Wins_Redirect_PassThrough) {
  SetupAndRegisterServiceWorker();
  const std::string path =
      "/service_worker/"
      "mock_response?server_redirect&server_slow&sw_pass_through";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&server_slow&sw_pass_through";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(path), 1);
  // The final response is actually from the fetch handler but the fetch handler
  // just passes thorough to the network. So the expected output is "from
  // the network".
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            GetInnerText());
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a pass through
  // fetch request.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    FetchHandler_PassThrough) {
  // Register the ServiceWorker and navigate to the in scope URL.
  scoped_refptr<ServiceWorkerVersion> version = SetupAndRegisterServiceWorker();
  // Capture the response head.
  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);

  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  //
  // TODO(crbug.com/40258805) Add the mechanism to wait for the fetch handler
  // completion signal to ensure the request count is exactly not incremented
  // anymore. Currently we don't record the UMA for the fetch handler completion
  // if the RaceNetworkRequest wins.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    FetchHandler_PassThrough_Clone) {
  // Register the ServiceWorker and navigate to the in scope URL.
  scoped_refptr<ServiceWorkerVersion> version = SetupAndRegisterServiceWorker();
  // URL which create a cloned request and pass-through.
  const std::string relative_url_for_clone =
      "/service_worker/mock_response?sw_clone_pass_through";
  const GURL test_url_for_clone =
      embedded_test_server()->GetURL(relative_url_for_clone);

  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigationHandleObserver observer_for_clone(web_contents(),
                                              test_url_for_clone);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url_for_clone, 1);
  EXPECT_TRUE(observer_for_clone.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // Request count should be 2. RaceNetworkRequest + pass through cloned request
  // from fetch handler. The fetch handler will create a new request because the
  // request is cloned hence it may have different metadata from the one
  // initiated by RaceNetworkRequest.
  while (GetRequestCount(relative_url_for_clone) != 2) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(2, GetRequestCount(relative_url_for_clone));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_Subresource_NetworkRequest_Wins \
  DISABLED_Subresource_NetworkRequest_Wins
#else
#define MAYBE_Subresource_NetworkRequest_Wins Subresource_NetworkRequest_Wins
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_Subresource_NetworkRequest_Wins) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  // Fetch something from the service worker.
  EXPECT_EQ(
      "[ServiceWorkerRaceNetworkRequest] Response from the network",
      EvalJs(GetPrimaryMainFrame(),
             "fetch('/service_worker/mock_response?sw_slow').then(response "
             "=> response.text())"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_Fetch_No_Respond) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  EXPECT_EQ(
      "[ServiceWorkerRaceNetworkRequest] Response from the network",
      EvalJs(GetPrimaryMainFrame(),
             "fetch('/service_worker/mock_response?sw_slow').then(response "
             "=> response.text())"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_NotFound) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();

  // Network request is faster, but the response is not found.
  // If the fetch handler respondWith a meaningful response (i.e. 200 response
  // from the cache API), then expect the response from the fetch handler.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/"
                   "mock_response?sw_respond&server_notfound').then(response "
                   "=> response.text())"));

  // If the fallback request is not found. Then expect 404.
  EXPECT_EQ(404, EvalJs(GetPrimaryMainFrame(),
                        "fetch('/service_worker/mock_response?"
                        "server_notfound').then(response => response.status)"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_FetchHandler_Fallback) {
  SetupAndRegisterServiceWorker();
  // Network request is faster, and the fetch handler will fallback.
  // This case the response from RaceNetworkRequset is used.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/mock_response?"
                   "sw_fallback&sw_slow').then(response => "
                   "response.text())"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_FetchHandler_Fallback_LargeData) {
  SetupAndRegisterServiceWorker();
  EXPECT_EQ("race-network-request-with-large-data",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/mock_response?"
                   "sw_fallback&sw_slow&server_large_data').then(response => "
                   "response.headers.get('X-Response-From'))"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_FetchHandler_Fallback_Redirect) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();

  const std::string path =
      "/service_worker/mock_response?server_redirect&sw_fallback&sw_slow";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&sw_fallback&sw_slow";

  // Network request is faster, and the fetch handler will fallback.
  // This case the response from RaceNetworkRequset is used.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));

  // The first request is NOT deduped.
  EXPECT_EQ(2, GetRequestCount(path));
  // The second request is sent as a fallback, the RaceNetworkRequest is reused
  // for the fallback.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_NetworkRequest_Wins_Post) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // RaceNetworkRequest only supports GET method. So the fetch handler is always
  // involved for the request via POST.
  const std::string script = R"(
    const option = {
      method: 'POST',
      body: 'fake body text'
    };
    fetch('service_worker/mock_response?sw_slow&sw_respond', option)
      .then(response => response.text());
  )";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(), script));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_Redirect_Multiple) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string path1 =
      "/service_worker/"
      "mock_response?server_redirect&server_redirect&sw_pass_through";
  const std::string path2 =
      "/service_worker/mock_response?&server_redirect&sw_pass_through";
  const std::string path3 = "/service_worker/mock_response?&&sw_pass_through";

  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path1 + "').then(response => response.text())"));
  // The first request (redirect) is deduped.
  EXPECT_EQ(1, GetRequestCount(path1));
  // The second request (redirect) is also deduped.
  EXPECT_EQ(1, GetRequestCount(path2));
  // The final request is also deduped.
  EXPECT_EQ(1, GetRequestCount(path3));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_Subresource_NetworkRequest_Wins_Redirect \
  DISABLED_Subresource_NetworkRequest_Wins_Redirect
#else
#define MAYBE_Subresource_NetworkRequest_Wins_Redirect \
  Subresource_NetworkRequest_Wins_Redirect
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_Subresource_NetworkRequest_Wins_Redirect) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string path =
      "/service_worker/mock_response?server_redirect&sw_slow&sw_respond";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&sw_slow&sw_respond";

  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a cached
  // resource. RaceNetworkRequest is not triggered.
  EXPECT_EQ(0, GetRequestCount(path_after_redirect));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_Subresource_NetworkRequest_Wins_Redirect_PassThrough \
  DISABLED_Subresource_NetworkRequest_Wins_Redirect_PassThrough
#else
#define MAYBE_Subresource_NetworkRequest_Wins_Redirect_PassThrough \
  Subresource_NetworkRequest_Wins_Redirect_PassThrough
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_Subresource_NetworkRequest_Wins_Redirect_PassThrough) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string path =
      "/service_worker/mock_response?server_redirect&sw_slow&sw_pass_through";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&sw_slow&sw_pass_through";

  // The final response is actually from the fetch handler but the fetch handler
  // just passes thorough to the network. So the expected output is "from
  // the network".
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a pass through
  // fetch request.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_FetchHandler_Wins) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  // RaceNetworkRequest takes long time, but the fetch handler should respond
  // from the cache.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/mock_response?"
                   "server_slow&sw_respond').then(response => "
                   "response.text())"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_FetchHandler_Wins_Fallback) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  // Fetch handler will fallback. This case the response from RaceNetworkRequest
  // is returned as a final response.
  const std::string relative_url =
      "/service_worker/mock_response?server_slow&sw_fallback";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));
  // Request count should be 1 (RaceNetworkRequest). No additional request to
  // the server.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // TODO(crbug.com/40258805) Ensure if the network error result is from
  // RaceNetworkRequest. The current code only tests if the network error
  // happens.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_FALSE(ExecJs(GetPrimaryMainFrame(), "fetch('" + relative_url + "')"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_FetchHandler_Wins_Fallback_Redirect) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();

  // Fetch handler will fallback. This case the response from RaceNetworkRequest
  // is returned as a final response.
  const std::string path =
      "/service_worker/mock_response?server_redirect&server_slow&sw_fallback";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&server_slow&sw_fallback";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));

  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // The second request is sent as a fallback, the RaceNetworkRequest is reused
  // for the fallback.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_FetchHandler_Wins_NotFound) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  // Fetch handler is fallback but the response is 404. In this case
  // RaceNetworkRequest is not involved.
  EXPECT_EQ(404,
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/mock_response?"
                   "server_slow&sw_fallback&server_notfound').then(response => "
                   "response.status)"));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
// TODO(crbug.com/41490535): Flaky on Android.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
#define MAYBE_Subresource_FetchHandler_Wins_Redirect \
  DISABLED_Subresource_FetchHandler_Wins_Redirect
#else
#define MAYBE_Subresource_FetchHandler_Wins_Redirect \
  Subresource_FetchHandler_Wins_Redirect
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_Subresource_FetchHandler_Wins_Redirect) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string path =
      "/service_worker/mock_response?server_redirect&server_slow&sw_respond";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&server_slow&sw_respond";

  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a cached
  // resource. RaceNetworkRequest is not triggered.
  EXPECT_EQ(0, GetRequestCount(path_after_redirect));
}

// TODO(crbug.com/40263529): Flaky on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_Subresource_FetchHandler_Wins_Redirect_PassThrough \
  DISABLED_Subresource_FetchHandler_Wins_Redirect_PassThrough
#else
#define MAYBE_Subresource_FetchHandler_Wins_Redirect_PassThrough \
  Subresource_FetchHandler_Wins_Redirect_PassThrough
#endif
IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    MAYBE_Subresource_FetchHandler_Wins_Redirect_PassThrough) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string path =
      "/service_worker/"
      "mock_response?server_redirect&server_slow&sw_pass_through";
  const std::string path_after_redirect =
      "/service_worker/mock_response?&server_slow&sw_pass_through";

  // The final response is actually from the fetch handler but the fetch handler
  // just passes thorough to the network. So the expected output is "from
  // the network".
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + path + "').then(response => response.text())"));
  // The first request is deduped.
  EXPECT_EQ(1, GetRequestCount(path));
  // Fetch handler handles the second request, and respond with a pass through
  // fetch request.
  EXPECT_EQ(1, GetRequestCount(path_after_redirect));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest,
    Subresource_FetchHandler_PassThrough) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();

  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through";
  EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(), "fetch('" + relative_url + "')"));

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  //
  // TODO(crbug.com/40258805) Add the mechanism to wait for the fetch handler
  // completion signal to ensure the request count is exactly not incremented
  // anymore. Currently we don't record the UMA for the fetch handler
  // completion if the RaceNetworkRequest wins.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

class ServiceWorkerAutoPreloadBrowserTest
    : public ServiceWorkerStaticRouterRaceNetworkAndFetchHandlerSourceBrowserTest {
 public:
  static constexpr char kSwScriptUrl[] = "/service_worker/auto_preload.js";

  ServiceWorkerAutoPreloadBrowserTest() {
    feature_list_.InitWithFeatures({{features::kServiceWorkerAutoPreload}}, {});
    RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
        1024);
  }

  ~ServiceWorkerAutoPreloadBrowserTest() override = default;

  scoped_refptr<ServiceWorkerVersion> SetupAndRegisterServiceWorker() {
    return SetupAndRegisterServiceWorkerInternal(kSwScriptUrl);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       NetworkRequestRepliedFirstButFetchHandlerResultIsUsed) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);
  NavigationHandleObserver observer(web_contents(), test_url);
  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // ServiceWorker will respond after the delay, so we expect the network
  // request initiated by the RaceNetworkRequest is requested to the server
  // although it's not actually used.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Unlike RaceNetworkRequest, AutoPreload always waits for the response is
  // from the fetch handler.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());

  // Check the response header. "X-Response-From: fetch-handler" is returned
  // when the result from the fetch handler is used.
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest, PassThrough) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  // Capture the response head.
  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);

  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       PassThrough_LargeData) {
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through&server_large_data";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);

  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
  EXPECT_EQ("race-network-request-with-large-data",
            observer.GetNormalizedResponseHeader("X-Response-From"));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       NetworkRequest_Wins_FetchHandler_Fallback) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_fallback";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(relative_url), 1);

  // ServiceWorker will respond after the delay, so we expect the network
  // request initiated by the RaceNetworkRequest responds first, then get a
  // fallback result from the fetch handler. In that case AutoPreload doesn't
  // send another network request for the fallback, but reuses the
  // RaceNetworkRequest.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());
  EXPECT_EQ(1, GetRequestCount(relative_url));

  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(2, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       FetchHandler_Wins_Fallback) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  const std::string relative_url =
      "/service_worker/mock_response?server_slow&sw_fallback";
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL(relative_url), 1);

  // Server will respond after the delay, so we expect the fetch handler
  // responds first and the result is fallback. In that case AutoPreload doesn't
  // send another network request for the fallback, but reuses the
  // RaceNetworkRequest.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            GetInnerText());
  EXPECT_EQ(1, GetRequestCount(relative_url));

  ReloadBlockUntilNavigationsComplete(shell(), 1);
  EXPECT_EQ(2, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerAutoPreloadBrowserTest,
    Subresource_NetworkRequestRepliedFirstButFetchHandlerResultIsUsed) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // Fetch something from the service worker.
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));

  // ServiceWorker will respond after the delay, so we expect the network
  // request initiated by the RaceNetworkRequest is requested to the server
  // although it's not actually used.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       Subresource_PassThrough) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       Subresource_PassThrough_LargeData) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string relative_url =
      "/service_worker/mock_response?sw_pass_through&server_large_data";
  EXPECT_EQ(200, EvalJs(GetPrimaryMainFrame(),
                        "fetch('" + relative_url +
                            "').then(response => response.status)"));

  // Request count should be 1. RaceNetworkRequest + pass through request from
  // fetch handler but the fetch handler request will reuse the response from
  // RaceNetworkRequest.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       Subresource_NetworkRequest_Wins_FetchHandler_Fallback) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_fallback";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));

  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadBrowserTest,
                       Subresource_FetchHandler_Wins_Fallback) {
  SetupAndRegisterServiceWorker();
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  const std::string relative_url =
      "/service_worker/mock_response?server_slow&sw_fallback";
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Slow response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));

  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

class ServiceWorkerAutoPreloadWithBlockedHostsBrowserTest
    : public ServiceWorkerAutoPreloadBrowserTest {
 public:
  ServiceWorkerAutoPreloadWithBlockedHostsBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kServiceWorkerAutoPreload,
             {
                 {"blocked_hosts", blocked_host()},
             }},
        },
        {});
  }

  void RegisterServiceWorkerWithBlockedHost() {
    const GURL create_service_worker_url(embedded_test_server()->GetURL(
        blocked_host(), "/service_worker/create_service_worker.html"));
    WorkerRunningStatusObserver observer1(public_context());
    EXPECT_TRUE(NavigateToURL(shell(), create_service_worker_url));
    EXPECT_EQ("DONE", EvalJs(GetPrimaryMainFrame(),
                             "register('/service_worker/auto_preload.js')"));
    observer1.WaitUntilRunning();
  }

  std::string blocked_host() { return "localhost"; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadWithBlockedHostsBrowserTest,
                       BlockedHosts) {
  // Register the ServiceWorker and navigate to the in scope URL.
  SetupAndRegisterServiceWorker();
  RegisterServiceWorkerWithBlockedHost();

  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  const GURL test_url =
      embedded_test_server()->GetURL(blocked_host(), relative_url);

  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());

  // Request count should be 0. AutoPreload is not triggered when the navigation
  // request is for the host in the blocked host params.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, GetRequestCount(relative_url));
}

class ServiceWorkerAutoPreloadWithEnableSubresourcePreloadBrowserTest
    : public ServiceWorkerAutoPreloadBrowserTest {
 public:
  ServiceWorkerAutoPreloadWithEnableSubresourcePreloadBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kServiceWorkerAutoPreload,
          {
              {"enable_subresource_preload", "false"},
          }}},
        {});
    RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
        1024);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerAutoPreloadWithEnableSubresourcePreloadBrowserTest,
    Disabled) {
  SetupAndRegisterServiceWorker();
  // Check the main resource request, ensuring the preload request is
  // dispatched.
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);
  NavigationHandleObserver observer(web_contents(), test_url);
  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // ServiceWorker will respond after the delay, so we expect the network
  // request initiated by the RaceNetworkRequest is requested to the server
  // although it's not actually used.
  while (GetRequestCount(relative_url) != 1) {
    base::RunLoop().RunUntilIdle();
  }
  EXPECT_EQ(1, GetRequestCount(relative_url));
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));

  // Check the subresource request, ensuring the preload request is not
  // dispatched. The request recorded count is not changed, because the
  // subresource preload request is not dispatched.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

class ServiceWorkerAutoPreloadWithEnableOnlyWhenSWNotRunningBrowserTest
    : public ServiceWorkerAutoPreloadBrowserTest {
 public:
  ServiceWorkerAutoPreloadWithEnableOnlyWhenSWNotRunningBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kServiceWorkerAutoPreload,
          {
              {"enable_only_when_service_worker_not_running", "true"},
          }}},
        {});
    RaceNetworkRequestWriteBufferManager::SetDataPipeCapacityBytesForTesting(
        1024);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerAutoPreloadWithEnableOnlyWhenSWNotRunningBrowserTest,
    NotRunning) {
  // Ensure the ServiceWorker is stopped.
  scoped_refptr<ServiceWorkerVersion> version = SetupAndRegisterServiceWorker();
  StopServiceWorker(version.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());

  // Check the main resource request, ensuring the preload request is
  // dispatched.
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);
  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  // ServiceWorker will respond after the delay, so we expect the network
  // request initiated by the RaceNetworkRequest is requested to the server
  // although it's not actually used.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetRequestCount(relative_url));
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));
}

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerAutoPreloadWithEnableOnlyWhenSWNotRunningBrowserTest,
    Running) {
  // Ensure the ServiceWorker is running.
  scoped_refptr<ServiceWorkerVersion> version = SetupAndRegisterServiceWorker();
  EXPECT_EQ(StartServiceWorker(version.get()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // Check the main resource request, ensuring the preload request is not
  // dispatched.
  const std::string relative_url =
      "/service_worker/mock_response?sw_slow&sw_respond";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);
  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, GetRequestCount(relative_url));
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));
}

class ServiceWorkerAutoPreloadAllowListBrowserTest
    : public ServiceWorkerAutoPreloadBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerAutoPreloadAllowListBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kServiceWorkerAutoPreload, {{"use_allowlist", "true"}}},
         {features::kServiceWorkerBypassFetchHandlerHashStrings,
          {{"script_checksum_to_bypass",
            ShouldUseValidChecksum() ? kValidChecksum : kInvalidChecksum}}}},
        {});
  }
  ~ServiceWorkerAutoPreloadAllowListBrowserTest() override = default;

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

 protected:
  bool ShouldUseValidChecksum() { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  static constexpr char kValidChecksum[] =
      "E3F7EBC59086064254D833F18B01BAAE4B9DB5F5321E271AC345F2648518324A";
  static constexpr char kInvalidChecksum[] = "";
};

INSTANTIATE_TEST_SUITE_P(ALL,
                         ServiceWorkerAutoPreloadAllowListBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(ServiceWorkerAutoPreloadAllowListBrowserTest,
                       EnableAutoPreloadIfScriptIsAllowed) {
  SetupAndRegisterServiceWorker();
  const std::string relative_url = "/service_worker/mock_response?sw_respond";
  const GURL test_url = embedded_test_server()->GetURL(relative_url);
  NavigationHandleObserver observer(web_contents(), test_url);
  WorkerRunningStatusObserver service_worker_running_status_observer(
      public_context());
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());
  service_worker_running_status_observer.WaitUntilRunning();

  // The final response comes from the fetch handler, which returns a custom
  // response without requiring network request. Expect the network request by
  // the AutoPrealod feature only when the script checksum is in the allowlist.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ShouldUseValidChecksum() ? 1 : 0, GetRequestCount(relative_url));
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());
}

class ServiceWorkerAutoPreloadOptOutBrowserTest
    : public ServiceWorkerAutoPreloadBrowserTest {
 public:
  ServiceWorkerAutoPreloadOptOutBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kServiceWorkerAutoPreload, {{"strategy", "optin"}}},
            {blink::features::kServiceWorkerStaticRouterNotConditionEnabled,
             {}},
        },
        {});
  }
  ~ServiceWorkerAutoPreloadOptOutBrowserTest() override = default;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadOptOutBrowserTest,
                       MainResourceFetchHandlerShouldNotRace) {
  SetupAndRegisterServiceWorker();
  const std::string relative_url = "/service_worker/no_race?sw_slow&sw_respond";
  // Capture the response head.
  const GURL test_url = embedded_test_server()->GetURL(relative_url);

  NavigationHandleObserver observer(web_contents(), test_url);
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 1);
  EXPECT_TRUE(observer.has_committed());

  // ServiceWorker will respond after the delay.
  // If race is enabled, the response will come from the network request.
  // This test expects not.
  EXPECT_NE("[ServiceWorkerRaceNetworkRequest] Response from the network",
            GetInnerText());
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            GetInnerText());

  // Check the response header. "X-Response-From: fetch-handler" is returned
  // when the result from the fetch handler is used.
  EXPECT_EQ("fetch-handler",
            observer.GetNormalizedResponseHeader("X-Response-From"));

  EXPECT_EQ(0, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerAutoPreloadOptOutBrowserTest,
                       SubresourceFetchHandlerShouldNotRace) {
  SetupAndRegisterServiceWorker();
  WorkerRunningStatusObserver observer(public_context());
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  observer.WaitUntilRunning();
  const std::string relative_url = "/service_worker/no_race?sw_slow&sw_respond";
  // Fetch something from the service worker.
  EXPECT_EQ("[ServiceWorkerRaceNetworkRequest] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('" + relative_url +
                       "').then(response => response.text())"));

  EXPECT_EQ(0, GetRequestCount(relative_url));
}

class CacheStorageDataChecker
    : public base::RefCounted<CacheStorageDataChecker> {
 public:
  enum class Status {
    kUnknown,
    kExist,
    kNotExist,
  };
  static Status Exist(
      storage::mojom::CacheStorageControl* cache_storage_control,
      const GURL& origin,
      const std::string& cache_name,
      const GURL& url) {
    mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage_remote;
    network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;
    network::DocumentIsolationPolicy document_isolation_policy;
    cache_storage_control->AddReceiver(
        cross_origin_embedder_policy, mojo::NullRemote(),
        document_isolation_policy,
        storage::BucketLocator::ForDefaultBucket(
            blink::StorageKey::CreateFirstParty(url::Origin::Create(origin))),
        storage::mojom::CacheStorageOwner::kCacheAPI,
        cache_storage_remote.InitWithNewPipeAndPassReceiver());

    auto checker = base::MakeRefCounted<CacheStorageDataChecker>(
        std::move(cache_storage_remote), cache_name, url);
    return checker->ExistImpl();
  }

  CacheStorageDataChecker(
      mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
      const std::string& cache_name,
      const GURL& url)
      : cache_storage_(std::move(cache_storage)),
        cache_name_(cache_name),
        url_(url) {}

  CacheStorageDataChecker(const CacheStorageDataChecker&) = delete;
  CacheStorageDataChecker& operator=(const CacheStorageDataChecker&) = delete;

 private:
  friend class base::RefCounted<CacheStorageDataChecker>;

  virtual ~CacheStorageDataChecker() = default;

  Status ExistImpl() {
    Status result = Status::kUnknown;
    base::RunLoop loop;
    cache_storage_->Open(
        base::UTF8ToUTF16(cache_name_), /*trace_id=*/0,
        base::BindOnce(&CacheStorageDataChecker::OnCacheStorageOpenCallback,
                       this, &result, loop.QuitClosure()));
    loop.Run();
    return result;
  }

  void OnCacheStorageOpenCallback(Status* result,
                                  base::OnceClosure continuation,
                                  blink::mojom::OpenResultPtr open_result) {
    ASSERT_TRUE(open_result->is_cache());

    auto scoped_request = blink::mojom::FetchAPIRequest::New();
    scoped_request->url = url_;

    // Preserve lifetime of this remote across the Match call.
    cache_storage_cache_.emplace(std::move(open_result->get_cache()));

    (*cache_storage_cache_)
        ->Match(std::move(scoped_request),
                blink::mojom::CacheQueryOptions::New(),
                /*in_related_fetch_event=*/false,
                /*in_range_fetch_event=*/false, /*trace_id=*/0,
                base::BindOnce(
                    &CacheStorageDataChecker::OnCacheStorageCacheMatchCallback,
                    this, result, std::move(continuation)));
  }

  void OnCacheStorageCacheMatchCallback(
      Status* result,
      base::OnceClosure continuation,
      blink::mojom::MatchResultPtr match_result) {
    if (match_result->is_status()) {
      ASSERT_EQ(match_result->get_status(), CacheStorageError::kErrorNotFound);
      *result = Status::kNotExist;
      std::move(continuation).Run();
      return;
    }
    ASSERT_TRUE(match_result->is_response());
    *result = Status::kExist;
    std::move(continuation).Run();
  }

  mojo::Remote<blink::mojom::CacheStorage> cache_storage_;
  std::optional<mojo::AssociatedRemote<blink::mojom::CacheStorageCache>>
      cache_storage_cache_;
  const std::string cache_name_;
  const GURL url_;
};

// Test class for static routing API (crbug.com/1420517) browsertest.
class ServiceWorkerStaticRouterBrowserTest : public ServiceWorkerBrowserTest {
 public:
  enum class TestType {
    kNetwork,
    kRaceNetworkAndFetch,
  };

  ServiceWorkerStaticRouterBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kServiceWorkerStaticRouter,
         blink::features::kServiceWorkerStaticRouterNotConditionEnabled},
        {});
  }
  ~ServiceWorkerStaticRouterBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ServiceWorkerBrowserTest::SetUpOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  WebContents* web_contents() const { return shell()->web_contents(); }

  RenderFrameHost* GetPrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ukm::TestAutoSetUkmRecorder& test_ukm_recorder() {
    return *test_ukm_recorder_;
  }

  void SetupAndRegisterServiceWorker(TestType type) {
    RegisterRequestMonitorForRequestCount();
    RegisterRequestHandlerForTest();
    StartServerAndNavigateToSetup();

    const GURL create_service_worker_url(embedded_test_server()->GetURL(
        "/service_worker/create_service_worker.html"));

    // Register a service worker.
    WorkerRunningStatusObserver observer1(public_context());
    ASSERT_TRUE(NavigateToURL(shell(), create_service_worker_url));
    if (type == TestType::kNetwork) {
      ASSERT_EQ("DONE", EvalJs(GetPrimaryMainFrame(),
                               "register('/service_worker/static_router.js')"));
    } else if (type == TestType::kRaceNetworkAndFetch) {
      ASSERT_EQ("DONE",
                EvalJs(GetPrimaryMainFrame(),
                       "register('/service_worker/static_router_race.js')"));
    }
    observer1.WaitUntilRunning();
    active_version_ = wrapper()->GetLiveVersion(observer1.version_id());
    ASSERT_EQ(blink::EmbeddedWorkerStatus::kRunning,
              active_version_->running_status());

    // Stop the current running service worker.
    StopServiceWorker(active_version_.get());
    ASSERT_EQ(blink::EmbeddedWorkerStatus::kStopped,
              active_version_->running_status());

    // Remove any UKMs recorded during setup
    test_ukm_recorder().Purge();
  }

  int GetRequestCount(const std::string& relative_url) const {
    const auto& it = request_log_.find(relative_url);
    if (it == request_log_.end()) {
      return 0;
    }
    return it->second.size();
  }

  EvalJsResult GetInnerText() {
    return EvalJs(GetPrimaryMainFrame(), "document.body.innerText;");
  }

  scoped_refptr<ServiceWorkerVersion> version() { return active_version_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void WaitUntilRelativeUrlStoredInCache(const std::string& relative_url) {
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    int retries = 10;
    while (retries-- > 0) {
      if (CacheStorageDataChecker::Exist(
              partition->GetCacheStorageControl(),
              embedded_test_server()->base_url(), std::string("test"),
              embedded_test_server()->GetURL(relative_url)) ==
          CacheStorageDataChecker::Status::kExist) {
        return;
      }
      EXPECT_TRUE(ExecJs(GetPrimaryMainFrame(),
                         R"(
          caches.open("test").then((c) => {
              const headers = new Headers();
              headers.append('Content-Type', 'text/html');
              headers.append('X-Response-From', 'cache');
              const options = {
                  status: 200,
                  statusText: 'Custom response from cache',
                  headers
              };
              const response = new Response(
                  '[ServiceWorkerStaticRouter] Response from the cache',
                  options);
              c.put("/service_worker/cache_hit", response.clone());
              c.put("/service_worker/cache_with_name", response.clone());
              c.put("/service_worker/cache_with_wrong_name", response.clone());
          });)"));
    }
  }

 private:
  void RegisterRequestHandlerForTest() {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (base::Contains(request.GetURL().path(),
                             "/service_worker/direct") ||
              base::Contains(request.GetURL().path(),
                             "/service_worker/direct_if_not_running") ||
              base::Contains(request.GetURL().path(),
                             "/service_worker/cache_with_wrong_name") ||
              base::Contains(request.GetURL().path(),
                             "/service_worker/cache_miss") ||
              base::Contains(request.GetURL().path(),
                             "/service_worker/not_not_match")) {
            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_code(net::HTTP_OK);
            http_response->set_content_type("text/plain");
            http_response->set_content(
                "[ServiceWorkerStaticRouter] "
                "Response from the network");
            return http_response;
          }
          if (base::Contains(request.GetURL().path(),
                             "/service_worker/race_network_and_fetch")) {
            auto http_response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            http_response->set_code(net::HTTP_OK);
            http_response->set_content_type("text/plain");
            http_response->set_content(
                "[ServiceWorkerStaticRouter] "
                "Response from the race network");
            return http_response;
          }
          return nullptr;
        }));
  }

  void RegisterRequestMonitorForRequestCount() {
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ServiceWorkerStaticRouterBrowserTest::MonitorRequestHandler,
        base::Unretained(this)));
  }
  void MonitorRequestHandler(const net::test_server::HttpRequest& request) {
    request_log_[request.relative_url].push_back(request);
  }

  std::map<std::string, std::vector<net::test_server::HttpRequest>>
      request_log_;
  base::test::ScopedFeatureList feature_list_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceFromFetchEventRule) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  // UKM records reloads conducted in `ReloadBlockUntilNavigationComplete`.
  // Remove them to make sue UKM only has record for main test.
  test_ukm_recorder().Purge();

  const std::string relative_url = "/service_worker/fetch_event_rule";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the fetch handler",
            GetInnerText());
  // The result should be got from the fetch handler, and no network access is
  // expected.
  EXPECT_EQ(0, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kFetchEvent));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kFetchEvent));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceNoRuleMatchedResponseFromFetchHandler) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  // UKM records reloads conducted in `ReloadBlockUntilNavigationComplete`.
  // Remove them to make sue UKM only has record for main test.
  test_ukm_recorder().Purge();

  const std::string relative_url = "/service_worker/fetch_handler";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the fetch handler",
            GetInnerText());
  // The result should be got from the fetch handler, and no network access is
  // expected.
  EXPECT_EQ(0, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName));

  EXPECT_FALSE(ukm::TestAutoSetUkmRecorder::EntryHasMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceFromNetworkRule) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/direct";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Ensure the ServiceWorker will start even if the navigation results in the
  // fallback via Static Routing.
  observer.WaitUntilRunning();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version()->running_status());
  histogram_tester().ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::STATIC_ROUTER), 1);

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kNetwork));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kNetwork));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceNoRuleMatchedResponseFromFetchHandler) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the fetch handler",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/fetch_handler').then(response => "
                   "response.text())"));
  // The result should be got from the fetch handler, and no network access is
  // expected.
  EXPECT_EQ(0, GetRequestCount("/service_worker/fetch_handler"));
  // Fetch handler source starts the ServiceWorker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version()->running_status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceFromNetworkRule) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/direct').then(response => "
                   "response.text())"));
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount("/service_worker/direct"));
  // Network fallback doesn't start the ServiceWorker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version()->running_status());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceRaceNetworkAndFetch) {
  SetupAndRegisterServiceWorker(TestType::kRaceNetworkAndFetch);
  WorkerRunningStatusObserver observer(public_context());
  // If the race happens, we expect the network access happens twice;
  // the browser process directly fetch from the network, and the SW fetch
  // handler calls fetch API to fetch from the network.  The latter one
  // can be deduped, and enforcing clone to avoid the dedupe.
  const std::string relative_url =
      "/service_worker/race_network_and_fetch?sw_clone_pass_through";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the race network",
            GetInnerText());

  while (GetRequestCount(relative_url) != 2) {
    base::RunLoop().RunUntilIdle();
  }
  // two fetches for clone.
  EXPECT_EQ(2, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceRaceNetworkAndFetch) {
  SetupAndRegisterServiceWorker(TestType::kRaceNetworkAndFetch);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  // If the race happens, we expect the network access happens twice;
  // the browser process directly fetch from the network, and the SW fetch
  // handler calls fetch API to fetch from the network.  The latter one
  // can be deduped, and enforcing clone to avoid the dedupe.
  const std::string relative_url =
      "/service_worker/race_network_and_fetch?sw_clone_pass_through";
  EXPECT_EQ(
      "[ServiceWorkerStaticRouter] Response from the race network",
      EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                        "')"
                                        ".then(response => response.text())"));

  while (GetRequestCount(relative_url) != 2) {
    base::RunLoop().RunUntilIdle();
  }
  // two fetches for clone.
  EXPECT_EQ(2, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceRunningStatus) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/direct_if_not_running";

  // Initial state is stopped, and should go to network.
  ASSERT_EQ(blink::EmbeddedWorkerStatus::kStopped, version()->running_status());
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Since the ServiceWorker will start after using static routing,
  // wait for that.
  observer.WaitUntilRunning();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version()->running_status());

  // Ensure that the fetch handler is used this time.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the fetch handler",
            GetInnerText());
  // The result should not be got from the network, and access count
  // should not change.
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceRunningStatus) {
  const std::string relative_url = "/service_worker/direct_if_not_running";
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());
  ASSERT_EQ(blink::EmbeddedWorkerStatus::kStopped, version()->running_status());
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                              "').then(response => "
                                              "response.text())"));
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Start service worker, and the result should eventually got from
  // the fetch handler.
  EXPECT_EQ(StartServiceWorker(version().get()),
            blink::ServiceWorkerStatusCode::kOk);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version()->running_status());
  // We know there is a delay to make the renderer know the running
  // status. Or, test will timeout.
  for (;;) {
    auto result = EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                                    "').then(response => "
                                                    "response.text())");
    if (result != "[ServiceWorkerStaticRouter] Response from the network") {
      EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the fetch handler",
                result);
      break;
    }
  }
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceCacheStorageHit) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/cache_hit";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the cache",
            GetInnerText());
  // The result should be got from the cache.
  EXPECT_EQ(0, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceCacheStorageMissThenNetworkFallback) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/cache_miss";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kNetwork));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceCacheStorageHitWithName) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/cache_with_name";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the cache",
            GetInnerText());
  // The result should be got from the cache.
  EXPECT_EQ(0, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       MainResourceCacheStorageMissDueToNameMismatch) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);

  // UKM records reloads conducted in `ReloadBlockUntilNavigationComplete`.
  // Remove them to make sue UKM only has record for main test.
  test_ukm_recorder().Purge();

  const std::string relative_url = "/service_worker/cache_with_wrong_name";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // Due to the cache miss, the result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Check if the ukm shows the expected matched / actual source
  auto entries = test_ukm_recorder().GetEntriesByName(
      MainResourceLoadCompletedUkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);

  auto* entry = entries[0].get();

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry,
      MainResourceLoadCompletedUkmEntry::kMatchedFirstRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kCache));

  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entry, MainResourceLoadCompletedUkmEntry::kActualRouterSourceTypeName,
      static_cast<std::int64_t>(
          network::mojom::ServiceWorkerRouterSourceType::kNetwork));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceCacheStorageHit) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());

  const std::string relative_url = "/service_worker/cache_hit";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the cache",
            EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                              "').then(response => "
                                              "response.text())"));
  // The result should be got from the cache, and no network access is
  // expected.
  EXPECT_EQ(0, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceCacheStorageMiss) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());

  const std::string relative_url = "/service_worker/cache_miss";
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                              "').then(response => "
                                              "response.text())"));
  // Due to the cache miss, the result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceCacheStorageHitWithName) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());

  const std::string relative_url = "/service_worker/cache_with_name";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the cache",
            EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                              "').then(response => "
                                              "response.text())"));
  // The result should be got from the cache, and no network access is
  // expected.
  EXPECT_EQ(0, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       SubresourceCacheStorageMissDueToNameMissmatch) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());

  const std::string relative_url = "/service_worker/cache_with_wrong_name";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(), "fetch('" + relative_url +
                                              "').then(response => "
                                              "response.text())"));
  // Due to the cache miss, the result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest,
                       FetchEventShouldBeUsedWithFetchHandler) {
  StartServerAndNavigateToSetup();

  const GURL create_service_worker_url(embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html"));

  WorkerRunningStatusObserver observer1(public_context());
  ASSERT_TRUE(NavigateToURL(shell(), create_service_worker_url));
  ASSERT_EQ("DONE",
            EvalJs(GetPrimaryMainFrame(),
                   "register('/service_worker/static_router_no_handler.js')"));
  observer1.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer1.version_id());
  // Expected to raise during addRoutes.
  // No route should be added.
  EXPECT_FALSE(version->router_evaluator());
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest, MainResourceNot) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/not_not_match";
  WaitUntilRelativeUrlStoredInCache(relative_url);
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));
}

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterBrowserTest, SubresourceNot) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  ReloadBlockUntilNavigationsComplete(shell(), 1);
  StopServiceWorker(version().get());
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/not_not_match').then(response => "
                   "response.text())"));
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount("/service_worker/not_not_match"));
  // Network fallback doesn't start the ServiceWorker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version()->running_status());
}

// Test class for static routing API, if disables starting the ServiceWorker
// automatically when the request matches the registered route.
class ServiceWorkerStaticRouterDisablingServiceWorkerStartBrowserTest
    : public ServiceWorkerStaticRouterBrowserTest {
 public:
  ServiceWorkerStaticRouterDisablingServiceWorkerStartBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kServiceWorkerStaticRouter,
         blink::features::kServiceWorkerStaticRouterNotConditionEnabled},
        {features::kServiceWorkerStaticRouterStartServiceWorker});
  }
  ~ServiceWorkerStaticRouterDisablingServiceWorkerStartBrowserTest() override =
      default;
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(
    ServiceWorkerStaticRouterDisablingServiceWorkerStartBrowserTest,
    MainResourceNetworkFallback) {
  SetupAndRegisterServiceWorker(TestType::kNetwork);
  WorkerRunningStatusObserver observer(public_context());
  const std::string relative_url = "/service_worker/direct";
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(relative_url)));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  // The result should be got from the network.
  EXPECT_EQ(1, GetRequestCount(relative_url));

  // Ensure the ServiceWorker won't start if the navigation results in the
  // fallback via Static Routing.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version()->running_status());
  histogram_tester().ExpectBucketCount(
      "ServiceWorker.StartWorker.Purpose",
      static_cast<int>(ServiceWorkerMetrics::EventType::STATIC_ROUTER), 0);
}

class ServiceWorkerStaticRouterOriginTrialBrowserTest
    : public ServiceWorkerStaticRouterBrowserTest {
 public:
  ServiceWorkerStaticRouterOriginTrialBrowserTest() {
    // Explicitly disable the feature to ensure the feature is enabled by the
    // Origin Trial token.
    feature_list_.InitWithFeatures(
        {blink::features::kServiceWorkerStaticRouterNotConditionEnabled},
        {features::kServiceWorkerStaticRouter});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // The public key for the default privatey key used by the
    // tools/origin_trials/generate_token.py tool.
    static constexpr char kOriginTrialTestPublicKey[] =
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=";
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kOriginTrialTestPublicKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerStaticRouterOriginTrialBrowserTest,
                       Fallback) {
  embedded_test_server()->StartAcceptingConnections();

  // The URL that was used to register the Origin Trial token.
  static constexpr char kOriginUrl[] = "https://127.0.0.1:44444";
  // Generated by running (in tools/origin_trials):
  // tools/origin_trials/generate_token.py https://127.0.0.1:44444 \
  // ServiceWorkerBypassFetchHandlerWithRaceNetworkRequest \
  // --expire-timestamp=2000000000
  static constexpr char kOriginTrialToken[] =
      "AzX0kWd3mFeKWSeRT8ffvcxWodihqbB/WzApFm94qJPZmSbhO2a6fLLk1ilkzxf88qOOYI/"
      "TYr+"
      "K9VKvgI4w3QgAAABjeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmV"
      "hdHVyZSI6ICJTZXJ2aWNlV29ya2VyU3RhdGljUm91dGVyIiwgImV4cGlyeSI6IDIwMDAwMDA"
      "wMDB9";

  const GURL registation_url(
      base::StrCat({kOriginUrl, "/create_service_worker.html"}));
  const GURL network_fallback_url(
      base::StrCat({kOriginUrl, "/service_worker/direct"}));
  const GURL service_worker_url(
      base::StrCat({kOriginUrl, "/static_router.js"}));

  std::map<GURL, int /* number_of_invocations */> expected_request_urls = {
      {registation_url, 1},
      {network_fallback_url, 2},
      {service_worker_url, 1},
  };

  base::RunLoop run_loop;

  // The origin trial token is associated with an origin. We can't guarantee the
  // EmbeddedTestServer to use a specific port. So the URLLoaderInterceptor is
  // used instead.
  URLLoaderInterceptor service_worker_loader(base::BindLambdaForTesting(
      [&](URLLoaderInterceptor::RequestParams* params) {
        auto it = expected_request_urls.find(params->url_request.url);
        if (it == expected_request_urls.end()) {
          return false;
        }

        const std::string content_type =
            base::EndsWith(params->url_request.url.path_piece(), ".js")
                ? "text/javascript"
                : "text/html";

        const std::string origin_trial_token =
            params->url_request.url == service_worker_url ? kOriginTrialToken
                                                          : "";

        const std::string headers = base::ReplaceStringPlaceholders(
            "HTTP/1.1 200 OK\n"
            "Content-type: $1\n"
            "Origin-Trial: $2\n"
            "\n",
            {content_type, origin_trial_token}, {});

        if (base::Contains(params->url_request.url.path(), "/direct")) {
          const std::string body =
              "[ServiceWorkerStaticRouter] Response from the network";
          URLLoaderInterceptor::WriteResponse(
              headers, body, params->client.get(),
              std::optional<net::SSLInfo>(), params->url_request.url);
        } else {
          URLLoaderInterceptor::WriteResponse(
              "content/test/data/service_worker" +
                  params->url_request.url.path(),
              params->client.get(), &headers, std::optional<net::SSLInfo>(),
              params->url_request.url);
        }

        if (--it->second == 0) {
          expected_request_urls.erase(it);
        }
        if (expected_request_urls.empty()) {
          run_loop.Quit();
        }
        return true;
      }));

  // Register a service worker.
  WorkerRunningStatusObserver observer(public_context());
  EXPECT_TRUE(NavigateToURL(shell(), registation_url));
  EXPECT_EQ("DONE",
            EvalJs(GetPrimaryMainFrame(), "register('/static_router.js')"));
  observer.WaitUntilRunning();
  scoped_refptr<ServiceWorkerVersion> version =
      wrapper()->GetLiveVersion(observer.version_id());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());

  // The result should be got from the network, both for main resource and
  // subresource.
  EXPECT_TRUE(NavigateToURL(shell(), network_fallback_url));
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            GetInnerText());
  EXPECT_EQ("[ServiceWorkerStaticRouter] Response from the network",
            EvalJs(GetPrimaryMainFrame(),
                   "fetch('/service_worker/direct').then(response => "
                   "response.text())"));
}
}  // namespace content
