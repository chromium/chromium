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

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Though absl::ascii_isspace() considers \t and \v to be whitespace, Win IE
// doesn't when parsing window features.
static bool IsWindowFeaturesSeparator(UChar c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '=' ||
         c == ',' || c == '\f';
}

WebWindowFeatures GetWindowFeaturesFromString(const String& feature_string,
                                              LocalDOMWindow* dom_window) {
  WebWindowFeatures window_features;

  const bool attribution_reporting_enabled =
      dom_window &&
      (RuntimeEnabledFeatures::AttributionReportingEnabled(dom_window) ||
       RuntimeEnabledFeatures::AttributionReportingCrossAppWebEnabled(
           dom_window));
  const bool explicit_opener_enabled =
      RuntimeEnabledFeatures::RelOpenerBcgDependencyHintEnabled(dom_window);

  // This code follows the HTML spec, specifically
  // https://html.spec.whatwg.org/C/#concept-window-open-features-tokenize
  if (feature_string.empty())
    return window_features;

  bool ui_features_were_disabled = false;
  bool menu_bar = true;
  bool status_bar = true;
  bool tool_bar = true;
  bool scrollbars = true;
  enum class PopupState { kUnknown, kPopup, kWindow };
  PopupState popup_state = PopupState::kUnknown;
  unsigned key_begin, key_end;
  unsigned value_begin, value_end;

  const String buffer = feature_string.LowerASCII();
  const unsigned length = buffer.length();
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

    if (key_begin == key_end)
      continue;

    StringView key_string(buffer, key_begin, key_end - key_begin);
    StringView value_string(buffer, value_begin, value_end - value_begin);

    // Listing a key with no value is shorthand for key=yes
    int value;
    if (value_string.empty() || value_string == "yes" ||
        value_string == "true") {
      value = 1;
    } else {
      value = CharactersToInt(value_string, WTF::NumberParsingOptions::Loose(),
                              /*ok=*/nullptr);
    }

    if (!ui_features_were_disabled && key_string != "noopener" &&
        (!explicit_opener_enabled || key_string != "opener") &&
        key_string != "noreferrer" &&
        (!attribution_reporting_enabled || key_string != "attributionsrc")) {
      ui_features_were_disabled = true;
      menu_bar = false;
      status_bar = false;
      tool_bar = false;
      scrollbars = false;
    }

    if (key_string == "left" || key_string == "screenx") {
      window_features.x_set = true;
      window_features.x = value;
    } else if (key_string == "top" || key_string == "screeny") {
      window_features.y_set = true;
      window_features.y = value;
    } else if (key_string == "width" || key_string == "innerwidth") {
      window_features.width_set = true;
      window_features.width = value;
    } else if (key_string == "popup") {
      // The 'popup' property explicitly triggers a popup.
      popup_state = value ? PopupState::kPopup : PopupState::kWindow;
    } else if (key_string == "height" || key_string == "innerheight") {
      window_features.height_set = true;
      window_features.height = value;
    } else if (key_string == "menubar") {
      menu_bar = value;
    } else if (key_string == "toolbar" || key_string == "location") {
      tool_bar |= static_cast<bool>(value);
    } else if (key_string == "status") {
      status_bar = value;
    } else if (key_string == "scrollbars") {
      scrollbars = value;
    } else if (key_string == "resizable") {
      window_features.resizable = value;
    } else if (key_string == "noopener") {
      window_features.noopener = value;
    } else if (explicit_opener_enabled && key_string == "opener") {
      window_features.explicit_opener = value;
    } else if (key_string == "noreferrer") {
      window_features.noreferrer = value;
    } else if (key_string == "background") {
      window_features.background = true;
    } else if (key_string == "persistent") {
      window_features.persistent = true;
    } else if (RuntimeEnabledFeatures::PartitionedPopinsEnabled(dom_window) &&
               key_string == "popin") {
      window_features.is_partitioned_popin = true;
    } else if (attribution_reporting_enabled &&
               key_string == "attributionsrc") {
      if (!window_features.attribution_srcs.has_value()) {
        window_features.attribution_srcs.emplace();
      }

      if (!value_string.empty()) {
        // attributionsrc values are URLs, and as such their original case needs
        // to be retained for correctness. Positions in both `feature_string`
        // and `buffer` correspond because ASCII-lowercasing doesn't add,
        // remove, or swap character positions; it only does in-place
        // transformations of capital ASCII characters. See crbug.com/1338698
        // for details.
        DCHECK_EQ(feature_string.length(), buffer.length());
        const StringView original_case_value_string(feature_string, value_begin,
                                                    value_end - value_begin);

        // attributionsrc values are encoded in order to support embedded
        // special characters, such as '='.
        window_features.attribution_srcs->emplace_back(DecodeURLEscapeSequences(
            original_case_value_string.ToString(), DecodeURLMode::kUTF8));
      }
    }
  }

  window_features.is_popup =
      popup_state == PopupState::kPopup || window_features.is_partitioned_popin;
  if (popup_state == PopupState::kUnknown) {
    window_features.is_popup = !tool_bar || !menu_bar || !scrollbars ||
                               !status_bar || !window_features.resizable;
  }

  if (window_features.noreferrer)
    window_features.noopener = true;

  if (window_features.noopener) {
    window_features.explicit_opener = false;
  }

  return window_features;
}

