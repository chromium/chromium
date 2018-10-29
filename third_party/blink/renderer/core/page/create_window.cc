/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/create_window.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

// Though isspace() considers \t and \v to be whitespace, Win IE doesn't when
// parsing window features.
static bool IsWindowFeaturesSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\f';
}

WebWindowFeatures GetWindowFeaturesFromString(const String& feature_string) {
  WebWindowFeatures window_features;

  // This code follows the HTML spec, specifically
  // https://html.spec.whatwg.org/#concept-window-open-features-tokenize
  if (feature_string.IsEmpty())
    return window_features;

  window_features.menu_bar_visible = false;
  window_features.status_bar_visible = false;
  window_features.tool_bar_visible = false;
  window_features.scrollbars_visible = false;

  unsigned key_begin, key_end;
  unsigned value_begin, value_end;

  String buffer = feature_string.DeprecatedLower();
  unsigned length = buffer.length();
  for (unsigned i = 0; i < length;) {
    // skip to first non-separator (start of key name), but don't skip
    // past the end of the string
    while (i < length && IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_begin = i;

    // skip to first separator (end of key name), but don't skip past
    // the end of the string
    while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
      i++;
    key_end = i;

    SECURITY_DCHECK(i <= length);

    // skip separators past the key name, except '=', and don't skip past
    // the end of the string
    while (i < length && buffer[i] != '=') {
      if (buffer[i] == ',' || !IsWindowFeaturesSeparator(buffer[i]))
        break;

      i++;
    }

    if (i < length && IsWindowFeaturesSeparator(buffer[i])) {
      // skip to first non-separator (start of value), but don't skip
      // past a ',' or the end of the string.
      while (i < length && IsWindowFeaturesSeparator(buffer[i])) {
        if (buffer[i] == ',')
          break;

        i++;
      }

      value_begin = i;

      SECURITY_DCHECK(i <= length);

      // skip to first separator (end of value)
      while (i < length && !IsWindowFeaturesSeparator(buffer[i]))
        i++;

      value_end = i;

      SECURITY_DCHECK(i <= length);
    } else {
      // No value given.
      value_begin = i;
      value_end = i;
    }

    String key_string(
        buffer.Substring(key_begin, key_end - key_begin).LowerASCII());
    String value_string(
        buffer.Substring(value_begin, value_end - value_begin).LowerASCII());

    // Listing a key with no value is shorthand for key=yes
    int value;
    if (value_string.IsEmpty() || value_string == "yes")
      value = 1;
    else
      value = value_string.ToInt();

    if (key_string.IsEmpty())
      continue;

    if (key_string == "left" || key_string == "screenx") {
      window_features.x_set = true;
      window_features.x = value;
    } else if (key_string == "top" || key_string == "screeny") {
      window_features.y_set = true;
      window_features.y = value;
    } else if (key_string == "width" || key_string == "innerwidth") {
      window_features.width_set = true;
      window_features.width = value;
    } else if (key_string == "height" || key_string == "innerheight") {
      window_features.height_set = true;
      window_features.height = value;
    } else if (key_string == "menubar") {
      window_features.menu_bar_visible = value;
    } else if (key_string == "toolbar" || key_string == "location") {
      window_features.tool_bar_visible |= static_cast<bool>(value);
    } else if (key_string == "status") {
      window_features.status_bar_visible = value;
    } else if (key_string == "scrollbars") {
      window_features.scrollbars_visible = value;
    } else if (key_string == "resizable") {
      window_features.resizable = value;
    } else if (key_string == "noopener") {
      window_features.noopener = true;
    } else if (key_string == "background") {
      window_features.background = true;
    } else if (key_string == "persistent") {
      window_features.persistent = true;
    }
  }

  return window_features;
}

static Frame* ReuseExistingWindow(LocalFrame& active_frame,
                                  LocalFrame& lookup_frame,
                                  const AtomicString& frame_name,
                                  const KURL& destination_url) {
  if (!frame_name.IsEmpty() && !EqualIgnoringASCIICase(frame_name, "_blank")) {
    if (Frame* frame = lookup_frame.FindFrameForNavigation(
            frame_name, active_frame, destination_url)) {
      if (!EqualIgnoringASCIICase(frame_name, "_self")) {
        if (Page* page = frame->GetPage()) {
          if (page == active_frame.GetPage())
            page->GetFocusController().SetFocusedFrame(frame);
          else
            page->GetChromeClient().Focus(&active_frame);
        }
      }
      return frame;
    }
  }
  return nullptr;
}

enum class WindowOpenFromAdState {
  // This is used for a UMA histogram. Please never alter existing values, only
  // append new ones and make sure to update enums.xml.
  kAdScriptAndAdFrame = 0,
  kNonAdScriptAndAdFrame = 1,
  kAdScriptAndNonAdFrame = 2,
  kNonAdScriptAndNonAdFrame = 3,
  kMaxValue = kNonAdScriptAndNonAdFrame,
};

static void MaybeLogWindowOpen(LocalFrame& opener_frame) {
  AdTracker* ad_tracker = opener_frame.GetAdTracker();
  if (!ad_tracker) {
    return;
  }

  bool is_ad_subframe = opener_frame.IsAdSubframe();
  bool is_ad_script_in_stack = ad_tracker->IsAdScriptInStack();

  // Log to UMA.
  WindowOpenFromAdState state =
      is_ad_subframe ? (is_ad_script_in_stack
                            ? WindowOpenFromAdState::kAdScriptAndAdFrame
                            : WindowOpenFromAdState::kNonAdScriptAndAdFrame)
                     : (is_ad_script_in_stack
                            ? WindowOpenFromAdState::kAdScriptAndNonAdFrame
                            : WindowOpenFromAdState::kNonAdScriptAndNonAdFrame);
  UMA_HISTOGRAM_ENUMERATION("Blink.WindowOpen.FromAdState", state);

  // Log to UKM.
  ukm::UkmRecorder* ukm_recorder = opener_frame.GetDocument()->UkmRecorder();
  ukm::SourceId source_id = opener_frame.GetDocument()->UkmSourceID();
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::AbusiveExperienceHeuristic_WindowOpen(source_id)
        .SetFromAdSubframe(is_ad_subframe)
        .SetFromAdScript(is_ad_script_in_stack)
        .Record(ukm_recorder);
  }
}

