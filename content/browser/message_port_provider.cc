// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port_provider.h"

#include <utility>

#include "build/build_config.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "content/browser/android/app_web_message_port.h"
#endif

using blink::MessagePortChannel;

namespace content {
namespace {

void PostMessageToFrameInternal(WebContents* web_contents,
                                const base::string16& source_origin,
                                const base::string16& target_origin,
                                const base::string16& data,
                                std::vector<MessagePortChannel> channels) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::TransferableMessage message;
  message.owned_encoded_message = blink::EncodeStringMessage(data);
  message.encoded_message = message.owned_encoded_message;
  message.ports = std::move(channels);
  int32_t source_routing_id = MSG_ROUTING_NONE;

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  rfh->PostMessageEvent(source_routing_id, source_origin, target_origin,
                        std::move(message));
}

#if defined(OS_ANDROID)
base::string16 ToString16(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& s) {
  if (s.is_null())
    return base::string16();
  return base::android::ConvertJavaStringToUTF16(env, s);
}
#endif

}  // namespace

// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const base::string16& source_origin,
    const base::string16& target_origin,
    const base::string16& data) {
  PostMessageToFrameInternal(web_contents, source_origin, target_origin, data,
                             std::vector<MessagePortChannel>());
}

#if defined(OS_ANDROID)
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& source_origin,
    const base::android::JavaParamRef<jstring>& target_origin,
    const base::android::JavaParamRef<jstring>& data,
    const base::android::JavaParamRef<jobjectArray>& ports) {
  PostMessageToFrameInternal(web_contents, ToString16(env, source_origin),
                             ToString16(env, target_origin),
                             ToString16(env, data),
                             AppWebMessagePort::UnwrapJavaArray(env, ports));
}
#endif

#if defined(OS_FUCHSIA) || defined(IS_CHROMECAST)
// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const base::string16& source_origin,
    const base::Optional<base::string16>& target_origin,
    const base::string16& data,
    std::vector<mojo::ScopedMessagePipeHandle> channels) {
  std::vector<MessagePortChannel> channels_wrapped;
  for (mojo::ScopedMessagePipeHandle& handle : channels) {
    channels_wrapped.emplace_back(std::move(handle));
  }
  PostMessageToFrameInternal(web_contents, source_origin,
                             target_origin.value_or(base::EmptyString16()),
                             data, channels_wrapped);
}
#endif

}  // namespace content
