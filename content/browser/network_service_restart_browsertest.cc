// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_message_filter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/test/storage_partition_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/test/ppapi_test_utils.h"
#endif

namespace content {

namespace {

using SharedURLLoaderFactoryGetterCallback =
    base::OnceCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

network::mojom::NetworkContextPtr CreateNetworkContext() {
  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  GetNetworkService()->CreateNetworkContext(mojo::MakeRequest(&network_context),
                                            std::move(context_params));
  return network_context;
}

int LoadBasicRequestOnUIThread(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;

  SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();
  return simple_loader->NetError();
}

std::vector<network::mojom::NetworkUsagePtr> GetTotalNetworkUsages() {
  std::vector<network::mojom::NetworkUsagePtr> network_usages;
  base::RunLoop run_loop;
  GetNetworkService()->GetTotalNetworkUsages(base::BindOnce(
      [](std::vector<network::mojom::NetworkUsagePtr>* p_network_usages,
         base::OnceClosure quit_closure,
         std::vector<network::mojom::NetworkUsagePtr> returned_usages) {
        *p_network_usages = std::move(returned_usages);
        std::move(quit_closure).Run();
      },
      base::Unretained(&network_usages), run_loop.QuitClosure()));
  run_loop.Run();
  return network_usages;
}

bool CheckContainsProcessID(
    const std::vector<network::mojom::NetworkUsagePtr>& usages,
    int process_id) {
  for (const auto& usage : usages) {
    if ((int)usage->process_id == process_id)
      return true;
  }
  return false;
}

// Wait until |condition| returns true.
void WaitForCondition(base::RepeatingCallback<bool()> condition) {
  while (!condition.Run()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

class ServiceWorkerStatusObserver : public ServiceWorkerContextCoreObserver {
 public:
  void WaitForState(EmbeddedWorkerStatus expected_status) {
    if (latest_status_ == expected_status)
      return;

    expected_status_ = expected_status;
    base::RunLoop loop;
    callback_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  void OnRunningStateChanged(int64_t version_id,
                             EmbeddedWorkerStatus running_status) override {
    latest_status_ = running_status;
    if (expected_status_.has_value() &&
        running_status == expected_status_.value()) {
      std::move(callback_).Run();
    }
  }

  base::Optional<EmbeddedWorkerStatus> expected_status_;
  EmbeddedWorkerStatus latest_status_;
  base::OnceClosure callback_;
};

}  // namespace

// This test source has been excluded from Android as Android doesn't have
// out-of-process Network Service.
class NetworkServiceRestartBrowserTest : public ContentBrowserTest {
 public:
  NetworkServiceRestartBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        network::features::kNetworkService);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(ENABLE_PLUGINS)
    ASSERT_TRUE(ppapi::RegisterCorbTestPlugin(command_line));
#endif
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&NetworkServiceRestartBrowserTest::MonitorRequest,
                            base::Unretained(this)));
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
    ContentBrowserTest::SetUpOnMainThread();
  }

  GURL GetTestURL() const {
    // Use '/echoheader' instead of '/echo' to avoid a disk_cache bug.
    // See https://crbug.com/792255.
    return embedded_test_server()->GetURL("/echoheader");
  }

  BrowserContext* browser_context() {
    return shell()->web_contents()->GetBrowserContext();
  }

  RenderFrameHostImpl* main_frame() {
    return static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
  }

