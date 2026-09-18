// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_info.h"

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/constants.mojom.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {
constexpr char kFilePath[] = "some_path";
}

TEST(WebRequestInfoTest, CreateRequestBodyDataFromFile) {
  content::BrowserTaskEnvironment task_environment_;

  network::ResourceRequest request;
  request.method = "POST";
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  request.request_body->AppendFileRange(base::FilePath::FromASCII(kFilePath), 0,
                                        std::numeric_limits<uint64_t>::max(),
                                        base::Time());
  WebRequestInfo info(WebRequestInfoInitParams(0, {}, nullptr, request, false,
                                               false, false, std::nullopt));
  ASSERT_TRUE(info.request_body_data);
  base::Value* value = info.request_body_data->Find(
      extension_web_request_api_constants::kRequestBodyRawKey);
  ASSERT_TRUE(value);

  base::Value expected_value(base::Value::Type::LIST);
  base::DictValue dict;
  dict.Set(extension_web_request_api_constants::kRequestBodyRawFileKey,
           kFilePath);
  expected_value.GetList().Append(std::move(dict));
  EXPECT_EQ(*value, expected_value);
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
// Tests that requests originating from a guest-only process without a frame
// routing ID (e.g. Service Workers or Shared Workers) are correctly classified
// as web view requests.
// Regression test for https://crbug.com/495471295.
TEST(WebRequestInfoTest, GuestWorkerRequestClassifiedAsWebView) {
  content::BrowserTaskEnvironment task_environment;
  content::TestBrowserContext browser_context;

  content::MockRenderProcessHost guest_rph(&browser_context,
                                           /*is_for_guests_only=*/true);
  network::ResourceRequest request;
  request.url = GURL("https://example.com/sw.js");
  request.method = "GET";
  request.destination = network::mojom::RequestDestination::kServiceWorker;

  WebRequestInfo guest_worker_info(WebRequestInfoInitParams(
      /*request_id=*/1,
      content::GlobalRenderFrameHostId(guest_rph.GetID(),
                                       IPC::mojom::kRoutingIdNone),
      /*navigation_ui_data=*/nullptr, request, /*is_download=*/false,
      /*is_async=*/true, /*is_service_worker_script=*/true,
      /*navigation_id=*/std::nullopt));
  EXPECT_TRUE(guest_worker_info.is_web_view);

  content::MockRenderProcessHost non_guest_rph(&browser_context,
                                               /*is_for_guests_only=*/false);
  WebRequestInfo non_guest_worker_info(WebRequestInfoInitParams(
      /*request_id=*/2,
      content::GlobalRenderFrameHostId(non_guest_rph.GetID(),
                                       IPC::mojom::kRoutingIdNone),
      /*navigation_ui_data=*/nullptr, request, /*is_download=*/false,
      /*is_async=*/true, /*is_service_worker_script=*/true,
      /*navigation_id=*/std::nullopt));
  EXPECT_FALSE(non_guest_worker_info.is_web_view);
}
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

}  // namespace extensions
