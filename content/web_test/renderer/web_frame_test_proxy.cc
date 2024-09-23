// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "base/memory/raw_ptr.h"

#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/renderer/blink_test_helpers.h"
#include "content/web_test/renderer/event_sender.h"
#include "content/web_test/renderer/gc_controller.h"
#include "content/web_test/renderer/layout_dump.h"
#include "content/web_test/renderer/spell_check_client.h"
#include "content/web_test/renderer/test_plugin.h"
#include "content/web_test/renderer/test_runner.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

namespace {

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
std::string DescriptionSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos || !pos)
    return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == std::string::npos)
    return "ERROR:" + url;

  return url.substr(pos + 1);
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
// TODO(danakj): Can we just use DescriptionSuitableForTestResult() and delete
// this version?
std::string URLSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos) {
#ifdef WIN32
    pos = url.rfind('\\');
    if (pos == std::string::npos)
      pos = 0;
#else
    pos = 0;
#endif
  }
  std::string filename = url.substr(pos + 1);
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

bool IsLocalHost(const std::string& host) {
  return host == "127.0.0.1" || host == "localhost" || host == "[::1]";
}

bool IsTestHost(const std::string& host) {
  return base::EndsWith(host, ".test", base::CompareCase::INSENSITIVE_ASCII) ||
         base::EndsWith(host, ".test.", base::CompareCase::INSENSITIVE_ASCII);
}

bool HostIsUsedBySomeTestsToGenerateError(const std::string& host) {
  return host == "255.255.255.255";
}

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char kLinkClickedString[] = "link clicked";
const char kFormSubmittedString[] = "form submitted";
const char kBackForwardString[] = "back/forward";
const char kReloadString[] = "reload";
const char kFormResubmittedString[] = "form resubmitted";
const char kOtherString[] = "other";

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(blink::WebNavigationType type) {
  switch (type) {
    case blink::kWebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case blink::kWebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case blink::kWebNavigationTypeBackForward:
    case blink::kWebNavigationTypeRestore:
      return kBackForwardString;
    case blink::kWebNavigationTypeReload:
      return kReloadString;
    case blink::kWebNavigationTypeFormResubmittedBackForward:
    case blink::kWebNavigationTypeFormResubmittedReload:
      return kFormResubmittedString;
    case blink::kWebNavigationTypeOther:
      return kOtherString;
  }
  return web_test_string_util::kIllegalString;
}

void PrintFrameUserGestureStatus(WebFrameTestProxy* frame_proxy,
                                 blink::WebLocalFrame* frame,
                                 const char* msg) {
  bool is_user_gesture = frame->HasTransientUserActivation();
  frame_proxy->GetWebTestControlHostRemote()->PrintMessage(
      std::string("Frame with user gesture \"") +
      (is_user_gesture ? "true" : "false") + "\"" + msg);
}

class TestRenderFrameObserver : public RenderFrameObserver {
 public:
  TestRenderFrameObserver(RenderFrame* frame, TestRunner* test_runner)
      : RenderFrameObserver(frame), test_runner_(test_runner) {}

  TestRenderFrameObserver(const TestRenderFrameObserver&) = delete;
  TestRenderFrameObserver& operator=(const TestRenderFrameObserver&) = delete;

  ~TestRenderFrameObserver() override {}

 private:
  WebFrameTestProxy* frame_proxy() {
    return static_cast<WebFrameTestProxy*>(render_frame());
  }

  // RenderFrameObserver overrides.
  void OnDestruct() override { delete this; }

