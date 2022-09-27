// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/websockets/websocket_common.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/security_context/insecure_request_policy.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

constexpr char kWebSocketSubprotocolSeparator[] = ", ";
constexpr size_t kMaxReasonSizeInBytes = 123;

}  // namespace

WebSocketCommon::ConnectResult WebSocketCommon::Connect(
    ExecutionContext* execution_context,
    const String& url,
    const Vector<String>& protocols,
    WebSocketChannel* channel,
    ExceptionState& exception_state) {
  url_ = KURL(NullURL(), url);

  bool upgrade_insecure_requests_set =
      (execution_context->GetSecurityContext().GetInsecureRequestPolicy() &
       mojom::blink::InsecureRequestPolicy::kUpgradeInsecureRequests) !=
      mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone;

  if (upgrade_insecure_requests_set && url_.Protocol() == "ws" &&
      !network::IsUrlPotentiallyTrustworthy(GURL(url_))) {
    UseCounter::Count(
        execution_context,
        WebFeature::kUpgradeInsecureRequestsUpgradedRequestWebsocket);
    url_.SetProtocol("wss");
    if (url_.Port() == 80)
      url_.SetPort(443);
  }

  if (!url_.IsValid()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The URL '" + url + "' is invalid.");
    return ConnectResult::kException;
  }
  if (!url_.ProtocolIs("ws") && !url_.ProtocolIs("wss")) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL's scheme must be either 'ws' or 'wss'. '" + url_.Protocol() +
            "' is not allowed.");
    return ConnectResult::kException;
  }

  if (url_.HasFragmentIdentifier()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in WebSocket URLs.");
    return ConnectResult::kException;
  }

  if (!execution_context->GetContentSecurityPolicyForCurrentWorld()
           ->AllowConnectToSource(url_, url_, RedirectStatus::kNoRedirect)) {
    state_ = kClosed;

    return ConnectResult::kAsyncError;
  }

  // Fail if not all elements in |protocols| are valid.
  for (const String& protocol : protocols) {
    if (!IsValidSubprotocolString(protocol)) {
      state_ = kClosed;
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "The subprotocol '" +
                                            EncodeSubprotocolString(protocol) +
                                            "' is invalid.");
      return ConnectResult::kException;
    }
  }

  // Fail if there're duplicated elements in |protocols|.
  HashSet<String> visited;
  for (const String& protocol : protocols) {
    if (!visited.insert(protocol).is_new_entry) {
      state_ = kClosed;
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "The subprotocol '" +
                                            EncodeSubprotocolString(protocol) +
                                            "' is duplicated.");
      return ConnectResult::kException;
    }
  }

  String protocol_string;
  if (!protocols.empty())
    protocol_string = JoinStrings(protocols, kWebSocketSubprotocolSeparator);

  if (!channel->Connect(url_, protocol_string)) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "An insecure WebSocket connection may not be initiated from a page "
        "loaded over HTTPS.");
    channel->Disconnect();
    return ConnectResult::kException;
  }

  return ConnectResult::kSuccess;
}

void WebSocketCommon::CloseInternal(int code,
                                    const String& reason,
                                    WebSocketChannel* channel,
                                    ExceptionState& exception_state) {
  String cleansed_reason = reason;
  if (code == WebSocketChannel::kCloseEventCodeNotSpecified) {
    DVLOG(1) << "WebSocket " << this << " close() without code and reason";
  } else {
    DVLOG(1) << "WebSocket " << this << " close() code=" << code
             << " reason=" << reason;
    if (!(code == WebSocketChannel::kCloseEventCodeNormalClosure ||
          (WebSocketChannel::kCloseEventCodeMinimumUserDefined <= code &&
           code <= WebSocketChannel::kCloseEventCodeMaximumUserDefined))) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "The code must be either 1000, or between 3000 and 4999. " +
              String::Number(code) + " is neither.");
      return;
    }
    // Bindings specify USVString, so unpaired surrogates are already replaced
    // with U+FFFD.
    StringUTF8Adaptor utf8(reason);
    if (utf8.size() > kMaxReasonSizeInBytes) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The message must not be greater than " +
              String::Number(kMaxReasonSizeInBytes) + " bytes.");
      return;
    }
    if (!reason.empty() && !reason.Is8Bit()) {
      DCHECK_GT(utf8.size(), 0u);
      // reason might contain unpaired surrogates. Reconstruct it from
      // utf8.
      cleansed_reason = String::FromUTF8(utf8.data(), utf8.size());
    }
  }

  if (state_ == kClosing || state_ == kClosed)
    return;
  if (state_ == kConnecting) {
    state_ = kClosing;
    channel->Fail(
        "WebSocket is closed before the connection is established.",
        mojom::ConsoleMessageLevel::kWarning,
        std::make_unique<SourceLocation>(String(), String(), 0, 0, nullptr));
    return;
  }
  state_ = kClosing;
  if (channel)
    channel->Close(code, cleansed_reason);
}

inline bool WebSocketCommon::IsValidSubprotocolCharacter(UChar character) {
  const UChar kMinimumProtocolCharacter = '!';  // U+0021.
  const UChar kMaximumProtocolCharacter = '~';  // U+007E.
  // Set to true if character does not matches "separators" ABNF defined in
  // RFC2616. SP and HT are excluded since the range check excludes them.
  bool is_not_separator =
      character != '"' && character != '(' && character != ')' &&
      character != ',' && character != '/' &&
      !(character >= ':' &&
        character <=
            '@')  // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
      && !(character >= '[' &&
           character <= ']')  // U+005B - U+005D ('[', '\\', ']').
      && character != '{' && character != '}';
  return character >= kMinimumProtocolCharacter &&
         character <= kMaximumProtocolCharacter && is_not_separator;
}

bool WebSocketCommon::IsValidSubprotocolString(const String& protocol) {
  if (protocol.empty())
    return false;
  for (wtf_size_t i = 0; i < protocol.length(); ++i) {
    if (!IsValidSubprotocolCharacter(protocol[i]))
      return false;
  }
  return true;
}

String WebSocketCommon::EncodeSubprotocolString(const String& protocol) {
  StringBuilder builder;
  for (wtf_size_t i = 0; i < protocol.length(); i++) {
    if (protocol[i] < 0x20 || protocol[i] > 0x7E)
      builder.AppendFormat("\\u%04X", protocol[i]);
    else if (protocol[i] == 0x5c)
      builder.Append("\\\\");
    else
      builder.Append(protocol[i]);
  }
  return builder.ToString();
}

String WebSocketCommon::JoinStrings(const Vector<String>& strings,
                                    const char* separator) {
  StringBuilder builder;
  for (wtf_size_t i = 0; i < strings.size(); ++i) {
    if (i)
      builder.Append(separator);
    builder.Append(strings[i]);
  }
  return builder.ToString();
}

}  // namespace blink
