// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#endif

namespace content {
class WebContents;

// An interface consisting of methods that can be called to use Message ports.
class CONTENT_EXPORT MessagePortProvider {
 public:
  // Posts a MessageEvent to the main frame using the given source and target
  // origins and data.
  // See https://html.spec.whatwg.org/multipage/comms.html#messageevent for
  // further information on message events.
  // Should be called on UI thread.
  static void PostMessageToFrame(
      WebContents* web_contents,
      const base::string16& source_origin,
      const base::string16& target_origin,
      const base::string16& data);

#if defined(OS_ANDROID)
  static void PostMessageToFrame(
      WebContents* web_contents,
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& source_origin,
      const base::android::JavaParamRef<jstring>& target_origin,
      const base::android::JavaParamRef<jstring>& data,
      const base::android::JavaParamRef<jobjectArray>& ports);
#endif  // OS_ANDROID

#if defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
  // If |target_origin| is unset, then no origin scoping is applied.
  static void PostMessageToFrame(
      WebContents* web_contents,
      const base::string16& source_origin,
      const base::Optional<base::string16>& target_origin,
      const base::string16& data,
      std::vector<mojo::ScopedMessagePipeHandle> channels);
#endif  // OS_FUCHSIA || IS_CHROMECAST

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MessagePortProvider);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