static void MaybeLogWindowOpen(LocalFrame& opener_frame) {
  AdTracker* ad_tracker = opener_frame.GetAdTracker();
  if (!ad_tracker)
    return;

  bool is_ad_frame = opener_frame.IsAdFrame();
  bool is_ad_script_in_stack =
      ad_tracker->IsAdScriptInStack(AdTracker::StackType::kBottomAndTop);

  // Log to UKM.
  ukm::UkmRecorder* ukm_recorder = opener_frame.GetDocument()->UkmRecorder();
  ukm::SourceId source_id = opener_frame.GetDocument()->UkmSourceID();
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::AbusiveExperienceHeuristic_WindowOpen(source_id)
        .SetFromAdSubframe(is_ad_frame)
        .SetFromAdScript(is_ad_script_in_stack)
        .Record(ukm_recorder);
  }
}

Frame* CreateNewWindow(LocalFrame& opener_frame,
                       FrameLoadRequest& request,
                       const AtomicString& frame_name) {
  LocalDOMWindow& opener_window = *opener_frame.DomWindow();
  DCHECK(request.GetResourceRequest().RequestorOrigin() ||
         opener_window.Url().IsEmpty());
  DCHECK_EQ(kNavigationPolicyCurrentTab, request.GetNavigationPolicy());

  if (opener_window.document()->PageDismissalEventBeingDispatched() !=
      Document::kNoDismissal) {
    return nullptr;
  }

  request.SetFrameType(mojom::RequestContextFrameType::kAuxiliary);

  const KURL& url = request.GetResourceRequest().Url();
  if (url.ProtocolIsJavaScript()) {
    if (opener_window
            .CheckAndGetJavascriptUrl(request.JavascriptWorld(), url,
                                      nullptr /* element */)
            .empty()) {
      return nullptr;
    }
  }

  if (!opener_window.GetSecurityOrigin()->CanDisplay(url)) {
    opener_window.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        "Not allowed to load local resource: " + url.ElidedString()));
    return nullptr;
  }

  const WebWindowFeatures& features = request.GetWindowFeatures();
  const auto& picture_in_picture_window_options =
      request.GetPictureInPictureWindowOptions();
  if (picture_in_picture_window_options.has_value()) {
    request.SetNavigationPolicy(kNavigationPolicyPictureInPicture);
  } else {
    request.SetNavigationPolicy(NavigationPolicyForCreateWindow(features));
    probe::WindowOpen(&opener_window, url, frame_name, features,
                      LocalFrame::HasTransientUserActivation(&opener_frame));
  }

  // Sandboxed frames cannot open new auxiliary browsing contexts.
  if (opener_window.IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kPopups)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    opener_window.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        "Blocked opening '" + url.ElidedString() +
            "' in a new window because the request was made in a sandboxed "
            "frame whose 'allow-popups' permission is not set."));
    return nullptr;
  }

  network::mojom::blink::WebSandboxFlags sandbox_flags =
      opener_window.IsSandboxed(network::mojom::blink::WebSandboxFlags::
                                    kPropagatesToAuxiliaryBrowsingContexts)
          ? opener_window.GetSandboxFlags()
          : network::mojom::blink::WebSandboxFlags::kNone;

  SessionStorageNamespaceId new_namespace_id =
      AllocateSessionStorageNamespaceId();

  Page* old_page = opener_frame.GetPage();
  if (!features.noopener) {
    CoreInitializer::GetInstance().CloneSessionStorage(old_page,
                                                       new_namespace_id);
  }

  bool consumed_user_gesture = false;
  Page* page = old_page->GetChromeClient().CreateWindow(
      &opener_frame, request, frame_name, features, sandbox_flags,
      new_namespace_id, consumed_user_gesture);
  if (!page)
    return nullptr;

  if (page == old_page) {
    Frame* frame = &opener_frame.Tree().Top();
    if (!opener_frame.CanNavigate(*frame))
      return nullptr;
    if (!features.noopener)
      frame->SetOpener(&opener_frame);
    return frame;
  }

  DCHECK(page->MainFrame());
  LocalFrame& frame = *To<LocalFrame>(page->MainFrame());

  page->SetWindowFeatures(features);

  frame.View()->SetCanHaveScrollbars(!features.is_popup);

  page->GetChromeClient().Show(frame, opener_frame,
                               request.GetNavigationPolicy(),
                               consumed_user_gesture);
  MaybeLogWindowOpen(opener_frame);
  return &frame;
}

}  // namespace blink
