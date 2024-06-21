// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"

#include <tuple>

#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using FormatOption = CachedStorageArea::FormatOption;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class CachedStorageAreaTest : public testing::Test {
 public:
  const String kKey = "key";
  const String kValue = "value";
  const String kValue2 = "another value";
  const std::string kRootString = "http://dom_storage/";
  const KURL kRootUrl = KURL(kRootString.c_str());
  const BlinkStorageKey kRootStorageKey =
      BlinkStorageKey::CreateFromStringForTesting(kRootString.c_str());
  const std::string kPageString = "http://dom_storage/page";
  const KURL kPageUrl = KURL(kPageString.c_str());
  const std::string kPageString2 = "http://dom_storage/other_page";
  const KURL kPageUrl2 = KURL(kPageString2.c_str());
  const String kRemoteSourceId = "1234";
  const String kRemoteSource = kPageUrl2.GetString() + "\n" + kRemoteSourceId;

  void SetUp() override {
    const CachedStorageArea::AreaType area_type =
        IsSessionStorage() ? CachedStorageArea::AreaType::kSessionStorage
                           : CachedStorageArea::AreaType::kLocalStorage;
    test::ScopedMockedURLLoad scoped_mocked_url_load_root(
        kRootUrl, test::CoreTestDataPath("foo.html"));
    LocalDOMWindow* local_dom_window_root =
        To<LocalDOMWindow>(web_view_helper_root_.InitializeAndLoad(kRootString)
                               ->GetPage()
                               ->MainFrame()
                               ->DomWindow());
    cached_area_ = base::MakeRefCounted<CachedStorageArea>(
        area_type, kRootStorageKey, local_dom_window_root, nullptr,
        /*is_session_storage_for_prerendering=*/false);
    cached_area_->SetRemoteAreaForTesting(
        mock_storage_area_.GetInterfaceRemote());
    test::ScopedMockedURLLoad scoped_mocked_url_load(
        kPageUrl, test::CoreTestDataPath("foo.html"));
    LocalDOMWindow* local_dom_window =
        To<LocalDOMWindow>(web_view_helper_.InitializeAndLoad(kPageString)
                               ->GetPage()
                               ->MainFrame()
                               ->DomWindow());
    source_area_ =
        MakeGarbageCollected<FakeAreaSource>(kPageUrl, local_dom_window);
    source_area_id_ = cached_area_->RegisterSource(source_area_);
    source_ = kPageUrl.GetString() + "\n" + source_area_id_;
    test::ScopedMockedURLLoad scoped_mocked_url_load2(
        kPageUrl2, test::CoreTestDataPath("foo.html"));
    LocalDOMWindow* local_dom_window2 =
        To<LocalDOMWindow>(web_view_helper2_.InitializeAndLoad(kPageString2)
                               ->GetPage()
                               ->MainFrame()
                               ->DomWindow());
    source_area2_ =
        MakeGarbageCollected<FakeAreaSource>(kPageUrl2, local_dom_window2);
    cached_area_->RegisterSource(source_area2_);
  }

  virtual bool IsSessionStorage() { return false; }

  bool IsCacheLoaded() { return cached_area_->map_.get(); }

  bool IsIgnoringKeyMutations(const String& key) {
    return cached_area_->pending_mutations_by_key_.Contains(key);
  }

  static Vector<uint8_t> StringToUint8Vector(const String& input,
                                             FormatOption format) {
    return CachedStorageArea::StringToUint8Vector(input, format);
  }

  static String Uint8VectorToString(const Vector<uint8_t>& input,
                                    FormatOption format) {
    return CachedStorageArea::Uint8VectorToString(input, format);
  }

  Vector<uint8_t> KeyToUint8Vector(const String& key) {
    return StringToUint8Vector(
        key, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF8
                                : FormatOption::kLocalStorageDetectFormat);
  }

  Vector<uint8_t> ValueToUint8Vector(const String& value) {
    return StringToUint8Vector(
        value, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF16
                                  : FormatOption::kLocalStorageDetectFormat);
  }

  String KeyFromUint8Vector(const Vector<uint8_t>& key) {
    return Uint8VectorToString(
        key, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF8
                                : FormatOption::kLocalStorageDetectFormat);
  }

  String ValueFromUint8Vector(const Vector<uint8_t>& value) {
    return Uint8VectorToString(
        value, IsSessionStorage() ? FormatOption::kSessionStorageForceUTF16
                                  : FormatOption::kLocalStorageDetectFormat);
  }

  MockStorageArea::ObservedPut ObservedPut(const String& key,
                                           const String& value,
                                           const String& source) {
    return MockStorageArea::ObservedPut{KeyToUint8Vector(key),
                                        ValueToUint8Vector(value), source};
  }

  MockStorageArea::ObservedDelete ObservedDelete(const String& key,
                                                 const String& source) {
    return MockStorageArea::ObservedDelete{KeyToUint8Vector(key), source};
  }

  FakeAreaSource::Event Event(const String& key,
                              const String& old_value,
                              const String& new_value) {
    return FakeAreaSource::Event{key, old_value, new_value, ""};
  }

  void InjectKeyValue(const String& key, const String& value) {
    mock_storage_area_.InjectKeyValue(KeyToUint8Vector(key),
                                      ValueToUint8Vector(value));
  }

 protected:
  test::TaskEnvironment task_environment_;
  MockStorageArea mock_storage_area_;
  Persistent<FakeAreaSource> source_area_;
  Persistent<FakeAreaSource> source_area2_;
  scoped_refptr<CachedStorageArea> cached_area_;
  String source_area_id_;
  String source_;
  frame_test_helpers::WebViewHelper web_view_helper_root_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  frame_test_helpers::WebViewHelper web_view_helper2_;
};