  void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - DidStartNavigation\n");
    }

    if (test_runner_->ShouldDumpUserGestureInFrameLoadCallbacks()) {
      PrintFrameUserGestureStatus(frame_proxy(), render_frame()->GetWebFrame(),
                                  " - in DidStartNavigation\n");
    }
  }

  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - ReadyToCommitNavigation\n");
    }
  }

  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didCommitLoadForFrame\n");
    }

    // Track main frames once they are swapped in, if they started provisional.
    if (render_frame()->IsMainFrame())
      test_runner_->AddMainFrame(*frame_proxy());
  }

  void DidFinishSameDocumentNavigation() override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didCommitLoadForFrame\n");
    }
  }

  void DidFailProvisionalLoad() override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didFailProvisionalLoadWithError\n");
    }
  }

  void DidDispatchDOMContentLoadedEvent() override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didFinishDocumentLoadForFrame\n");
    }
  }

  void DidFinishLoad() override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didFinishLoadForFrame\n");
    }
  }

  void DidHandleOnloadEvents() override {
    if (test_runner_->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      frame_proxy()->GetWebTestControlHostRemote()->PrintMessage(
          description + " - didHandleOnloadEventsForFrame\n");
    }
  }

  void ScriptedPrint(bool user_initiated) override {
    // This is using the main frame for the size, but maybe it should be using
    // the frame's size.
    gfx::SizeF page_size_in_pixels(
        frame_proxy()->GetLocalRootWebFrameWidget()->Size());
    if (page_size_in_pixels.IsEmpty())
      return;
    blink::WebPrintParams print_params(page_size_in_pixels);
    render_frame()->GetWebFrame()->PrintBegin(print_params, blink::WebNode());
    render_frame()->GetWebFrame()->PrintEnd();
  }

  const raw_ptr<TestRunner> test_runner_;
};

}  // namespace

WebFrameTestProxy::WebFrameTestProxy(RenderFrameImpl::CreateParams params,
                                     TestRunner* test_runner)
    : RenderFrameImpl(std::move(params)), test_runner_(test_runner) {}

WebFrameTestProxy::~WebFrameTestProxy() {
  if (IsMainFrame())
    test_runner_->RemoveMainFrame(*this);
}

void WebFrameTestProxy::Initialize(blink::WebFrame* parent) {
  RenderFrameImpl::Initialize(parent);

  // Track main frames if they started in the frame tree. Otherwise they are
  // provisional and will be tracked once swapped in.
  if (IsMainFrame() && in_frame_tree())
    test_runner_->AddMainFrame(*this);

  GetWebFrame()->SetContentSettingsClient(
      new WebTestContentSettingsClient(this, test_runner_));

  spell_check_ = std::make_unique<SpellCheckClient>(GetWebFrame());
  GetWebFrame()->SetTextCheckClient(spell_check_.get());

  GetAssociatedInterfaceRegistry()->AddInterface<mojom::WebTestRenderFrame>(
      base::BindRepeating(&WebFrameTestProxy::BindReceiver,
                          // The registry goes away and stops using this
                          // callback when RenderFrameImpl (which is this class)
                          // is destroyed.
                          base::Unretained(this)));

  new TestRenderFrameObserver(this, test_runner_);  // deletes itself.

  // Bind the channel to the host right away.
  GetWebTestControlHostRemote();
}

void WebFrameTestProxy::ResetRendererAfterWebTest() {
  // TODO(crbug.com/40615943): Some of this work is no longer needed if the
  // RenderDocument project causes us to replace the main frame on each
  // navigation. But some of it will continue to be necessary since it modifies
  // process-global state, e.g. ResetMockOverlayScrollbars in internals.cc.
  // The content::TestRunner object also persists for the life of the renderer.
  // So the steps in this method need to be audited piecemeal for redundancy.
  CHECK(IsMainFrame());

  if (IsMainFrame()) {
    GetWebFrame()->ClearActiveFindMatchForTesting();
    GetWebFrame()->SetName(blink::WebString());
    GetWebFrame()->ClearOpener();

    blink::WebTestingSupport::ResetMainFrame(GetWebFrame());
    // Resetting the internals object also overrides the WebPreferences, so we
    // have to sync them to WebKit again.
    blink::WebView* web_view = GetWebFrame()->View();
    web_view->SetWebPreferences(web_view->GetWebPreferences());

    // Resets things on the WebView that TestRunnerBindings can modify.
    test_runner()->ResetWebView(web_view);
  }
  if (IsLocalRoot()) {
    test_runner()->ResetWebFrameWidget(GetLocalRootWebFrameWidget());
    GetLocalRootFrameWidgetTestHelper()->Reset();
  }

  accessibility_controller_.Reset();
  spell_check_->Reset();
  test_runner_->Reset();
}

std::string WebFrameTestProxy::GetFrameNameForWebTests() {
  return blink::UniqueNameHelper::ExtractStableNameForTesting(unique_name());
}