  bool CheckCanLoadHttp(Shell* shell, const std::string& relative_url) {
    GURL test_url = embedded_test_server()->GetURL(relative_url);
    std::string script(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '");
    script += test_url.spec() +
              "', true);"
              "xhr.onload = function (e) {"
              "  if (xhr.readyState === 4) {"
              "    window.domAutomationController.send(xhr.status === 200);"
              "  }"
              "};"
              "xhr.onerror = function () {"
              "  window.domAutomationController.send(false);"
              "};"
              "xhr.send(null)";
    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell, script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Will reuse the single opened windows through the test case.
  bool CheckCanLoadHttpInWindowOpen(const std::string& relative_url) {
    GURL test_url = embedded_test_server()->GetURL(relative_url);
    std::string inject_script = base::StringPrintf(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '%s', true);"
        "xhr.onload = function (e) {"
        "  if (xhr.readyState === 4) {"
        "    window.opener.domAutomationController.send(xhr.status === 200);"
        "  }"
        "};"
        "xhr.onerror = function () {"
        "  window.opener.domAutomationController.send(false);"
        "};"
        "xhr.send(null)",
        test_url.spec().c_str());
    std::string window_open_script = base::StringPrintf(
        "var new_window = new_window || window.open('');"
        "var inject_script = document.createElement('script');"
        "inject_script.innerHTML = \"%s\";"
        "new_window.document.body.appendChild(inject_script);",
        inject_script.c_str());

    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), window_open_script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Workers will live throughout the test case unless terminated.
  bool CheckCanWorkerFetch(const std::string& worker_name,
                           const std::string& relative_url) {
    GURL worker_url =
        embedded_test_server()->GetURL("/workers/worker_common.js");
    GURL fetch_url = embedded_test_server()->GetURL(relative_url);
    std::string script = base::StringPrintf(
        "var workers = workers || {};"
        "var worker_name = '%s';"
        "workers[worker_name] = workers[worker_name] || new Worker('%s');"
        "workers[worker_name].onmessage = evt => {"
        "  if (evt.data != 'wait')"
        "    window.domAutomationController.send(evt.data === 200);"
        "};"
        "workers[worker_name].postMessage(\"eval "
        "  fetch(new Request('%s'))"
        "    .then(res => postMessage(res.status))"
        "    .catch(error => postMessage(error.toString()));"
        "  'wait'"
        "\");",
        worker_name.c_str(), worker_url.spec().c_str(),
        fetch_url.spec().c_str());
    bool fetch_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), script, &fetch_result);
    return fetch_result && execute_result;
  }

  // Terminate and delete the worker.
  bool TerminateWorker(const std::string& worker_name) {
    std::string script = base::StringPrintf(
        "var workers = workers || {};"
        "var worker_name = '%s';"
        "if (workers[worker_name]) {"
        "  workers[worker_name].terminate();"
        "  delete workers[worker_name];"
        "  window.domAutomationController.send(true);"
        "} else {"
        "  window.domAutomationController.send(false);"
        "}",
        worker_name.c_str());
    bool fetch_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(shell(), script, &fetch_result);
    return fetch_result && execute_result;
  }

  // Called by |embedded_test_server()|.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    last_request_relative_url_ = request.relative_url;
  }

  std::string last_request_relative_url() const {
    return last_request_relative_url_;
  }

 private:
  std::string last_request_relative_url_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceRestartBrowserTest);
};

IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       NetworkServiceProcessRecovery) {
  network::mojom::NetworkContextPtr network_context = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context.get(), GetTestURL()));
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_FALSE(network_context.encountered_error());

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // |network_context| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the pointer to make sure
  // the notification has been received.
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());
  // Make sure we could get |net::ERR_FAILED| with an invalid |network_context|.
  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequest(network_context.get(), GetTestURL()));

  // NetworkService should restart automatically and return valid interface.
  network::mojom::NetworkContextPtr network_context2 = CreateNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(network_context2.get(), GetTestURL()));
  EXPECT_TRUE(network_context2.is_bound());
  EXPECT_FALSE(network_context2.encountered_error());
}

void IncrementInt(int* i) {
  *i = *i + 1;
}

