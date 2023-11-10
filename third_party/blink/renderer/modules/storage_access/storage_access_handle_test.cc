// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

using TestParams = std::
    tuple<bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>;

template <size_t N>
TestParams MakeParamsWithSetBit() {
  TestParams params;
  std::get<N>(params) = true;
  return params;
}

}  // namespace

class StorageAccessHandleTest : public testing::TestWithParam<TestParams> {
 public:
  bool all() { return std::get<0>(GetParam()); }
  bool sessionStorage() { return std::get<1>(GetParam()); }
  bool localStorage() { return std::get<2>(GetParam()); }
  bool indexedDB() { return std::get<3>(GetParam()); }
  bool locks() { return std::get<4>(GetParam()); }
  bool caches() { return std::get<5>(GetParam()); }
  bool getDirectory() { return std::get<6>(GetParam()); }
  bool estimate() { return std::get<7>(GetParam()); }
  bool createObjectURL() { return std::get<8>(GetParam()); }
  bool revokeObjectURL() { return std::get<9>(GetParam()); }
  bool BroadcastChannel() { return std::get<10>(GetParam()); }

  LocalDOMWindow* getLocalDOMWindow() {
    test::ScopedMockedURLLoad scoped_mocked_url_load_root(
        KURL(kRootString), test::CoreTestDataPath("foo.html"));
    return To<LocalDOMWindow>(web_view_helper_.InitializeAndLoad(kRootString)
                                  ->GetPage()
                                  ->MainFrame()
                                  ->DomWindow());
  }

 private:
  static constexpr char kRootString[] = "http://storage/";
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_P(StorageAccessHandleTest, LoadHandle) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setAll(all());
  storage_access_types->setSessionStorage(sessionStorage());
  storage_access_types->setLocalStorage(localStorage());
  storage_access_types->setIndexedDB(indexedDB());
  storage_access_types->setLocks(locks());
  storage_access_types->setCaches(caches());
  storage_access_types->setGetDirectory(getDirectory());
  storage_access_types->setEstimate(estimate());
  storage_access_types->setCreateObjectURL(createObjectURL());
  storage_access_types->setRevokeObjectURL(revokeObjectURL());
  storage_access_types->setBroadcastChannel(BroadcastChannel());
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all),
      all());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage),
      sessionStorage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage),
      localStorage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB),
      indexedDB());
  EXPECT_EQ(window->document()->IsUseCounted(
                WebFeature::
                    kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks),
            locks());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_caches),
      caches());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory),
      getDirectory());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate),
      estimate());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL),
      createObjectURL());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL),
      revokeObjectURL());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel),
      BroadcastChannel());
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_caches_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL_Use));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel_Use));
  {
    V8TestingScope scope;
    storage_access_handle->sessionStorage(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || sessionStorage()) ? DOMExceptionCode::kNoError
                                          : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || sessionStorage())
                  ? nullptr
                  : StorageAccessHandle::kSessionStorageNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || localStorage()) ? DOMExceptionCode::kNoError
                                        : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || localStorage())
                  ? nullptr
                  : StorageAccessHandle::kLocalStorageNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || indexedDB()) ? DOMExceptionCode::kNoError
                                     : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || indexedDB())
                  ? nullptr
                  : StorageAccessHandle::kIndexedDBNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || locks()) ? DOMExceptionCode::kNoError
                                 : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(
        scope.GetExceptionState().Message(),
        (all() || locks()) ? nullptr : StorageAccessHandle::kLocksNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->caches(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || caches()) ? DOMExceptionCode::kNoError
                                  : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || caches()) ? nullptr
                                  : StorageAccessHandle::kCachesNotRequested);
  }
  {
    V8TestingScope scope;
    ScriptPromise promise = storage_access_handle->getDirectory(
        scope.GetScriptState(), scope.GetExceptionState());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
    auto* dom_exception = V8DOMException::ToWrappable(scope.GetIsolate(),
                                                      tester.Value().V8Value());
    EXPECT_EQ(dom_exception->code(),
              (uint16_t)DOMExceptionCode::kSecurityError);
    EXPECT_EQ(dom_exception->message(),
              (all() || getDirectory())
                  ? "Storage directory access is denied."
                  : StorageAccessHandle::kGetDirectoryNotRequested);
  }
  {
    V8TestingScope scope;
    ScriptPromise promise = storage_access_handle->estimate(
        scope.GetScriptState(), scope.GetExceptionState());
    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    if (all() || estimate()) {
      EXPECT_FALSE(tester.IsFulfilled());
      EXPECT_FALSE(tester.IsRejected());
    } else {
      tester.WaitUntilSettled();
      EXPECT_TRUE(tester.IsRejected());
      auto* dom_exception = V8DOMException::ToWrappable(
          scope.GetIsolate(), tester.Value().V8Value());
      EXPECT_EQ(dom_exception->code(),
                (uint16_t)DOMExceptionCode::kSecurityError);
      EXPECT_EQ(dom_exception->message(),
                StorageAccessHandle::kEstimateNotRequested);
    }
  }
  {
    V8TestingScope scope;
    storage_access_handle->createObjectURL(
        Blob::Create(scope.GetExecutionContext()), scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || createObjectURL()) ? DOMExceptionCode::kNoError
                                           : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || createObjectURL())
                  ? nullptr
                  : StorageAccessHandle::kCreateObjectURLNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->revokeObjectURL("", scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || revokeObjectURL()) ? DOMExceptionCode::kNoError
                                           : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || revokeObjectURL())
                  ? nullptr
                  : StorageAccessHandle::kRevokeObjectURLNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->BroadcastChannel(scope.GetExecutionContext(), "",
                                            scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || BroadcastChannel()) ? DOMExceptionCode::kNoError
                                            : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || BroadcastChannel())
                  ? nullptr
                  : StorageAccessHandle::kBroadcastChannelNotRequested);
  }
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use),
      all() || sessionStorage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use),
      all() || localStorage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use),
      all() || indexedDB());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use),
      all() || locks());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_caches_Use),
      all() || caches());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_getDirectory_Use),
      all() || getDirectory());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_estimate_Use),
      all() || estimate());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_createObjectURL_Use),
      all() || createObjectURL());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_revokeObjectURL_Use),
      all() || revokeObjectURL());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_BroadcastChannel_Use),
      all() || BroadcastChannel());
}

// Test all handles.
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    StorageAccessHandleTest,
    testing::ValuesIn(std::vector<TestParams>{
        // Nothing:
        TestParams(),
        // All:
        MakeParamsWithSetBit<0>(),
        // Session Storage:
        MakeParamsWithSetBit<1>(),
        // Local Storage:
        MakeParamsWithSetBit<2>(),
        // IndexedDB:
        MakeParamsWithSetBit<3>(),
        // Web Locks:
        MakeParamsWithSetBit<4>(),
        // Cache Storage:
        MakeParamsWithSetBit<5>(),
        // Origin Private File System:
        MakeParamsWithSetBit<6>(),
        // Quota:
        MakeParamsWithSetBit<7>(),
        // createObjectURL:
        MakeParamsWithSetBit<8>(),
        // revokeObjectURL:
        MakeParamsWithSetBit<9>(),
        // BroadcastChannel:
        MakeParamsWithSetBit<10>(),
    }));

}  // namespace blink
