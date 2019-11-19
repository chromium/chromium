// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/request_init.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/fetch/response_init.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::mojom::CacheStorageError;
using blink::mojom::blink::CacheStorageVerboseError;

namespace blink {

namespace {

const char kNotImplementedString[] =
    "NotSupportedError: Method is not implemented.";

class ScopedFetcherForTests final
    : public GarbageCollected<ScopedFetcherForTests>,
      public GlobalFetch::ScopedFetcher {
  USING_GARBAGE_COLLECTED_MIXIN(ScopedFetcherForTests);

 public:
  ScopedFetcherForTests() : fetch_count_(0), expected_url_(nullptr) {}

  ScriptPromise Fetch(ScriptState* script_state,
                      const RequestInfo& request_info,
                      const RequestInit*,
                      ExceptionState&) override {
    ++fetch_count_;
    if (expected_url_) {
      String fetched_url;
      if (request_info.IsRequest())
        EXPECT_EQ(*expected_url_, request_info.GetAsRequest()->url());
      else
        EXPECT_EQ(*expected_url_, request_info.GetAsUSVString());
    }

    if (response_) {
      auto* resolver =
          MakeGarbageCollected<ScriptPromiseResolver>(script_state);
      const ScriptPromise promise = resolver->Promise();
      resolver->Resolve(response_);
      response_ = nullptr;
      return promise;
    }
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "Unexpected call to fetch, no response available."));
  }

  // This does not take ownership of its parameter. The provided sample object
  // is used to check the parameter when called.
  void SetExpectedFetchUrl(const String* expected_url) {
    expected_url_ = expected_url;
  }
  void SetResponse(Response* response) { response_ = response; }

  int FetchCount() const { return fetch_count_; }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(response_);
    GlobalFetch::ScopedFetcher::Trace(visitor);
  }

 private:
  int fetch_count_;
  const String* expected_url_;
  Member<Response> response_;
};

// A test implementation of the CacheStorageCache interface which returns a
// (provided) error for every operation, and optionally checks arguments to
// methods against provided arguments. Also used as a base class for test
// specific caches.
class ErrorCacheForTests : public mojom::blink::CacheStorageCache {
 public:
  ErrorCacheForTests(const mojom::blink::CacheStorageError error)
      : error_(error),
        expected_url_(nullptr),
        expected_query_options_(nullptr),
        expected_batch_operations_(nullptr) {}

  std::string GetAndClearLastErrorWebCacheMethodCalled() {
    test::RunPendingTasks();
    std::string old = last_error_web_cache_method_called_;
    last_error_web_cache_method_called_.clear();
    return old;
  }

  // These methods do not take ownership of their parameter. They provide an
  // optional sample object to check parameters against.
  void SetExpectedUrl(const String* expected_url) {
    expected_url_ = expected_url;
  }
  void SetExpectedCacheQueryOptions(
      const mojom::blink::CacheQueryOptionsPtr* expected_query_options) {
    expected_query_options_ = expected_query_options;
  }
  void SetExpectedBatchOperations(const Vector<mojom::blink::BatchOperationPtr>*
                                      expected_batch_operations) {
    expected_batch_operations_ = expected_batch_operations;
  }

  void Match(mojom::blink::FetchAPIRequestPtr fetch_api_request,
             mojom::blink::CacheQueryOptionsPtr query_options,
             bool in_related_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    last_error_web_cache_method_called_ = "dispatchMatch";
    CheckUrlIfProvided(fetch_api_request->url);
    CheckCacheQueryOptionsIfProvided(query_options);
    mojom::blink::MatchResultPtr result = mojom::blink::MatchResult::New();
    result->set_status(error_);
    std::move(callback).Run(std::move(result));
  }
  void MatchAll(mojom::blink::FetchAPIRequestPtr fetch_api_request,
                mojom::blink::CacheQueryOptionsPtr query_options,
                int64_t trace_id,
                MatchAllCallback callback) override {
    last_error_web_cache_method_called_ = "dispatchMatchAll";
    if (fetch_api_request)
      CheckUrlIfProvided(fetch_api_request->url);
    CheckCacheQueryOptionsIfProvided(query_options);
    mojom::blink::MatchAllResultPtr result =
        mojom::blink::MatchAllResult::New();
    result->set_status(error_);
    std::move(callback).Run(std::move(result));
  }
  void Keys(mojom::blink::FetchAPIRequestPtr fetch_api_request,
            mojom::blink::CacheQueryOptionsPtr query_options,
            int64_t trace_id,
            KeysCallback callback) override {
    last_error_web_cache_method_called_ = "dispatchKeys";
    if (fetch_api_request && !fetch_api_request->url.IsEmpty()) {
      CheckUrlIfProvided(fetch_api_request->url);
      CheckCacheQueryOptionsIfProvided(query_options);
    }
    mojom::blink::CacheKeysResultPtr result =
        mojom::blink::CacheKeysResult::New();
    result->set_status(error_);
    std::move(callback).Run(std::move(result));
  }
  void Batch(Vector<mojom::blink::BatchOperationPtr> batch_operations,
             int64_t trace_id,
             BatchCallback callback) override {
    last_error_web_cache_method_called_ = "dispatchBatch";
    CheckBatchOperationsIfProvided(batch_operations);
    std::move(callback).Run(CacheStorageVerboseError::New(error_, String()));
  }