std::string WebFrameTestProxy::GetFrameDescriptionForWebTests() {
  std::string name = GetFrameNameForWebTests();
  if (IsMainFrame()) {
    DCHECK(name.empty());
    return "main frame";
  }
  if (name.empty()) {
    return "frame (anonymous)";
  }
  return std::string("frame \"") + name + "\"";
}

blink::WebPlugin* WebFrameTestProxy::CreatePlugin(
    const blink::WebPluginParams& params) {
  if (TestPlugin::IsSupportedMimeType(params.mime_type))
    return TestPlugin::Create(params, test_runner(), GetWebFrame());

  if (params.mime_type == "application/x-plugin-placeholder-test") {
    auto* placeholder = plugins::PluginPlaceholder::Create(
        this, params, "<div>Test content</div>");
    return placeholder->plugin();
  }

  return RenderFrameImpl::CreatePlugin(params);
}

void WebFrameTestProxy::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  RenderFrameImpl::DidAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);

  if (!test_runner()->ShouldDumpConsoleMessages())
    return;

  std::string level;
  switch (message.level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      level = "DEBUG";
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      level = "MESSAGE";
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      level = "WARNING";
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      level = "ERROR";
      break;
    default:
      level = "MESSAGE";
  }
  std::string console_message(std::string("CONSOLE ") + level + ": ");
  if (!message.text.IsEmpty()) {
    std::string new_message;
    new_message = message.text.Utf8();
    size_t file_protocol = new_message.find("file://");
    if (file_protocol != std::string::npos) {
      new_message = new_message.substr(0, file_protocol) +
                    URLSuitableForTestResult(new_message.substr(file_protocol));
    }
    console_message += new_message;
  }
  console_message += "\n";

  // Console messages shouldn't be included in the expected output for
  // web-platform-tests because they may create non-determinism not
  // intended by the test author. They are still included in the stderr
  // output for debugging purposes.
  GetWebTestControlHostRemote()->PrintMessageToStderr(console_message);
  if (!test_runner()->IsWebPlatformTestsMode())
    GetWebTestControlHostRemote()->PrintMessage(console_message);
}

void WebFrameTestProxy::DidStartLoading() {
  test_runner()->AddLoadingFrame(GetWebFrame());

  RenderFrameImpl::DidStartLoading();
}

void WebFrameTestProxy::DidStopLoading() {
  RenderFrameImpl::DidStopLoading();

  test_runner()->RemoveLoadingFrame(GetWebFrame());
}

void WebFrameTestProxy::DidChangeSelection(bool is_selection_empty,
                                           blink::SyncCondition force_sync) {
  if (test_runner()->ShouldDumpEditingCallbacks()) {
    GetWebTestControlHostRemote()->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
  }
  RenderFrameImpl::DidChangeSelection(is_selection_empty, force_sync);
}

void WebFrameTestProxy::DidChangeContents() {
  if (test_runner()->ShouldDumpEditingCallbacks()) {
    GetWebTestControlHostRemote()->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
  }
  RenderFrameImpl::DidChangeContents();
}

blink::WebEffectiveConnectionType
WebFrameTestProxy::GetEffectiveConnectionType() {
  blink::WebEffectiveConnectionType connection_type =
      test_runner()->effective_connection_type();
  if (connection_type != blink::WebEffectiveConnectionType::kTypeUnknown)
    return connection_type;
  return RenderFrameImpl::GetEffectiveConnectionType();
}

void WebFrameTestProxy::UpdateContextMenuDataForTesting(
    const blink::ContextMenuData& context_menu_data,
    const std::optional<gfx::Point>& location) {
  blink::FrameWidgetTestHelper* frame_widget =
      GetLocalRootFrameWidgetTestHelper();
  frame_widget->GetEventSender()->SetContextMenuData(context_menu_data);

  RenderFrameImpl::UpdateContextMenuDataForTesting(context_menu_data, location);
}

void WebFrameTestProxy::DidDispatchPingLoader(const blink::WebURL& url) {
  if (test_runner()->ShouldDumpPingLoaderCallbacks()) {
    GetWebTestControlHostRemote()->PrintMessage(
        std::string("PingLoader dispatched to '") +
        web_test_string_util::URLDescription(url).c_str() + "'.\n");
  }

  RenderFrameImpl::DidDispatchPingLoader(url);
}

