// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/message_port_provider.h"

#include <optional>
#include <utility>

#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/app_web_message_port.h"
#include "content/public/browser/android/message_payload.h"
#endif

using blink::MessagePortChannel;

namespace content {
namespace {

void PostMessageToFrameInternal(
    Page& page,
    const url::Origin* source_origin,
    const url::Origin* target_origin,
    const blink::WebMessagePayload& data,
    std::vector<blink::MessagePortDescriptor> ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(chrisha): Kill off MessagePortChannel, as MessagePortDescriptor now
  // plays that role.
  std::vector<MessagePortChannel> channels;
  for (auto& port : ports)
    channels.emplace_back(MessagePortChannel(std::move(port)));

  blink::TransferableMessage message = blink::EncodeWebMessagePayload(data);
  message.ports = std::move(channels);
  // As the message is posted from the embedder and not from another renderer,
  // set the agent cluster ID to the embedder's.
  message.sender_agent_cluster_id =
      blink::WebMessagePort::GetEmbedderAgentClusterID();

  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(&page.GetMainDocument());
  rfh->PostMessageEvent(std::nullopt, source_origin, target_origin,
                        std::move(message));
}

#if BUILDFLAG(IS_ANDROID)
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
    Page& page,
    const url::Origin* source_origin,
    const url::Origin* target_origin,
    const blink::WebMessagePayload& data) {
  PostMessageToFrameInternal(page, source_origin, target_origin, data,
                             std::vector<blink::MessagePortDescriptor>());
}

#if BUILDFLAG(IS_ANDROID)
void MessagePortProvider::PostMessageToFrame(
    Page& page,
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& source_origin,
    const base::android::JavaParamRef<jstring>& target_origin,
    const base::android::JavaParamRef<jobject>& payload,
    const base::android::JavaParamRef<jobjectArray>& ports) {
  std::optional<url::Origin> source;
  std::u16string serialized_source = ToString16(env, source_origin);
  if (!serialized_source.empty()) {
    source = url::Origin::Create(GURL(serialized_source));
  }

  std::optional<url::Origin> target;
  std::u16string serialized_target = ToString16(env, target_origin);
  if (!serialized_target.empty()) {
    target = url::Origin::Create(GURL(serialized_target));
  }

  PostMessageToFrameInternal(
      page, source.has_value() ? &(*source) : nullptr,
      target.has_value() ? &(*target) : nullptr,
      android::ConvertToWebMessagePayloadFromJava(
          base::android::ScopedJavaLocalRef<jobject>(payload)),
      android::AppWebMessagePort::Release(env, ports));
}
#endif

#if BUILDFLAG(IS_FUCHSIA) ||           \
    BUILDFLAG(ENABLE_CAST_RECEIVER) && \
        (BUILDFLAG(IS_CASTOS) || BUILDFLAG(IS_CAST_ANDROID))
// static
void MessagePortProvider::PostMessageToFrame(
    Page& page,
    const url::Origin* source_origin,
    const url::Origin* target_origin,
    const std::u16string& data,
    std::vector<blink::WebMessagePort> ports) {
  // Extract the underlying descriptors.
  std::vector<blink::MessagePortDescriptor> descriptors;
  descriptors.reserve(ports.size());
  for (size_t i = 0; i < ports.size(); ++i)
    descriptors.push_back(ports[i].PassPort());
  PostMessageToFrameInternal(page, source_origin, target_origin, data,
                             std::move(descriptors));
}
#endif

}  // namespace content
