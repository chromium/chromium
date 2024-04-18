// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_client.h"

#include <string_view>

#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/user_agent.h"
#include "ui/gfx/image/image.h"

namespace content {

static ContentClient* g_client;

static bool g_can_change_browser_client = true;

class InternalTestInitializer {
 public:
  static ContentBrowserClient* SetBrowser(ContentBrowserClient* b) {
    CHECK(g_can_change_browser_client)
        << "The wrong ContentBrowserClient subclass is being used. In "
           "content_browsertests, subclass "
           "ContentBrowserTestContentBrowserClient.";
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

// static
void ContentClient::SetCanChangeContentBrowserClientForTesting(bool value) {
  g_can_change_browser_client = value;
}

// static
void ContentClient::SetBrowserClientAlwaysAllowForTesting(
    ContentBrowserClient* b) {
  bool old = g_can_change_browser_client;
  g_can_change_browser_client = true;
  SetBrowserClientForTesting(b);  // IN-TEST
  g_can_change_browser_client = old;
}

void SetContentClient(ContentClient* client) {
  g_client = client;
}

ContentClient* GetContentClient() {
  return g_client;
}

ContentClient* GetContentClientForTesting() {
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

std::vector<url::Origin> ContentClient::GetPdfInternalPluginAllowedOrigins() {
  return {};
}

std::u16string ContentClient::GetLocalizedString(int message_id) {
  return std::u16string();
}

std::u16string ContentClient::GetLocalizedString(
    int message_id,
    const std::u16string& replacement) {
  return std::u16string();
}

std::string_view ContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  return std::string_view();
}

base::RefCountedMemory* ContentClient::GetDataResourceBytes(int resource_id) {
  return nullptr;
}

std::string ContentClient::GetDataResourceString(int resource_id) {
  // Default implementation in terms of GetDataResourceBytes.
  scoped_refptr<base::RefCountedMemory> memory =
      GetDataResourceBytes(resource_id);
  if (!memory)
    return std::string();
  return std::string(base::as_string_view(*memory));
}

gfx::Image& ContentClient::GetNativeImageNamed(int resource_id) {
  static base::NoDestructor<gfx::Image> kEmptyImage;
  return *kEmptyImage;
}

std::string ContentClient::GetProcessTypeNameInEnglish(int type) {
  NOTIMPLEMENTED();
  return std::string();
}

blink::OriginTrialPolicy* ContentClient::GetOriginTrialPolicy() {
  return nullptr;
}

#if BUILDFLAG(IS_ANDROID)
bool ContentClient::UsingSynchronousCompositing() {
  return false;
}

media::MediaDrmBridgeClient* ContentClient::GetMediaDrmBridgeClient() {
  return nullptr;
}
#endif  // BUILDFLAG(IS_ANDROID)

void ContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {}

}  // namespace content
