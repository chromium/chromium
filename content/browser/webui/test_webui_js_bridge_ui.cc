// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/test_webui_js_bridge_ui.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/test/grit/web_ui_mojo_test_resources.h"

namespace content {

namespace {}

TestWebUIJsBridgeUI::TestWebUIJsBridgeUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // nullptr in unit tests. No need to register any resources for unit tests.
  if (!web_ui->GetWebContents()) {
    return;
  }

  WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kTestWebUIJsBridgeUIHost);
  data_source->SetDefaultResource(IDR_WEB_UI_MANAGED_INTERFACE_TEST_HTML);
  data_source->AddResourcePath(
      "web_ui_managed_interface_test.test-mojom-webui.js",
      IDR_WEB_UI_MANAGED_INTERFACE_TEST_TEST_MOJOM_WEBUI_JS);
  data_source->AddResourcePath("web_ui_managed_interface_test.js",
                               IDR_WEB_UI_MANAGED_INTERFACE_TEST_JS);
}

WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIJsBridgeUI)

WEB_UI_CONTROLLER_TYPE_IMPL(TestWebUIJsBridgeUI2)

}  // namespace content