std::optional<blink::WebURL> WebFrameTestProxy::WillSendRequest(
    const blink::WebURL& target,
    const blink::WebSecurityOrigin& security_origin,
    const net::SiteForCookies& site_for_cookies,
    ForRedirect for_redirect,
    const blink::WebURL& upstream_url) {
  std::optional<blink::WebURL> adjusted_url = RenderFrameImpl::WillSendRequest(
      target, security_origin, site_for_cookies, for_redirect, upstream_url);
  // Need to use GURL for host() and SchemeIs()
  GURL url = adjusted_url.has_value() ? *adjusted_url : target;

  std::string host = url.host();
  if (!host.empty() &&
      (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    if (!IsLocalHost(host) && !IsTestHost(host) &&
        !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((site_for_cookies.scheme() != url::kHttpScheme &&
          site_for_cookies.scheme() != url::kHttpsScheme) ||
         IsLocalHost(site_for_cookies.registrable_domain())) &&
        !test_runner_->TestConfig().allow_external_pages) {
      GetWebTestControlHostRemote()->PrintMessage(
          std::string("Blocked access to external URL ") +
          url.possibly_invalid_spec() + "\n");
      return GURL("255.255.255.255");
    }
  }

  // Set the new substituted URL.
  return RewriteWebTestsURL(url.spec(),
                            test_runner()->IsWebPlatformTestsMode());
}

void WebFrameTestProxy::FinalizeRequest(blink::WebURLRequest& request) {
  RenderFrameImpl::FinalizeRequest(request);

  // Warning: this may be null in some cross-site cases.
  net::SiteForCookies site_for_cookies = request.SiteForCookies();

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      request.ClearHttpHeaderField(blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    request.SetReferrerString(blink::WebString());
    request.SetReferrerPolicy(blink::ReferrerUtils::NetToMojoReferrerPolicy(
        blink::ReferrerUtils::GetDefaultNetReferrerPolicy()));
  }
}

void WebFrameTestProxy::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  // This check for whether the test is running ensures we do not intercept
  // the about:blank navigation between tests.
  if (!test_runner()->TestIsRunning()) {
    RenderFrameImpl::BeginNavigation(std::move(info));
    return;
  }

  if (test_runner()->ShouldDumpNavigationPolicy()) {
    GetWebTestControlHostRemote()->PrintMessage(
        "Default policy for navigation to '" +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        "' is '" +
        web_test_string_util::WebNavigationPolicyToString(
            info->navigation_policy) +
        "'\n");
  }

  if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
    GURL url = info->url_request.Url();
    std::string description = GetFrameDescriptionForWebTests();
    GetWebTestControlHostRemote()->PrintMessage(
        description + " - BeginNavigation request to '");
    GetWebTestControlHostRemote()->PrintMessage(
        DescriptionSuitableForTestResult(url.possibly_invalid_spec()));
    GetWebTestControlHostRemote()->PrintMessage("', http method ");
    GetWebTestControlHostRemote()->PrintMessage(
        info->url_request.HttpMethod().Utf8().data());
    GetWebTestControlHostRemote()->PrintMessage("\n");
  }

  bool should_continue = true;
  if (test_runner()->PolicyDelegateEnabled()) {
    GetWebTestControlHostRemote()->PrintMessage(
        std::string("Policy delegate: attempt to load ") +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        " with navigation type '" +
        WebNavigationTypeToString(info->navigation_type) + "'\n");
    should_continue = test_runner()->PolicyDelegateIsPermissive();
    if (test_runner()->PolicyDelegateShouldNotifyDone()) {
      test_runner()->PolicyDelegateDone(*this);
      should_continue = false;
    }
  }

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      info->url_request.ClearHttpHeaderField(
          blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    info->url_request.SetReferrerString(blink::WebString());
    info->url_request.SetReferrerPolicy(
        network::mojom::ReferrerPolicy::kDefault);
  }

  info->url_request.SetUrl(
      RewriteWebTestsURL(info->url_request.Url().GetString().Utf8(),
                         test_runner()->IsWebPlatformTestsMode()));

  if (!should_continue)
    return;

  RenderFrameImpl::BeginNavigation(std::move(info));
}