// This test verifies basic functionality of RegisterNetworkServiceCrashHandler
// and UnregisterNetworkServiceCrashHandler.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, CrashHandlers) {
  network::mojom::NetworkContextPtr network_context = CreateNetworkContext();
  EXPECT_TRUE(network_context.is_bound());

  // Register 2 crash handlers.
  int counter1 = 0;
  int counter2 = 0;
  NetworkServiceCrashHandlerId handler_id1 = RegisterNetworkServiceCrashHandler(
      base::BindRepeating(&IncrementInt, base::Unretained(&counter1)));
  NetworkServiceCrashHandlerId handler_id2 = RegisterNetworkServiceCrashHandler(
      base::BindRepeating(&IncrementInt, base::Unretained(&counter2)));

  // Crash the NetworkService process.
  SimulateNetworkServiceCrash();
  // |network_context| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the pointer to make sure
  // the notification has been received.
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());

  // Verify the crash handlers executed.
  EXPECT_EQ(1, counter1);
  EXPECT_EQ(1, counter2);

  // Revive the NetworkService process.
  network_context = CreateNetworkContext();
  EXPECT_TRUE(network_context.is_bound());

  // Unregister one of the handlers.
  UnregisterNetworkServiceCrashHandler(handler_id2);

  // Crash the NetworkService process.
  SimulateNetworkServiceCrash();
  // |network_context| will receive an error notification, but it's not
  // guaranteed to have arrived at this point. Flush the pointer to make sure
  // the notification has been received.
  network_context.FlushForTesting();
  EXPECT_TRUE(network_context.is_bound());
  EXPECT_TRUE(network_context.encountered_error());

  // Verify only the first crash handler executed.
  EXPECT_EQ(2, counter1);
  EXPECT_EQ(1, counter2);

  // Test cleanup.
  UnregisterNetworkServiceCrashHandler(handler_id1);
}

// Make sure |StoragePartitionImpl::GetNetworkContext()| returns valid interface
// after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       StoragePartitionImplGetNetworkContext) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  network::mojom::NetworkContext* old_network_context =
      partition->GetNetworkContext();
  EXPECT_EQ(net::OK, LoadBasicRequest(old_network_context, GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // |partition->GetNetworkContext()| should return a valid new pointer after
  // crash.
  EXPECT_NE(old_network_context, partition->GetNetworkContext());
  EXPECT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), GetTestURL()));
}

// Make sure |URLLoaderFactoryGetter| returns valid interface after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       URLLoaderFactoryGetterGetNetworkFactory) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter =
      partition->url_loader_factory_getter();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      url_loader_factory_getter.get());
  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  url_loader_factory_getter->FlushNetworkInterfaceOnIOThreadForTesting();

  // |url_loader_factory_getter| should be able to get a valid new pointer after
  // crash.
  factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      url_loader_factory_getter.get());
  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory returned from
// |URLLoaderFactoryGetter::GetNetworkFactory()| continues to work after
// crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserIOSharedURLLoaderFactory) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      partition->url_loader_factory_getter().get());

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  partition->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  // |shared_factory| should continue to work.
  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory returned from
// |URLLoaderFactoryGetter::GetNetworkFactory()| doesn't crash if
// it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserIOSharedFactoryAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context.get()));
  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      partition->url_loader_factory_getter().get());

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure basic navigation works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       NavigationURLLoaderBasic) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  partition->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
}

// Make sure basic XHR works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BasicXHR) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanLoadHttp(shell(), "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanLoadHttp(shell(), "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcess()| continues to work
// after crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BrowserUIFactory) {
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  auto* factory = partition->GetURLLoaderFactoryForBrowserProcess().get();

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory, GetTestURL()));

  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory, GetTestURL()));
}

// Make sure the factory returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcess()| doesn't crash if
// it's called after the StoragePartition is deleted.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       BrowserUIFactoryAfterStoragePartitionGone) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::unique_ptr<ShellBrowserContext> browser_context =
      std::make_unique<ShellBrowserContext>(true, nullptr);
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context.get());
  scoped_refptr<network::SharedURLLoaderFactory> factory(
      partition->GetURLLoaderFactoryForBrowserProcess());

  EXPECT_EQ(net::OK, LoadBasicRequestOnUIThread(factory.get(), GetTestURL()));

  browser_context.reset();

  EXPECT_EQ(net::ERR_FAILED,
            LoadBasicRequestOnUIThread(factory.get(), GetTestURL()));
}