static Frame* CreateNewWindow(LocalFrame& opener_frame,
                              const FrameLoadRequest& request,
                              const WebWindowFeatures& features,
                              bool force_new_foreground_tab,
                              bool& created) {
  Page* old_page = opener_frame.GetPage();
  if (!old_page)
    return nullptr;

  NavigationPolicy policy = force_new_foreground_tab
                                ? kNavigationPolicyNewForegroundTab
                                : NavigationPolicyForCreateWindow(features);

  const SandboxFlags sandbox_flags =
      opener_frame.GetDocument()->IsSandboxed(
          kSandboxPropagatesToAuxiliaryBrowsingContexts)
          ? opener_frame.GetSecurityContext()->GetSandboxFlags()
          : kSandboxNone;

  SessionStorageNamespaceId new_namespace_id =
      AllocateSessionStorageNamespaceId();

  if (base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage)) {
    // TODO(dmurph): Don't copy session storage when features.noopener is true:
    // https://html.spec.whatwg.org/multipage/browsers.html#copy-session-storage
    // https://crbug.com/771959
    CoreInitializer::GetInstance().CloneSessionStorage(old_page,
                                                       new_namespace_id);
  }

  Page* page = old_page->GetChromeClient().CreateWindow(
      &opener_frame, request, features, policy, sandbox_flags,
      new_namespace_id);
  if (!page)
    return nullptr;

  if (page == old_page) {
    Frame* frame = &opener_frame.Tree().Top();
    if (!opener_frame.CanNavigate(*frame))
      return nullptr;
    if (request.GetShouldSetOpener() == kMaybeSetOpener)
      frame->Client()->SetOpener(&opener_frame);
    return frame;
  }

  DCHECK(page->MainFrame());
  LocalFrame& frame = *ToLocalFrame(page->MainFrame());

  page->SetWindowFeatures(features);

  frame.View()->SetCanHaveScrollbars(features.scrollbars_visible);

  // 'x' and 'y' specify the location of the window, while 'width' and 'height'
  // specify the size of the viewport. We can only resize the window, so adjust
  // for the difference between the window size and the viewport size.

  IntRect window_rect = page->GetChromeClient().RootWindowRect();
  IntSize viewport_size = page->GetChromeClient().PageRect().Size();

  if (features.x_set)
    window_rect.SetX(features.x);
  if (features.y_set)
    window_rect.SetY(features.y);
  if (features.width_set)
    window_rect.SetWidth(features.width +
                         (window_rect.Width() - viewport_size.Width()));
  if (features.height_set)
    window_rect.SetHeight(features.height +
                          (window_rect.Height() - viewport_size.Height()));

  page->GetChromeClient().SetWindowRectWithAdjustment(window_rect, frame);
  page->GetChromeClient().Show(policy);

  MaybeLogWindowOpen(opener_frame);
  created = true;
  return &frame;
}