class CachedStorageAreaTestWithParam
    : public CachedStorageAreaTest,
      public testing::WithParamInterface<bool> {
 public:
  bool IsSessionStorage() override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(CachedStorageAreaTest,
                         CachedStorageAreaTestWithParam,
                         ::testing::Bool());

TEST_P(CachedStorageAreaTestWithParam, Basics) {
  EXPECT_FALSE(IsCacheLoaded());

  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_EQ(kKey, cached_area_->GetKey(0));
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_EQ(0u, cached_area_->GetLength());

  mock_storage_area_.Flush();
  EXPECT_EQ(1u, mock_storage_area_.observer_count());
}

TEST_P(CachedStorageAreaTestWithParam, GetLength) {
  // Expect GetLength to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
}

TEST_P(CachedStorageAreaTestWithParam, GetKey) {
  // Expect GetKey to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->GetKey(2).IsNull());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
}

TEST_P(CachedStorageAreaTestWithParam, GetItem) {
  // Expect GetItem to load the cache.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
}

TEST_P(CachedStorageAreaTestWithParam, SetItem) {
  // Expect SetItem to load the cache and then generate a change event.
  EXPECT_FALSE(IsCacheLoaded());
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  mock_storage_area_.Flush();
  EXPECT_THAT(mock_storage_area_.observed_puts(),
              ElementsAre(ObservedPut(kKey, kValue, source_)));

  EXPECT_TRUE(source_area_->events.empty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_EQ(kKey, source_area2_->events[0].key);
    EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
    EXPECT_EQ(kValue, source_area2_->events[0].new_value);
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.empty());
  }
}

// Verify that regardless of how many times `SetItem` is called in one task,
// only one checkpoint is generated.
TEST_P(CachedStorageAreaTestWithParam, SetItemCheckpoints) {
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_EQ(mock_storage_area_.observed_checkpoints(), 0U);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(mock_storage_area_.observed_checkpoints(), 1U);

  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue2, source_area_));
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  EXPECT_TRUE(cached_area_->SetItem("key2", kValue, source_area_));
  EXPECT_EQ(mock_storage_area_.observed_checkpoints(), 1U);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(mock_storage_area_.observed_checkpoints(), 2U);
}