// Make sure the factory info returned from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| can be
// used after crashes.
// Flaky on Windows. https://crbug.com/840127
#if defined(OS_WIN)
#define MAYBE_BrowserIOFactoryInfo DISABLED_BrowserIOFactoryInfo
#else
#define MAYBE_BrowserIOFactoryInfo BrowserIOFactoryInfo
#endif
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       MAYBE_BrowserIOFactoryInfo) {
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  auto shared_url_loader_factory_info =
      partition->GetURLLoaderFactoryForBrowserProcessIOThread();

  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  static_cast<StoragePartitionImpl*>(partition)
      ->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      std::move(shared_url_loader_factory_info));

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the factory constructed from
// |StoragePartition::GetURLLoaderFactoryForBrowserProcessIOThread()| continues
// to work after crashes.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, BrowserIOFactory) {
  auto* partition =
      BrowserContext::GetDefaultStoragePartition(browser_context());
  auto factory_owner = IOThreadSharedURLLoaderFactoryOwner::Create(
      partition->GetURLLoaderFactoryForBrowserProcessIOThread());

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));

  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  static_cast<StoragePartitionImpl*>(partition)
      ->url_loader_factory_getter()
      ->FlushNetworkInterfaceOnIOThreadForTesting();

  EXPECT_EQ(net::OK, factory_owner->LoadBasicRequestOnIOThread(GetTestURL()));
}

// Make sure the window from |window.open()| can load XHR after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, WindowOpenXHR) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanLoadHttpInWindowOpen("/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanLoadHttpInWindowOpen("/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure worker fetch works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, WorkerFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Make sure multiple workers are tracked correctly and work after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, MultipleWorkerFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL("/echo")));
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title1.html"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();
  // Flush the interface to make sure the frame host has received error
  // notification and the new URLLoaderFactoryBundle has been received by the
  // frame.
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  // Both workers should work after crash.
  EXPECT_TRUE(CheckCanWorkerFetch("worker1", "/title2.html"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");

  // Terminate "worker1". "worker2" shouldn't be affected.
  EXPECT_TRUE(TerminateWorker("worker1"));
  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title1.html"));
  EXPECT_EQ(last_request_relative_url(), "/title1.html");

  // Crash the NetworkService process again. "worker2" should still work.
  SimulateNetworkServiceCrash();
  partition->FlushNetworkInterfaceForTesting();
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  EXPECT_TRUE(CheckCanWorkerFetch("worker2", "/title2.html"));
  EXPECT_EQ(last_request_relative_url(), "/title2.html");
}

// Flaky on Linux TSan (https://crbug.com/889855)
#if defined(OS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_FetchFromServiceWorkerControlledPage_NoFetchHandler \
  DISABLED_FetchFromServiceWorkerControlledPage_NoFetchHandler
#else
#define MAYBE_FetchFromServiceWorkerControlledPage_NoFetchHandler \
  FetchFromServiceWorkerControlledPage_NoFetchHandler
#endif
// Make sure fetch from a page controlled by a service worker which doesn't have
// a fetch handler works after crash.
IN_PROC_BROWSER_TEST_F(
    NetworkServiceRestartBrowserTest,
    MAYBE_FetchFromServiceWorkerControlledPage_NoFetchHandler) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  ServiceWorkerStatusObserver observer;
  ServiceWorkerContextWrapper* service_worker_context =
      partition->GetServiceWorkerContext();
  service_worker_context->AddObserver(&observer);

  // Register a service worker which controls /service_worker/.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('empty.js')"));

  // Navigate to a controlled page.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html")));

  // Fetch from the controlled page.
  const std::string script = "fetch_from_page('/echo');";
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // Service worker should be stopped when network service crashes.
  observer.WaitForState(EmbeddedWorkerStatus::STOPPED);

  // Fetch from the controlled page again.
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  service_worker_context->RemoveObserver(&observer);
}