static Frame* CreateWindowHelper(LocalFrame& opener_frame,
                                 LocalFrame& active_frame,
                                 LocalFrame& lookup_frame,
                                 const FrameLoadRequest& request,
                                 const WebWindowFeatures& features,
                                 bool force_new_foreground_tab,
                                 bool& created) {
  DCHECK(request.GetResourceRequest().RequestorOrigin() ||
         opener_frame.GetDocument()->Url().IsEmpty());
  DCHECK_EQ(request.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kAuxiliary);
  probe::windowOpen(opener_frame.GetDocument(),
                    request.GetResourceRequest().Url(), request.FrameName(),
                    features,
                    LocalFrame::HasTransientUserActivation(&opener_frame));
  created = false;

  Frame* window =
      features.noopener || force_new_foreground_tab
          ? nullptr
          : ReuseExistingWindow(active_frame, lookup_frame, request.FrameName(),
                                request.GetResourceRequest().Url());

  if (!window) {
    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (opener_frame.GetDocument()->IsSandboxed(kSandboxPopups)) {
      // FIXME: This message should be moved off the console once a solution to
      // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
      opener_frame.GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
          kSecurityMessageSource, kErrorMessageLevel,
          "Blocked opening '" +
              request.GetResourceRequest().Url().ElidedString() +
              "' in a new window because the request was made in a sandboxed "
              "frame whose 'allow-popups' permission is not set."));
      return nullptr;
    }
  }

  if (window) {
    // JS can run inside ReuseExistingWindow (via onblur), which can detach
    // the target window.
    if (!window->Client())
      return nullptr;
    if (request.GetShouldSetOpener() == kMaybeSetOpener)
      window->Client()->SetOpener(&opener_frame);
    return window;
  }

  return CreateNewWindow(opener_frame, request, features,
                         force_new_foreground_tab, created);
}