TEST_P(CachedStorageAreaTestWithParam, Clear_AlreadyEmpty) {
  // Clear, we expect just the one call to clear in the db since
  // there's no need to load the data prior to deleting it.
  // Except if we're testing session storage, in which case we also expect a
  // load call first, since it needs that for event dispatching.
  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->Clear(source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_THAT(mock_storage_area_.observed_delete_alls(), ElementsAre(source_));
  if (IsSessionStorage()) {
    EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
  } else {
    EXPECT_EQ(0, mock_storage_area_.observed_get_alls());
  }

  // Neither should have events since area was already empty.
  EXPECT_TRUE(source_area_->events.empty());
  EXPECT_TRUE(source_area2_->events.empty());
}

TEST_P(CachedStorageAreaTestWithParam, Clear_WithData) {
  InjectKeyValue(kKey, kValue);

  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->Clear(source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_THAT(mock_storage_area_.observed_delete_alls(), ElementsAre(source_));
  if (IsSessionStorage()) {
    EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
  } else {
    EXPECT_EQ(0, mock_storage_area_.observed_get_alls());
  }

  EXPECT_TRUE(source_area_->events.empty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_TRUE(source_area2_->events[0].key.IsNull());
    EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
    EXPECT_TRUE(source_area2_->events[0].new_value.IsNull());
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.empty());
  }
}

TEST_P(CachedStorageAreaTestWithParam, RemoveItem_NothingToRemove) {
  // RemoveItem with nothing to remove, expect just one call to load.
  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
  EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());

  // Neither should have events since area was already empty.
  EXPECT_TRUE(source_area_->events.empty());
  EXPECT_TRUE(source_area2_->events.empty());
}

TEST_P(CachedStorageAreaTestWithParam, RemoveItem) {
  // RemoveItem with something to remove, expect a call to load followed
  // by a call to remove.
  InjectKeyValue(kKey, kValue);

  EXPECT_FALSE(IsCacheLoaded());
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());
  EXPECT_THAT(mock_storage_area_.observed_deletes(),
              ElementsAre(ObservedDelete(kKey, source_)));

  EXPECT_TRUE(source_area_->events.empty());
  if (IsSessionStorage()) {
    ASSERT_EQ(1u, source_area2_->events.size());
    EXPECT_EQ(kKey, source_area2_->events[0].key);
    EXPECT_EQ(kValue, source_area2_->events[0].old_value);
    EXPECT_TRUE(source_area2_->events[0].new_value.IsNull());
    EXPECT_EQ(kPageUrl, source_area2_->events[0].url);
  } else {
    EXPECT_TRUE(source_area2_->events.empty());
  }
}

TEST_P(CachedStorageAreaTestWithParam, BrowserDisconnect) {
  InjectKeyValue(kKey, kValue);

  // GetLength to prime the cache.
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  mock_storage_area_.ResetObservations();

  // Now disconnect the pipe from the browser, simulating situations where the
  // browser might be forced to destroy the LevelDBWrapperImpl.
  mock_storage_area_.CloseAllBindings();

  // Getters should still function.
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));

  // And setters should also still function.
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_EQ(0u, cached_area_->GetLength());
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_P(CachedStorageAreaTestWithParam, ResetConnectionWithNoDelta) {
  const String kKey1 = "key1";
  const String kValue1 = "value1";
  const String kKey2 = "key2";
  const String kValue2 = "value2";
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kValue2);

  // Prime the cache.
  EXPECT_EQ(2u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  // Simulate a connection reset, which should always re-initialize the local
  // cache.
  cached_area_->ResetConnection(mock_storage_area_.GetInterfaceRemote());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(2, mock_storage_area_.observed_get_alls());
  EXPECT_EQ(2u, cached_area_->GetLength());

  // Cached values should be unchanged for both Session and Local Storage.
  EXPECT_EQ(kValue1, cached_area_->GetItem(kKey1));
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey2));

  // There should be no observed operations on the backend.
  mock_storage_area_.Flush();
  EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
  EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());

  // There should also be no generated storage events.
  EXPECT_TRUE(source_area_->events.empty());
}

