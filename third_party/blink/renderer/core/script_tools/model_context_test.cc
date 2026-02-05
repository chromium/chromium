// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
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

  bool EvalJsBoolean(const char* script) {
    return MainFrame()
        .ExecuteScriptAndReturnValue(
            WebScriptSource(WebString::FromUTF8(script)))
        .As<v8::Boolean>()
        ->Value();
  }

  String EvalJsString(const char* script) {
    return ToCoreString(Window().GetIsolate(),
                        MainFrame()
                            .ExecuteScriptAndReturnValue(
                                WebScriptSource(WebString::FromUTF8(script)))
                            .As<v8::String>());
  }

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
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
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
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
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
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
        <button type=submit>Submit</button>
      </form>
      <script>
        document.querySelector('form').addEventListener('submit', e => {
          window.submit_event_fired = true;
          window.input_value = document.querySelector('input').value;
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
  EXPECT_TRUE(EvalJsBoolean("window.submit_event_fired"));
  EXPECT_EQ(EvalJsString("window.input_value"), "testing");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_Reject) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
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
  EXPECT_TRUE(EvalJsBoolean("window.submit_event_fired"));
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
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
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

  EXPECT_EQ(EvalJsString("window.respond_with_error"), "InvalidStateError");
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

  EXPECT_FALSE(EvalJsBoolean("window.agent_invoked"));
  EXPECT_EQ(EvalJsString("window.error_name"), "InvalidStateError");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_LateRespondWithThrows) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
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

  EXPECT_EQ(EvalJsString(R"(
        try {
          window.saved_event.respondWith(Promise.resolve("late"));
          "no error";
        } catch (err) {
          err.name;
        }
      )"),
            "InvalidStateError");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_PseudoClasses) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <style>
        form:tool-form-active { background-color: rgb(0, 255, 0); }
        button:tool-submit-active { color: rgb(255, 0, 0); }
      </style>
      <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
        <input type=text name=query>
        <button type=submit>Submit</button>
      </form>
      <script>
        document.querySelector('form').addEventListener('submit', e => {
          const form = document.querySelector('form');
          const button = document.querySelector('button');
          const input = document.querySelector('input');
          window.form_active = form.matches(':tool-form-active');
          window.form_submit_active = form.matches(':tool-submit-active');
          window.button_active = button.matches(':tool-submit-active');
          window.input_form_active = input.matches(':tool-form-active');
          window.input_submit_active = input.matches(':tool-submit-active');
          window.form_bg = getComputedStyle(form).backgroundColor;
          window.button_color = getComputedStyle(button).color;
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
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(EvalJsBoolean("window.form_active"));
  EXPECT_FALSE(EvalJsBoolean("window.form_submit_active"));
  EXPECT_TRUE(EvalJsBoolean("window.button_active"));
  EXPECT_FALSE(EvalJsBoolean("window.input_form_active"));
  EXPECT_FALSE(EvalJsBoolean("window.input_submit_active"));
  EXPECT_EQ(EvalJsString("window.form_bg"), "rgb(0, 255, 0)");
  EXPECT_EQ(EvalJsString("window.button_color"), "rgb(255, 0, 0)");
  EXPECT_FALSE(EvalJsBoolean(
      "document.querySelector('form').matches(':tool-form-active')"));
  EXPECT_FALSE(EvalJsBoolean(
      "document.querySelector('button').matches(':tool-submit-active')"));
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_NoAutoSubmit) {
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
        window.submit_event_fired = false;
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

  // Run until the tool fills the form and focuses the button.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsBoolean(
        "document.activeElement === document.querySelector('button')");
  }));

  EXPECT_FALSE(got_result) << "No toolautosubmit means no automatic submission";
  EXPECT_TRUE(EvalJsBoolean(
      "document.querySelector('form').matches(':tool-form-active')"));

  EXPECT_FALSE(EvalJsBoolean("window.submit_event_fired"));

  // Manually click to submit
  MainFrame().ExecuteScript(
      WebScriptSource("document.querySelector('button').click()"));

  run_loop.Run();

  EXPECT_TRUE(got_result);
  EXPECT_TRUE(EvalJsBoolean("window.submit_event_fired"));

  EXPECT_FALSE(EvalJsBoolean(
      "document.querySelector('form').matches(':tool-form-active')"));
}

TEST_F(ModelContextTest, CancelTool) {
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

  std::optional<uint32_t> execution_id = model_context->ExecuteTool(
      "echo", "{\"text\": \"hello\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            ASSERT_FALSE(res.has_value());
            run_loop.Quit();
          }));

  ASSERT_TRUE(execution_id.has_value());
  model_context->CancelTool(execution_id.value());
  run_loop.Run();
}

TEST_F(ModelContextTest, ToolEventsDispatched) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));

  main_resource.Complete(R"(
<body>
    <script>
    async function longRunning(obj) {
      await new Promise(r => setTimeout(r, 10000));
      return "done";
    }

    navigator.modelContext.registerTool({
      execute: longRunning,
      name: "slow",
      description: "slow tool",
    });

    window.events = [];
    window.addEventListener('toolactivation', e => window.events.push('activation:' + e.toolName));
    window.addEventListener('toolcancel', e => window.events.push('cancel:' + e.toolName));
  </script>
  </body>
)");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;

  // Execute and Cancel
  std::optional<uint32_t> execution_id = model_context->ExecuteTool(
      "slow", "{}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            run_loop.Quit();
          }));

  EXPECT_EQ(EvalJsString("window.events.join(',')"), "activation:slow");

  ASSERT_TRUE(execution_id.has_value());
  model_context->CancelTool(*execution_id);
  run_loop.Run();

  EXPECT_EQ(EvalJsString("window.events.join(',')"),
            "activation:slow,cancel:slow");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_Reset_Cancels) {
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
    </body>
  )");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_error = false;
  model_context->ExecuteTool(
      "search_tool", "{\"query\": \"testing\"}",
      base::BindLambdaForTesting(
          [&](base::expected<WebString, WebDocument::ScriptToolError> res) {
            got_error = true;
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(),
                      WebDocument::ScriptToolError::kToolCancelled);
            run_loop.Quit();
          }));

  // Run until the tool fills the form and focuses the button.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsBoolean(
        "document.activeElement === document.querySelector('button')");
  }));

  EXPECT_FALSE(got_error);
  EXPECT_TRUE(EvalJsBoolean(
      "document.querySelector('form').matches(':tool-form-active')"));

  // Reset the form
  MainFrame().ExecuteScript(
      WebScriptSource("document.querySelector('form').reset();"));

  run_loop.Run();

  EXPECT_TRUE(got_error);
  EXPECT_FALSE(EvalJsBoolean(
      "document.querySelector('form').matches(':tool-form-active')"));
}

}  // namespace blink
