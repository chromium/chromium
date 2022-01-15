// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMECAST)
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#endif

namespace content {
class Page;

// An interface consisting of methods that can be called to use Message ports.
class CONTENT_EXPORT MessagePortProvider {
 public:
  MessagePortProvider() = delete;
  MessagePortProvider(const MessagePortProvider&) = delete;
  MessagePortProvider& operator=(const MessagePortProvider&) = delete;

  // Posts a MessageEvent to the main frame using the given source and target
  // origins and data.
  // See https://html.spec.whatwg.org/multipage/comms.html#messageevent for
  // further information on message events.
  // Should be called on UI thread.
  static void PostMessageToFrame(Page& page,
                                 const std::u16string& source_origin,
                                 const std::u16string& target_origin,
                                 const std::u16string& data);

#if BUILDFLAG(IS_ANDROID)
  static void PostMessageToFrame(
      Page& page,
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& source_origin,
      const base::android::JavaParamRef<jstring>& target_origin,
      const base::android::JavaParamRef<jstring>& data,
      const base::android::JavaParamRef<jobjectArray>& ports);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMECAST)
  // If |target_origin| is unset, then no origin scoping is applied.
  static void PostMessageToFrame(
      Page& page,
      const std::u16string& source_origin,
      const absl::optional<std::u16string>& target_origin,
      const std::u16string& data,
      std::vector<blink::WebMessagePort> ports);
#endif  // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMECAST)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MESSAGE_PORT_PROVIDER_H_
