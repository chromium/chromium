// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webshare/navigator_share.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_property_bag.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_share_data.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using mojom::blink::SharedFile;
using mojom::blink::SharedFilePtr;
using mojom::blink::ShareService;

// A mock ShareService used to intercept calls to the mojo methods.
class MockShareService : public ShareService {
 public:
  MockShareService() : error_(mojom::ShareError::OK) {}
  ~MockShareService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::ShareService>(std::move(handle)));
  }

  void set_error(mojom::ShareError value) { error_ = value; }

  const WTF::String& title() const { return title_; }
  const WTF::String& text() const { return text_; }
  const KURL& url() const { return url_; }
  const WTF::Vector<SharedFilePtr>& files() const { return files_; }
  mojom::ShareError error() const { return error_; }

 private:
  void Share(const WTF::String& title,
             const WTF::String& text,
             const KURL& url,
             WTF::Vector<SharedFilePtr> files,
             ShareCallback callback) override {
    title_ = title;
    text_ = text;
    url_ = url;

    files_.clear();
    files_.ReserveInitialCapacity(files.size());
    for (const auto& entry : files) {
      files_.push_back(entry->Clone());
    }

    std::move(callback).Run(error_);
  }

  mojo::Receiver<ShareService> receiver_{this};
  WTF::String title_;
  WTF::String text_;
  KURL url_;
  WTF::Vector<SharedFilePtr> files_;
  mojom::ShareError error_;
};

class NavigatorShareTest : public testing::Test {
 public:
  NavigatorShareTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  Document& GetDocument() { return holder_->GetDocument(); }

  LocalFrame& GetFrame() { return holder_->GetFrame(); }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  void Share(const ShareData& share_data) {
    LocalFrame::NotifyUserActivation(
        &GetFrame(), mojom::UserActivationNotificationType::kTest);
    Navigator* navigator = GetFrame().DomWindow()->navigator();
    NonThrowableExceptionState exception_state;
    ScriptPromiseUntyped promise = NavigatorShare::share(
        GetScriptState(), *navigator, &share_data, exception_state);
    test::RunPendingTasks();
    EXPECT_EQ(mock_share_service_.error() == mojom::ShareError::OK
                  ? v8::Promise::kFulfilled
                  : v8::Promise::kRejected,
              promise.V8Promise()->State());
  }

  MockShareService& mock_share_service() { return mock_share_service_; }

 protected:
  void SetUp() override {
    GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(
            KURL("https://example.com")),
        nullptr /* extra_data */);
    test::RunPendingTasks();

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        ShareService::Name_,
        WTF::BindRepeating(&MockShareService::Bind,
                           WTF::Unretained(&mock_share_service_)));
  }

  void TearDown() override {
    // Remove the testing binder to avoid crashes between tests caused by
    // MockShareService rebinding an already-bound Binding.
    // See https://crbug.com/1010116 for more information.
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        ShareService::Name_, {});

    MemoryCache::Get()->EvictResources();
  }

 public:
  test::TaskEnvironment task_environment;
  MockShareService mock_share_service_;

  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(NavigatorShareTest, ShareText) {
  const String title = "Subject";
  const String message = "Body";
  const String url = "https://example.com/path?query#fragment";

  ShareData* share_data = MakeGarbageCollected<ShareData>();
  share_data->setTitle(title);
  share_data->setText(message);
  share_data->setUrl(url);
  Share(*share_data);

  EXPECT_EQ(mock_share_service().title(), title);
  EXPECT_EQ(mock_share_service().text(), message);
  EXPECT_EQ(mock_share_service().url(), KURL(url));
  EXPECT_EQ(mock_share_service().files().size(), 0U);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingTitle));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingText));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingUrl));
  EXPECT_TRUE(
      GetDocument().IsUseCounted(WebFeature::kWebShareSuccessfulWithoutFiles));
}

File* CreateSampleFile(ExecutionContext* context,
                       const String& file_name,
                       const String& content_type,
                       const String& file_contents) {
  HeapVector<Member<V8BlobPart>> blob_parts;
  blob_parts.push_back(MakeGarbageCollected<V8BlobPart>(file_contents));

  FilePropertyBag* file_property_bag = MakeGarbageCollected<FilePropertyBag>();
  file_property_bag->setType(content_type);
  return File::Create(context, blob_parts, file_name, file_property_bag);
}

TEST_F(NavigatorShareTest, ShareFile) {
  const String file_name = "chart.svg";
  const String content_type = "image/svg+xml";
  const String file_contents = "<svg></svg>";

  HeapVector<Member<File>> files;
  files.push_back(CreateSampleFile(ExecutionContext::From(GetScriptState()),
                                   file_name, content_type, file_contents));

  ShareData* share_data = MakeGarbageCollected<ShareData>();
  share_data->setFiles(files);
  Share(*share_data);

  EXPECT_EQ(mock_share_service().files().size(), 1U);
  EXPECT_EQ(mock_share_service().files()[0]->name.path(),
            StringToFilePath(file_name));
  EXPECT_EQ(mock_share_service().files()[0]->blob->GetType(), content_type);
  EXPECT_EQ(mock_share_service().files()[0]->blob->size(),
            file_contents.length());
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingFiles));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kWebShareSuccessfulContainingFiles));
}

TEST_F(NavigatorShareTest, CancelShare) {
  const String title = "Subject";
  ShareData* share_data = MakeGarbageCollected<ShareData>();
  share_data->setTitle(title);

  mock_share_service().set_error(mojom::blink::ShareError::CANCELED);
  Share(*share_data);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingTitle));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kWebShareUnsuccessfulWithoutFiles));
}

TEST_F(NavigatorShareTest, CancelShareWithFile) {
  const String file_name = "counts.csv";
  const String content_type = "text/csv";
  const String file_contents = "1,2,3";

  const String url = "https://example.site";

  HeapVector<Member<File>> files;
  files.push_back(CreateSampleFile(ExecutionContext::From(GetScriptState()),
                                   file_name, content_type, file_contents));

  ShareData* share_data = MakeGarbageCollected<ShareData>();
  share_data->setFiles(files);
  share_data->setUrl(url);

  mock_share_service().set_error(mojom::blink::ShareError::CANCELED);
  Share(*share_data);
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingFiles));
  EXPECT_TRUE(GetDocument().IsUseCounted(WebFeature::kWebShareContainingUrl));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kWebShareUnsuccessfulContainingFiles));
}

}  // namespace blink