TEST_P(CachedStorageAreaTestWithParam, ResetConnectionWithKeyDiff) {
  const String kKey1 = "key1";
  const String kValue1 = "value1";
  const String kKey2 = "key2";
  const String kCachedValue2 = "cached_value2";
  const String kPersistedValue2 = "persisted_value2";
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kCachedValue2);

  // Prime the cache.
  EXPECT_EQ(2u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  // Now modify the backend so it's out of sync with the cache. Namely, the
  // value of |kKey2| is no different.
  mock_storage_area_.Clear();
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kPersistedValue2);

  // Resetting the connection should re-initialize the local cache, with
  // different outcomes for Local and Session Storage.
  cached_area_->ResetConnection(mock_storage_area_.GetInterfaceRemote());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(2, mock_storage_area_.observed_get_alls());
  EXPECT_EQ(2u, cached_area_->GetLength());
  EXPECT_EQ(kValue1, cached_area_->GetItem(kKey1));
  mock_storage_area_.Flush();

  if (IsSessionStorage()) {
    // For Session Storage, we expect the local cache to push changes to the
    // backend, as the local cache is the source of truth.
    EXPECT_EQ(kCachedValue2, cached_area_->GetItem(kKey2));
    EXPECT_THAT(mock_storage_area_.observed_puts(),
                ElementsAre(ObservedPut(kKey2, kCachedValue2, "\n")));
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_TRUE(source_area_->events.empty());
  } else {
    // For Local Storage, we expect no mutations to the backend but instead a
    // storage event to be broadcast for the diff.
    EXPECT_EQ(kPersistedValue2, cached_area_->GetItem(kKey2));
    EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_THAT(source_area_->events,
                ElementsAre(Event(kKey2, kCachedValue2, kPersistedValue2)));
  }
}

TEST_P(CachedStorageAreaTestWithParam, ResetConnectionWithMissingBackendKey) {
  const String kKey1 = "key1";
  const String kValue1 = "value1";
  const String kKey2 = "key2";
  const String kValue2 = "value2";
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kValue2);

  // Prime the cache.
  EXPECT_EQ(2u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  // Now modify the backend so it's out of sync with the cache. Namely, |kKey2|
  // is no longer present in the backend.
  mock_storage_area_.Clear();
  InjectKeyValue(kKey1, kValue1);

  // Resetting the connection should re-initialize the local cache, with
  // different outcomes for Local and Session Storage.
  cached_area_->ResetConnection(mock_storage_area_.GetInterfaceRemote());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(2, mock_storage_area_.observed_get_alls());
  EXPECT_EQ(kValue1, cached_area_->GetItem(kKey1));
  mock_storage_area_.Flush();

  if (IsSessionStorage()) {
    // For Session Storage, we expect the local cache to push changes to the
    // backend, as the local cache is the source of truth.
    EXPECT_EQ(2u, cached_area_->GetLength());
    EXPECT_EQ(kValue2, cached_area_->GetItem(kKey2));
    EXPECT_THAT(mock_storage_area_.observed_puts(),
                ElementsAre(ObservedPut(kKey2, kValue2, "\n")));
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_TRUE(source_area_->events.empty());
  } else {
    // For Local Storage, we expect no mutations to the backend but instead a
    // storage event to be broadcast for the diff.
    EXPECT_EQ(1u, cached_area_->GetLength());
    EXPECT_TRUE(cached_area_->GetItem(kKey2).IsNull());
    EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_THAT(source_area_->events,
                ElementsAre(Event(kKey2, kValue2, String())));
  }
}