// Flaky on Linux TSan (https://crbug.com/889855)
#if defined(OS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_FetchFromServiceWorkerControlledPage_PassThrough \
  DISABLED_FetchFromServiceWorkerControlledPage_PassThrough
#else
#define MAYBE_FetchFromServiceWorkerControlledPage_PassThrough \
  FetchFromServiceWorkerControlledPage_PassThrough
#endif
// Make sure fetch from a page controlled by a service worker which has a fetch
// handler but falls back to the network works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       MAYBE_FetchFromServiceWorkerControlledPage_PassThrough) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  ServiceWorkerStatusObserver observer;
  ServiceWorkerContextWrapper* service_worker_context =
      partition->GetServiceWorkerContext();
  service_worker_context->AddObserver(&observer);

  // Register a service worker which controls /service_worker/.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('fetch_event_pass_through.js')"));

  // Navigate to a controlled page.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html")));

  // Fetch from the controlled page.
  const std::string script = "fetch_from_page('/echo');";
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // Service worker should be stopped when network service crashes.
  observer.WaitForState(EmbeddedWorkerStatus::STOPPED);

  // Fetch from the controlled page again.
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  service_worker_context->RemoveObserver(&observer);
}

// Flaky on Linux TSan (https://crbug.com/889855)
#if defined(OS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_FetchFromServiceWorkerControlledPage_RespondWithFetch \
  DISABLED_FetchFromServiceWorkerControlledPage_RespondWithFetch
#else
#define MAYBE_FetchFromServiceWorkerControlledPage_RespondWithFetch \
  FetchFromServiceWorkerControlledPage_RespondWithFetch
#endif
// Make sure fetch from a page controlled by a service worker which has a fetch
// handler and responds with fetch() works after crash.
IN_PROC_BROWSER_TEST_F(
    NetworkServiceRestartBrowserTest,
    MAYBE_FetchFromServiceWorkerControlledPage_RespondWithFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  ServiceWorkerStatusObserver observer;
  ServiceWorkerContextWrapper* service_worker_context =
      partition->GetServiceWorkerContext();
  service_worker_context->AddObserver(&observer);

  // Register a service worker which controls /service_worker/.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE",
            EvalJs(shell(), "register('fetch_event_respond_with_fetch.js')"));

  // Navigate to a controlled page.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html")));

  // Fetch from the controlled page.
  const std::string script = "fetch_from_page('/echo');";
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // Service worker should be stopped when network service crashes.
  observer.WaitForState(EmbeddedWorkerStatus::STOPPED);

  // Fetch from the controlled page again.
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  service_worker_context->RemoveObserver(&observer);
}

// Make sure fetch from service worker context works after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, ServiceWorkerFetch) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));
  ServiceWorkerStatusObserver observer;
  ServiceWorkerContextWrapper* service_worker_context =
      partition->GetServiceWorkerContext();
  service_worker_context->AddObserver(&observer);

  const GURL page_url = embedded_test_server()->GetURL(
      "/service_worker/fetch_from_service_worker.html");
  const GURL fetch_url = embedded_test_server()->GetURL("/echo");

  // Navigate to the page and register a service worker.
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ("ready", EvalJs(shell(), "setup();"));

  // Fetch from the service worker.
  const std::string script =
      "fetch_from_service_worker('" + fetch_url.spec() + "');";
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  // Crash the NetworkService process. Existing interfaces should receive error
  // notifications at some point.
  SimulateNetworkServiceCrash();
  // Flush the interface to make sure the error notification was received.
  partition->FlushNetworkInterfaceForTesting();

  // Service worker should be stopped when network service crashes.
  observer.WaitForState(EmbeddedWorkerStatus::STOPPED);

  // Fetch from the service worker again.
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  service_worker_context->RemoveObserver(&observer);
}

