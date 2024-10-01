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

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "cc/trees/layer_tree_host.h"
#include "components/autofill/core/common/autofill_features.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom-blink.h"
#include "third_party/blink/public/web/web_autofill_state.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser.h"
#include "third_party/blink/renderer/core/html/forms/color_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser.h"
#include "third_party/blink/renderer/core/html/forms/date_time_chooser_client.h"
#include "third_party/blink/renderer/core/html/forms/file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/mock_file_chooser.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scoped_page_pauser.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CreateWindow

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Not;

namespace blink {

namespace {
class FakeChromeClientForAutofill : public EmptyChromeClient {
 public:
  void JavaScriptChangedValue(HTMLFormControlElement& element,
                              const String& old_value,
                              bool was_autofilled) override {
    last_notification_ = {element.GetIdAttribute().Utf8(), old_value.Utf8()};
  }
  std::vector<std::string> GetAndResetLastEvent() {
    return std::exchange(last_notification_, {});
  }

 private:
  std::vector<std::string> last_notification_;
};
}  // namespace

class ViewCreatingClient : public frame_test_helpers::TestWebFrameClient {
 public:
  WebView* CreateNewWindow(
      const WebURLRequest&,
      const WebWindowFeatures&,
      const WebString& name,
      WebNavigationPolicy,
      network::mojom::blink::WebSandboxFlags,
      const SessionStorageNamespaceId&,
      bool& consumed_user_gesture,
      const std::optional<Impression>&,
      const std::optional<WebPictureInPictureWindowOptions>&,
      const WebURL& creator_base_url) override {
    return web_view_helper_.InitializeWithOpener(Frame());
  }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

class CreateWindowTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize(&web_frame_client_);
    main_frame_ = helper_.LocalMainFrame();
    chrome_client_impl_ =
        To<ChromeClientImpl>(&web_view_->GetPage()->GetChromeClient());
  }

  test::TaskEnvironment task_environment_;
  ViewCreatingClient web_frame_client_;
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  WebLocalFrame* main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
};

TEST_F(CreateWindowTest, CreateWindowFromPausedPage) {
  ScopedPagePauser pauser;
  LocalFrame* frame = To<WebLocalFrameImpl>(main_frame_)->GetFrame();
  FrameLoadRequest request(frame->DomWindow(), ResourceRequest());
  request.SetNavigationPolicy(kNavigationPolicyNewForegroundTab);
  WebWindowFeatures features;
  bool consumed_user_gesture = false;
  EXPECT_EQ(nullptr, chrome_client_impl_->CreateWindow(
                         frame, request, g_empty_atom, features,
                         network::mojom::blink::WebSandboxFlags::kNone, "",
                         consumed_user_gesture));
}

class NewWindowUrlCapturingChromeClient : public EmptyChromeClient {
 public:
  NewWindowUrlCapturingChromeClient() = default;

  const KURL& GetLastUrl() { return last_url_; }

 protected:
  Page* CreateWindowDelegate(LocalFrame*,
                             const FrameLoadRequest& frame_load_request,
                             const AtomicString&,
                             const WebWindowFeatures&,
                             network::mojom::blink::WebSandboxFlags,
                             const SessionStorageNamespaceId&,
                             bool& consumed_user_gesture) override {
    LOG(INFO) << "create window delegate called";
    last_url_ = frame_load_request.GetResourceRequest().Url();
    return nullptr;
  }

 private:
  KURL last_url_;
};

class FormSubmissionTest : public PageTestBase {
 public:
  void SubmitForm(HTMLFormElement& form_elem) {
    form_elem.submitFromJavaScript();
  }

 protected:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<NewWindowUrlCapturingChromeClient>();
    SetupPageWithClients(chrome_client_);
  }

  Persistent<NewWindowUrlCapturingChromeClient> chrome_client_;
};

TEST_F(FormSubmissionTest, FormGetSubmissionNewFrameUrlTest) {
  SetHtmlInnerHTML(
      "<!DOCTYPE HTML>"
      "<form id='form' method='GET' action='https://internal.test/' "
      "target='_blank'>"
      "<input name='foo' value='bar'>"
      "</form>");
  auto* form_elem = To<HTMLFormElement>(GetElementById("form"));
  ASSERT_TRUE(form_elem);

  SubmitForm(*form_elem);
  EXPECT_EQ("foo=bar", chrome_client_->GetLastUrl().Query());
}

