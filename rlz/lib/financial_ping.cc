// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Library functions related to the Financial Server ping.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/financial_ping.h"

#include <stdint.h>

#include <memory>

#include "base/atomicops.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/machine_id.h"
#include "rlz/lib/rlz_lib.h"
#include "rlz/lib/rlz_value_store.h"
#include "rlz/lib/string_utils.h"
#include "rlz/lib/time_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if !BUILDFLAG(IS_WIN)
#include "base/time/time.h"
#endif

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace rlz_lib {

using base::subtle::AtomicWord;

bool FinancialPing::FormRequest(Product product,
    const AccessPoint* access_points, const char* product_signature,
    const char* product_brand, const char* product_id,
    const char* product_lang, bool exclude_machine_id,
    std::string* request) {
  if (!request) {
    ASSERT_STRING("FinancialPing::FormRequest: request is NULL");
    return false;
  }

  request->clear();

  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess))
    return false;

  if (!access_points) {
    ASSERT_STRING("FinancialPing::FormRequest: access_points is NULL");
    return false;
  }

  if (!product_signature) {
    ASSERT_STRING("FinancialPing::FormRequest: product_signature is NULL");
    return false;
  }

  if (!SupplementaryBranding::GetBrand().empty()) {
    if (SupplementaryBranding::GetBrand() != product_brand) {
      ASSERT_STRING("FinancialPing::FormRequest: supplementary branding bad");
      return false;
    }
  }

  base::StringAppendF(request, "%s?", kFinancialPingPath);

  // Add the signature, brand, product id and language.
  base::StringAppendF(request, "%s=%s", kProductSignatureCgiVariable,
                      product_signature);
  if (product_brand)
    base::StringAppendF(request, "&%s=%s", kProductBrandCgiVariable,
                        product_brand);

  if (product_id)
    base::StringAppendF(request, "&%s=%s", kProductIdCgiVariable, product_id);

  if (product_lang)
    base::StringAppendF(request, "&%s=%s", kProductLanguageCgiVariable,
                        product_lang);

  // Add the product events.
  char cgi[kMaxCgiLength + 1];
  cgi[0] = 0;
  bool has_events = GetProductEventsAsCgi(product, cgi, std::size(cgi));
  if (has_events)
    base::StringAppendF(request, "&%s", cgi);

  // If we don't have any events, we should ping all the AP's on the system
  // that we know about and have a current RLZ value, even if they are not
  // used by this product.
  AccessPoint all_points[LAST_ACCESS_POINT];
  if (!has_events) {
    char rlz[kMaxRlzLength + 1];
    int idx = 0;
    for (int ap = NO_ACCESS_POINT + 1; ap < LAST_ACCESS_POINT; ap++) {
      rlz[0] = 0;
      AccessPoint point = static_cast<AccessPoint>(ap);
      if (GetAccessPointRlz(point, rlz, std::size(rlz)) && rlz[0] != '\0')
        all_points[idx++] = point;
    }
    all_points[idx] = NO_ACCESS_POINT;
  }

  // Add the RLZ's and the DCC if needed. This is the same as get PingParams.
  // This will also include the RLZ Exchange Protocol CGI Argument.
  cgi[0] = 0;
  if (GetPingParams(product, has_events ? access_points : all_points, cgi,
                    std::size(cgi)))
    base::StringAppendF(request, "&%s", cgi);

  if (has_events && !exclude_machine_id) {
    std::string machine_id;
    if (GetMachineId(&machine_id)) {
      base::StringAppendF(request, "&%s=%s", kMachineIdCgiVariable,
                          machine_id.c_str());
    }
  }

  return true;
}

namespace {

// A waitable event used to detect when either:
//
//   1/ the RLZ ping request completes
//   2/ the RLZ ping request times out
//   3/ browser shutdown begins
class RefCountedWaitableEvent
    : public base::RefCountedThreadSafe<RefCountedWaitableEvent> {
 public:
  RefCountedWaitableEvent()
      : event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SignalShutdown() { event_.Signal(); }

  void SignalFetchComplete(int response_code, std::string response) {
    base::AutoLock autolock(lock_);
    response_code_ = response_code;
    response_ = std::move(response);
    event_.Signal();
  }

  bool TimedWait(base::TimeDelta timeout) { return event_.TimedWait(timeout); }

  int GetResponseCode() {
    base::AutoLock autolock(lock_);
    return response_code_;
  }

  std::string TakeResponse() {
    base::AutoLock autolock(lock_);
    std::string temp = std::move(response_);
    response_.clear();
    return temp;
  }

 private:
  ~RefCountedWaitableEvent() = default;
  friend class base::RefCountedThreadSafe<RefCountedWaitableEvent>;

  base::WaitableEvent event_;
  base::Lock lock_;
  std::string response_;
  int response_code_ = -1;
};

// The URL load complete callback signals an instance of
// RefCountedWaitableEvent when the load completes.
void OnURLLoadComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       scoped_refptr<RefCountedWaitableEvent> event,
                       std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }

  std::string response;
  if (response_body) {
    response = std::move(*response_body);
  }

  event->SignalFetchComplete(response_code, std::move(response));
}

bool send_financial_ping_interrupted_for_test = false;

}  // namespace

// The signal for the current ping request. It can be used to cancel the request
// in case of a shutdown.
scoped_refptr<RefCountedWaitableEvent>& GetPingResultEvent() {
  static base::NoDestructor<scoped_refptr<RefCountedWaitableEvent>>
      g_pingResultEvent;
  return *g_pingResultEvent;
}

