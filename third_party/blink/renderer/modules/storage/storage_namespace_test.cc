// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_namespace.h"

#include <tuple>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {
namespace {

constexpr size_t kTestCacheLimit = 100;

TEST(StorageNamespaceTest, BasicStorageAreas) {
  const String kKey("key");
  const String kValue("value");
  const String kSessionStorageNamespace("abcd");
  const std::string kRootString = "http://dom_storage/page";
  const KURL kRootUrl = KURL(kRootString.c_str());
  const std::string kPageString = "http://dom_storage1/";
  const KURL kPageUrl = KURL(kPageString.c_str());
  const std::string kPageString2 = "http://dom_storage2/";
  const KURL kPageUrl2 = KURL(kPageString2.c_str());
  const std::string kPageString3 = "http://dom_storage3/";
  const KURL kPageUrl3 = KURL(kPageString3.c_str());

  test::TaskEnvironment task_environment;
  test::ScopedMockedURLLoad scoped_mocked_url_load_root(
      kRootUrl, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper_root;
  LocalDOMWindow* local_dom_window_root =
      To<LocalDOMWindow>(web_view_helper_root.InitializeAndLoad(kRootString)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  Persistent<FakeAreaSource> source_area =
      MakeGarbageCollected<FakeAreaSource>(kRootUrl, local_dom_window_root);

  StorageController::DomStorageConnection connection;
  std::ignore = connection.dom_storage_remote.BindNewPipeAndPassReceiver();
  StorageController controller(std::move(connection), kTestCacheLimit);

  StorageNamespace* localStorage =
      MakeGarbageCollected<StorageNamespace>(&controller);
  StorageNamespace* sessionStorage = MakeGarbageCollected<StorageNamespace>(
      *local_dom_window_root->GetFrame()->GetPage(), &controller,
      kSessionStorageNamespace);

  EXPECT_FALSE(localStorage->IsSessionStorage());
  EXPECT_TRUE(sessionStorage->IsSessionStorage());

  test::ScopedMockedURLLoad scoped_mocked_url_load(
      kPageUrl, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper;
  LocalDOMWindow* local_dom_window =
      To<LocalDOMWindow>(web_view_helper.InitializeAndLoad(kPageString)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area1 = localStorage->GetCachedArea(local_dom_window);
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);

  test::ScopedMockedURLLoad scoped_mocked_url_load2(
      kPageUrl2, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper2;
  LocalDOMWindow* local_dom_window2 =
      To<LocalDOMWindow>(web_view_helper2.InitializeAndLoad(kPageString2)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area2 = localStorage->GetCachedArea(local_dom_window2);
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);

  test::ScopedMockedURLLoad scoped_mocked_url_load3(
      kPageUrl3, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper3;
  LocalDOMWindow* local_dom_window3 =
      To<LocalDOMWindow>(web_view_helper3.InitializeAndLoad(kPageString3)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area3 = sessionStorage->GetCachedArea(local_dom_window3);
  cached_area3->RegisterSource(source_area);
  cached_area3->SetItem(kKey, kValue, source_area);

  EXPECT_EQ(cached_area1->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area2->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area3->GetItem(kKey), kValue);
}

}  // namespace
}  // namespace blink