 protected:
  void CheckUrlIfProvided(const KURL& url) {
    if (!expected_url_)
      return;
    EXPECT_EQ(*expected_url_, url);
  }

  void CheckCacheQueryOptionsIfProvided(
      const mojom::blink::CacheQueryOptionsPtr& query_options) {
    if (!expected_query_options_)
      return;
    CompareCacheQueryOptionsForTest(*expected_query_options_, query_options);
  }

  void CheckBatchOperationsIfProvided(
      const Vector<mojom::blink::BatchOperationPtr>& batch_operations) {
    if (!expected_batch_operations_)
      return;
    const Vector<mojom::blink::BatchOperationPtr>& expected_batch_operations =
        *expected_batch_operations_;
    EXPECT_EQ(expected_batch_operations.size(), batch_operations.size());
    for (int i = 0, minsize = std::min(expected_batch_operations.size(),
                                       batch_operations.size());
         i < minsize; ++i) {
      EXPECT_EQ(expected_batch_operations[i]->operation_type,
                batch_operations[i]->operation_type);
      const String expected_request_url =
          expected_batch_operations[i]->request->url;
      EXPECT_EQ(expected_request_url, batch_operations[i]->request->url);
      if (expected_batch_operations[i]->response) {
        ASSERT_EQ(expected_batch_operations[i]->response->url_list.size(),
                  batch_operations[i]->response->url_list.size());
        for (wtf_size_t j = 0;
             j < expected_batch_operations[i]->response->url_list.size(); ++j) {
          EXPECT_EQ(expected_batch_operations[i]->response->url_list[j],
                    batch_operations[i]->response->url_list[j]);
        }
      }
      if (expected_batch_operations[i]->match_options ||
          batch_operations[i]->match_options) {
        CompareCacheQueryOptionsForTest(
            expected_batch_operations[i]->match_options,
            batch_operations[i]->match_options);
      }
    }
  }

 private:
  static void CompareCacheQueryOptionsForTest(
      const mojom::blink::CacheQueryOptionsPtr& expected_query_options,
      const mojom::blink::CacheQueryOptionsPtr& query_options) {
    EXPECT_EQ(expected_query_options->ignore_search,
              query_options->ignore_search);
    EXPECT_EQ(expected_query_options->ignore_method,
              query_options->ignore_method);
    EXPECT_EQ(expected_query_options->ignore_vary, query_options->ignore_vary);
  }

  const mojom::blink::CacheStorageError error_;

  const String* expected_url_;
  const mojom::blink::CacheQueryOptionsPtr* expected_query_options_;
  const Vector<mojom::blink::BatchOperationPtr>* expected_batch_operations_;

  std::string last_error_web_cache_method_called_;
};

class NotImplementedErrorCache : public ErrorCacheForTests {
 public:
  NotImplementedErrorCache()
      : ErrorCacheForTests(
            mojom::blink::CacheStorageError::kErrorNotImplemented) {}
};

class CacheStorageTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(IntSize(1, 1)); }

  Cache* CreateCache(ScopedFetcherForTests* fetcher,
                     std::unique_ptr<ErrorCacheForTests> cache) {
    mojo::AssociatedRemote<mojom::blink::CacheStorageCache> cache_remote;
    cache_ = std::move(cache);
    receiver_ = std::make_unique<
        mojo::AssociatedReceiver<mojom::blink::CacheStorageCache>>(
        cache_.get(),
        cache_remote.BindNewEndpointAndPassDedicatedReceiverForTesting());
    return MakeGarbageCollected<Cache>(
        fetcher, cache_remote.Unbind(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  ErrorCacheForTests* test_cache() { return cache_.get(); }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }
  ExecutionContext* GetExecutionContext() {
    return ExecutionContext::From(GetScriptState());
  }
  v8::Isolate* GetIsolate() { return GetScriptState()->GetIsolate(); }
  v8::Local<v8::Context> GetContext() { return GetScriptState()->GetContext(); }

  Request* NewRequestFromUrl(const String& url) {
    DummyExceptionStateForTesting exception_state;
    Request* request = Request::Create(GetScriptState(), url, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    return exception_state.HadException() ? nullptr : request;
  }

  // Convenience methods for testing the returned promises.
  ScriptValue GetRejectValue(ScriptPromise& promise) {
    ScriptPromiseTester tester(GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
    return tester.Value();
  }

  std::string GetRejectString(ScriptPromise& promise) {
    ScriptValue on_reject = GetRejectValue(promise);
    return ToCoreString(
               on_reject.V8Value()->ToString(GetContext()).ToLocalChecked())
        .Ascii()
        .data();
  }

  ScriptValue GetResolveValue(ScriptPromise& promise) {
    ScriptPromiseTester tester(GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
    return tester.Value();
  }

  std::string GetResolveString(ScriptPromise& promise) {
    ScriptValue on_resolve = GetResolveValue(promise);
    return ToCoreString(
               on_resolve.V8Value()->ToString(GetContext()).ToLocalChecked())
        .Ascii()
        .data();
  }

 private:
  std::unique_ptr<ErrorCacheForTests> cache_;
  std::unique_ptr<mojo::AssociatedReceiver<mojom::blink::CacheStorageCache>>
      receiver_;
};

RequestInfo StringToRequestInfo(const String& value) {
  RequestInfo info;
  info.SetUSVString(value);
  return info;
}

RequestInfo RequestToRequestInfo(Request* value) {
  RequestInfo info;
  info.SetRequest(value);
  return info;
}

TEST_F(CacheStorageTest, Basics) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  Cache* cache =
      CreateCache(fetcher, std::make_unique<NotImplementedErrorCache>());
  DCHECK(cache);

  const String url = "http://www.cachetest.org/";

  CacheQueryOptions* options = CacheQueryOptions::Create();
  ScriptPromise match_promise = cache->match(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_promise));

  cache = CreateCache(fetcher, std::make_unique<ErrorCacheForTests>(
                                   CacheStorageError::kErrorNotFound));
  match_promise = cache->match(GetScriptState(), StringToRequestInfo(url),
                               options, exception_state);
  ScriptValue script_value = GetResolveValue(match_promise);
  EXPECT_TRUE(script_value.IsUndefined());

  cache = CreateCache(fetcher, std::make_unique<ErrorCacheForTests>(
                                   CacheStorageError::kErrorExists));
  match_promise = cache->match(GetScriptState(), StringToRequestInfo(url),
                               options, exception_state);
  EXPECT_EQ("InvalidAccessError: Entry already exists.",
            GetRejectString(match_promise));
}

// Tests that arguments are faithfully passed on calls to Cache methods, except
// for methods which use batch operations, which are tested later.
TEST_F(CacheStorageTest, BasicArguments) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  Cache* cache =
      CreateCache(fetcher, std::make_unique<NotImplementedErrorCache>());
  DCHECK(cache);

  ScriptPromise match_all_result_no_arguments =
      cache->matchAll(GetScriptState(), exception_state);
  EXPECT_EQ("dispatchMatchAll",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString,
            GetRejectString(match_all_result_no_arguments));

  const String url = "http://www.cache.arguments.test/";
  test_cache()->SetExpectedUrl(&url);

  mojom::blink::CacheQueryOptionsPtr expected_query_options =
      mojom::blink::CacheQueryOptions::New();
  expected_query_options->ignore_vary = true;
  test_cache()->SetExpectedCacheQueryOptions(&expected_query_options);

  CacheQueryOptions* options = CacheQueryOptions::Create();
  options->setIgnoreVary(1);

  Request* request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise match_result =
      cache->match(GetScriptState(), RequestToRequestInfo(request), options,
                   exception_state);
  EXPECT_EQ("dispatchMatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_result));

  ScriptPromise string_match_result = cache->match(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchMatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_match_result));

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise match_all_result =
      cache->matchAll(GetScriptState(), RequestToRequestInfo(request), options,
                      exception_state);
  EXPECT_EQ("dispatchMatchAll",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(match_all_result));

  ScriptPromise string_match_all_result = cache->matchAll(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchMatchAll",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_match_all_result));

  ScriptPromise keys_result1 = cache->keys(GetScriptState(), exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(keys_result1));

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise keys_result2 =
      cache->keys(GetScriptState(), RequestToRequestInfo(request), options,
                  exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(keys_result2));

  ScriptPromise string_keys_result2 = cache->keys(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchKeys",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_keys_result2));
}

// Tests that arguments are faithfully passed to API calls that degrade to batch
// operations.
TEST_F(CacheStorageTest, BatchOperationArguments) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  Cache* cache =
      CreateCache(fetcher, std::make_unique<NotImplementedErrorCache>());
  DCHECK(cache);

  mojom::blink::CacheQueryOptionsPtr expected_query_options =
      mojom::blink::CacheQueryOptions::New();
  test_cache()->SetExpectedCacheQueryOptions(&expected_query_options);

  CacheQueryOptions* options = CacheQueryOptions::Create();

  const String url = "http://batch.operations.test/";
  Request* request = NewRequestFromUrl(url);
  DCHECK(request);

  auto fetch_response = mojom::blink::FetchAPIResponse::New();
  fetch_response->url_list.push_back(KURL(url));
  fetch_response->response_type = network::mojom::FetchResponseType::kDefault;
  fetch_response->status_text = String("OK");
  Response* response = Response::Create(GetScriptState(), *fetch_response);

  Vector<mojom::blink::BatchOperationPtr> expected_delete_operations;
  {
    expected_delete_operations.push_back(mojom::blink::BatchOperation::New());
    auto& delete_operation = expected_delete_operations.back();
    delete_operation->operation_type = mojom::blink::OperationType::kDelete;
    delete_operation->request = request->CreateFetchAPIRequest();
    delete_operation->match_options = expected_query_options->Clone();
  }
  test_cache()->SetExpectedBatchOperations(&expected_delete_operations);

  ScriptPromise delete_result =
      cache->Delete(GetScriptState(), RequestToRequestInfo(request), options,
                    exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(delete_result));

  ScriptPromise string_delete_result = cache->Delete(
      GetScriptState(), StringToRequestInfo(url), options, exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_delete_result));

  Vector<mojom::blink::BatchOperationPtr> expected_put_operations;
  {
    expected_put_operations.push_back(mojom::blink::BatchOperation::New());
    auto& put_operation = expected_put_operations.back();
    put_operation->operation_type = mojom::blink::OperationType::kPut;
    put_operation->request = request->CreateFetchAPIRequest();
    put_operation->response =
        response->PopulateFetchAPIResponse(request->url());
  }
  test_cache()->SetExpectedBatchOperations(&expected_put_operations);

  request = NewRequestFromUrl(url);
  DCHECK(request);
  ScriptPromise put_result = cache->put(
      GetScriptState(), RequestToRequestInfo(request),
      response->clone(GetScriptState(), exception_state), exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(put_result));

  ScriptPromise string_put_result = cache->put(
      GetScriptState(), StringToRequestInfo(url), response, exception_state);
  EXPECT_EQ("dispatchBatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
  EXPECT_EQ(kNotImplementedString, GetRejectString(string_put_result));

  // FIXME: test add & addAll.
}

class MatchTestCache : public NotImplementedErrorCache {
 public:
  MatchTestCache(mojom::blink::FetchAPIResponsePtr response)
      : response_(std::move(response)) {}

  // From WebServiceWorkerCache:
  void Match(mojom::blink::FetchAPIRequestPtr fetch_api_request,
             mojom::blink::CacheQueryOptionsPtr query_options,
             bool in_related_fetch_event,
             int64_t trace_id,
             MatchCallback callback) override {
    mojom::blink::MatchResultPtr result = mojom::blink::MatchResult::New();
    result->set_response(std::move(response_));
    std::move(callback).Run(std::move(result));
  }

 private:
  mojom::blink::FetchAPIResponsePtr response_;
};

TEST_F(CacheStorageTest, MatchResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  const String request_url = "http://request.url/";
  const String response_url = "http://match.response.test/";

  mojom::blink::FetchAPIResponsePtr fetch_api_response =
      mojom::blink::FetchAPIResponse::New();
  fetch_api_response->url_list.push_back(KURL(response_url));
  fetch_api_response->response_type =
      network::mojom::FetchResponseType::kDefault;
  fetch_api_response->status_text = String("OK");

  Cache* cache = CreateCache(
      fetcher, std::make_unique<MatchTestCache>(std::move(fetch_api_response)));
  CacheQueryOptions* options = CacheQueryOptions::Create();

  ScriptPromise result =
      cache->match(GetScriptState(), StringToRequestInfo(request_url), options,
                   exception_state);
  ScriptValue script_value = GetResolveValue(result);
  Response* response =
      V8Response::ToImplWithTypeCheck(GetIsolate(), script_value.V8Value());
  ASSERT_TRUE(response);
  EXPECT_EQ(response_url, response->url());
}

class KeysTestCache : public NotImplementedErrorCache {
 public:
  KeysTestCache(Vector<mojom::blink::FetchAPIRequestPtr> requests)
      : requests_(std::move(requests)) {}

  void Keys(mojom::blink::FetchAPIRequestPtr fetch_api_request,
            mojom::blink::CacheQueryOptionsPtr query_options,
            int64_t trace_id,
            KeysCallback callback) override {
    mojom::blink::CacheKeysResultPtr result =
        mojom::blink::CacheKeysResult::New();
    result->set_keys(std::move(requests_));
    std::move(callback).Run(std::move(result));
  }

 private:
  Vector<mojom::blink::FetchAPIRequestPtr> requests_;
};

TEST_F(CacheStorageTest, KeysResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  const String url1 = "http://first.request/";
  const String url2 = "http://second.request/";

  Vector<String> expected_urls(size_t(2));
  expected_urls[0] = url1;
  expected_urls[1] = url2;

  Vector<mojom::blink::FetchAPIRequestPtr> fetch_api_requests(size_t(2));
  fetch_api_requests[0] = mojom::blink::FetchAPIRequest::New();
  fetch_api_requests[0]->url = KURL(url1);
  fetch_api_requests[0]->method = String("GET");
  fetch_api_requests[1] = mojom::blink::FetchAPIRequest::New();
  fetch_api_requests[1]->url = KURL(url2);
  fetch_api_requests[1]->method = String("GET");

  Cache* cache = CreateCache(
      fetcher, std::make_unique<KeysTestCache>(std::move(fetch_api_requests)));

  ScriptPromise result = cache->keys(GetScriptState(), exception_state);
  ScriptValue script_value = GetResolveValue(result);

  HeapVector<Member<Request>> requests =
      NativeValueTraits<IDLSequence<Request>>::NativeValue(
          GetIsolate(), script_value.V8Value(), exception_state);
  EXPECT_EQ(expected_urls.size(), requests.size());
  for (int i = 0, minsize = std::min(expected_urls.size(), requests.size());
       i < minsize; ++i) {
    Request* request = requests[i];
    EXPECT_TRUE(request);
    if (request)
      EXPECT_EQ(expected_urls[i], request->url());
  }
}

class MatchAllAndBatchTestCache : public NotImplementedErrorCache {
 public:
  MatchAllAndBatchTestCache(Vector<mojom::blink::FetchAPIResponsePtr> responses)
      : responses_(std::move(responses)) {}

  void MatchAll(mojom::blink::FetchAPIRequestPtr fetch_api_request,
                mojom::blink::CacheQueryOptionsPtr query_options,
                int64_t trace_id,
                MatchAllCallback callback) override {
    mojom::blink::MatchAllResultPtr result =
        mojom::blink::MatchAllResult::New();
    result->set_responses(std::move(responses_));
    std::move(callback).Run(std::move(result));
  }
  void Batch(Vector<mojom::blink::BatchOperationPtr> batch_operations,
             int64_t trace_id,
             BatchCallback callback) override {
    std::move(callback).Run(CacheStorageVerboseError::New(
        mojom::blink::CacheStorageError::kSuccess, String()));
  }

 private:
  Vector<mojom::blink::FetchAPIResponsePtr> responses_;
};

TEST_F(CacheStorageTest, MatchAllAndBatchResponseTest) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  const String url1 = "http://first.response/";
  const String url2 = "http://second.response/";

  Vector<String> expected_urls(size_t(2));
  expected_urls[0] = url1;
  expected_urls[1] = url2;

  Vector<mojom::blink::FetchAPIResponsePtr> fetch_api_responses;
  fetch_api_responses.push_back(mojom::blink::FetchAPIResponse::New());
  fetch_api_responses[0]->url_list = Vector<KURL>({KURL(url1)});
  fetch_api_responses[0]->response_type =
      network::mojom::FetchResponseType::kDefault;
  fetch_api_responses[0]->status_text = String("OK");
  fetch_api_responses.push_back(mojom::blink::FetchAPIResponse::New());
  fetch_api_responses[1]->url_list = Vector<KURL>({KURL(url2)});
  fetch_api_responses[1]->response_type =
      network::mojom::FetchResponseType::kDefault;
  fetch_api_responses[1]->status_text = String("OK");

  Cache* cache =
      CreateCache(fetcher, std::make_unique<MatchAllAndBatchTestCache>(
                               std::move(fetch_api_responses)));

  CacheQueryOptions* options = CacheQueryOptions::Create();
  ScriptPromise result =
      cache->matchAll(GetScriptState(), StringToRequestInfo("http://some.url/"),
                      options, exception_state);
  ScriptValue script_value = GetResolveValue(result);

  HeapVector<Member<Response>> responses =
      NativeValueTraits<IDLSequence<Response>>::NativeValue(
          GetIsolate(), script_value.V8Value(), exception_state);
  EXPECT_EQ(expected_urls.size(), responses.size());
  for (int i = 0, minsize = std::min(expected_urls.size(), responses.size());
       i < minsize; ++i) {
    Response* response = responses[i];
    EXPECT_TRUE(response);
    if (response)
      EXPECT_EQ(expected_urls[i], response->url());
  }

  result =
      cache->Delete(GetScriptState(), StringToRequestInfo("http://some.url/"),
                    options, exception_state);
  script_value = GetResolveValue(result);
  EXPECT_TRUE(script_value.V8Value()->IsBoolean());
  EXPECT_EQ(true, script_value.V8Value().As<v8::Boolean>()->Value());
}

TEST_F(CacheStorageTest, Add) {
  ScriptState::Scope scope(GetScriptState());
  NonThrowableExceptionState exception_state;
  auto* fetcher = MakeGarbageCollected<ScopedFetcherForTests>();
  const String url = "http://www.cacheadd.test/";
  const String content_type = "text/plain";
  const String content = "hello cache";

  Cache* cache =
      CreateCache(fetcher, std::make_unique<NotImplementedErrorCache>());

  fetcher->SetExpectedFetchUrl(&url);

  Request* request = NewRequestFromUrl(url);
  Response* response = Response::Create(
      GetScriptState(),
      MakeGarbageCollected<BodyStreamBuffer>(
          GetScriptState(),
          MakeGarbageCollected<FormDataBytesConsumer>(content), nullptr),
      content_type, ResponseInit::Create(), exception_state);
  fetcher->SetResponse(response);

  Vector<mojom::blink::BatchOperationPtr> expected_put_operations(size_t(1));
  {
    mojom::blink::BatchOperationPtr put_operation =
        mojom::blink::BatchOperation::New();

    put_operation->operation_type = mojom::blink::OperationType::kPut;
    put_operation->request = request->CreateFetchAPIRequest();
    put_operation->response =
        response->PopulateFetchAPIResponse(request->url());
    expected_put_operations[0] = std::move(put_operation);
  }
  test_cache()->SetExpectedBatchOperations(&expected_put_operations);

  ScriptPromise add_result = cache->add(
      GetScriptState(), RequestToRequestInfo(request), exception_state);

  EXPECT_EQ(kNotImplementedString, GetRejectString(add_result));
  EXPECT_EQ(1, fetcher->FetchCount());
  EXPECT_EQ("dispatchBatch",
            test_cache()->GetAndClearLastErrorWebCacheMethodCalled());
}

}  // namespace

}  // namespace blink