class FakeColorChooserClient : public GarbageCollected<FakeColorChooserClient>,
                               public ColorChooserClient {
 public:
  FakeColorChooserClient(Element* owner_element)
      : owner_element_(owner_element) {}
  ~FakeColorChooserClient() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(owner_element_);
    ColorChooserClient::Trace(visitor);
  }

  // ColorChooserClient
  void DidChooseColor(const Color& color) override {}
  void DidEndChooser() override {}
  Element& OwnerElement() const override { return *owner_element_; }
  gfx::Rect ElementRectRelativeToLocalRoot() const override {
    return gfx::Rect();
  }
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

  void Trace(Visitor* visitor) const override {
    visitor->Trace(owner_element_);
    DateTimeChooserClient::Trace(visitor);
  }

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
    ColorChooser* chooser = chrome_client_impl_->OpenColorChooser(
        frame, color_chooser_client_, color);
    if (chooser)
      chooser->EndChooser();
    return !!chooser;
  }

  bool CanOpenDateTimeChooser() {
    LocalFrame* frame = main_frame_->GetFrame();
    DateTimeChooserParameters params;
    params.locale = DefaultLanguage();
    params.type = InputType::Type::kTime;
    DateTimeChooser* chooser = chrome_client_impl_->OpenDateTimeChooser(
        frame, date_time_chooser_client_, params);
    if (chooser)
      chooser->EndChooser();
    return !!chooser;
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
        To<ChromeClientImpl>(&web_view_->GetPage()->GetChromeClient());
    LocalFrame* frame = helper_.LocalMainFrame()->GetFrame();
    color_chooser_client_ = MakeGarbageCollected<FakeColorChooserClient>(
        frame->GetDocument()->documentElement());
    date_time_chooser_client_ = MakeGarbageCollected<FakeDateTimeChooserClient>(
        frame->GetDocument()->documentElement());
    select_ = MakeGarbageCollected<HTMLSelectElement>(*(frame->GetDocument()));
  }

  void TearDown() override {}

 protected:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
  WebViewImpl* web_view_;
  Persistent<WebLocalFrameImpl> main_frame_;
  Persistent<ChromeClientImpl> chrome_client_impl_;
  Persistent<FakeColorChooserClient> color_chooser_client_;
  Persistent<FakeDateTimeChooserClient> date_time_chooser_client_;
  Persistent<HTMLSelectElement> select_;
};

// A FileChooserClient which makes FileChooser::OpenFileChooser() success.
class MockFileChooserClient : public GarbageCollected<MockFileChooserClient>,
                              public FileChooserClient {
 public:
  explicit MockFileChooserClient(LocalFrame* frame) : frame_(frame) {}
  void Trace(Visitor* visitor) const override {
    visitor->Trace(frame_);
    FileChooserClient::Trace(visitor);
  }

 private:
  // FilesChosen() and WillOpenPopup() are never called in the test.
  void FilesChosen(FileChooserFileInfoList, const base::FilePath&) override {}
  void WillOpenPopup() override {}

  LocalFrame* FrameOrNull() const override { return frame_.Get(); }

  Member<LocalFrame> frame_;
};

class FileChooserQueueTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_ = helper_.Initialize();
    chrome_client_impl_ =
        To<ChromeClientImpl>(&web_view_->GetPage()->GetChromeClient());
  }

  test::TaskEnvironment task_environment_;
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

class AutofillChromeClientTest : public PageTestBase {
 public:
  void SetUp() override {
    chrome_client_ = MakeGarbageCollected<FakeChromeClientForAutofill>();
    SetupPageWithClients(chrome_client_);
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void ExecuteScript(const char* script) {
    ClassicScript::CreateUnspecifiedScript(script)->RunScript(
        GetFrame().DomWindow());
  }

  Persistent<FakeChromeClientForAutofill> chrome_client_;
};

// Validates the JavaScriptChangedValue notification if JavaScript
// overrides the autofilled content of form controls *after* the fill has been
// concluded.
TEST_F(AutofillChromeClientTest, NotificationsOfJavaScriptChangesAfterFill) {
  SetHtmlInnerHTML(R"HTML(
    <!DOCTYPE HTML>
    <form id='form' method='GET' action='https://internal.test/'
        target='_blank'>
      <input id='text'>
      <textarea id='textarea'></textarea>
      <select id='select'>
        <option value='initial' selected>a</option>
        <option value='autofilled_select'>b</option>
        <option value='overridden'>c</option>
      </select>
      <input id='not_autofilled_text'>
    </form>
  )HTML");

  auto* text_element = To<HTMLInputElement>(GetElementById("text"));
  auto* textarea_element = To<HTMLTextAreaElement>(GetElementById("textarea"));
  auto* select_element = To<HTMLSelectElement>(GetElementById("select"));
  auto* not_autofilled_text =
      To<HTMLInputElement>(GetElementById("not_autofilled_text"));

  text_element->SetAutofillValue("autofilled_text");
  textarea_element->SetAutofillValue("autofilled_textarea");
  select_element->SetAutofillValue("autofilled_select",
                                   WebAutofillState::kAutofilled);

  EXPECT_THAT(text_element->Value(), Eq("autofilled_text"));
  EXPECT_THAT(text_element->GetAutofillState(),
              Eq(WebAutofillState::kAutofilled));
  ExecuteScript("document.getElementById('text').value = 'new_text';");
  EXPECT_THAT(text_element->Value(), Eq("new_text"));
  EXPECT_THAT(text_element->GetAutofillState(),
              Eq(WebAutofillState::kNotFilled));
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("text", "autofilled_text"));

