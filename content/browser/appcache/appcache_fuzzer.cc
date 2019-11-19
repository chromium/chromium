// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "content/browser/appcache/appcache_fuzzer.pb.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/test/test_url_loader_factory.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace content {

namespace {

struct Env {
  Env()
      : task_environment((base::CommandLine::Init(0, nullptr),
                          TestTimeouts::Initialize(),
                          base::test::TaskEnvironment::MainThreadType::IO)) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    test_content_client = std::make_unique<TestContentClient>();
    test_content_browser_client = std::make_unique<TestContentBrowserClient>();
    SetContentClient(test_content_client.get());
    SetBrowserClientForTesting(test_content_browser_client.get());
    CHECK(base::i18n::InitializeICU());
  }

  void InitializeAppCacheService(
      network::TestURLLoaderFactory* mock_url_loader_factory) {
    appcache_service = base::MakeRefCounted<ChromeAppCacheService>(
        /*proxy=*/nullptr, /*partition=*/nullptr);

    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ChromeAppCacheService::Initialize, appcache_service,
                       base::FilePath(), &test_browser_context,
                       /*special_storage_policy=*/nullptr));
    task_environment.RunUntilIdle();
  }

  BrowserTaskEnvironment task_environment;
  scoped_refptr<ChromeAppCacheService> appcache_service;
  std::unique_ptr<TestContentClient> test_content_client;
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client;
  TestBrowserContext test_browser_context;

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

Env& SingletonEnv() {
  static base::NoDestructor<Env> env;
  return *env;
}

void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    headers += '\0';
}

std::string GetUrl(const fuzzing::proto::Url& url) {
  if (url.url_test_case_idx() == fuzzing::proto::EMPTY) {
    return "";
  }

  return "http://localhost/" + std::to_string(url.url_test_case_idx());
}

std::string GetManifest(
    const fuzzing::proto::ManifestResponse& manifest_response) {
  std::string response = "CACHE MANIFEST\n";
  for (const fuzzing::proto::Url& url : manifest_response.urls()) {
    response += GetUrl(url) + "\n";
  }
  return response;
}

void DoRequest(network::TestURLLoaderFactory* factory,
               const fuzzing::proto::Url& url,
               uint32_t code,
               bool do_not_cache,
               const fuzzing::proto::ManifestResponse& manifest_response) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;

  auto response = network::mojom::URLResponseHead::New();

  std::string headers = "HTTP/1.1 ";
  headers += std::to_string(code);
  headers += "\n";

  if (do_not_cache) {
    headers += "Cache-Control: no-cache, no-store, must-revalidate\n";
    headers += "Pragma: no-cache\n";
    headers += "Expires: 0\n";
    headers += "DNT: 1\n";
  }
  HeadersToRaw(&headers);

  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(headers);

  // To simplify the fuzzer, we respond to all requests with a manifest.
  // When we're performing a manifest fetch, this data will affect the
  // control flow of the appcache code. Otherwise it's just treated like
  // a blob of data.
  std::string content = GetManifest(manifest_response);
  content += "\n# ";

  factory->SimulateResponseForPendingRequest(
      GURL(GetUrl(url)), status, std::move(response), content,
      network::TestURLLoaderFactory::kUrlMatchPrefix);
}

}  // anonymous namespace