TEST_P(CachedStorageAreaTestWithParam, ResetConnectionWithMissingLocalKey) {
  const String kKey1 = "key1";
  const String kValue1 = "value1";
  const String kKey2 = "key2";
  const String kValue2 = "value2";
  InjectKeyValue(kKey1, kValue1);

  // Prime the cache.
  EXPECT_EQ(1u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  // Now modify the backend so it's out of sync with the cache. Namely, |kKey2|
  // is present in the backend despite never being cached locally.
  InjectKeyValue(kKey2, kValue2);

  // Resetting the connection should re-initialize the local cache, with
  // different outcomes for Local and Session Storage.
  cached_area_->ResetConnection(mock_storage_area_.GetInterfaceRemote());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(2, mock_storage_area_.observed_get_alls());
  EXPECT_EQ(kValue1, cached_area_->GetItem(kKey1));
  mock_storage_area_.Flush();

  if (IsSessionStorage()) {
    // For Session Storage, we expect the local cache to push changes to the
    // backend, as the local cache is the source of truth.
    EXPECT_EQ(1u, cached_area_->GetLength());
    EXPECT_TRUE(cached_area_->GetItem(kKey2).IsNull());
    EXPECT_THAT(mock_storage_area_.observed_deletes(),
                ElementsAre(ObservedDelete(kKey2, "\n")));
    EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
    EXPECT_TRUE(source_area_->events.empty());
  } else {
    // For Local Storage, we expect no mutations to the backend but instead a
    // storage event to be broadcast for the diff.
    EXPECT_EQ(2u, cached_area_->GetLength());
    EXPECT_EQ(kValue2, cached_area_->GetItem(kKey2));
    EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_THAT(source_area_->events,
                ElementsAre(Event(kKey2, String(), kValue2)));
  }
}

TEST_P(CachedStorageAreaTestWithParam, ResetConnectionWithComplexDiff) {
  const String kKey1 = "key1";
  const String kValue1 = "value1";
  const String kKey2 = "key2";
  const String kValue2 = "value2";
  const String kAltValue2 = "alt_value2";
  const String kKey3 = "key3";
  const String kValue3 = "value3";
  const String kKey4 = "key4";
  const String kValue4 = "value4";
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kValue2);
  InjectKeyValue(kKey3, kValue3);

  // Prime the cache.
  EXPECT_EQ(3u, cached_area_->GetLength());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(1, mock_storage_area_.observed_get_alls());

  // Now modify the backend so it's out of sync with the cache. Namely, the
  // value of |kKey2| differs, |kKey3| is no longer present in the backend, and
  // |kKey4| is now present where it wasn't before.
  mock_storage_area_.Clear();
  InjectKeyValue(kKey1, kValue1);
  InjectKeyValue(kKey2, kAltValue2);
  InjectKeyValue(kKey4, kValue4);

  // Resetting the connection should re-initialize the local cache, with
  // different outcomes for Local and Session Storage.
  cached_area_->ResetConnection(mock_storage_area_.GetInterfaceRemote());
  EXPECT_TRUE(IsCacheLoaded());
  EXPECT_EQ(2, mock_storage_area_.observed_get_alls());
  EXPECT_EQ(3u, cached_area_->GetLength());
  EXPECT_EQ(kValue1, cached_area_->GetItem(kKey1));
  mock_storage_area_.Flush();

  if (IsSessionStorage()) {
    // For Session Storage, we expect the local cache to push changes to the
    // backend, as the local cache is the source of truth.
    EXPECT_EQ(kValue2, cached_area_->GetItem(kKey2));
    EXPECT_EQ(kValue3, cached_area_->GetItem(kKey3));
    EXPECT_TRUE(cached_area_->GetItem(kKey4).IsNull());
    EXPECT_THAT(mock_storage_area_.observed_puts(),
                UnorderedElementsAre(ObservedPut(kKey2, kValue2, "\n"),
                                     ObservedPut(kKey3, kValue3, "\n")));
    EXPECT_THAT(mock_storage_area_.observed_deletes(),
                ElementsAre(ObservedDelete(kKey4, "\n")));
    EXPECT_TRUE(source_area_->events.empty());
  } else {
    // For Local Storage, we expect no mutations to the backend but instead a
    // storage event to be broadcast for the diff.
    EXPECT_EQ(kAltValue2, cached_area_->GetItem(kKey2));
    EXPECT_TRUE(cached_area_->GetItem(kKey3).IsNull());
    EXPECT_EQ(kValue4, cached_area_->GetItem(kKey4));
    EXPECT_TRUE(mock_storage_area_.observed_puts().empty());
    EXPECT_TRUE(mock_storage_area_.observed_deletes().empty());
    EXPECT_THAT(source_area_->events,
                UnorderedElementsAre(Event(kKey2, kValue2, kAltValue2),
                                     Event(kKey3, kValue3, String()),
                                     Event(kKey4, String(), kValue4)));
  }
}