  EXPECT_THAT(textarea_element->Value(), Eq("autofilled_textarea"));
  EXPECT_THAT(textarea_element->GetAutofillState(),
              Eq(WebAutofillState::kAutofilled));
  ExecuteScript("document.getElementById('textarea').value = 'new_text';");
  EXPECT_THAT(textarea_element->Value(), Eq("new_text"));
  EXPECT_THAT(textarea_element->GetAutofillState(),
              Eq(WebAutofillState::kNotFilled));
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("textarea", "autofilled_textarea"));

  EXPECT_THAT(select_element->Value(), Eq("autofilled_select"));
  EXPECT_THAT(select_element->GetAutofillState(),
              Eq(WebAutofillState::kAutofilled));
  ExecuteScript("document.getElementById('select').value = 'overridden';");
  EXPECT_THAT(select_element->Value(), Eq("overridden"));
  EXPECT_THAT(select_element->GetAutofillState(),
              Eq(WebAutofillState::kNotFilled));
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("select", "autofilled_select"));

  // Even for elements that are not in state "autofilled", the chrome client is
  // informed about the change.
  EXPECT_THAT(not_autofilled_text->Value().IsNull(), ::testing::IsTrue());
  ExecuteScript(
      "document.getElementById('not_autofilled_text').value = 'new_text';");
  EXPECT_THAT(not_autofilled_text->Value(), Eq("new_text"));
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("not_autofilled_text", ""));
}

// Validates the JavaScriptChangedValue notification if JavaScript
// overrides the autofilled content of form controls during the fill operation.
// This is the case because a JavaScript event handler on change signals is
// is triggered during the autofill operation.
TEST_F(AutofillChromeClientTest, NotificationsOfJavaScriptChangesDuringFill) {
  SetHtmlInnerHTML(R"HTML(
    <!DOCTYPE HTML>
    <form id='form' method='GET' action='https://internal.test/'
        target='_blank'>
      <input id='text'>
      <textarea id='textarea'></textarea>
      <select id='select'>
        <option value='initial' selected>a</option>
        <option value='autofilled_select'>b</option>
        <option value='overridden'>c</option>
      </select>
    </form>
  )HTML");

  ExecuteScript(R"JS(
    for (const id of ['text', 'textarea', 'select']) {
      document.getElementById(id).addEventListener('change', () => {
        document.getElementById(id).value = 'overridden';
      });
    }
  )JS");

  auto* text_element = To<HTMLInputElement>(GetElementById("text"));
  auto* textarea_element = To<HTMLTextAreaElement>(GetElementById("textarea"));
  auto* select_element = To<HTMLSelectElement>(GetElementById("select"));
  text_element->SetAutofillValue("autofilled_text");
  EXPECT_THAT(text_element->Value(), Eq("overridden"));
  // Note that we expect WebAutofillState::kAutofilled. This is a product
  // decision: Even if the website messes with the content of the field after
  // an autofill, we show it as autofilled. This applies only if the change
  // via JavaScript happens instantaneously during the fill operation, not if
  // JavaScript edits the value later. A common usecase is that we fill a
  // credit card as a sequence of digits and the website inserts spaces to
  // group the digits into blocks of four.
  // When the feature AllowJavaScriptToResetAutofillState is enabled, we expect
  // the opposite since this feature experiments with changing the product
  // decision.
  EXPECT_EQ(
      text_element->GetAutofillState() == WebAutofillState::kAutofilled,
      !RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled());
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("text", "autofilled_text"));

  textarea_element->SetAutofillValue("autofilled_textarea");
  EXPECT_THAT(textarea_element->Value(), Eq("overridden"));
  EXPECT_EQ(
      textarea_element->GetAutofillState() == WebAutofillState::kAutofilled,
      !RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled());
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("textarea", "autofilled_textarea"));

  select_element->SetAutofillValue("autofilled_select",
                                   WebAutofillState::kAutofilled);
  EXPECT_THAT(select_element->Value(), Eq("overridden"));
  EXPECT_EQ(
      select_element->GetAutofillState() == WebAutofillState::kAutofilled,
      !RuntimeEnabledFeatures::AllowJavaScriptToResetAutofillStateEnabled());
  EXPECT_THAT(chrome_client_->GetAndResetLastEvent(),
              ::testing::ElementsAre("select", "autofilled_select"));
}

}  // namespace blink
