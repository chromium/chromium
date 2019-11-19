// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_client.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/user_agent.h"
#include "ui/gfx/image/image.h"

namespace content {

static ContentClient* g_client;

class InternalTestInitializer {
 public:
  static ContentBrowserClient* SetBrowser(ContentBrowserClient* b) {
    ContentBrowserClient* rv = g_client->browser_;
    g_client->browser_ = b;
    return rv;
  }

  static ContentRendererClient* SetRenderer(ContentRendererClient* r) {
    ContentRendererClient* rv = g_client->renderer_;
    g_client->renderer_ = r;
    return rv;
  }

  static ContentUtilityClient* SetUtility(ContentUtilityClient* u) {
    ContentUtilityClient* rv = g_client->utility_;
    g_client->utility_ = u;
    return rv;
  }
};

void SetContentClient(ContentClient* client) {
  g_client = client;
}

ContentClient* GetContentClient() {
  return g_client;
}

ContentBrowserClient* SetBrowserClientForTesting(ContentBrowserClient* b) {
  return InternalTestInitializer::SetBrowser(b);
}

ContentRendererClient* SetRendererClientForTesting(ContentRendererClient* r) {
  return InternalTestInitializer::SetRenderer(r);
}

ContentUtilityClient* SetUtilityClientForTesting(ContentUtilityClient* u) {
  return InternalTestInitializer::SetUtility(u);
}

ContentClient::Schemes::Schemes() = default;
ContentClient::Schemes::~Schemes() = default;

ContentClient::ContentClient()
    : browser_(nullptr), gpu_(nullptr), renderer_(nullptr), utility_(nullptr) {}

ContentClient::~ContentClient() {
}

bool ContentClient::CanSendWhileSwappedOut(const IPC::Message* message) {
  return false;
}

base::string16 ContentClient::GetLocalizedString(int message_id) {
  return base::string16();
}

base::string16 ContentClient::GetLocalizedString(
    int message_id,
    const base::string16& replacement) {
  return base::string16();
}

base::StringPiece ContentClient::GetDataResource(int resource_id,
                                                 ui::ScaleFactor scale_factor) {
  return base::StringPiece();
}

base::RefCountedMemory* ContentClient::GetDataResourceBytes(int resource_id) {
  return nullptr;
}

gfx::Image& ContentClient::GetNativeImageNamed(int resource_id) {
  static base::NoDestructor<gfx::Image> kEmptyImage;
  return *kEmptyImage;
}

std::string ContentClient::GetProcessTypeNameInEnglish(int type) {
  NOTIMPLEMENTED();
  return std::string();
}

base::DictionaryValue ContentClient::GetNetLogConstants() {
  return base::DictionaryValue();
}

blink::OriginTrialPolicy* ContentClient::GetOriginTrialPolicy() {
  return nullptr;
}

bool ContentClient::AllowScriptExtensionForServiceWorker(
    const url::Origin& script_origin) {
  return false;
}

#if defined(OS_ANDROID)
bool ContentClient::UsingSynchronousCompositing() {
  return false;
}

media::MediaDrmBridgeClient* ContentClient::GetMediaDrmBridgeClient() {
  return nullptr;
}
#endif  // OS_ANDROID

void ContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {}

}  // namespace content
