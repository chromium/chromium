// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage_access/storage_access_handle.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_storage_access_types.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class StorageAccessHandleTest
    : public testing::TestWithParam<
          testing::tuple<bool, bool, bool, bool, bool>> {
 public:
  bool all() { return std::get<0>(GetParam()); }
  bool session_storage() { return std::get<1>(GetParam()); }
  bool local_storage() { return std::get<2>(GetParam()); }
  bool indexed_db() { return std::get<3>(GetParam()); }
  bool locks() { return std::get<4>(GetParam()); }

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
  storage_access_types->setSessionStorage(session_storage());
  storage_access_types->setLocalStorage(local_storage());
  storage_access_types->setIndexedDB(indexed_db());
  storage_access_types->setLocks(locks());
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
      session_storage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage),
      local_storage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB),
      indexed_db());
  EXPECT_EQ(window->document()->IsUseCounted(
                WebFeature::
                    kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks),
            locks());
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
  {
    V8TestingScope scope;
    storage_access_handle->sessionStorage(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || session_storage()) ? DOMExceptionCode::kNoError
                                           : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || session_storage())
                  ? nullptr
                  : StorageAccessHandle::kSessionStorageNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || local_storage()) ? DOMExceptionCode::kNoError
                                         : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || local_storage())
                  ? nullptr
                  : StorageAccessHandle::kLocalStorageNotRequested);
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
              (all() || indexed_db()) ? DOMExceptionCode::kNoError
                                      : DOMExceptionCode::kSecurityError);
    EXPECT_EQ(scope.GetExceptionState().Message(),
              (all() || indexed_db())
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
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use),
      all() || session_storage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use),
      all() || local_storage());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use),
      all() || indexed_db());
  EXPECT_EQ(
      window->document()->IsUseCounted(
          WebFeature::
              kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use),
      all() || locks());
}

INSTANTIATE_TEST_SUITE_P(All,
                         StorageAccessHandleTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace blink
