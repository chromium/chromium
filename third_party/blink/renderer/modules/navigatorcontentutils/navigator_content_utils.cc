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

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils_client.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const char NavigatorContentUtils::kSupplementName[] = "NavigatorContentUtils";

static const HashSet<String>& SupportedSchemes() {
  DEFINE_STATIC_LOCAL(
      HashSet<String>, supported_schemes,
      ({
          "bitcoin", "geo",  "im",   "irc",         "ircs", "magnet", "mailto",
          "mms",     "news", "nntp", "openpgp4fpr", "sip",  "sms",    "smsto",
          "ssh",     "tel",  "urn",  "webcal",      "wtai", "xmpp",
      }));
  return supported_schemes;
}

static bool VerifyCustomHandlerURL(const Document& document,
                                   const String& url,
                                   ExceptionState& exception_state) {
  // The specification requires that it is a SyntaxError if the "%s" token is
  // not present.
  static const char kToken[] = "%s";
  int index = url.Find(kToken);
  if (-1 == index) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The url provided ('" + url + "') does not contain '%s'.");
    return false;
  }

  // It is also a SyntaxError if the custom handler URL, as created by removing
  // the "%s" token and prepending the base url, does not resolve.
  String new_url = url;
  new_url.Remove(index, base::size(kToken) - 1);
  KURL kurl = document.CompleteURL(new_url);

  if (kurl.IsEmpty() || !kurl.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The custom handler URL created by removing '%s' and prepending '" +
            document.BaseURL().GetString() + "' is invalid.");
    return false;
  }

  // Although not enforced in the spec the spec gives freedom to do additional
  // security checks. Bugs have arisen from allowing non-http/https URLs, e.g.
  // https://crbug.com/971917 and it doesn't make a lot of sense to support
  // them. We also need to allow extensions to continue using the API.
  if (!kurl.ProtocolIsInHTTPFamily() && !kurl.ProtocolIs("chrome-extension")) {
    exception_state.ThrowSecurityError(
        "The scheme of the url provided must be the 'http' or 'https'.");
    return false;
  }

  // The specification says that the API throws SecurityError exception if the
  // URL's origin differs from the document's origin.
  if (!document.GetSecurityOrigin()->CanRequest(kurl)) {
    exception_state.ThrowSecurityError(
        "Can only register custom handler in the document's origin.");
    return false;
  }

  return true;
}

static bool VerifyCustomHandlerScheme(const String& scheme,
                                      ExceptionState& exception_state) {
  if (!IsValidProtocol(scheme)) {
    exception_state.ThrowSecurityError("The scheme '" + scheme +
                                       "' is not valid protocol");
    return false;
  }

  if (scheme.StartsWith("web+")) {
    // The specification requires that the length of scheme is at least five
    // characteres (including 'web+' prefix).
    if (scheme.length() >= 5)
      return true;

    exception_state.ThrowSecurityError("The scheme '" + scheme +
                                       "' is less than five characters long.");
    return false;
  }

  if (SupportedSchemes().Contains(scheme.LowerASCII()))
    return true;

  exception_state.ThrowSecurityError(
      "The scheme '" + scheme +
      "' doesn't belong to the scheme whitelist. "
      "Please prefix non-whitelisted schemes "
      "with the string 'web+'.");
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
    const String& title,
    ExceptionState& exception_state) {
  LocalFrame* frame = navigator.GetFrame();
  if (!frame)
    return;
  Document* document = frame->GetDocument();
  DCHECK(document);

  if (!VerifyCustomHandlerURL(*document, url, exception_state))
    return;

  if (!VerifyCustomHandlerScheme(scheme, exception_state))
    return;

  // Count usage; perhaps we can lock this to secure contexts.
  UseCounter::Count(*document,
                    document->IsSecureContext()
                        ? WebFeature::kRegisterProtocolHandlerSecureOrigin
                        : WebFeature::kRegisterProtocolHandlerInsecureOrigin);

  NavigatorContentUtils::From(navigator, *frame)
      .Client()
      ->RegisterProtocolHandler(scheme, document->CompleteURL(url), title);
}

void NavigatorContentUtils::unregisterProtocolHandler(
    Navigator& navigator,
    const String& scheme,
    const String& url,
    ExceptionState& exception_state) {
  LocalFrame* frame = navigator.GetFrame();
  if (!frame)
    return;
  Document* document = frame->GetDocument();
  DCHECK(document);

  if (!VerifyCustomHandlerURL(*document, url, exception_state))
    return;

  if (!VerifyCustomHandlerScheme(scheme, exception_state))
    return;

  NavigatorContentUtils::From(navigator, *frame)
      .Client()
      ->UnregisterProtocolHandler(scheme, document->CompleteURL(url));
}

void NavigatorContentUtils::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
