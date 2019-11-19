/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/chrome_client_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-blink.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/mock_file_chooser.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/testing/stub_graphics_layer_client.h"

namespace blink {

class ViewCreatingClient : public frame_test_helpers::TestWebViewClient {
 public:
  WebView* CreateView(WebLocalFrame* opener,
                      const WebURLRequest&,
                      const WebWindowFeatures&,
                      const WebString& name,
                      WebNavigationPolicy,
                      WebSandboxFlags,
                      const FeaturePolicy::FeatureState&,
                      const SessionStorageNamespaceId&) override {
    return web_view_helper_.InitializeWithOpener(opener);
  }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

class CreateWindowTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize(nullptr, &web_view_client_);
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
  }

  ViewCreatingClient web_view_client_;
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  WebLocalFrame* main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(CreateWindowTest, CreateWindowFromPausedPage) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  FrameLoadRequest request(frame->GetDocument(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  WebWindowFeatures features;
  EXPECT_EQ(nullptr, chrome_client_impl_->CreateWindow(
                         frame, request, "", features, WebSandboxFlags::kNone,
                         FeaturePolicy::FeatureState(), ""));
}

class FakeColorChooserClient : public GarbageCollected<FakeColorChooserClient>,
                               public ColorChooserClient {
 public:
  FakeColorChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeColorChooserClient() override = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(owner_element_);
    ColorChooserClient::Trace(visitor);
  }

  USING_GARBAGE_COLLECTED_MIXIN(FakeColorChooserClient);

  // ColorChooserClient
  void DidChooseColor(const Color& color) override {}
  void DidEndChooser() override {}
  Element& OwnerElement() const override { return *owner_element_; }
  IntRect ElementRectRelativeToViewport() const override { return IntRect(); }
  Color CurrentColor() override { return Color(); }
  bool ShouldShowSuggestions() const override { return false; }
  Vector<mojom::blink::ColorSuggestionPtr> Suggestions() const override {
    return Vector<mojom::blink::ColorSuggestionPtr>();
  }

 private:
  Member<Element> owner_element_;
};

class FakeDateTimeChooserClient
    : public GarbageCollected<FakeDateTimeChooserClient>,
      public DateTimeChooserClient {
 public:
  FakeDateTimeChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeDateTimeChooserClient() override = default;

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(owner_element_);
    DateTimeChooserClient::Trace(visitor);
  }

  USING_GARBAGE_COLLECTED_MIXIN(FakeDateTimeChooserClient);

  // DateTimeChooserClient
  Element& OwnerElement() const override { return *owner_element_; }
  void DidChooseValue(const String&) override {}
  void DidChooseValue(double) override {}
  void DidEndChooser() override {}

 private:
  Member<Element> owner_element_;
};

// TODO(crbug.com/779126): A number of popups are not supported in immersive
// mode. The PagePopupSuppressionTests ensure that these unsupported popups
// do not appear in immersive mode.
class PagePopupSuppressionTest : public testing::Test {
 public:
  PagePopupSuppressionTest() = default;

  bool CanOpenColorChooser() {
    LocalFrame* frame = main_frame_->GetFrame();
    Color color;
    return !!chrome_client_impl_->OpenColorChooser(frame, color_chooser_client_,
                                                   color);
  }

  bool CanOpenDateTimeChooser() {
    LocalFrame* frame = main_frame_->GetFrame();
    DateTimeChooserParameters params;
    params.locale = DefaultLanguage();
    return !!chrome_client_impl_->OpenDateTimeChooser(
        frame, date_time_chooser_client_, params);
  }

