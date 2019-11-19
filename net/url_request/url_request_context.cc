// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

#include <inttypes.h>

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/process_memory_dump.h"
#include "net/base/http_user_agent_settings.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/socket/ssl_client_socket_impl.h"
#include "net/url_request/url_request.h"

namespace net {

URLRequestContext::URLRequestContext(bool allow_copy)
    : net_log_(nullptr),
      host_resolver_(nullptr),
      cert_verifier_(nullptr),
      http_auth_handler_factory_(nullptr),
      proxy_resolution_service_(nullptr),
      proxy_delegate_(nullptr),
      ssl_config_service_(nullptr),
      network_delegate_(nullptr),
      http_server_properties_(nullptr),
      http_user_agent_settings_(nullptr),
      cookie_store_(nullptr),
      transport_security_state_(nullptr),
      cert_transparency_verifier_(nullptr),
      ct_policy_enforcer_(nullptr),
      http_transaction_factory_(nullptr),
      job_factory_(nullptr),
      throttler_manager_(nullptr),
      quic_context_(nullptr),
      network_quality_estimator_(nullptr),
#if BUILDFLAG(ENABLE_REPORTING)
      reporting_service_(nullptr),
      network_error_logging_service_(nullptr),
#endif  // BUILDFLAG(ENABLE_REPORTING)
      url_requests_(std::make_unique<std::set<const URLRequest*>>()),
      enable_brotli_(false),
      check_cleartext_permitted_(false),
      name_("unknown"),
      allow_copy_(allow_copy) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "URLRequestContext", base::ThreadTaskRunnerHandle::Get());
}

URLRequestContext::~URLRequestContext() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AssertNoURLRequests();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

const HttpNetworkSession::Params* URLRequestContext::GetNetworkSessionParams(
    ) const {
  HttpTransactionFactory* transaction_factory = http_transaction_factory();
  if (!transaction_factory)
    return nullptr;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  if (!network_session)
    return nullptr;
  return &network_session->params();
}

const HttpNetworkSession::Context* URLRequestContext::GetNetworkSessionContext()
    const {
  HttpTransactionFactory* transaction_factory = http_transaction_factory();
  if (!transaction_factory)
    return nullptr;
  HttpNetworkSession* network_session = transaction_factory->GetSession();
  if (!network_session)
    return nullptr;
  return &network_session->context();
}

#if (!defined(OS_WIN) && !defined(OS_LINUX)) || defined(OS_CHROMEOS)
std::unique_ptr<URLRequest> URLRequestContext::CreateRequest(
    const GURL& url,
    RequestPriority priority,
    URLRequest::Delegate* delegate) const {
  return CreateRequest(url, priority, delegate, MISSING_TRAFFIC_ANNOTATION);
}
#endif

std::unique_ptr<URLRequest> URLRequestContext::CreateRequest(
    const GURL& url,
    RequestPriority priority,
    URLRequest::Delegate* delegate,
    NetworkTrafficAnnotationTag traffic_annotation) const {
  return base::WrapUnique(new URLRequest(
      url, priority, delegate, this, network_delegate_, traffic_annotation));
}

void URLRequestContext::set_cookie_store(CookieStore* cookie_store) {
  cookie_store_ = cookie_store;
}

void URLRequestContext::AssertNoURLRequests() const {
  int num_requests = url_requests_->size();
  if (num_requests != 0) {
    // We're leaking URLRequests :( Dump the URL of the first one and record how
    // many we leaked so we have an idea of how bad it is.
    const URLRequest* request = *url_requests_->begin();
    int load_flags = request->load_flags();
    DEBUG_ALIAS_FOR_GURL(url_buf, request->url());
    base::debug::Alias(&num_requests);
    base::debug::Alias(&load_flags);
    CHECK(false) << "Leaked " << num_requests << " URLRequest(s). First URL: "
                 << request->url().spec().c_str() << ".";
  }
}

bool URLRequestContext::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  std::string dump_name =
      base::StringPrintf("net/url_request_context/%s/0x%" PRIxPTR,
                         name_.c_str(), reinterpret_cast<uintptr_t>(this));
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                  base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                  url_requests_->size());
  HttpTransactionFactory* transaction_factory = http_transaction_factory();
  if (transaction_factory) {
    HttpNetworkSession* network_session = transaction_factory->GetSession();
    if (network_session)
      network_session->DumpMemoryStats(pmd, dump->absolute_name());
    HttpCache* http_cache = transaction_factory->GetCache();
    if (http_cache)
      http_cache->DumpMemoryStats(pmd, dump->absolute_name());
  }
  if (cookie_store_) {
    cookie_store_->DumpMemoryStats(pmd, dump->absolute_name());
  }
  return true;
}

void URLRequestContext::CopyFrom(const URLRequestContext* other) {
  // Copy disallowed in cases known to not currently need it. New copying should
  // not be added as CopyFrom is fundamentally broken.
  DCHECK(allow_copy_ && other->allow_copy_);

  // Copy URLRequestContext parameters.
  set_net_log(other->net_log_);
  set_host_resolver(other->host_resolver_);
  set_cert_verifier(other->cert_verifier_);
  set_http_auth_handler_factory(other->http_auth_handler_factory_);
  set_proxy_resolution_service(other->proxy_resolution_service_);
  set_proxy_delegate(other->proxy_delegate_);
  set_ssl_config_service(other->ssl_config_service_);
  set_network_delegate(other->network_delegate_);
  set_http_server_properties(other->http_server_properties_);
  set_cookie_store(other->cookie_store_);
  set_transport_security_state(other->transport_security_state_);
  set_cert_transparency_verifier(other->cert_transparency_verifier_);
  set_ct_policy_enforcer(other->ct_policy_enforcer_);
  set_http_transaction_factory(other->http_transaction_factory_);
  set_job_factory(other->job_factory_);
  set_throttler_manager(other->throttler_manager_);
  set_quic_context(other->quic_context_);
  set_http_user_agent_settings(other->http_user_agent_settings_);
  set_network_quality_estimator(other->network_quality_estimator_);
#if BUILDFLAG(ENABLE_REPORTING)
  set_reporting_service(other->reporting_service_);
  set_network_error_logging_service(other->network_error_logging_service_);
#endif  // BUILDFLAG(ENABLE_REPORTING)
  set_enable_brotli(other->enable_brotli_);
  set_check_cleartext_permitted(other->check_cleartext_permitted_);
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  set_ftp_auth_cache(other->ftp_auth_cache_);
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)
}

}  // namespace net
