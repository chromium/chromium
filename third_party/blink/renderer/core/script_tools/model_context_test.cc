// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class ModelContextTest : public SimTest {
 public:
  ModelContextTest() = default;

 private:
  ScopedWebMCPForTest scoped_webmcp_{true};
};

TEST_F(ModelContextTest, ExecuteTool) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(
<body>
    <script>
    async function echo(obj) {
      return obj.text;
    }

    navigator.modelContext.registerTool({
      execute: echo,
      name: "echo",
      description: "echo input",
      inputSchema: {
          type: "object",
          properties: {
              "text": {
                  description: "Value to echo",
                  type: "string",
              }
          },
          required: ["text"]
      },
    });
  </script>
  </body>
)");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  String result;

  model_context->ExecuteTool(
      "echo", "{\"text\": \"hello\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            if (res.has_value()) {
              result = *res;
            } else {
              result = "error";
            }
            run_loop.Quit();
          }));

  run_loop.Run();

  EXPECT_EQ(result, "hello");
}

TEST_F(ModelContextTest, ExecuteToolReturnsObject) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(
<body>
    <script>
    async function echo(obj) {
      return obj;
    }

    navigator.modelContext.registerTool({
      execute: echo,
      name: "echo",
      description: "echo input",
      inputSchema: {
          type: "object",
          properties: {
              "text": {
                  description: "Value to echo",
                  type: "string",
              }
          },
          required: ["text"]
      },
      annotations: {
        readOnlyHint: "true"
      },
    });
  </script>
</body>
)");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  String result;

  model_context->ExecuteTool(
      "echo", "{\"text\": \"hello\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            if (res.has_value()) {
              result = *res;
            } else {
              result = "error";
            }
            run_loop.Quit();
          }));

  run_loop.Run();

  EXPECT_EQ(result, "{\"text\":\"hello\"}");
}

}  // namespace blink
