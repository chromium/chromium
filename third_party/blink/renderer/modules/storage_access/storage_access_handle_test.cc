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

// TODO(crbug.com/1484966): Simplify test with provider to avoid so much
// duplicated code.

class StorageAccessHandleTest : public testing::Test {
 public:
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

TEST_F(StorageAccessHandleTest, LoadAllHandles) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setAll(true);
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

TEST_F(StorageAccessHandleTest, LoadSessionStorageHandle) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setSessionStorage(true);
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocalStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kIndexedDBNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocksNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

TEST_F(StorageAccessHandleTest, LoadLocalStorageHandle) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setLocalStorage(true);
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kSessionStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kIndexedDBNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocksNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

TEST_F(StorageAccessHandleTest, LoadIndexedDBHandle) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setIndexedDB(true);
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kSessionStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocalStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocksNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

TEST_F(StorageAccessHandleTest, LoadLocksHandle) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  storage_access_types->setLocks(true);
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kSessionStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocalStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kIndexedDBNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kNoError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(nullptr, scope.GetExceptionState().Message());
    EXPECT_TRUE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

TEST_F(StorageAccessHandleTest, LoadNoHandles) {
  LocalDOMWindow* window = getLocalDOMWindow();
  StorageAccessTypes* storage_access_types =
      MakeGarbageCollected<StorageAccessTypes>();
  StorageAccessHandle* storage_access_handle =
      MakeGarbageCollected<StorageAccessHandle>(*window, storage_access_types);
  EXPECT_TRUE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_all));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::
          kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB));
  EXPECT_FALSE(window->document()->IsUseCounted(
      WebFeature::kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks));
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
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kSessionStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_sessionStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->localStorage(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocalStorageNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_localStorage_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->indexedDB(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kIndexedDBNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_indexedDB_Use));
  }
  {
    V8TestingScope scope;
    storage_access_handle->locks(scope.GetExceptionState());
    EXPECT_EQ(DOMExceptionCode::kSecurityError,
              scope.GetExceptionState().CodeAs<DOMExceptionCode>());
    EXPECT_EQ(StorageAccessHandle::kLocksNotRequested,
              scope.GetExceptionState().Message());
    EXPECT_FALSE(window->document()->IsUseCounted(
        WebFeature::
            kStorageAccessAPI_requestStorageAccess_BeyondCookies_locks_Use));
  }
}

}  // namespace blink