TEST_F(CachedStorageAreaTest, KeyMutationsAreIgnoredUntilCompletion) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();

  // SetItem
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyDeleted(KeyToUint8Vector(kKey), std::nullopt, kRemoteSource);
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       std::nullopt, source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // RemoveItem
  cached_area_->RemoveItem(kKey, source_area_);
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // Multiple mutations to the same key.
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  cached_area_->RemoveItem(kKey, source_area_);
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  mock_storage_area_.Flush();
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       std::nullopt, source_);
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       source_);
  EXPECT_FALSE(IsIgnoringKeyMutations(kKey));

  // A failed set item operation should reset the key's cached value.
  EXPECT_TRUE(cached_area_->SetItem(kKey, kValue, source_area_));
  mock_storage_area_.Flush();
  EXPECT_TRUE(IsIgnoringKeyMutations(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, ChangeEvents) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();

  cached_area_->SetItem(kKey, kValue, source_area_);
  cached_area_->SetItem(kKey, kValue2, source_area_);
  cached_area_->RemoveItem(kKey, source_area_);
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       std::nullopt, source_);
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       ValueToUint8Vector(kValue), source_);
  observer->KeyDeleted(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       source_);

  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       std::nullopt, kRemoteSource);
  observer->AllDeleted(/*was_nonempty=*/true, kRemoteSource);

  // Source area should have ignored all but the last two events.
  ASSERT_EQ(2u, source_area_->events.size());

  EXPECT_EQ(kKey, source_area_->events[0].key);
  EXPECT_TRUE(source_area_->events[0].old_value.IsNull());
  EXPECT_EQ(kValue, source_area_->events[0].new_value);
  EXPECT_EQ(kPageUrl2, source_area_->events[0].url);

  EXPECT_TRUE(source_area_->events[1].key.IsNull());
  EXPECT_TRUE(source_area_->events[1].old_value.IsNull());
  EXPECT_TRUE(source_area_->events[1].new_value.IsNull());
  EXPECT_EQ(kPageUrl2, source_area_->events[1].url);

  // Second area should not have ignored any of the events.
  ASSERT_EQ(5u, source_area2_->events.size());

  EXPECT_EQ(kKey, source_area2_->events[0].key);
  EXPECT_TRUE(source_area2_->events[0].old_value.IsNull());
  EXPECT_EQ(kValue, source_area2_->events[0].new_value);
  EXPECT_EQ(kPageUrl, source_area2_->events[0].url);

  EXPECT_EQ(kKey, source_area2_->events[1].key);
  EXPECT_EQ(kValue, source_area2_->events[1].old_value);
  EXPECT_EQ(kValue2, source_area2_->events[1].new_value);
  EXPECT_EQ(kPageUrl, source_area2_->events[1].url);

  EXPECT_EQ(kKey, source_area2_->events[2].key);
  EXPECT_EQ(kValue2, source_area2_->events[2].old_value);
  EXPECT_TRUE(source_area2_->events[2].new_value.IsNull());
  EXPECT_EQ(kPageUrl, source_area2_->events[2].url);

  EXPECT_EQ(kKey, source_area2_->events[3].key);
  EXPECT_TRUE(source_area2_->events[3].old_value.IsNull());
  EXPECT_EQ(kValue, source_area2_->events[3].new_value);
  EXPECT_EQ(kPageUrl2, source_area2_->events[3].url);

  EXPECT_TRUE(source_area2_->events[4].key.IsNull());
  EXPECT_TRUE(source_area2_->events[4].old_value.IsNull());
  EXPECT_TRUE(source_area2_->events[4].new_value.IsNull());
  EXPECT_EQ(kPageUrl2, source_area2_->events[4].url);
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailed) {
  // Verifies that when local key changes fail, the cache is restored to an
  // appropriate state.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedWithSubsequentChanges) {
  // Failure of an operation observed while another subsequent operation is
  // still queued. In this case, no revert should happen because the change that
  // would be reverted has already been overwritten.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       std::nullopt, source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
}

