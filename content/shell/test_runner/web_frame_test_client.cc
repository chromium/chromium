// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_frame_test_client.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/test_runner_support.h"
#include "content/shell/test_runner/accessibility_controller.h"
#include "content/shell/test_runner/event_sender.h"
#include "content/shell/test_runner/mock_screen_orientation_client.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/test_interfaces.h"
#include "content/shell/test_runner/test_plugin.h"
#include "content/shell/test_runner/test_runner.h"
#include "content/shell/test_runner/web_frame_test_proxy.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_view_test_proxy.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_user_gesture_indicator.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace test_runner {

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

void BlockRequest(blink::WebURLRequest& request) {
  request.SetUrl(GURL("255.255.255.255"));
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

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
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

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(blink::WebNavigationType type) {
  switch (type) {
    case blink::kWebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case blink::kWebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case blink::kWebNavigationTypeBackForward:
      return kBackForwardString;
    case blink::kWebNavigationTypeReload:
      return kReloadString;
    case blink::kWebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case blink::kWebNavigationTypeOther:
      return kOtherString;
  }
  return kIllegalString;
}

}  // namespace

WebFrameTestClient::WebFrameTestClient(WebTestDelegate* delegate,
                                       WebViewTestProxy* web_view_test_proxy,
                                       WebFrameTestProxy* web_frame_test_proxy)
    : delegate_(delegate),
      web_view_test_proxy_(web_view_test_proxy),
      web_frame_test_proxy_(web_frame_test_proxy) {
  DCHECK(delegate_);
  DCHECK(web_frame_test_proxy_);
  DCHECK(web_view_test_proxy_);
}

WebFrameTestClient::~WebFrameTestClient() {}

// static
void WebFrameTestClient::PrintFrameDescription(WebTestDelegate* delegate,
                                               blink::WebLocalFrame* frame) {
  std::string name = content::GetFrameNameForWebTests(frame);
  if (frame == frame->View()->MainFrame()) {
    DCHECK(name.empty());
    delegate->PrintMessage("main frame");
    return;
  }
  if (name.empty()) {
    delegate->PrintMessage("frame (anonymous)");
    return;
  }
  delegate->PrintMessage(std::string("frame \"") + name + "\"");
}

void WebFrameTestClient::RunModalAlertDialog(const blink::WebString& message) {
  if (!test_runner()->ShouldDumpJavaScriptDialogs())
    return;
  delegate_->PrintMessage(std::string("ALERT: ") + message.Utf8().data() +
                          "\n");
}

bool WebFrameTestClient::RunModalConfirmDialog(
    const blink::WebString& message) {
  if (!test_runner()->ShouldDumpJavaScriptDialogs())
    return true;
  delegate_->PrintMessage(std::string("CONFIRM: ") + message.Utf8().data() +
                          "\n");
  return true;
}

bool WebFrameTestClient::RunModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  if (!test_runner()->ShouldDumpJavaScriptDialogs())
    return true;
  delegate_->PrintMessage(std::string("PROMPT: ") + message.Utf8().data() +
                          ", default text: " + default_value.Utf8().data() +
                          "\n");
  return true;
}

bool WebFrameTestClient::RunModalBeforeUnloadDialog(bool is_reload) {
  if (test_runner()->ShouldDumpJavaScriptDialogs())
    delegate_->PrintMessage(std::string("CONFIRM NAVIGATION\n"));
  return !test_runner()->ShouldStayOnPageAfterHandlingBeforeUnload();
}

void WebFrameTestClient::PostAccessibilityEvent(
    const blink::WebAXObject& obj,
    ax::mojom::Event event,
    ax::mojom::EventFrom event_from) {
  const char* event_name = nullptr;
  switch (event) {
    case ax::mojom::Event::kActiveDescendantChanged:
      event_name = "ActiveDescendantChanged";
      break;
    case ax::mojom::Event::kAriaAttributeChanged:
      event_name = "AriaAttributeChanged";
      break;
    case ax::mojom::Event::kAutocorrectionOccured:
      event_name = "AutocorrectionOccured";
      break;
    case ax::mojom::Event::kBlur:
      event_name = "Blur";
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      event_name = "CheckedStateChanged";
      break;
    case ax::mojom::Event::kChildrenChanged:
      event_name = "ChildrenChanged";
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
    case ax::mojom::Event::kFocus:
      event_name = "Focus";
      break;
    case ax::mojom::Event::kHover:
      event_name = "Hover";
      break;
    case ax::mojom::Event::kInvalidStatusChanged:
      event_name = "InvalidStatusChanged";
      break;
    case ax::mojom::Event::kLayoutComplete:
      event_name = "LayoutComplete";
      break;
    case ax::mojom::Event::kLiveRegionChanged:
      event_name = "LiveRegionChanged";
      break;
    case ax::mojom::Event::kLoadComplete:
      event_name = "LoadComplete";
      break;
    case ax::mojom::Event::kLocationChanged:
      event_name = "LocationChanged";
      break;
    case ax::mojom::Event::kMenuListItemSelected:
      event_name = "MenuListItemSelected";
      break;
    case ax::mojom::Event::kMenuListValueChanged:
      event_name = "MenuListValueChanged";
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
    case ax::mojom::Event::kTextSelectionChanged:
      event_name = "SelectedTextChanged";
      break;
    case ax::mojom::Event::kTextChanged:
      event_name = "TextChanged";
      break;
    case ax::mojom::Event::kValueChanged:
      event_name = "ValueChanged";
      break;
    default:
      event_name = "Unknown";
      break;
  }

  HandleWebAccessibilityEvent(obj, event_name);
}