void WebFrameTestProxy::PostAccessibilityEvent(const ui::AXEvent& event) {
  HandleWebAccessibilityEventForTest(event);
  RenderFrameImpl::PostAccessibilityEvent(event);
}

void WebFrameTestProxy::HandleAXObjectDetachedForTest(unsigned axid) {
  accessibility_controller_.Remove(axid);
}

void WebFrameTestProxy::HandleWebAccessibilityEventForTest(
    const blink::WebAXObject& object,
    const char* event_name,
    const std::vector<ui::AXEventIntent>& event_intents) {
  // Only hook the accessibility events that occurred during the test run.
  // This check prevents false positives in BlinkLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_runner()->TestIsRunning()) {
    return;
  }

  accessibility_controller_.NotificationReceived(GetWebFrame(), object,
                                                 event_name, event_intents);

  if (accessibility_controller_.ShouldLogAccessibilityEvents()) {
    std::string message("AccessibilityNotification - ");
    message += event_name;

    blink::WebNode node = object.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      blink::WebElement element = node.To<blink::WebElement>();
      if (element.HasAttribute("id")) {
        message += " - id:";
        message += element.GetAttribute("id").Utf8().data();
      }
    }

    GetWebTestControlHostRemote()->PrintMessage(message + "\n");
  }
}

void WebFrameTestProxy::HandleWebAccessibilityEventForTest(
    const ui::AXEvent& event) {
  const char* event_name = nullptr;
  switch (event.event_type) {
    case ax::mojom::Event::kActiveDescendantChanged:
      event_name = "ActiveDescendantChanged";
      break;
    case ax::mojom::Event::kAriaAttributeChangedDeprecated:
      NOTREACHED();
    case ax::mojom::Event::kBlur:
      event_name = "Blur";
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      event_name = "CheckedStateChanged";
      break;
    case ax::mojom::Event::kClicked:
      event_name = "Clicked";
      break;
    case ax::mojom::Event::kDocumentSelectionChanged:
      event_name = "DocumentSelectionChanged";
      break;
    case ax::mojom::Event::kDocumentTitleChanged:
      event_name = "DocumentTitleChanged";
      break;
    case ax::mojom::Event::kExpandedChanged:
      event_name = "ExpandedChanged";
      break;
    case ax::mojom::Event::kFocus:
      event_name = "Focus";
      break;
    case ax::mojom::Event::kHide:
      event_name = "Hide";
      break;
    case ax::mojom::Event::kHover:
      event_name = "Hover";
      break;
    case ax::mojom::Event::kLayoutComplete:
      event_name = "LayoutComplete";
      break;
    case ax::mojom::Event::kLoadComplete:
      event_name = "LoadComplete";
      break;
    case ax::mojom::Event::kLoadStart:
      event_name = "LoadStart";
      break;
    case ax::mojom::Event::kLocationChanged:
      event_name = "LocationChanged";
      break;
    case ax::mojom::Event::kRowCollapsed:
      event_name = "RowCollapsed";
      break;
    case ax::mojom::Event::kRowCountChanged:
      event_name = "RowCountChanged";
      break;
    case ax::mojom::Event::kRowExpanded:
      event_name = "RowExpanded";
      break;
    case ax::mojom::Event::kScrollPositionChanged:
      event_name = "ScrollPositionChanged";
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      event_name = "ScrolledToAnchor";
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      event_name = "SelectedChildrenChanged";
      break;
    case ax::mojom::Event::kShow:
      event_name = "Show";
      break;
    case ax::mojom::Event::kTextChanged:
      event_name = "TextChanged";
      break;
    case ax::mojom::Event::kValueChanged:
      event_name = "ValueChanged";
      break;

    // These events are not fired from Blink.
    // This list is duplicated in
    // RenderAccessibilityImpl::IsImmediateProcessingRequiredForEvent().
    case ax::mojom::Event::kAlert:
    case ax::mojom::Event::kAutocorrectionOccured:
    case ax::mojom::Event::kChildrenChanged:
    case ax::mojom::Event::kControlsChanged:
    case ax::mojom::Event::kEndOfTest:
    case ax::mojom::Event::kFocusAfterMenuClose:
    case ax::mojom::Event::kFocusContext:
    case ax::mojom::Event::kHitTestResult:
    case ax::mojom::Event::kImageFrameUpdated:
    case ax::mojom::Event::kLiveRegionCreated:
    case ax::mojom::Event::kLiveRegionChanged:
    case ax::mojom::Event::kMediaStartedPlaying:
    case ax::mojom::Event::kMediaStoppedPlaying:
    case ax::mojom::Event::kMenuEnd:
    case ax::mojom::Event::kMenuListValueChangedDeprecated:
    case ax::mojom::Event::kMenuPopupEnd:
    case ax::mojom::Event::kMenuPopupStart:
    case ax::mojom::Event::kMenuStart:
    case ax::mojom::Event::kMouseCanceled:
    case ax::mojom::Event::kMouseDragged:
    case ax::mojom::Event::kMouseMoved:
    case ax::mojom::Event::kMousePressed:
    case ax::mojom::Event::kMouseReleased:
    case ax::mojom::Event::kNone:
    case ax::mojom::Event::kSelection:
    case ax::mojom::Event::kSelectionAdd:
    case ax::mojom::Event::kSelectionRemove:
    case ax::mojom::Event::kStateChanged:
    case ax::mojom::Event::kTextSelectionChanged:
    case ax::mojom::Event::kTooltipClosed:
    case ax::mojom::Event::kTooltipOpened:
    case ax::mojom::Event::kTreeChanged:
    case ax::mojom::Event::kWindowActivated:
    case ax::mojom::Event::kWindowDeactivated:
    case ax::mojom::Event::kWindowVisibilityChanged:
      // Never fired from Blink.
      NOTREACHED_IN_MIGRATION()
          << "Event not expected from Blink: " << event.event_type;
  }

  blink::WebDocument document = GetWebFrame()->GetDocument();
  auto object = blink::WebAXObject::FromWebDocumentByID(document, event.id);
  HandleWebAccessibilityEventForTest(std::move(object), event_name,
                                     event.event_intents);
}

