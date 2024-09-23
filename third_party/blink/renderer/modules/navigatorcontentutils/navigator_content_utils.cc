/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2014, Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils.h"

#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

const char NavigatorContentUtils::kSupplementName[] = "NavigatorContentUtils";

namespace {

// Verify custom handler URL security as described in steps 6 and 7
// https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
static bool VerifyCustomHandlerURLSecurity(
    const LocalDOMWindow& window,
    const KURL& full_url,
    String& error_message,
    ProtocolHandlerSecurityLevel security_level) {
  // The specification says that the API throws SecurityError exception if the
  // URL's protocol isn't HTTP(S) or is potentially trustworthy.
  if (!IsAllowedCustomHandlerURL(GURL(full_url), security_level)) {
    error_message = "The scheme of the url provided must be HTTP(S).";
    return false;
  }

  // The specification says that the API throws SecurityError exception if the
  // URL's origin differs from the window's origin.
  if (security_level < ProtocolHandlerSecurityLevel::kUntrustedOrigins &&
      !window.GetSecurityOrigin()->CanRequest(full_url)) {
    error_message =
        "Can only register custom handler in the document's origin.";
    return false;
  }

  return true;
}

static bool VerifyCustomHandlerURL(
    const LocalDOMWindow& window,
    const String& user_url,
    ExceptionState& exception_state,
    ProtocolHandlerSecurityLevel security_level) {
  KURL full_url = window.CompleteURL(user_url);
  KURL base_url = window.BaseURL();
  String error_message;

  if (!VerifyCustomHandlerURLSyntax(full_url, base_url, user_url,
                                    error_message)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      error_message);
    return false;
  }

  if (!VerifyCustomHandlerURLSecurity(window, full_url, error_message,
                                      security_level)) {
    exception_state.ThrowSecurityError(error_message);
    return false;
  }

  return true;
}

}  // namespace

bool VerifyCustomHandlerScheme(const String& scheme,
                               String& error_string,
                               ProtocolHandlerSecurityLevel security_level) {
  if (!IsValidProtocol(scheme)) {
    error_string = "The scheme name '" + scheme +
                   "' is not allowed by URI syntax (RFC3986).";
    return false;
  }

  bool has_custom_scheme_prefix = false;
  StringUTF8Adaptor scheme_adaptor(scheme);
  if (!IsValidCustomHandlerScheme(scheme_adaptor.AsStringView(), security_level,
                                  &has_custom_scheme_prefix)) {
    if (has_custom_scheme_prefix) {
      error_string = "The scheme name '" + scheme +
                     "' is not allowed. Schemes starting with '" + scheme +
                     "' must be followed by one or more ASCII letters.";
    } else {
      error_string = "The scheme '" + scheme +
                     "' doesn't belong to the scheme allowlist. "
                     "Please prefix non-allowlisted schemes "
                     "with the string 'web+'.";
    }
    return false;
  }

  return true;
}

bool VerifyCustomHandlerURLSyntax(const KURL& full_url,
                                  const KURL& base_url,
                                  const String& user_url,
                                  String& error_message) {
  StringUTF8Adaptor url_adaptor(user_url);
  URLSyntaxErrorCode code =
      IsValidCustomHandlerURLSyntax(GURL(full_url), url_adaptor.AsStringView());
  switch (code) {
    case URLSyntaxErrorCode::kNoError:
      return true;
    case URLSyntaxErrorCode::kMissingToken:
      error_message =
          "The url provided ('" + user_url + "') does not contain '%s'.";
      break;
    case URLSyntaxErrorCode::kInvalidUrl:
      error_message =
          "The custom handler URL created by removing '%s' and prepending '" +
          base_url.GetString() + "' is invalid.";
      break;
  }

  return false;
}

NavigatorContentUtils& NavigatorContentUtils::From(Navigator& navigator,
                                                   LocalFrame& frame) {
  NavigatorContentUtils* navigator_content_utils =
      Supplement<Navigator>::From<NavigatorContentUtils>(navigator);
  if (!navigator_content_utils) {
    navigator_content_utils = MakeGarbageCollected<NavigatorContentUtils>(
        navigator, MakeGarbageCollected<NavigatorContentUtilsClient>(&frame));
    ProvideTo(navigator, navigator_content_utils);
  }
  return *navigator_content_utils;
}

NavigatorContentUtils::~NavigatorContentUtils() = default;

void NavigatorContentUtils::registerProtocolHandler(
    Navigator& navigator,
    const String& scheme,
    const String& url,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = navigator.DomWindow();
  if (!window)
    return;

  WebSecurityOrigin origin(window->GetSecurityOrigin());
  ProtocolHandlerSecurityLevel security_level =
      Platform::Current()->GetProtocolHandlerSecurityLevel(origin);

  // Per the HTML specification, exceptions for arguments must be surfaced in
  // the order of the arguments.
  String error_message;
  if (!VerifyCustomHandlerScheme(scheme, error_message, security_level)) {
    exception_state.ThrowSecurityError(error_message);
    return;
  }

  if (!VerifyCustomHandlerURL(*window, url, exception_state, security_level))
    return;

  // Count usage; perhaps we can forbid this from cross-origin subframes as
  // proposed in https://crbug.com/977083.
  UseCounter::Count(
      window, window->GetFrame()->IsCrossOriginToOutermostMainFrame()
                  ? WebFeature::kRegisterProtocolHandlerCrossOriginSubframe
                  : WebFeature::kRegisterProtocolHandlerSameOriginAsTop);
  // Count usage. Context should now always be secure due to the same-origin
  // check and the requirement that the calling context be secure.
  UseCounter::Count(window,
                    window->IsSecureContext()
                        ? WebFeature::kRegisterProtocolHandlerSecureOrigin
                        : WebFeature::kRegisterProtocolHandlerInsecureOrigin);

  NavigatorContentUtils::From(navigator, *window->GetFrame())
      .Client()
      ->RegisterProtocolHandler(scheme, window->CompleteURL(url));
}

void NavigatorContentUtils::unregisterProtocolHandler(
    Navigator& navigator,
    const String& scheme,
    const String& url,
    ExceptionState& exception_state) {
  LocalDOMWindow* window = navigator.DomWindow();
  if (!window)
    return;

  WebSecurityOrigin origin(window->GetSecurityOrigin());
  ProtocolHandlerSecurityLevel security_level =
      Platform::Current()->GetProtocolHandlerSecurityLevel(origin);

  String error_message;
  if (!VerifyCustomHandlerScheme(scheme, error_message, security_level)) {
    exception_state.ThrowSecurityError(error_message);
    return;
  }

  if (!VerifyCustomHandlerURL(*window, url, exception_state, security_level))
    return;

  NavigatorContentUtils::From(navigator, *window->GetFrame())
      .Client()
      ->UnregisterProtocolHandler(scheme, window->CompleteURL(url));
}

void NavigatorContentUtils::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