void WebFrameTestClient::MarkWebAXObjectDirty(const blink::WebAXObject& obj,
                                              bool subtree) {
  HandleWebAccessibilityEvent(obj, "MarkDirty");
}

void WebFrameTestClient::HandleWebAccessibilityEvent(
    const blink::WebAXObject& obj,
    const char* event_name) {
  // Only hook the accessibility events that occurred during the test run.
  // This check prevents false positives in BlinkLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_runner()->TestIsRunning())
    return;

  AccessibilityController* accessibility_controller =
      web_view_test_proxy_->accessibility_controller();
  accessibility_controller->NotificationReceived(obj, event_name);
  if (accessibility_controller->ShouldLogAccessibilityEvents()) {
    std::string message("AccessibilityNotification - ");
    message += event_name;

    blink::WebNode node = obj.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      blink::WebElement element = node.To<blink::WebElement>();
      if (element.HasAttribute("id")) {
        message += " - id:";
        message += element.GetAttribute("id").Utf8().data();
      }
    }

    delegate_->PrintMessage(message + "\n");
  }
}

void WebFrameTestClient::DidChangeSelection(bool is_empty_callback) {
  if (test_runner()->ShouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebFrameTestClient::DidChangeContents() {
  if (test_runner()->ShouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

blink::WebPlugin* WebFrameTestClient::CreatePlugin(
    const blink::WebPluginParams& params) {
  blink::WebLocalFrame* frame = web_frame_test_proxy_->GetWebFrame();
  if (TestPlugin::IsSupportedMimeType(params.mime_type))
    return TestPlugin::Create(params, delegate_, frame);
  return delegate_->CreatePluginPlaceholder(params);
}

void WebFrameTestClient::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  delegate_->GetWebWidgetTestProxy(web_frame_test_proxy_->GetWebFrame())
      ->event_sender()
      ->SetContextMenuData(context_menu_data);
}

void WebFrameTestClient::DownloadURL(
    const blink::WebURLRequest& request,
    network::mojom::RedirectMode cross_origin_redirect_behavior,
    mojo::ScopedMessagePipeHandle blob_url_token) {
  if (test_runner()->ShouldWaitUntilExternalURLLoad()) {
    delegate_->PrintMessage(std::string("Download started\n"));
    delegate_->TestFinished();
  }
}

void WebFrameTestClient::DidReceiveTitle(const blink::WebString& title,
                                         blink::WebTextDirection direction) {
  if (test_runner()->ShouldDumpFrameLoadCallbacks() &&
      web_frame_test_proxy_->GetWebFrame()) {
    PrintFrameDescription(delegate_, web_frame_test_proxy_->GetWebFrame());
    delegate_->PrintMessage(std::string(" - didReceiveTitle: ") + title.Utf8() +
                            "\n");
  }

  if (test_runner()->ShouldDumpTitleChanges())
    delegate_->PrintMessage(std::string("TITLE CHANGED: '") + title.Utf8() +
                            "'\n");
}

void WebFrameTestClient::DidChangeIcon(blink::WebIconURL::Type icon_type) {
  if (test_runner()->ShouldDumpIconChanges()) {
    PrintFrameDescription(delegate_, web_frame_test_proxy_->GetWebFrame());
    delegate_->PrintMessage(std::string(" - didChangeIcons\n"));
  }
}

void WebFrameTestClient::DidFailLoad(const blink::WebURLError& error,
                                     blink::WebHistoryCommitType commit_type) {
  if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, web_frame_test_proxy_->GetWebFrame());
    delegate_->PrintMessage(" - didFailLoadWithError\n");
  }
}

void WebFrameTestClient::DidStartLoading() {
  test_runner()->AddLoadingFrame(web_frame_test_proxy_->GetWebFrame());
}

void WebFrameTestClient::DidStopLoading() {
  test_runner()->RemoveLoadingFrame(web_frame_test_proxy_->GetWebFrame());
}

void WebFrameTestClient::DidDispatchPingLoader(const blink::WebURL& url) {
  if (test_runner()->ShouldDumpPingLoaderCallbacks())
    delegate_->PrintMessage(std::string("PingLoader dispatched to '") +
                            URLDescription(url).c_str() + "'.\n");
}

void WebFrameTestClient::WillSendRequest(blink::WebURLRequest& request) {
  // Need to use GURL for host() and SchemeIs()
  GURL url = request.Url();
  GURL main_document_url = request.SiteForCookies();

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear())
      request.ClearHttpHeaderField(blink::WebString::FromUTF8(header));
  }

  std::string host = url.host();
  if (!host.empty() &&
      (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    if (!IsLocalHost(host) && !IsTestHost(host) &&
        !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((!main_document_url.SchemeIs(url::kHttpScheme) &&
          !main_document_url.SchemeIs(url::kHttpsScheme)) ||
         IsLocalHost(main_document_url.host())) &&
        !delegate_->AllowExternalPages()) {
      delegate_->PrintMessage(std::string("Blocked access to external URL ") +
                              url.possibly_invalid_spec() + "\n");
      BlockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.SetUrl(delegate_->RewriteWebTestsURL(
      request.Url().GetString().Utf8(),
      test_runner()->is_web_platform_tests_mode()));
}

void WebFrameTestClient::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
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
  // Do not print line numbers if there is no associated source file name.
  // TODO(crbug.com/896194): Figure out why the source line is flaky for empty
  // source names.
  if (!source_name.IsEmpty() && source_line) {
    console_message += base::StringPrintf("line %d: ", source_line);
  }
  // Console messages shouldn't be included in the expected output for
  // web-platform-tests because they may create non-determinism not
  // intended by the test author. They are still included in the stderr
  // output for debug purposes.
  bool dump_to_stderr = test_runner()->is_web_platform_tests_mode();
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

  if (dump_to_stderr) {
    delegate_->PrintMessageToStderr(console_message);
  } else {
    delegate_->PrintMessage(console_message);
  }
}

bool WebFrameTestClient::ShouldContinueNavigation(
    blink::WebNavigationInfo* info) {
  if (test_runner()->ShouldDumpNavigationPolicy()) {
    delegate_->PrintMessage(
        "Default policy for navigation to '" +
        URLDescription(info->url_request.Url()) + "' is '" +
        WebNavigationPolicyToString(info->navigation_policy) + "'\n");
  }

  if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
    GURL url = info->url_request.Url();
    WebFrameTestClient::PrintFrameDescription(
        delegate_, web_frame_test_proxy_->GetWebFrame());
    delegate_->PrintMessage(" - BeginNavigation request to '");
    delegate_->PrintMessage(
        DescriptionSuitableForTestResult(url.possibly_invalid_spec()));
    delegate_->PrintMessage("', http method ");
    delegate_->PrintMessage(info->url_request.HttpMethod().Utf8().data());
    delegate_->PrintMessage("\n");
  }

  bool should_continue = true;
  if (test_runner()->PolicyDelegateEnabled()) {
    delegate_->PrintMessage(
        std::string("Policy delegate: attempt to load ") +
        URLDescription(info->url_request.Url()) + " with navigation type '" +
        WebNavigationTypeToString(info->navigation_type) + "'\n");
    should_continue = test_runner()->PolicyDelegateIsPermissive();
    if (test_runner()->PolicyDelegateShouldNotifyDone()) {
      test_runner()->PolicyDelegateDone();
      should_continue = false;
    }
  }

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      info->url_request.ClearHttpHeaderField(
          blink::WebString::FromUTF8(header));
    }
  }
  info->url_request.SetUrl(delegate_->RewriteWebTestsURL(
      info->url_request.Url().GetString().Utf8(),
      test_runner()->is_web_platform_tests_mode()));
  return should_continue;
}

void WebFrameTestClient::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::string device_id = sink_id.Utf8();
  if (device_id == "valid" || device_id.empty())
    std::move(completion_callback).Run(/*error =*/base::nullopt);
  else if (device_id == "unauthorized")
    std::move(completion_callback)
        .Run(blink::WebSetSinkIdError::kNotAuthorized);
  else
    std::move(completion_callback).Run(blink::WebSetSinkIdError::kNotFound);
}

void WebFrameTestClient::DidClearWindowObject() {
  blink::WebLocalFrame* frame = web_frame_test_proxy_->GetWebFrame();
  web_view_test_proxy_->test_interfaces()->BindTo(frame);
  web_view_test_proxy_->BindTo(frame);
  delegate_->GetWebWidgetTestProxy(frame)->BindTo(frame);
}

blink::WebEffectiveConnectionType
WebFrameTestClient::GetEffectiveConnectionType() {
  return test_runner()->effective_connection_type();
}

TestRunner* WebFrameTestClient::test_runner() {
  return web_view_test_proxy_->test_interfaces()->GetTestRunner();
}

}  // namespace test_runner