// Make sure shared workers terminate after crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, SharedWorker) {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context()));

  const GURL page_url =
      embedded_test_server()->GetURL("/workers/fetch_from_shared_worker.html");
  const GURL fetch_url = embedded_test_server()->GetURL("/echo");

  // Navigate to the page and prepare a shared worker.
  EXPECT_TRUE(NavigateToURL(shell(), page_url));

  // Fetch from the shared worker to ensure it has started.
  const std::string script =
      "fetch_from_shared_worker('" + fetch_url.spec() + "');";
  EXPECT_EQ("Echo", EvalJs(shell(), script));

  // There should be one worker host. We will later wait for it to terminate.
  SharedWorkerServiceImpl* service = partition->GetSharedWorkerService();
  EXPECT_EQ(1u, service->worker_hosts_.size());
  base::RunLoop loop;
  service->SetWorkerTerminationCallbackForTesting(loop.QuitClosure());

  // Crash the NetworkService process.
  SimulateNetworkServiceCrash();

  // Wait for the worker to detect the crash and self-terminate.
  loop.Run();
  EXPECT_TRUE(service->worker_hosts_.empty());
}

// Make sure the entry in |NetworkService::GetTotalNetworkUsages()| was cleared
// after process closed.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       GetNetworkUsagesClosed) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestURL()));
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, GetTestURL()));

  int process_id1 =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  int process_id2 =
      shell2->web_contents()->GetMainFrame()->GetProcess()->GetID();

  // Load resource on the renderer to make sure the traffic was recorded.
  EXPECT_TRUE(CheckCanLoadHttp(shell(), "/title2.html"));
  EXPECT_TRUE(CheckCanLoadHttp(shell2, "/title3.html"));

  // Both processes should have traffic recorded.
  auto network_usages = GetTotalNetworkUsages();
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id1));
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id2));

  // Closing |shell2| should cause the entry to be cleared.
  shell2->Close();
  shell2 = nullptr;

  // Wait until the Network Service has noticed the change. We don't have a
  // better way to force a flush on the Network Service side.
  WaitForCondition(base::BindRepeating(
      [](int process_id) {
        auto usages = GetTotalNetworkUsages();
        return !CheckContainsProcessID(usages, process_id);
      },
      process_id2));

  network_usages = GetTotalNetworkUsages();
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id1));
  EXPECT_FALSE(CheckContainsProcessID(network_usages, process_id2));
}

// Make sure |NetworkService::GetTotalNetworkUsages()| continues to work after
// crash. See 'network_usage_accumulator_unittest' for quantified tests.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest,
                       GetNetworkUsagesCrashed) {
  EXPECT_TRUE(NavigateToURL(shell(), GetTestURL()));
  Shell* shell2 = CreateBrowser();
  EXPECT_TRUE(NavigateToURL(shell2, GetTestURL()));

  int process_id1 =
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID();
  int process_id2 =
      shell2->web_contents()->GetMainFrame()->GetProcess()->GetID();

  // Load resource on the renderer to make sure the traffic was recorded.
  EXPECT_TRUE(CheckCanLoadHttp(shell(), "/title2.html"));
  EXPECT_TRUE(CheckCanLoadHttp(shell2, "/title3.html"));

  // Both processes should have traffic recorded.
  auto network_usages = GetTotalNetworkUsages();
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id1));
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id2));

  // Crashing Network Service should cause all entries to be cleared.
  SimulateNetworkServiceCrash();
  network_usages = GetTotalNetworkUsages();
  EXPECT_FALSE(CheckContainsProcessID(network_usages, process_id1));
  EXPECT_FALSE(CheckContainsProcessID(network_usages, process_id2));

  // Should still be able to recored new traffic after crash.
  EXPECT_TRUE(CheckCanLoadHttp(shell(), "/title2.html"));
  network_usages = GetTotalNetworkUsages();
  EXPECT_TRUE(CheckContainsProcessID(network_usages, process_id1));
  EXPECT_FALSE(CheckContainsProcessID(network_usages, process_id2));
}