TEST_F(CachedStorageAreaTest, RevertOnConsecutiveChangeFailures) {
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  // If two operations fail in a row, the cache should revert to the original
  // state before either |SetItem()|.
  cached_area_->SetItem(kKey, kValue, source_area_);
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  // Still caching |kValue2| because that operation is still pending.
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  // Now that the second operation also failed, the cache should revert to the
  // value from before the first |SetItem()|, i.e. no value.
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedWithNonLocalChanges) {
  // If a non-local mutation is observed while a local mutation is pending
  // acknowledgement, and that local mutation ends up getting rejected, the
  // cache should revert to a state reflecting the non-local change that was
  // temporarily ignored.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  // Should be ignored.
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue2),
                       std::nullopt, kRemoteSource);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  // Now that we fail the pending |SetItem()|, the above remote change should be
  // reflected.
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
}

TEST_F(CachedStorageAreaTest, RevertOnChangeFailedAfterNonLocalClear) {
  // If a non-local clear is observed while a local mutation is pending
  // acknowledgement and that local mutation ends up getting rejected, the cache
  // should revert the key to have no value, even if it had a value during the
  // corresponding |SetItem()| call.
  mojom::blink::StorageAreaObserver* observer = cached_area_.get();
  cached_area_->SetItem(kKey, kValue, source_area_);
  EXPECT_EQ(kValue, cached_area_->GetItem(kKey));
  cached_area_->SetItem(kKey, kValue2, source_area_);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));
  observer->KeyChanged(KeyToUint8Vector(kKey), ValueToUint8Vector(kValue),
                       std::nullopt, source_);
  // We still have |kValue2| cached since its mutation is still pending.
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));

  // Even after a non-local clear is observed, |kValue2| remains cached because
  // pending local mutations are replayed over a non-local clear.
  observer->AllDeleted(true, kRemoteSource);
  EXPECT_EQ(kValue2, cached_area_->GetItem(kKey));

  // But if that pending mutation fails, we should "revert" to the cleared
  // value, as that's what the backend would have.
  observer->KeyChangeFailed(KeyToUint8Vector(kKey), source_);
  EXPECT_TRUE(cached_area_->GetItem(kKey).IsNull());
}

