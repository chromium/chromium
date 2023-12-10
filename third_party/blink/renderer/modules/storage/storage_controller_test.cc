// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/testing/scoped_mocked_url.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {
namespace {

const size_t kTestCacheLimit = 100;
class MockDomStorage : public mojom::blink::DomStorage {
 public:
  // mojom::blink::DomStorage implementation:
  void OpenLocalStorage(
      const blink::BlinkStorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      mojo::PendingReceiver<mojom::blink::StorageArea> receiver) override {}
  void BindSessionStorageNamespace(
      const String& namespace_id,
      mojo::PendingReceiver<mojom::blink::SessionStorageNamespace> receiver)
      override {}
  void BindSessionStorageArea(
      const blink::BlinkStorageKey& storage_key,
      const blink::LocalFrameToken& local_frame_token,
      const String& namespace_id,
      mojo::PendingReceiver<mojom::blink::StorageArea> receiver) override {
    session_storage_opens++;
  }

  void GetSessionStorageUsage(int32_t* out) const {
    *out = session_storage_opens;
  }

  int32_t session_storage_opens = 0;
};

}  // namespace

TEST(StorageControllerTest, CacheLimit) {
  const String kKey("key");
  const String kValue("value");
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
  PostCrossThreadTask(
      *base::ThreadPool::CreateSequencedTaskRunner({}), FROM_HERE,
      CrossThreadBindOnce(
          [](mojo::PendingReceiver<mojom::blink::DomStorage> receiver) {
            mojo::MakeSelfOwnedReceiver(std::make_unique<MockDomStorage>(),
                                        std::move(receiver));
          },
          connection.dom_storage_remote.BindNewPipeAndPassReceiver()));

  StorageController controller(std::move(connection), kTestCacheLimit);

  test::ScopedMockedURLLoad scoped_mocked_url_load(
      kPageUrl, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper;
  LocalDOMWindow* local_dom_window =
      To<LocalDOMWindow>(web_view_helper.InitializeAndLoad(kPageString)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area1 = controller.GetLocalStorageArea(local_dom_window);
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  const auto* area1_ptr = cached_area1.get();
  size_t expected_total = (kKey.length() + kValue.length()) * 2;
  EXPECT_EQ(expected_total, cached_area1->quota_used());
  EXPECT_EQ(expected_total, controller.TotalCacheSize());
  cached_area1 = nullptr;

  test::ScopedMockedURLLoad scoped_mocked_url_load2(
      kPageUrl2, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper2;
  LocalDOMWindow* local_dom_window2 =
      To<LocalDOMWindow>(web_view_helper2.InitializeAndLoad(kPageString2)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area2 = controller.GetLocalStorageArea(local_dom_window2);
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  // Area for local_dom_window should still be alive.
  EXPECT_EQ(2 * cached_area2->quota_used(), controller.TotalCacheSize());
  EXPECT_EQ(area1_ptr, controller.GetLocalStorageArea(local_dom_window));

  test::ScopedMockedURLLoad scoped_mocked_url_load3(
      kPageUrl3, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper3;
  LocalDOMWindow* local_dom_window3 =
      To<LocalDOMWindow>(web_view_helper3.InitializeAndLoad(kPageString3)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  String long_value(Vector<UChar>(kTestCacheLimit, 'a'));
  cached_area2->SetItem(kKey, long_value, source_area);
  // Cache is cleared when a new area is opened.
  auto cached_area3 = controller.GetLocalStorageArea(local_dom_window3);
  EXPECT_EQ(cached_area2->quota_used(), controller.TotalCacheSize());
}

TEST(StorageControllerTest, CacheLimitSessionStorage) {
  const String kNamespace1 = WTF::CreateCanonicalUUIDString();
  const String kNamespace2 = WTF::CreateCanonicalUUIDString();
  const String kKey("key");
  const String kValue("value");
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

  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner({});

  auto mock_dom_storage = std::make_unique<MockDomStorage>();
  MockDomStorage* dom_storage_ptr = mock_dom_storage.get();

  StorageController::DomStorageConnection connection;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<MockDomStorage> dom_storage_ptr,
             mojo::PendingReceiver<mojom::blink::DomStorage> receiver) {
            mojo::MakeSelfOwnedReceiver(std::move(dom_storage_ptr),
                                        std::move(receiver));
          },
          std::move(mock_dom_storage),
          connection.dom_storage_remote.BindNewPipeAndPassReceiver()));

  StorageController controller(std::move(connection), kTestCacheLimit);

  StorageNamespace* ns1 = controller.CreateSessionStorageNamespace(
      *local_dom_window_root->GetFrame()->GetPage(), kNamespace1);
  StorageNamespace* ns2 = controller.CreateSessionStorageNamespace(
      *local_dom_window_root->GetFrame()->GetPage(), kNamespace2);

  test::ScopedMockedURLLoad scoped_mocked_url_load(
      kPageUrl, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper;
  LocalDOMWindow* local_dom_window =
      To<LocalDOMWindow>(web_view_helper.InitializeAndLoad(kPageString)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area1 = ns1->GetCachedArea(local_dom_window);
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  const auto* area1_ptr = cached_area1.get();
  size_t expected_total = (kKey.length() + kValue.length()) * 2;
  EXPECT_EQ(expected_total, cached_area1->quota_used());
  EXPECT_EQ(expected_total, controller.TotalCacheSize());
  cached_area1 = nullptr;

  test::ScopedMockedURLLoad scoped_mocked_url_load2(
      kPageUrl2, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper2;
  LocalDOMWindow* local_dom_window2 =
      To<LocalDOMWindow>(web_view_helper2.InitializeAndLoad(kPageString2)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  auto cached_area2 = ns2->GetCachedArea(local_dom_window2);
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  // Area for local_dom_window should still be alive.
  EXPECT_EQ(2 * cached_area2->quota_used(), controller.TotalCacheSize());
  EXPECT_EQ(area1_ptr, ns1->GetCachedArea(local_dom_window));

  test::ScopedMockedURLLoad scoped_mocked_url_load3(
      kPageUrl3, test::CoreTestDataPath("foo.html"));
  frame_test_helpers::WebViewHelper web_view_helper3;
  LocalDOMWindow* local_dom_window3 =
      To<LocalDOMWindow>(web_view_helper3.InitializeAndLoad(kPageString3)
                             ->GetPage()
                             ->MainFrame()
                             ->DomWindow());
  String long_value(Vector<UChar>(kTestCacheLimit, 'a'));
  cached_area2->SetItem(kKey, long_value, source_area);
  // Cache is cleared when a new area is opened.
  auto cached_area3 = ns1->GetCachedArea(local_dom_window3);
  EXPECT_EQ(cached_area2->quota_used(), controller.TotalCacheSize());

  int32_t opens = 0;
  {
    base::RunLoop loop;
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MockDomStorage::GetSessionStorageUsage,
                       base::Unretained(dom_storage_ptr), &opens),
        loop.QuitClosure());
    loop.Run();
  }
  EXPECT_EQ(opens, 3);
}

}  // namespace blink
