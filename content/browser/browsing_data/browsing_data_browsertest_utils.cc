// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

namespace browsing_data_browsertest_utils {

namespace {

void AddServiceWorkerCallback(blink::ServiceWorkerStatusCode status) {
  ASSERT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
}

void GetServiceWorkersCallback(
    base::OnceClosure callback,
    std::vector<StorageUsageInfo>* out_service_workers,
    const std::vector<StorageUsageInfo>& service_workers) {
  *out_service_workers = service_workers;
  std::move(callback).Run();
}

}  // namespace

// static
void ServiceWorkerActivationObserver::SignalActivation(
    ServiceWorkerContextWrapper* context,
    base::OnceClosure callback) {
  new ServiceWorkerActivationObserver(context, std::move(callback));
}

ServiceWorkerActivationObserver::ServiceWorkerActivationObserver(
    ServiceWorkerContextWrapper* context,
    base::OnceClosure callback)
    : context_(context),
      callback_(std::move(callback)) {
  scoped_observation_.Observe(context);
}

ServiceWorkerActivationObserver::~ServiceWorkerActivationObserver() {}

void ServiceWorkerActivationObserver::OnVersionStateChanged(
    int64_t version_id,
    const GURL& scope,
    const blink::StorageKey& key,
    ServiceWorkerVersion::Status) {
  if (context_->GetLiveVersion(version_id)->status() ==
      ServiceWorkerVersion::ACTIVATED) {
    std::move(callback_).Run();
    delete this;
  }
}

void SetIgnoreCertificateErrors(base::CommandLine* command_line) {
  if (IsOutOfProcessNetworkService()) {
    // |MockCertVerifier| only seems to work when Network Service was enabled.
    command_line->AppendSwitch(switches::kUseMockCertVerifierForTesting);
  } else {
    // We're redirecting all hosts to localhost even on HTTPS, so we'll get
    // certificate errors.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
}

GURL AddServiceWorker(const std::string& origin,
                      StoragePartition* storage_partition,
                      net::EmbeddedTestServer* https_server) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage_partition->GetServiceWorkerContext());

  GURL scope_url = https_server->GetURL(origin, "/");
  GURL js_url = https_server->GetURL(origin, "/?file=worker.js");

  // Register the worker.
  blink::mojom::ServiceWorkerRegistrationOptions options(
      scope_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  service_worker_context->RegisterServiceWorker(
      js_url, key, options, base::BindOnce(&AddServiceWorkerCallback));

  // Wait for its activation.
  base::RunLoop run_loop;
  ServiceWorkerActivationObserver::SignalActivation(service_worker_context,
                                                    run_loop.QuitClosure());
  run_loop.Run();
  return scope_url;
}

std::vector<StorageUsageInfo> GetServiceWorkers(
    StoragePartition* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          storage_partition->GetServiceWorkerContext());

  std::vector<StorageUsageInfo> service_workers;
  base::RunLoop run_loop;

  service_worker_context->GetAllStorageKeysInfo(
      base::BindOnce(&GetServiceWorkersCallback, run_loop.QuitClosure(),
                     base::Unretained(&service_workers)));
  run_loop.Run();

  return service_workers;
}

void SetResponseContent(const GURL& url,
                        std::string* value,
                        net::test_server::BasicHttpResponse* response) {
  if (net::GetValueForKeyInQuery(url, "file", value)) {
    base::FilePath path(GetTestFilePath("browsing_data", value->c_str()));
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    EXPECT_TRUE(file.IsValid());

    int64_t length = file.GetLength();
    EXPECT_GE(length, 0);

    auto buffer = base::HeapArray<uint8_t>::WithSize(length);
    file.Read(0, buffer);

    if (path.Extension() == FILE_PATH_LITERAL(".js"))
      response->set_content_type("application/javascript");
    else if (path.Extension() == FILE_PATH_LITERAL(".html"))
      response->set_content_type("text/html");
    else
      NOTREACHED_IN_MIGRATION();

    response->set_content(base::as_string_view(buffer));
  }
}

void SetUpMockCertVerifier(int32_t default_result) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  network_service_test->MockCertVerifierSetDefaultResult(
      default_result, run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace browsing_data_browsertest_utils

}  // namespace content