DOMWindow* CreateWindow(const String& url_string,
                        const AtomicString& frame_name,
                        const String& window_features_string,
                        LocalDOMWindow& calling_window,
                        LocalFrame& first_frame,
                        LocalFrame& opener_frame,
                        ExceptionState& exception_state) {
  LocalFrame* active_frame = calling_window.GetFrame();
  DCHECK(active_frame);

  KURL completed_url = url_string.IsEmpty()
                           ? KURL(g_empty_string)
                           : first_frame.GetDocument()->CompleteURL(url_string);
  if (!completed_url.IsEmpty() && !completed_url.IsValid()) {
    UseCounter::Count(active_frame, WebFeature::kWindowOpenWithInvalidURL);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Unable to open a window with invalid URL '" +
            completed_url.GetString() + "'.\n");
    return nullptr;
  }

  if (completed_url.ProtocolIsJavaScript() &&
      opener_frame.GetDocument()->GetContentSecurityPolicy() &&
      !ContentSecurityPolicy::ShouldBypassMainWorld(
          opener_frame.GetDocument())) {
    String script_source = DecodeURLEscapeSequences(completed_url.GetString());

    if (!opener_frame.GetDocument()
             ->GetContentSecurityPolicy()
             ->AllowJavaScriptURLs(nullptr, script_source,
                                   opener_frame.GetDocument()->Url(),
                                   OrdinalNumber())) {
      return nullptr;
    }
  }

  WebWindowFeatures window_features =
      GetWindowFeaturesFromString(window_features_string);

  FrameLoadRequest frame_request(calling_window.document(),
                                 ResourceRequest(completed_url), frame_name);
  frame_request.SetShouldSetOpener(window_features.noopener ? kNeverSetOpener
                                                            : kMaybeSetOpener);
  frame_request.GetResourceRequest().SetFrameType(
      network::mojom::RequestContextFrameType::kAuxiliary);

  // Normally, FrameLoader would take care of setting the referrer for a
  // navigation that is triggered from javascript. However, creating a window
  // goes through sufficient processing that it eventually enters FrameLoader as
  // an embedder-initiated navigation.  FrameLoader assumes no responsibility
  // for generating an embedder-initiated navigation's referrer, so we need to
  // ensure the proper referrer is set now.
  // TODO(domfarolino): Stop setting ResourceRequest's HTTP Referrer and store
  // this is a separate member. See https://crbug.com/850813.
  frame_request.GetResourceRequest().SetHTTPReferrer(
      SecurityPolicy::GenerateReferrer(
          active_frame->GetDocument()->GetReferrerPolicy(), completed_url,
          active_frame->GetDocument()->OutgoingReferrer()));

  // Records HasUserGesture before the value is invalidated inside
  // createWindow(LocalFrame& openerFrame, ...).
  // This value will be set in ResourceRequest loaded in a new LocalFrame.
  bool has_user_gesture = LocalFrame::HasTransientUserActivation(&opener_frame);

  // We pass the opener frame for the lookupFrame in case the active frame is
  // different from the opener frame, and the name references a frame relative
  // to the opener frame.
  bool created;
  Frame* new_frame = CreateWindowHelper(
      opener_frame, *active_frame, opener_frame, frame_request, window_features,
      false /* force_new_foreground_tab */, created);
  if (!new_frame)
    return nullptr;
  if (new_frame->DomWindow()->IsInsecureScriptAccess(calling_window,
                                                     completed_url))
    return window_features.noopener ? nullptr : new_frame->DomWindow();

  // TODO(dcheng): Special case for window.open("about:blank") to ensure it
  // loads synchronously into a new window. This is our historical behavior, and
  // it's consistent with the creation of a new iframe with src="about:blank".
  // Perhaps we could get rid of this if we started reporting the initial empty
  // document's url as about:blank? See crbug.com/471239.
  // TODO(japhet): This special case is also necessary for behavior asserted by
  // some extensions tests.  Using NavigationScheduler::scheduleNavigationChange
  // causes the navigation to be flagged as a client redirect, which is
  // observable via the webNavigation extension api.
  if (created) {
    FrameLoadRequest request(calling_window.document(),
                             ResourceRequest(completed_url));
    request.GetResourceRequest().SetHasUserGesture(has_user_gesture);
    if (const WebInputEvent* input_event = CurrentInputEvent::Get()) {
      request.SetInputStartTime(input_event->TimeStamp());
    }
    new_frame->Navigate(request, WebFrameLoadType::kStandard);
  } else if (!url_string.IsEmpty()) {
    new_frame->ScheduleNavigation(*calling_window.document(), completed_url,
                                  WebFrameLoadType::kStandard,
                                  has_user_gesture ? UserGestureStatus::kActive
                                                   : UserGestureStatus::kNone);
  }
  return window_features.noopener ? nullptr : new_frame->DomWindow();
}

void CreateWindowForRequest(const FrameLoadRequest& request,
                            LocalFrame& opener_frame) {
  DCHECK(request.GetResourceRequest().RequestorOrigin() ||
         (opener_frame.GetDocument() &&
          opener_frame.GetDocument()->Url().IsEmpty()));

  if (opener_frame.GetDocument()->PageDismissalEventBeingDispatched() !=
      Document::kNoDismissal)
    return;

  if (opener_frame.GetDocument() &&
      opener_frame.GetDocument()->IsSandboxed(kSandboxPopups))
    return;

  WebWindowFeatures features;
  features.noopener = request.GetShouldSetOpener() == kNeverSetOpener;
  bool created;
  Frame* new_frame = CreateWindowHelper(
      opener_frame, opener_frame, opener_frame, request, features,
      true /* force_new_foreground_tab */, created);
  if (!new_frame)
    return;
  if (request.GetShouldSendReferrer() == kMaybeSendReferrer) {
    // TODO(japhet): Does ReferrerPolicy need to be proagated for RemoteFrames?
    if (new_frame->IsLocalFrame())
      ToLocalFrame(new_frame)->GetDocument()->SetReferrerPolicy(
          opener_frame.GetDocument()->GetReferrerPolicy());
  }

  // TODO(japhet): Form submissions on RemoteFrames don't work yet.
  FrameLoadRequest new_request(nullptr, request.GetResourceRequest());
  new_request.SetForm(request.Form());
  if (const WebInputEvent* input_event = CurrentInputEvent::Get()) {
    new_request.SetInputStartTime(input_event->TimeStamp());
  }
  auto blob_url_token = request.GetBlobURLToken();
  if (blob_url_token)
    new_request.SetBlobURLToken(std::move(blob_url_token));
  if (new_frame->IsLocalFrame())
    ToLocalFrame(new_frame)->Loader().StartNavigation(new_request);
}

}  // namespace blink