// The pointer to URLRequestContextGetter used by FinancialPing::PingServer().
// It is atomic pointer because it can be accessed and modified by multiple
// threads.
AtomicWord g_URLLoaderFactory;

bool FinancialPing::SetURLLoaderFactory(
    network::mojom::URLLoaderFactory* factory) {
  base::subtle::Release_Store(&g_URLLoaderFactory,
                              reinterpret_cast<AtomicWord>(factory));
  scoped_refptr<RefCountedWaitableEvent> event = GetPingResultEvent();
  if (!factory && event) {
    send_financial_ping_interrupted_for_test = true;
    event->SignalShutdown();
  }
  return true;
}

void PingRlzServer(std::string url,
                   scoped_refptr<RefCountedWaitableEvent> event) {
  // Copy the pointer to stack because g_URLLoaderFactory may be set to NULL
  // in different thread. The instance is guaranteed to exist while
  // the method is running.
  network::mojom::URLLoaderFactory* url_loader_factory =
      reinterpret_cast<network::mojom::URLLoaderFactory*>(
          base::subtle::Acquire_Load(&g_URLLoaderFactory));

  // Browser shutdown will cause the factory to be reset to NULL.
  // ShutdownCheck will catch this.
  if (!url_loader_factory) {
    event->SignalFetchComplete(-1, "");
    return;
  }

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("rlz_ping", R"(
        semantics {
          sender: "RLZ Ping"
          description:
            "Used for measuring the effectiveness of a promotion. See the "
            "Chrome Privacy Whitepaper for complete details."
          trigger:
            "1- At Chromium first run.\n"
            "2- When Chromium is re-activated by a new promotion.\n"
            "3- Once a week thereafter as long as Chromium is used.\n"
          data:
            "1- Non-unique cohort tag of when Chromium was installed.\n"
            "2- Unique machine id on desktop platforms.\n"
            "3- Whether Google is the default omnibox search.\n"
            "4- Whether google.com is the default home page."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  constexpr int kMaxNetworkRetries = 3;
  url_loader->SetRetryOptions(
      kMaxNetworkRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
          network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED);

  // Pass ownership of the loader to the bound function. Otherwise the load will
  // be canceled when the SimpleURLLoader object is destroyed.
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&OnURLLoadComplete, std::move(url_loader),
                     std::move(event)));
}

FinancialPing::PingResponse FinancialPing::PingServer(const char* request,
                                                      std::string* response) {
  if (!response)
    return PING_FAILURE;

  response->clear();

  std::string url =
      base::StringPrintf("https://%s%s", kFinancialServer, request);

  // Use a waitable event to cause this function to block, to match the
  // wininet implementation.
  auto event = base::MakeRefCounted<RefCountedWaitableEvent>();
  scoped_refptr<RefCountedWaitableEvent>& event_ref = GetPingResultEvent();
  event_ref = event;

  // PingRlzServer must be run in a separate sequence so that the TimedWait()
  // call below does not block the URL fetch response from being handled by
  // the URL delegate.
  scoped_refptr<base::SequencedTaskRunner> background_runner(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::BEST_EFFORT}));
  background_runner->PostTask(FROM_HERE,
                              base::BindOnce(&PingRlzServer, url, event));

  bool is_signaled;
  {
    base::ScopedAllowBaseSyncPrimitives allow_base_sync_primitives;
    is_signaled = event->TimedWait(base::Minutes(5));
  }

  event_ref.reset();
  if (!is_signaled)
    return PING_FAILURE;

  if (event->GetResponseCode() == -1) {
    send_financial_ping_interrupted_for_test = true;
    return PING_SHUTDOWN;
  } else if (event->GetResponseCode() != 200) {
    return PING_FAILURE;
  }

  *response = event->TakeResponse();
  return PING_SUCCESSFUL;
}

bool FinancialPing::IsPingTime(Product product, bool no_delay) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kReadAccess))
    return false;

  int64_t last_ping = 0;
  if (!store->ReadPingTime(product, &last_ping))
    return true;

  uint64_t now = GetSystemTimeAsInt64();
  int64_t interval = now - last_ping;

  // If interval is negative, clock was probably reset. So ping.
  if (interval < 0)
    return true;

  // Check if this product has any unreported events.
  char cgi[kMaxCgiLength + 1];
  cgi[0] = 0;
  bool has_events = GetProductEventsAsCgi(product, cgi, std::size(cgi));
  if (no_delay && has_events)
    return true;

  return interval >= (has_events ? kEventsPingInterval : kNoEventsPingInterval);
}


bool FinancialPing::UpdateLastPingTime(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;

  uint64_t now = GetSystemTimeAsInt64();
  return store->WritePingTime(product, now);
}


bool FinancialPing::ClearLastPingTime(Product product) {
  ScopedRlzValueStoreLock lock;
  RlzValueStore* store = lock.GetStore();
  if (!store || !store->HasAccess(RlzValueStore::kWriteAccess))
    return false;
  return store->ClearPingTime(product);
}

namespace test {

void ResetSendFinancialPingInterrupted() {
  send_financial_ping_interrupted_for_test = false;
}

bool WasSendFinancialPingInterrupted() {
  return send_financial_ping_interrupted_for_test;
}

}  // namespace test

}  // namespace rlz_lib
