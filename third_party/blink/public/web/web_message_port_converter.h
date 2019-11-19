// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MESSAGE_PORT_CONVERTER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MESSAGE_PORT_CONVERTER_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"

namespace v8 {
class Isolate;
template <class T>
class Local;
class Value;
}  // namespace v8

namespace blink {
class MessagePortChannel;

class WebMessagePortConverter {
 public:
  // Disentangle and extract a MessagePortChannel from a v8 wrapper of
  // MessagePort. If the wrapper is not MessagePort or the MessagePort is
  // neutered, it will return nullopt.
  BLINK_EXPORT static base::Optional<MessagePortChannel>
  DisentangleAndExtractMessagePortChannel(v8::Isolate*, v8::Local<v8::Value>);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_MESSAGE_PORT_CONVERTER_H_
