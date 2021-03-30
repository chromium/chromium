// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port_provider.h"

#include <utility>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "content/public/browser/android/app_web_message_port.h"
#endif

using blink::MessagePortChannel;

namespace content {
namespace {

void PostMessageToFrameInternal(
    WebContents* web_contents,
    const std::u16string& source_origin,
    const std::u16string& target_origin,
    const std::u16string& data,
    std::vector<blink::MessagePortDescriptor> ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(chrisha): Kill off MessagePortChannel, as MessagePortDescriptor now
  // plays that role.
  std::vector<MessagePortChannel> channels;
  for (auto& port : ports)
    channels.emplace_back(MessagePortChannel(std::move(port)));

  blink::TransferableMessage message;
  message.owned_encoded_message = blink::EncodeStringMessage(data);
  message.encoded_message = message.owned_encoded_message;
  message.ports = std::move(channels);

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
  rfh->PostMessageEvent(base::nullopt, source_origin, target_origin,
                        std::move(message));
}

#if defined(OS_ANDROID)
std::u16string ToString16(JNIEnv* env,
                          const base::android::JavaParamRef<jstring>& s) {
  if (s.is_null())
    return std::u16string();
  return base::android::ConvertJavaStringToUTF16(env, s);
}
#endif

}  // namespace

// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const std::u16string& source_origin,
    const std::u16string& target_origin,
    const std::u16string& data) {
  PostMessageToFrameInternal(web_contents, source_origin, target_origin, data,
                             std::vector<blink::MessagePortDescriptor>());
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

#if defined(OS_FUCHSIA) || BUILDFLAG(IS_CHROMECAST)
// static
void MessagePortProvider::PostMessageToFrame(
    WebContents* web_contents,
    const std::u16string& source_origin,
    const base::Optional<std::u16string>& target_origin,
    const std::u16string& data,
    std::vector<blink::WebMessagePort> ports) {
  // Extract the underlying descriptors.
  std::vector<blink::MessagePortDescriptor> descriptors;
  descriptors.reserve(ports.size());
  for (size_t i = 0; i < ports.size(); ++i)
    descriptors.push_back(ports[i].PassPort());
  PostMessageToFrameInternal(web_contents, source_origin,
                             target_origin.value_or(base::EmptyString16()),
                             data, std::move(descriptors));
}
#endif

}  // namespace content
