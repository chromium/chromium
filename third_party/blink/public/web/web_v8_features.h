// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/public/platform/web_common.h"
#include "v8/include/v8-forward.h"

namespace blink {

// WebV8Features is used in conjunction with IDL interface features which
// specify a [ContextEnabled] extended attribute. Such features may be enabled
// for arbitrary main-world V8 contexts by using these methods during
// WebLocalFrameClient::DidCreateScriptContext. Enabling a given feature causes
// its binding(s) to be installed on the appropriate global object(s) during
// context initialization.
//
// See src/third_party/blink/renderer/bindings/IDLExtendedAttributes.md for more
// information.
class BLINK_EXPORT WebV8Features {
 public:
  static void EnableMojoJS(v8::Local<v8::Context>, bool);

  static void EnableMojoJSAndUseBroker(
      v8::Local<v8::Context> context,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker_remote);

  static void EnableMojoJSFileSystemAccessHelper(v8::Local<v8::Context>, bool);

  // Enables SharedArrayBuffer for this process.
  static void EnableSharedArrayBuffer();

 private:
  WebV8Features() = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_V8_FEATURES_H_