void WebFrameTestProxy::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::string device_id = sink_id.Utf8();
  if (device_id == "valid" || device_id.empty())
    std::move(completion_callback).Run(/*error =*/std::nullopt);
  else if (device_id == "unauthorized")
    std::move(completion_callback)
        .Run(blink::WebSetSinkIdError::kNotAuthorized);
  else
    std::move(completion_callback).Run(blink::WebSetSinkIdError::kNotFound);

  // Intentionally does not call RenderFrameImpl.
}

void WebFrameTestProxy::DidClearWindowObject() {
  // Avoid installing bindings on the about:blank in between tests. This is
  // especially problematic for web platform tests that would inject javascript
  // into the page when installing bindings.
  if (test_runner()->TestIsRunning()) {
    blink::WebLocalFrame* frame = GetWebFrame();
    // These calls will install the various JS bindings for web tests into the
    // frame before JS has a chance to run.
    GCController::Install(frame);
    test_runner()->Install(this, spell_check_.get());
    accessibility_controller_.Install(frame);
    text_input_controller_.Install(frame);
    GetLocalRootFrameWidgetTestHelper()->GetEventSender()->Install(this);
    blink::WebTestingSupport::InjectInternalsObject(frame);
  }
  RenderFrameImpl::DidClearWindowObject();
}

void WebFrameTestProxy::DidCommitNavigation(
    blink::WebHistoryCommitType commit_type,
    bool should_reset_browser_interface_broker,
    const blink::ParsedPermissionsPolicy& permissions_policy_header,
    const blink::DocumentPolicyFeatureState& document_policy_header) {
  if (should_block_parsing_in_next_commit_) {
    should_block_parsing_in_next_commit_ = false;
    GetWebFrame()->BlockParserForTesting();
  }
  RenderFrameImpl::DidCommitNavigation(
      commit_type, should_reset_browser_interface_broker,
      permissions_policy_header, document_policy_header);
}

void WebFrameTestProxy::OnDeactivated() {
  test_runner()->OnFrameDeactivated(*this);
}