// Make sure cookie access doesn't hang or fail after a network process crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, Cookies) {
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_TRUE(ExecuteScript(web_contents, "document.cookie = 'foo=bar';"));

  std::string cookie;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(document.cookie);",
      &cookie));
  EXPECT_EQ("foo=bar", cookie);

  SimulateNetworkServiceCrash();

  auto* process = web_contents->GetMainFrame()->GetProcess();
  scoped_refptr<RenderFrameMessageFilter> filter(
      static_cast<RenderProcessHostImpl*>(process)
          ->render_frame_message_filter_for_testing());
  // Need to use FlushAsyncForTesting instead of FlushForTesting because the IO
  // thread doesn't support nested message loops.
  base::RunLoop run_loop;
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindLambdaForTesting([&]() {
                             filter->GetCookieManager()->FlushAsyncForTesting(
                                 run_loop.QuitClosure());
                           }));

  // content_shell uses in-memory cookie database, so the value saved earlier
  // won't persist across crashes. What matters is that new access works.
  EXPECT_TRUE(ExecuteScript(web_contents, "document.cookie = 'foo=bar';"));

  // This will hang without the fix.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(document.cookie);",
      &cookie));
  EXPECT_EQ("foo=bar", cookie);
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Make sure that "trusted" plugins continue to be able to issue cross-origin
// requests after a network process crash.
IN_PROC_BROWSER_TEST_F(NetworkServiceRestartBrowserTest, Plugin) {
  auto* web_contents = shell()->web_contents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            embedded_test_server()->GetURL("/title1.html")));

  // Load the test plugin (see ppapi::RegisterFlashTestPlugin and
  // ppapi/tests/power_saver_test_plugin.cc).
  const char kLoadingScript[] = R"(
      var obj = document.createElement('object');
      obj.id = 'plugin';
      obj.data = 'test.swf';
      obj.type = 'application/x-shockwave-flash';
      obj.width = 400;
      obj.height = 400;

      document.body.appendChild(obj);
  )";
  ASSERT_TRUE(ExecJs(web_contents, kLoadingScript));

  // Ask the plugin to perform a cross-origin, CORB-eligible (i.e.
  // application/json + nosniff) URL request.  Plugins with universal access
  // should not be subject to CORS/CORB and so the request should go through.
  // See also https://crbug.com/874515 and https://crbug.com/846339.
  GURL cross_origin_url = embedded_test_server()->GetURL(
      "cross.origin.com", "/site_isolation/nosniff.json");
  const char kFetchScriptTemplate[] = R"(
      new Promise(function (resolve, reject) {
          var obj = document.getElementById('plugin');
          function callback(event) {
              // Ignore plugin messages unrelated to requestUrl.
              if (!event.data.startsWith('requestUrl: '))
                return;

              obj.removeEventListener('message', callback);
              resolve('msg-from-plugin: ' + event.data);
          };
          obj.addEventListener('message', callback);
          obj.postMessage('requestUrl: ' + $1);
      });
  )";
  std::string fetch_script = JsReplace(kFetchScriptTemplate, cross_origin_url);
  ASSERT_EQ(
      "msg-from-plugin: requestUrl: RESPONSE BODY: "
      "runMe({ \"name\" : \"chromium\" });\n",
      EvalJs(web_contents, fetch_script));

  // Crash the Network Service process and wait until host frame's
  // URLLoaderFactory has been refreshed.
  SimulateNetworkServiceCrash();
  main_frame()->FlushNetworkAndNavigationInterfacesForTesting();

  // Try the fetch again - it should still work (i.e. the mechanism for relaxing
  // CORB for universal-access-plugins should be resilient to network process
  // crashes).  See also https://crbug.com/891904.
  ASSERT_EQ(
      "msg-from-plugin: requestUrl: RESPONSE BODY: "
      "runMe({ \"name\" : \"chromium\" });\n",
      EvalJs(web_contents, fetch_script));
}
#endif

}  // namespace content