DEFINE_BINARY_PROTO_FUZZER(const fuzzing::proto::Session& session) {
  network::TestURLLoaderFactory mock_url_loader_factory;
  SingletonEnv().InitializeAppCacheService(&mock_url_loader_factory);

  // Create a context for mojo::ReportBadMessage.
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  mojo::Remote<blink::mojom::AppCacheBackend> host;
  SingletonEnv().appcache_service->CreateBackend(
      /*process_id=*/1, /*routing_id=*/MSG_ROUTING_NONE,
      host.BindNewPipeAndPassReceiver());

  std::map<int, mojo::Remote<blink::mojom::AppCacheHost>> registered_hosts;
  std::map<int, base::UnguessableToken> registered_host_ids_;
  for (const fuzzing::proto::Command& command : session.commands()) {
    switch (command.command_case()) {
      case fuzzing::proto::Command::kRegisterHost: {
        int32_t host_id = command.register_host().host_id();
        auto& host_id_token = registered_host_ids_[host_id];
        if (host_id_token.is_empty())
          host_id_token = base::UnguessableToken::Create();
        mojo::PendingRemote<blink::mojom::AppCacheFrontend> frontend;
        ignore_result(frontend.InitWithNewPipeAndPassReceiver());
        host->RegisterHost(
            registered_hosts[host_id].BindNewPipeAndPassReceiver(),
            std::move(frontend), host_id_token);
        break;
      }
      case fuzzing::proto::Command::kUnregisterHost: {
        int32_t host_id = command.unregister_host().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;

        registered_hosts.erase(host_it);
        break;
      }
      case fuzzing::proto::Command::kSelectCache: {
        int32_t host_id = command.select_cache().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        int32_t from_id = command.select_cache().from_id();
        GURL document_url = GURL(GetUrl(command.select_cache().document_url()));
        GURL opt_manifest_url =
            GURL(GetUrl(command.select_cache().opt_manifest_url()));

        host_it->second->SelectCache(document_url, from_id, opt_manifest_url);
        break;
      }
      case fuzzing::proto::Command::kSetSpawningHostId: {
        int32_t host_id = command.set_spawning_host_id().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        int32_t spawning_host_id =
            command.set_spawning_host_id().spawning_host_id();
        auto& spawning_host_id_token = registered_host_ids_[spawning_host_id];
        if (spawning_host_id_token.is_empty())
          spawning_host_id_token = base::UnguessableToken::Create();
        host_it->second->SetSpawningHostId(spawning_host_id_token);
        break;
      }
      case fuzzing::proto::Command::kSelectCacheForWorker: {
        int32_t host_id = command.select_cache_for_worker().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        int64_t cache_document_was_loaded_from =
            command.select_cache_for_worker().cache_document_was_loaded_from();
        host_it->second->SelectCacheForWorker(cache_document_was_loaded_from);
        break;
      }
      case fuzzing::proto::Command::kMarkAsForeignEntry: {
        int32_t host_id = command.mark_as_foreign_entry().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        GURL url(GetUrl(command.mark_as_foreign_entry().document_url()));
        int64_t cache_document_was_loaded_from =
            command.mark_as_foreign_entry().cache_document_was_loaded_from();
        host_it->second->MarkAsForeignEntry(url,
                                            cache_document_was_loaded_from);
        break;
      }
      case fuzzing::proto::Command::kGetStatus: {
        int32_t host_id = command.get_status().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        host_it->second->GetStatus(base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kStartUpdate: {
        int32_t host_id = command.start_update().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        host_it->second->StartUpdate(base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kSwapCache: {
        int32_t host_id = command.swap_cache().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        host_it->second->SwapCache(base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kGetResourceList: {
        int32_t host_id = command.get_resource_list().host_id();
        auto host_it = registered_hosts.find(host_id);
        if (host_it == registered_hosts.end())
          break;
        host_it->second->GetResourceList(base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kDoRequest: {
        uint32_t code = command.do_request().http_code();
        bool do_not_cache = command.do_request().do_not_cache();
        const fuzzing::proto::ManifestResponse& manifest_response =
            command.do_request().manifest_response();
        DoRequest(&mock_url_loader_factory, command.do_request().url(), code,
                  do_not_cache, manifest_response);
        break;
      }
      case fuzzing::proto::Command::kRunUntilIdle: {
        SingletonEnv().task_environment.RunUntilIdle();
        break;
      }
      case fuzzing::proto::Command::COMMAND_NOT_SET: {
        break;
      }
    }
  }

  host.reset();
  // TODO(nedwilliamson): Investigate removing this or reinitializing
  // the appcache service as a fuzzer command.
  SingletonEnv().task_environment.RunUntilIdle();
}

}  // namespace content