namespace {

class StringEncoding : public CachedStorageAreaTest,
                       public testing::WithParamInterface<FormatOption> {};

INSTANTIATE_TEST_SUITE_P(
    CachedStorageAreaTest,
    StringEncoding,
    ::testing::Values(FormatOption::kLocalStorageDetectFormat,
                      FormatOption::kSessionStorageForceUTF16,
                      FormatOption::kSessionStorageForceUTF8));

TEST_P(StringEncoding, RoundTrip_ASCII) {
  String key("simplekey");
  EXPECT_EQ(
      Uint8VectorToString(StringToUint8Vector(key, GetParam()), GetParam()),
      key);
}

TEST_P(StringEncoding, RoundTrip_Latin1) {
  String key("Test\xf6\xb5");
  EXPECT_TRUE(key.Is8Bit());
  EXPECT_EQ(
      Uint8VectorToString(StringToUint8Vector(key, GetParam()), GetParam()),
      key);
}

TEST_P(StringEncoding, RoundTrip_UTF16) {
  StringBuilder key;
  key.Append("key");
  key.Append(UChar(0xd83d));
  key.Append(UChar(0xde00));
  EXPECT_EQ(Uint8VectorToString(StringToUint8Vector(key.ToString(), GetParam()),
                                GetParam()),
            key);
}

TEST_P(StringEncoding, RoundTrip_InvalidUTF16) {
  StringBuilder key;
  key.Append("foo");
  key.Append(UChar(0xd83d));
  key.Append(UChar(0xde00));
  key.Append(UChar(0xdf01));
  key.Append("bar");
  if (GetParam() != FormatOption::kSessionStorageForceUTF8) {
    EXPECT_EQ(Uint8VectorToString(
                  StringToUint8Vector(key.ToString(), GetParam()), GetParam()),
              key);
  } else {
    StringBuilder validKey;
    validKey.Append("foo");
    validKey.Append(UChar(0xd83d));
    validKey.Append(UChar(0xde00));
    validKey.Append(UChar(0xfffd));
    validKey.Append("bar");
    EXPECT_EQ(Uint8VectorToString(
                  StringToUint8Vector(key.ToString(), GetParam()), GetParam()),
              validKey.ToString());
  }
}

}  // namespace

TEST_F(CachedStorageAreaTest, StringEncoding_LocalStorage) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kLocalStorageDetectFormat)
          .size(),
      ascii_key.length() + 1);
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kLocalStorageDetectFormat)
                .size(),
            non_ascii_key.length() * 2 + 1);
}

TEST_F(CachedStorageAreaTest, StringEncoding_UTF8) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kSessionStorageForceUTF8)
          .size(),
      ascii_key.length());
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kSessionStorageForceUTF8)
                .size(),
            7u);
}

TEST_F(CachedStorageAreaTest, StringEncoding_UTF16) {
  String ascii_key("simplekey");
  StringBuilder non_ascii_key;
  non_ascii_key.Append("key");
  non_ascii_key.Append(UChar(0xd83d));
  non_ascii_key.Append(UChar(0xde00));
  EXPECT_EQ(
      StringToUint8Vector(ascii_key, FormatOption::kSessionStorageForceUTF16)
          .size(),
      ascii_key.length() * 2);
  EXPECT_EQ(StringToUint8Vector(non_ascii_key.ToString(),
                                FormatOption::kSessionStorageForceUTF16)
                .size(),
            non_ascii_key.length() * 2);
}

TEST_F(CachedStorageAreaTest, RecoveryWhenNoLocalDOMWindowPresent) {
  frame_test_helpers::WebViewHelper web_view_helper;
  test::ScopedMockedURLLoad scoped_mocked_url_load(
      CachedStorageAreaTest::kPageUrl, test::CoreTestDataPath("foo.html"));
  auto* local_dom_window = To<LocalDOMWindow>(
      web_view_helper.InitializeAndLoad(CachedStorageAreaTest::kPageString)
          ->GetPage()
          ->MainFrame()
          ->DomWindow());
  auto* source_area = MakeGarbageCollected<FakeAreaSource>(
      CachedStorageAreaTest::kPageUrl, local_dom_window);
  StorageController::DomStorageConnection connection;
  std::ignore = connection.dom_storage_remote.BindNewPipeAndPassReceiver();
  StorageController controller(std::move(connection), 100);
  auto* sessionStorage = MakeGarbageCollected<StorageNamespace>(
      *local_dom_window->GetFrame()->GetPage(), &controller, "foo");

  // When no local DOM window is present this shouldn't fatal, just not bind
  auto cached_area = base::MakeRefCounted<CachedStorageArea>(
      CachedStorageArea::AreaType::kSessionStorage,
      CachedStorageAreaTest::kRootStorageKey, nullptr, sessionStorage,
      /*is_session_storage_for_prerendering=*/false);

  // If we add an active source then re-bind it should work
  cached_area->RegisterSource(source_area);
  EXPECT_EQ(0u, cached_area->GetLength());
}

}  // namespace blink