void WebFrameTestProxy::OnReactivated() {
  test_runner()->OnFrameReactivated(*this);
}

void WebFrameTestProxy::BlockTestUntilStart() {
  should_block_parsing_in_next_commit_ = true;
}

void WebFrameTestProxy::StartTest() {
  CHECK(!should_block_parsing_in_next_commit_);
  GetWebFrame()->FlushInputForTesting(base::BindOnce(
      [](base::WeakPtr<RenderFrameImpl> render_frame,
         const TestRunner* test_runner) {
        if (!render_frame) {
          return;
        }

        auto* web_frame = render_frame->GetWebFrame();
        if (!web_frame) {
          return;
        }

        web_frame->ResumeParserForTesting();

        if (test_runner->IsPrinting()) {
          web_frame->WillPrintSoon();
        }
      },
      GetWeakPtr(), this->test_runner_));
}

blink::FrameWidgetTestHelper*
WebFrameTestProxy::GetLocalRootFrameWidgetTestHelper() {
  return GetLocalRootWebFrameWidget()->GetFrameWidgetTestHelperForTesting();
}

void WebFrameTestProxy::SynchronouslyCompositeAfterTest(
    SynchronouslyCompositeAfterTestCallback callback) {
  // When the TestFinished() occurred, if the browser is capturing pixels, it
  // asks each composited RenderFrame to submit a new frame via here.
  if (IsLocalRoot()) {
    GetLocalRootFrameWidgetTestHelper()->SynchronouslyCompositeAfterTest(
        std::move(callback));
  }
}

void WebFrameTestProxy::DumpFrameLayout(DumpFrameLayoutCallback callback) {
  std::string dump = DumpLayoutAsString(
      GetWebFrame(), test_runner()->ShouldGenerateTextResults());
  std::move(callback).Run(std::move(dump));
}

void WebFrameTestProxy::SetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr config,
    bool starting_test) {
  blink::WebLocalFrame* frame = GetWebFrame();
  test_runner_->SetMainWindowAndTestConfiguration(frame, std::move(config));
  if (starting_test) {
    // This should only be called on the main frame.
    DCHECK(!frame->Parent());
    // If focus was in a child frame, it gets lost when we navigate to the next
    // test, but we want to start with focus in the main frame for every test.
    // Focus is controlled by the renderer, so we must do the reset here.
    frame->View()->SetFocusedFrame(frame);
  }
}

void WebFrameTestProxy::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::WebTestRenderFrame> receiver) {
  web_test_render_frame_receiver_.reset();
  web_test_render_frame_receiver_.Bind(
      std::move(receiver),
      GetWebFrame()->GetTaskRunner(blink::TaskType::kInternalTest));
}

mojom::WebTestControlHost* WebFrameTestProxy::GetWebTestControlHostRemote() {
  if (!web_test_control_host_remote_) {
    GetRemoteAssociatedInterfaces()->GetInterface(
        &web_test_control_host_remote_);
    web_test_control_host_remote_.reset_on_disconnect();
  }
  return web_test_control_host_remote_.get();
}

TestRunner* WebFrameTestProxy::test_runner() {
  return test_runner_;
}

void WebFrameTestProxy::SetupRendererProcessForNonTestWindow() {
  // Allows the window to receive replicated WebTestRuntimeFlags and to
  // control or end the test.
  test_runner_->SetTestIsRunning(true);
}

void WebFrameTestProxy::TestFinishedFromSecondaryRenderer() {
  test_runner_->TestFinishedFromSecondaryRenderer(*this);
}

void WebFrameTestProxy::ProcessWorkItem(mojom::WorkItemPtr work_item) {
  test_runner_->ProcessWorkItem(std::move(work_item), *this);
}

void WebFrameTestProxy::ReplicateWorkQueueStates(
    base::Value::Dict work_queue_states) {
  test_runner_->ReplicateWorkQueueStates(std::move(work_queue_states), *this);
}

void WebFrameTestProxy::ReplicateWebTestRuntimeFlagsChanges(
    base::Value::Dict changed_layout_test_runtime_flags) {
  test_runner_->ReplicateWebTestRuntimeFlagsChanges(
      std::move(changed_layout_test_runtime_flags));
}

}  // namespace content
