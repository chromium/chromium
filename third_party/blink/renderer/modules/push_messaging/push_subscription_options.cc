// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {
namespace {

const int kMaxApplicationServerKeyLength = 255;

Vector<uint8_t> BufferSourceToVector(
    const ArrayBufferOrArrayBufferViewOrString& application_server_key,
    ExceptionState& exception_state) {
  char* input;
  int length;
  Vector<char> decoded_application_server_key;
  Vector<uint8_t> result;

  // Convert the input array into a string of bytes.
  if (application_server_key.IsArrayBuffer()) {
    input =
        static_cast<char*>(application_server_key.GetAsArrayBuffer()->Data());
    length = application_server_key.GetAsArrayBuffer()
                 ->DeprecatedByteLengthAsUnsigned();
  } else if (application_server_key.IsArrayBufferView()) {
    input = static_cast<char*>(
        application_server_key.GetAsArrayBufferView().View()->buffer()->Data());
    length = application_server_key.GetAsArrayBufferView()
                 .View()
                 ->buffer()
                 ->DeprecatedByteLengthAsUnsigned();
  } else if (application_server_key.IsString()) {
    if (!Base64UnpaddedURLDecode(application_server_key.GetAsString(),
                                 decoded_application_server_key)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidCharacterError,
          "The provided applicationServerKey is not encoded as base64url "
          "without padding.");
      return result;
    }
    input = reinterpret_cast<char*>(decoded_application_server_key.data());
    length = decoded_application_server_key.size();
  } else {
    NOTREACHED();
    return result;
  }

  // Check the validity of the sender info. It must either be a 65-byte
  // uncompressed VAPID key, which has the byte 0x04 as the first byte or a
  // numeric sender ID.
  const bool is_vapid = length == 65 && *input == 0x04;
  const bool is_sender_id =
      length > 0 && length < kMaxApplicationServerKeyLength &&
      (std::find_if_not(input, input + length, &WTF::IsASCIIDigit<char>) ==
       input + length);

  if (is_vapid || is_sender_id) {
    result.Append(input, length);
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidAccessError,
        "The provided applicationServerKey is not valid.");
  }

  return result;
}

}  // namespace

// static
PushSubscriptionOptions* PushSubscriptionOptions::FromOptionsInit(
    const PushSubscriptionOptionsInit* options_init,
    ExceptionState& exception_state) {
  Vector<uint8_t> application_server_key;
  if (options_init->hasApplicationServerKey()) {
    application_server_key.AppendVector(BufferSourceToVector(
        options_init->applicationServerKey(), exception_state));
  }

  return MakeGarbageCollected<PushSubscriptionOptions>(
      options_init->userVisibleOnly(), application_server_key);
}

PushSubscriptionOptions::PushSubscriptionOptions(
    bool user_visible_only,
    const Vector<uint8_t>& application_server_key)
    : user_visible_only_(user_visible_only),
      application_server_key_(DOMArrayBuffer::Create(
          application_server_key.data(),
          SafeCast<unsigned>(application_server_key.size()))) {}

bool PushSubscriptionOptions::IsApplicationServerKeyVapid() const {
  if (!application_server_key_)
    return false;
  return application_server_key_->ByteLengthAsSizeT() == 65 &&
         static_cast<uint8_t*>(application_server_key_->Data())[0] == 0x04;
}

void PushSubscriptionOptions::Trace(blink::Visitor* visitor) {
  visitor->Trace(application_server_key_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