  Settings* GetSettings() {
    LocalFrame* frame = main_frame_->GetFrame();
    return frame->GetDocument()->GetSettings();
  }

 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize();
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
    LocalFrame* frame = helper_.LocalMainFrame()->GetFrame();
    color_chooser_client_ = MakeGarbageCollected<FakeColorChooserClient>(
        frame->GetDocument()->documentElement());
    date_time_chooser_client_ = MakeGarbageCollected<FakeDateTimeChooserClient>(
        frame->GetDocument()->documentElement());
    select_ = MakeGarbageCollected<HTMLSelectElement>(*(frame->GetDocument()));
  }

 protected:
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  Persistent<WebLocalFrameImpl> main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
  Persistent<FakeColorChooserClient> color_chooser_client_;
  Persistent<FakeDateTimeChooserClient> date_time_chooser_client_;
  Persistent<HTMLSelectElement> select_;
};

TEST_F(PagePopupSuppressionTest, SuppressColorChooser) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenColorChooser());

  Settings* settings = GetSettings();
  settings->SetImmersiveModeEnabled(true);

  EXPECT_FALSE(CanOpenColorChooser());

  settings->SetImmersiveModeEnabled(false);
  EXPECT_TRUE(CanOpenColorChooser());
}

TEST_F(PagePopupSuppressionTest, SuppressDateTimeChooser) {
  // By default, the popup should be shown.
  EXPECT_TRUE(CanOpenDateTimeChooser());

  Settings* settings = GetSettings();
  settings->SetImmersiveModeEnabled(true);

  EXPECT_FALSE(CanOpenDateTimeChooser());

  settings->SetImmersiveModeEnabled(false);
  EXPECT_TRUE(CanOpenDateTimeChooser());
}

// A FileChooserClient which makes FileChooser::OpenFileChooser() success.
class MockFileChooserClient : public GarbageCollected<MockFileChooserClient>,
                              public FileChooserClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockFileChooserClient);

 public:
  explicit MockFileChooserClient(LocalFrame* frame) : frame_(frame) {}
  void Trace(Visitor* visitor) override {
    visitor->Trace(frame_);
    FileChooserClient::Trace(visitor);
  }

 private:
  // FilesChosen() and WillOpenPopup() are never called in the test.
  void FilesChosen(FileChooserFileInfoList, const base::FilePath&) override {}
  void WillOpenPopup() override {}

  LocalFrame* FrameOrNull() const override { return frame_; }

  Member<LocalFrame> frame_;
};

class FileChooserQueueTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize();
    chrome_client_impl_ =
        ToChromeClientImpl(&web_view_->GetPage()->GetChromeClient());
  }

  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(FileChooserQueueTest, DerefQueuedChooser) {
  LocalFrame* frame = helper_.LocalMainFrame()->GetFrame();
  base::RunLoop run_loop_for_chooser1;
  MockFileChooser chooser(frame->GetBrowserInterfaceBroker(),
                          run_loop_for_chooser1.QuitClosure());
  auto* client1 = MakeGarbageCollected<MockFileChooserClient>(frame);
  auto* client2 = MakeGarbageCollected<MockFileChooserClient>(frame);
  mojom::blink::FileChooserParams params;
  params.title = g_empty_string;
  scoped_refptr<FileChooser> chooser1 = client1->NewFileChooser(params);
  scoped_refptr<FileChooser> chooser2 = client2->NewFileChooser(params);

  chrome_client_impl_->OpenFileChooser(frame, chooser1);
  chrome_client_impl_->OpenFileChooser(frame, chooser2);
  EXPECT_EQ(2u, chrome_client_impl_->file_chooser_queue_.size());
  chooser2.reset();

  // Kicks ChromeClientImpl::DidCompleteFileChooser() for chooser1.
  run_loop_for_chooser1.Run();
  chooser.ResponseOnOpenFileChooser(FileChooserFileInfoList());

  EXPECT_EQ(1u, chrome_client_impl_->file_chooser_queue_.size());
  base::RunLoop run_loop_for_chooser2;

  chooser.SetQuitClosure(run_loop_for_chooser2.QuitClosure());
  run_loop_for_chooser2.Run();

  chooser.ResponseOnOpenFileChooser(FileChooserFileInfoList());
}

}  // namespace blink
