// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
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
            ASSERT_TRUE(res.has_value());
            result = *res;
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
            ASSERT_TRUE(res.has_value());
            result = *res;
            run_loop.Quit();
          }));

  run_loop.Run();

  EXPECT_EQ(result, "{\"text\":\"hello\"}");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_Navigation) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest search_resource("https://example.com/search?query=testing",
                             "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type="text" name="query">
      </form>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            EXPECT_TRUE(res.has_value());
            EXPECT_TRUE(res->IsNull());
            run_loop.Quit();
          }));
  run_loop.Run();
  search_resource.Complete("");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_InvalidInput) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type="text" name="query">
      </form>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  // Test with a field that doesn't exist in the form
  model_context->ExecuteTool(
      "search_tool", "{\"nonexistent\": \"value\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(),
                      WebDocument::ScriptToolError::kInvalidInputArguments);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
        <button type=submit>Submit</button>
      </form>
      <script>
        document.querySelector('form').addEventListener('submit', e => {
          window.submit_event_fired = true;
          e.preventDefault();
          e.respondWith(Promise.resolve("result value"));
        });
      </script>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            got_result = true;
            ASSERT_TRUE(res.has_value());
            EXPECT_EQ(*res, "result value");
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(got_result);
  EXPECT_TRUE(MainFrame()
                  .ExecuteScriptAndReturnValue(
                      WebScriptSource("window.submit_event_fired"))
                  .As<v8::Boolean>()
                  ->Value());
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_Reject) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
        <button type=submit>Submit</button>
      </form>
      <script>
        document.querySelector('form').addEventListener('submit', e => {
          window.submit_event_fired = true;
          e.preventDefault();
          e.respondWith(Promise.reject("rejection"));
        });
      </script>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            got_result = true;
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(),
                      WebDocument::ScriptToolError::kToolInvocationFailed);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(got_result);
  EXPECT_TRUE(MainFrame()
                  .ExecuteScriptAndReturnValue(
                      WebScriptSource("window.submit_event_fired"))
                  .As<v8::Boolean>()
                  ->Value());
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_NoPreventDefault) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest search_resource("https://example.com/search?query=testing",
                             "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
        <button type=submit>Submit</button>
      </form>
      <script>
        window.respond_with_error = "";
        document.querySelector('form').addEventListener('submit', e => {
          try {
            e.respondWith(Promise.resolve("result"));
          } catch (err) {
            window.respond_with_error = err.name;
          }
        });
      </script>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            got_result = true;
            ASSERT_TRUE(res.has_value());
            EXPECT_TRUE(res->IsNull());
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(got_result);

  EXPECT_EQ(ToCoreString(Window().GetIsolate(),
                         MainFrame()
                             .ExecuteScriptAndReturnValue(
                                 WebScriptSource("window.respond_with_error"))
                             .As<v8::String>()),
            "InvalidStateError");
  search_resource.Complete("");
}

TEST_F(ModelContextTest, ManualSubmit_RespondWithThrows) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form>
        <input type=text name=query>
      </form>
      <script>
        window.agent_invoked = undefined;
        window.error_name = "";
        document.querySelector('form').addEventListener('submit', e => {
          window.agent_invoked = e.agentInvoked;
          try {
            e.respondWith(Promise.resolve("result"));
          } catch (err) {
            window.error_name = err.name;
          }
          e.preventDefault();
        });
      </script>
    </body>
  )");

  MainFrame().ExecuteScript(
      WebScriptSource("document.querySelector('form').requestSubmit();"));

  EXPECT_FALSE(
      MainFrame()
          .ExecuteScriptAndReturnValue(WebScriptSource("window.agent_invoked"))
          .As<v8::Boolean>()
          ->Value());
  EXPECT_EQ(ToCoreString(Window().GetIsolate(),
                         MainFrame()
                             .ExecuteScriptAndReturnValue(
                                 WebScriptSource("window.error_name"))
                             .As<v8::String>()),
            "InvalidStateError");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_LateRespondWithThrows) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
      </form>
      <script>
        window.saved_event = null;
        document.querySelector('form').addEventListener('submit', e => {
          window.saved_event = e;
          e.preventDefault();
          e.respondWith(Promise.resolve("result"));
        });
      </script>
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            ASSERT_TRUE(res.has_value());
            EXPECT_EQ(*res, "result");
            run_loop.Quit();
          }));
  run_loop.Run();

  v8::Local<v8::Value> error_name =
      MainFrame().ExecuteScriptAndReturnValue(WebScriptSource(R"(
        try {
          window.saved_event.respondWith(Promise.resolve("late"));
          "no error";
        } catch (err) {
          err.name;
        }
      )"));
  EXPECT_EQ(ToCoreString(Window().GetIsolate(), error_name.As<v8::String>()),
            "InvalidStateError");
}

}  // namespace blink
