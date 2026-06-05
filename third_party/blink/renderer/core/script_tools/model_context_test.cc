// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context.h"

#include <optional>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "third_party/blink/public/mojom/content_extraction/script_tools.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_default_style_sheets.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"
#include "third_party/blink/renderer/core/script_tools/script_tool_types.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockScriptToolHost : public mojom::blink::ScriptToolHost {
 public:
  explicit MockScriptToolHost() = default;

  void Bind(mojo::ScopedMessagePipeHandle pipe) {
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::ScriptToolHost>(std::move(pipe)));
  }

  void PauseExecution() override {
    pause_called_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  bool pause_called() const { return pause_called_; }
  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

 private:
  mojo::Receiver<mojom::blink::ScriptToolHost> receiver_{this};
  bool pause_called_ = false;
  base::RunLoop* run_loop_ = nullptr;
};

class MockModelContextHost : public mojom::blink::ModelContextHost {
 public:
  explicit MockModelContextHost() = default;

  void Bind(mojo::ScopedMessagePipeHandle pipe) {
    receiver_.Bind(
        mojo::PendingReceiver<mojom::blink::ModelContextHost>(std::move(pipe)));
  }

  void BindModelContext(
      mojo::PendingRemote<mojom::blink::ModelContext> model_context) override {
    model_context_.Bind(std::move(model_context));
  }

  void RegisterScriptTool(mojom::blink::ScriptToolPtr tool) override {
    registered_tools_.push_back(tool->name);
    if (model_context_.is_bound()) {
      model_context_->NotifyToolChange();
    }
  }

  void UnregisterScriptTool(const String& name) override {
    registered_tools_.erase(
        std::remove(registered_tools_.begin(), registered_tools_.end(), name),
        registered_tools_.end());
    if (model_context_.is_bound()) {
      model_context_->NotifyToolChange();
    }
  }

  void GetScriptTools(
      const Vector<scoped_refptr<const SecurityOrigin>>& from_origins,
      GetScriptToolsCallback callback) override {
    std::move(callback).Run({});
  }

  void ExecuteRemoteScriptTool(
      const ::blink::FrameToken& tool_owner_frame_token,
      const ::scoped_refptr<const ::blink::SecurityOrigin>&
          expected_target_origin,
      const ::blink::String& name,
      const ::blink::String& input_arguments,
      ExecuteRemoteScriptToolCallback callback) override {
    std::move(callback).Run(String(), false);
  }

  const Vector<String>& registered_tools() const { return registered_tools_; }

 private:
  mojo::Receiver<mojom::blink::ModelContextHost> receiver_{this};
  mojo::Remote<mojom::blink::ModelContext> model_context_;
  Vector<String> registered_tools_;
};

class ModelContextTestBase : public SimTest {
 public:
  bool EvalJsBoolean(const char* script) {
    return MainFrame()
        .ExecuteScriptAndReturnValue(
            WebScriptSource(WebString::FromUtf8(script)))
        .As<v8::Boolean>()
        ->Value();
  }

  String EvalJsString(const char* script) {
    return ToCoreStringWithUndefinedOrNullCheck(
        Window().GetIsolate(),
        MainFrame().ExecuteScriptAndReturnValue(
            WebScriptSource(WebString::FromUtf8(script))));
  }

  int EvalJsInteger(const char* script) {
    return MainFrame()
        .ExecuteScriptAndReturnValue(
            WebScriptSource(WebString::FromUtf8(script)))
        .As<v8::Integer>()
        ->Value();
  }

 protected:
  void SetUp() override {
    SimTest::SetUp();
    // In SimTest, web security is disabled and universal/file access from file
    // URLs is enabled by default. This forces documents to always use a
    // universal non-origin-keyed agent, bypassing the mock navigation agent
    // cluster key. We adjust these settings here to ensure the mock browser
    // navigation's agent cluster key is respected.
    GetDocument().GetSettings()->SetWebSecurityEnabled(true);
    GetDocument().GetSettings()->SetAllowUniversalAccessFromFileURLs(false);
    GetDocument().GetSettings()->SetAllowFileAccessFromFileURLs(false);
    GetDocument()
        .GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(
            mojom::blink::ModelContextHost::Name_,
            base::BindRepeating(&MockModelContextHost::Bind,
                                base::Unretained(&mock_model_context_host_)));
  }

  void TearDown() override {
    GetDocument()
        .GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(mojom::blink::ModelContextHost::Name_,
                             base::NullCallback());
    SimTest::TearDown();
  }

  MockModelContextHost mock_model_context_host_;
};

class ModelContextTest : public ModelContextTestBase {
 public:
  ModelContextTest() = default;

 private:
  ScopedWebMCPForTest scoped_webmcp_{true};
  ScopedWebMCPTestingForTest scoped_webmcp_testing_{true};
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
  )");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  String result;

  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "echo", "{\"text\": \"hello\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ASSERT_TRUE(res.has_value());
            result = *res;
            run_loop.Quit();
          }));

  run_loop.Run();

  EXPECT_EQ(result, "hello");
}

TEST_F(ModelContextTest, GetTools_InsecureOrigin) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));

  main_resource.Complete(R"(
    <body>
    <script>
      window.test_error = "";
      navigator.modelContext.getTools({ fromOrigins: ["http://insecure.com"] })
        .then(() => { window.test_error = "Success"; })
        .catch(err => { window.test_error = err.name; });
    </script>
  )");
  test::RunPendingTasks();

  EXPECT_EQ(EvalJsString("window.test_error"), "SecurityError");
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
        readOnlyHint: true,
        untrustedContentHint: true
      },
    });
    </script>
  )");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  String result;

  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "echo", "{\"text\": \"hello\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type="text" name="query">
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type="text" name="query">
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  // Test with a field that doesn't exist in the form
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"nonexistent\": \"value\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kInvalidInputArguments);
            EXPECT_EQ(res.error().message,
                      "Input contains a parameter \"nonexistent\" but there is "
                      "no such parameter for the tool");
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_InvalidSelectValue) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(
    <form toolautosubmit toolname="select_tool" tooldescription="Select an option">
      <select name="choice">
        <option value="a">A</option>
        <option value="b">B</option>
      </select>
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "select_tool", "{\"choice\": \"c\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kInvalidInputArguments);
            EXPECT_EQ(res.error().message,
                      "Invalid value \"c\" for parameter choice");
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolInvocationFailed);
            EXPECT_EQ(res.error().message, "respondWith promise was rejected");
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(got_result);
  EXPECT_TRUE(EvalJsBoolean("window.submit_event_fired"));
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_NoRespondWith) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query>
      <button type=submit>Submit</button>
    </form>
    <script>
      document.querySelector("form").addEventListener("submit", e => {
        window.submit_event_fired = true;
        e.preventDefault();
      });
    </script>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolInvocationFailed);
            EXPECT_EQ(res.error().message,
                      "The site has a programming error: it called "
                      "preventDefault() on the 'submit' event, without also "
                      "calling respondWith() with the tool result");
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(got_result);
  EXPECT_TRUE(EvalJsBoolean("window.submit_event_fired"));
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_ValidationFailure) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query pattern="[a-z]+">
      <button type=submit>Submit</button>
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool", "{\"query\": \"123\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            EXPECT_FALSE(res.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(got_result);
}

class MockFormValidationMessageClient
    : public GarbageCollected<MockFormValidationMessageClient>,
      public ValidationMessageClient {
 public:
  void ShowValidationMessage(Element& anchor,
                             const String&,
                             TextDirection,
                             const String&,
                             TextDirection) override {
    anchor_ = anchor;
  }

  void HideValidationMessage(const Element& anchor) override {
    if (anchor_ == &anchor) {
      anchor_ = nullptr;
    }
  }

  bool IsValidationMessageVisible(const Element& anchor) override {
    return anchor_ == &anchor;
  }

  void DocumentDetached(const Document&) override {}
  void DidChangeFocusTo(const Element*) override {}
  void WillBeDestroyed() override {}
  void Trace(Visitor* visitor) const override {
    visitor->Trace(anchor_);
    ValidationMessageClient::Trace(visitor);
  }

 private:
  Member<const Element> anchor_;
};

class ModelContextValidationTest : public ModelContextTest {
 public:
  void SetUp() override {
    ModelContextTest::SetUp();
    mock_client_ = MakeGarbageCollected<MockFormValidationMessageClient>();
    original_client_ =
        &Window().GetFrame()->GetPage()->GetValidationMessageClient();
    Window().GetFrame()->GetPage()->SetValidationMessageClientForTesting(
        mock_client_);
  }

  void TearDown() override {
    Window().GetFrame()->GetPage()->SetValidationMessageClientForTesting(
        original_client_);
    ModelContextTest::TearDown();
  }

 private:
  Persistent<MockFormValidationMessageClient> mock_client_;
  Persistent<ValidationMessageClient> original_client_;
};

TEST_F(ModelContextValidationTest,
       ExecuteDeclarativeFormTool_ValidationFailureDetails) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolautosubmit toolname="validation_tool" tooldescription="Validation">
      <input id=mytext type=text name=text_required required>
      <input id=mynumber type=number name=number_min min=10>
      <button type=submit>Submit</button>
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "validation_tool",
      "{\"text_required\": \"\", \"number_min\": 5}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolInvocationFailed);
            EXPECT_EQ(
                res.error().message,
                "Form validation failed: text_required: "
                "<<ValidationValueMissing>>. number_min: range underflow. ");
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(got_result);
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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

class ModelContextOriginTrialTest : public ModelContextTestBase {
 public:
  ModelContextOriginTrialTest() = default;

 private:
  ScopedWebMCPForTest scoped_webmcp_{false};
  ScopedWebMCPTestingForTest scoped_webmcp_testing_{false};
  ScopedWebMCPDeclarativeFileInputForTest scoped_webmcp_file_feature_{false};
  ScopedWebMCPFormAssociatedCustomElementsForTest scoped_webmcp_face_feature_{
      false};
};

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_SPA_NoAutoSubmit) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
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
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
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

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_FormPopulatedAtEvent) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query value="initial">
    </form>
    <script>
      window.event_fired = false;
      window.input_value_at_event = "";
      window.addEventListener('toolactivated', e => {
        if (e.toolName === 'search_tool') {
          window.event_fired = true;
          window.input_value_at_event = document.querySelector('input').value;
        }
      });
      document.querySelector('form').addEventListener('submit', e => {
        e.preventDefault();
        e.respondWith(Promise.resolve("done"));
      });
    </script>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            EXPECT_TRUE(res.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(EvalJsBoolean("window.event_fired"));
  EXPECT_EQ(EvalJsString("window.input_value_at_event"), "testing");
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_PauseExecution) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(
    <form toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type="text" name="query">
      <button type="submit">Submit</button>
    </form>
  )");
  test::RunPendingTasks();

  MockScriptToolHost mock_host;
  GetDocument()
      .GetExecutionContext()
      ->GetBrowserInterfaceBroker()
      .SetBinderForTesting(mojom::blink::ScriptToolHost::Name_,
                           base::BindRepeating(&MockScriptToolHost::Bind,
                                               base::Unretained(&mock_host)));

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  mock_host.set_run_loop(&run_loop);

  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ADD_FAILURE() << "Callback should not be called";
          }));

  // Run until PauseExecution is called.
  run_loop.Run();

  EXPECT_TRUE(mock_host.pause_called());

  // Clean up the binder.
  GetDocument()
      .GetExecutionContext()
      ->GetBrowserInterfaceBroker()
      .SetBinderForTesting(mojom::blink::ScriptToolHost::Name_,
                           base::NullCallback());
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
)");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;

  base::UnguessableToken invocation_id = base::UnguessableToken::Create();
  bool success = model_context->ExecuteTool(
      invocation_id, "echo", "{\"text\": \"hello\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ASSERT_FALSE(res.has_value());
            run_loop.Quit();
          }));

  ASSERT_TRUE(success);
  model_context->CancelTool(invocation_id);
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
    window.addEventListener('toolactivated', e => window.events.push('activated:' + e.toolName));
    window.addEventListener('toolcancel', e => window.events.push('cancel:' + e.toolName));
  </script>
)");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;

  // Execute and Cancel
  base::UnguessableToken invocation_id = base::UnguessableToken::Create();
  bool success = model_context->ExecuteTool(
      invocation_id, "slow", "{}", /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            run_loop.Quit();
          }));

  EXPECT_EQ(EvalJsString("window.events.join(',')"), "activated:slow");

  ASSERT_TRUE(success);
  model_context->CancelTool(invocation_id);
  run_loop.Run();

  EXPECT_EQ(EvalJsString("window.events.join(',')"),
            "activated:slow,cancel:slow");
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
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_error = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_error = true;
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolCancelled);
            EXPECT_EQ(res.error().message,
                      "Tool execution cancelled by a form reset");
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

TEST_F(ModelContextTest, ToolSignalAborted) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState* script_state = ToScriptStateForMainWorld(Window().GetFrame());
  ScriptState::Scope script_scope(script_state);
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
  </script>
  </body>
)");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;

  auto* controller = AbortController::Create(script_state);
  controller->abort(script_state);

  // Execute with an aborted signal.
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "slow", "{}", controller->signal(),
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ASSERT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolInvocationFailed);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_FlexibleTypes) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"HTML(
    <form toolautosubmit toolname="flexible_tool" tooldescription="Flexible types tool">
      <input id=text name=text type=text>
      <input id=number name=number type=number>
      <input id=check1 name=check1 type=checkbox>
      <select id=select name=select>
        <option value="1">One</option>
        <option value="2">Two</option>
        <option value="3">Three</option>
      </select>
    </form>
    <script>
      document.querySelector('form').addEventListener('submit', e => {
        window.text_val = document.querySelector('#text').value;
        window.number_val = document.querySelector('#number').value;
        window.check_val = document.querySelector('#check1').checked;
        window.select_val = document.querySelector('#select').value;
        e.preventDefault();
        e.respondWith(Promise.resolve("done"));
      });
    </script>
  )HTML");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  String json_string = R"JSON(
    {
      "text": 123,
      "number": "456",
      "check1": 1,
      "select": 2
    }
  )JSON";

  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "flexible_tool", json_string,
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ASSERT_TRUE(res.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(EvalJsString("window.text_val"), "123");
  EXPECT_EQ(EvalJsString("window.number_val"), "456");
  EXPECT_TRUE(EvalJsBoolean("window.check_val"));
  EXPECT_EQ(EvalJsString("window.select_val"), "2");

  // Test with boolean strings and numeric strings for select
  base::RunLoop run_loop2;
  json_string = R"JSON(
    {
      "check1": "false",
      "select": "3"
    }
  )JSON";

  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "flexible_tool", json_string,
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            ASSERT_TRUE(res.has_value());
            run_loop2.Quit();
          }));
  run_loop2.Run();

  EXPECT_FALSE(EvalJsBoolean("window.check_val"));
  EXPECT_EQ(EvalJsString("window.select_val"), "3");
}

class ReentrantListener : public NativeEventListener {
 public:
  explicit ReentrantListener(ModelContext* model_context)
      : model_context_(model_context) {}
  void Invoke(ExecutionContext*, Event*) override {
    // Trigger HashMap modification by adding a new execution.
    model_context_->ExecuteTool(base::UnguessableToken::Create(), "echo", "{}",
                                nullptr, base::DoNothing());
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(model_context_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<ModelContext> model_context_;
};

TEST_F(ModelContextTest, CancelToolReentrancy) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));

  main_resource.Complete(R"(
    <body>
    <script>
    async function hang(obj) {
      return new Promise(() => {});
    }

    navigator.modelContext.registerTool({
      execute: hang,
      name: "hang",
      description: "never resolves",
    });

    // We also need another tool that can be executed.
    navigator.modelContext.registerTool({
      execute: async () => "done",
      name: "echo",
      description: "echo",
    });
  </script>
)");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  Window().addEventListener(
      event_type_names::kToolcancel,
      MakeGarbageCollected<ReentrantListener>(model_context), false);

  base::RunLoop run_loop;

  base::UnguessableToken invocation_id = base::UnguessableToken::Create();
  bool success = model_context->ExecuteTool(
      invocation_id, "hang", "{}", /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolCancelled);
            run_loop.Quit();
          }));

  ASSERT_TRUE(success);

  // This should trigger the toolcancel event, which re-enters and modifies
  // pending_executions_.
  model_context->CancelTool(invocation_id);

  run_loop.Run();
}

class MockDeclarativeTool : public GarbageCollected<MockDeclarativeTool>,
                            public DeclarativeWebMCPTool {
 public:
  void ExecuteTool(
      const base::UnguessableToken& invocation_id,
      String input_arguments,
      base::OnceCallback<void(base::expected<String, ScriptToolError>)>
          done_callback) override {}

  String ToolName() const override { return "test_tool"; }
  String ToolDescription() const override { return "description"; }
  String ToolTitle() const override { return "title"; }
  String ComputeInputSchema() override { return "{}"; }
  Element* FormElement() const override { return nullptr; }
  void Trace(Visitor* visitor) const override {}
};

TEST_F(ModelContextTest, ForEachScriptToolGC) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<body></body>");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  {
    auto* mock_tool = MakeGarbageCollected<MockDeclarativeTool>();
    model_context->RegisterDeclarativeTool(mock_tool);
  }

  // Trigger GC, which should not reclaim mock_tool.
  ThreadState::Current()->CollectAllGarbageForTesting();

  // This should not crash and should find the tool.
  bool found = false;
  model_context->ForEachScriptTool([&](const mojom::blink::ScriptTool& tool) {
    if (tool.name == "test_tool") {
      found = true;
    }
  });
  EXPECT_TRUE(found);

  // Now unregister it.
  model_context->UnregisterTool("test_tool");

  // Trigger GC again. Now it should be reclaimed.
  ThreadState::Current()->CollectAllGarbageForTesting();

  found = false;
  model_context->ForEachScriptTool([&](const mojom::blink::ScriptTool& tool) {
    if (tool.name == "test_tool") {
      found = true;
    }
  });
  EXPECT_FALSE(found);
}

TEST_F(ModelContextTest, ListTools) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(<!DOCTYPE html>
    <script>
    navigator.modelContext.registerTool({
      execute: () => "true",
      name: "delete",
      description: "Delete everything",
    });
    navigator.modelContext.registerTool({
      execute: () => "true",
      name: "squash",
      description: "Squash history",
    });
    navigator.modelContext.registerTool({
      execute: () => "true",
      name: "append",
      description: "Append something",
    });
    </script>
  )");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  HeapVector<Member<const ToolData>> tools = model_context->ListTools();
  ASSERT_EQ(3u, tools.size());

  EXPECT_EQ("append", tools[0]->Name());
  EXPECT_EQ("delete", tools[1]->Name());
  EXPECT_EQ("squash", tools[2]->Name());
}

TEST_F(ModelContextTest, SourceLocation) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(<!DOCTYPE html>
    <script>
    navigator.modelContext.registerTool({
      execute: () => "true",
      name: "append",
      description: "Append something",
    });
    navigator.modelContext.registerTool({
      execute: () => "true",
      name: "delete",
      description: "Delete everything",
    });
    </script>
  )");

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  HeapVector<Member<const ToolData>> tools = model_context->ListTools();
  ASSERT_EQ(2u, tools.size());

  EXPECT_EQ("append", tools[0]->Name());
  ASSERT_TRUE(tools[0]->GetSourceLocation());
  EXPECT_EQ(3u, tools[0]->GetSourceLocation()->LineNumber());

  EXPECT_EQ("delete", tools[1]->Name());
  ASSERT_TRUE(tools[1]->GetSourceLocation());
  EXPECT_EQ(8u, tools[1]->GetSourceLocation()->LineNumber());
}

TEST_F(ModelContextTest, BackingFormElement) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(<!DOCTYPE html>
    <form
      id=book-table
      toolname=book-table
      tooldescription="Book a table">
    </form>
    <form
      id=leave-feedback
      toolname=leave-feedback
      tooldescription="leave-feedback">
    </form>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  HeapVector<Member<const ToolData>> tools = model_context->ListTools();
  ASSERT_EQ(2u, tools.size());

  EXPECT_EQ("book-table", tools[0]->Name());
  ASSERT_TRUE(tools[0]->BackingFormElement());
  EXPECT_EQ("book-table", tools[0]->BackingFormElement()->GetIdAttribute());

  EXPECT_EQ("leave-feedback", tools[1]->Name());
  ASSERT_TRUE(tools[1]->BackingFormElement());
  EXPECT_EQ("leave-feedback", tools[1]->BackingFormElement()->GetIdAttribute());
}

class ModelContextMetricsTest : public SimTest {
 public:
  ModelContextMetricsTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    SimTest::SetUp();
    GetDocument().GetSettings()->SetWebSecurityEnabled(true);
    GetDocument().GetSettings()->SetAllowUniversalAccessFromFileURLs(false);
    GetDocument().GetSettings()->SetAllowFileAccessFromFileURLs(false);
  }

  void EvalJsString(std::string_view script) {
    MainFrame().ExecuteScript(WebScriptSource(WebString::FromUtf8(script)));
  }

 private:
  ScopedWebMCPForTest scoped_webmcp_{true};
};

TEST_F(ModelContextMetricsTest, RecordToolCountHistogram) {
  base::HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  main_resource.Complete(R"(<!DOCTYPE html>
    <form
      id=book-table
      toolname=book-table
      tooldescription="Book a table">
    </form>
    <form
      id=leave-feedback
      toolname=leave-feedback
      tooldescription="leave-feedback">
    </form>
  )");
  test::RunPendingTasks();

  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));

  task_environment().FastForwardBy(base::Seconds(2));
  EvalJsString(R"JS(navigator.modelContext.registerTool({
      execute: () => "true",
      name: "tool1",
      description: "Tool1",
    }))JS");

  task_environment().FastForwardBy(base::Seconds(2));
  for (int i = 2; i <= 4; i++) {
    // clang-format off
    EvalJsString(String::Format(R"JS(navigator.modelContext.registerTool({
      execute: () => "true",
      name: "tool%d",
      description: "Tool%d",
    }))JS", i, i).Utf8());
    // clang-format on
  }

  task_environment().FastForwardBy(base::Seconds(8));
  histogram_tester.ExpectUniqueSample("Blink.ModelContext.DelayedToolCount", 6,
                                      1);
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_ToolChangeOnNameChange) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form toolname="my_tool" tooldescription="desc">
        <input id="input1" type="text" name="input_name">
        <input id="input2" type="checkbox" name="checkbox_name">
        <select id="select1" name="select_name">
          <option value="a">A</option>
          <option value="b">B</option>
        </select>
        <textarea id="textarea1" name="textarea_name"></textarea>
      </form>
      <script>
        window.toolchangeCount = 0;
        navigator.modelContextTesting.addEventListener('toolchange', () => {
          window.toolchangeCount++;
        });

        window.testMutations = [
          // input1 (text)
          { id: 'input1', script: "el.setAttribute('name', 'new_input_name');" },
          { id: 'input1', script: "el.type = 'number';" },
          { id: 'input1', script: "el.required = true;" },
          { id: 'input1', script: "el.setAttribute('toolparamdescription', 'new desc');" },

          // input2 (checkbox)
          { id: 'input2', script: "el.setAttribute('name', 'new_checkbox_name');" },
          { id: 'input2', script: "el.type = 'radio';" },
          { id: 'input2', script: "el.required = true;" },
          { id: 'input2', script: "el.setAttribute('toolparamdescription', 'new desc');" },

          // select1
          { id: 'select1', script: "el.setAttribute('name', 'new_select_name');" },
          { id: 'select1', script: "el.multiple = true;" },
          { id: 'select1', script: "el.required = true;" },
          { id: 'select1', script: "el.setAttribute('toolparamdescription', 'new desc');" },

          // textarea1
          { id: 'textarea1', script: "el.setAttribute('name', 'new_textarea_name');" },
          { id: 'textarea1', script: "el.required = true;" },
          { id: 'textarea1', script: "el.setAttribute('toolparamdescription', 'new desc');" }
        ];
      </script>
    </body>
  )");
  test::RunPendingTasks();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsInteger("window.toolchangeCount") == 1;
  })) << "Failed on initial tool registration";

  int mutation_count = EvalJsInteger("window.testMutations.length");
  for (int i = 0; i < mutation_count; ++i) {
    String script = String::Format(
        "window.toolchangeCount = 0;"
        "var m = window.testMutations[%d];"
        "var el = document.getElementById(m.id);"
        "eval(m.script);",
        i);
    MainFrame().ExecuteScript(WebScriptSource(WebString(script)));

    EXPECT_TRUE(base::test::RunUntil([&]() {
      return EvalJsInteger("window.toolchangeCount") == 2;
    })) << "Failed on mutation "
        << i << ": "
        << EvalJsString(String::Format("window.testMutations[%d].script", i)
                            .Utf8()
                            .c_str());
  }
}

TEST_F(ModelContextTest,
       ExecuteDeclarativeFormTool_ToolChangeOnControlAddRemove) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form id="f1" toolname="my_tool" tooldescription="desc">
      <input id="input1" type="text" name="input_name">
    </form>
    <script>
      window.toolchangeCount = 0;
      navigator.modelContextTesting.addEventListener('toolchange', () => {
        window.toolchangeCount++;
      });
    </script>
  )");
  blink::test::RunPendingTasks();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsInteger("window.toolchangeCount") == 1;
  })) << "Failed on initial tool registration";

  // Test adding an input
  MainFrame().ExecuteScript(
      WebScriptSource("window.toolchangeCount = 0;"
                      "var i = document.createElement('input');"
                      "i.type = 'text';"
                      "i.name = 'new_input';"
                      "i.id = 'new_input';"
                      "document.getElementById('f1').appendChild(i);"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsInteger("window.toolchangeCount") == 2;
  })) << "Failed on adding input";

  // Test removing an input
  MainFrame().ExecuteScript(
      WebScriptSource("window.toolchangeCount = 0;"
                      "document.getElementById('f1').removeChild(document."
                      "getElementById('new_input'));"));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsInteger("window.toolchangeCount") == 2;
  })) << "Failed on removing input";
}

TEST_F(ModelContextTest,
       ExecuteDeclarativeFormTool_NoToolChangeOnUnrelatedAttribute) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolname="my_tool" tooldescription="desc">
      <input id="input1" type="text" name="input_name">
    </form>
  )");
  blink::test::RunPendingTasks();

  MainFrame().ExecuteScript(WebScriptSource(
      "window.toolchangeCount = 0;"
      "navigator.modelContextTesting.addEventListener('toolchange', () => {"
      "  window.toolchangeCount++;"
      "});"
      "document.getElementById('input1').setAttribute('data-unrelated', "
      "'value');"));

  blink::test::RunPendingTasks();
  EXPECT_EQ(0, EvalJsInteger("window.toolchangeCount"));
}

TEST_F(ModelContextTest,
       ExecuteDeclarativeFormTool_NoToolChangeOnRedundantSchemaAttribute) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <body>
      <form id="f1" toolname="my_tool" tooldescription="desc">
        <input id="input1" type="text" name="input_name">
      </form>
    </body>
  )");
  blink::test::RunPendingTasks();

  MainFrame().ExecuteScript(WebScriptSource(
      "window.toolchangeCount = 0;"
      "navigator.modelContextTesting.addEventListener('toolchange', () => {"
      "  window.toolchangeCount++;"
      "});"
      "const input = document.getElementById('input1');"
      "// Remove and immediately re-add input, which shouldn't change the tool:"
      "input.remove();"
      "document.getElementById('f1').appendChild(input);"));
  blink::test::RunPendingTasks();
  EXPECT_EQ(0, EvalJsInteger("window.toolchangeCount"));
}

TEST_F(ModelContextTest, ExecuteTool_RespondWith_And_RemoveForm) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"(
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query>
      <button type=submit>Submit</button>
    </form>
    <script>
      document.querySelector("form").addEventListener("submit", e => {
        e.preventDefault();
        e.respondWith(Promise.resolve("result value"));
        document.querySelector("form").remove();
      });
    </script>
  )");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            EXPECT_TRUE(res.has_value());
            if (res.has_value()) {
              EXPECT_EQ(*res, "result value");
            }
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(got_result);
}

TEST_F(ModelContextTest, ExecuteTool_RespondWith_And_Navigate) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest search_resource("https://example.com/search", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"HTML(
    <form toolautosubmit toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query>
      <button type=submit>Submit</button>
    </form>
    <script>
      document.querySelector("form").addEventListener("submit", e => {
        e.preventDefault();
        e.respondWith(Promise.resolve("result value"));
        window.location.href = "/search";
      });
    </script>
  )HTML");
  test::RunPendingTasks();

  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);

  base::RunLoop run_loop;
  bool got_result = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_result = true;
            EXPECT_TRUE(res.has_value());
            if (res.has_value()) {
              EXPECT_EQ(*res, "result value");
            }
            run_loop.Quit();
          }));
  run_loop.Run();

  search_resource.Complete("search page");

  EXPECT_TRUE(got_result);
}

TEST_F(ModelContextTest, ExecuteDeclarativeFormTool_UnrelatedSubmitAndRemove) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest search_resource("https://example.com/search", "text/html");
  LoadURL("https://example.com/");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));
  main_resource.Complete(R"HTML(
    <form toolname="search_tool" tooldescription="Search the web" action="/search">
      <input type=text name=query>
      <button type=submit>Submit</button>
    </form>
    <script>
      function doUnrelatedSubmit() {
        const form = document.querySelector('form');
        form.addEventListener('submit', e => {
          e.preventDefault();
          e.respondWith(Promise.resolve("dummy"));
        });
        form.requestSubmit();
        form.remove();
      }
    </script>
  )HTML");
  test::RunPendingTasks();
  auto* model_context = ModelContextSupplement::modelContext(GetDocument());
  ASSERT_TRUE(model_context);
  base::RunLoop run_loop;
  bool got_callback = false;
  model_context->ExecuteTool(
      base::UnguessableToken::Create(), "search_tool",
      "{\"query\": \"testing\"}",
      /* signal= */ nullptr,
      base::BindLambdaForTesting(
          [&](base::expected<String, ScriptToolError> res) {
            got_callback = true;
            EXPECT_FALSE(res.has_value());
            EXPECT_EQ(res.error(), ScriptToolErrorCode::kToolCancelled);
            run_loop.Quit();
          }));

  // Run until the tool populates the form and is waiting for the user.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return EvalJsBoolean(
        "document.querySelector('form').matches(':tool-form-active')");
  }));

  // Trigger the unrelated submit and remove.
  MainFrame().ExecuteScript(WebScriptSource(WebString("doUnrelatedSubmit()")));

  run_loop.Run();
  EXPECT_TRUE(got_callback);
}

TEST_F(ModelContextTest, fileURLAllowedWithoutOriginKeying) {
  SimRequest main_resource("file:///tmp/test.html", "text/html");
  LoadURL("file:///tmp/test.html");
  v8::HandleScope handle_scope(Window().GetIsolate());
  ScriptState::Scope script_scope(
      ToScriptStateForMainWorld(Window().GetFrame()));

  main_resource.Complete(R"HTML(
    <body>
    <script>
    window.registerToolError = null;
    window.registerToolMessage = null;
    try {
      document.modelContext.registerTool({
        name: "test_tool",
        description: "a test tool",
        execute: () => "success",
      });
    } catch (e) {
      window.registerToolError = e.name;
      window.registerToolMessage = e.message;
    }
    </script>
    </body>
  )HTML");
  test::RunPendingTasks();

  EXPECT_TRUE(EvalJsBoolean("window.registerToolError === null"));
  EXPECT_TRUE(EvalJsBoolean("window.registerToolMessage === null"));
}

TEST_F(ModelContextTest, fileURLAllowedForDeclarativeTool) {
  SimRequest main_resource("file:///tmp/test.html", "text/html");
  LoadURL("file:///tmp/test.html");
  main_resource.Complete("<body></body>");

  auto* model_context =
      ModelContextSupplement::modelContext(*Window().navigator());
  ASSERT_TRUE(model_context);

  auto* mock_tool = MakeGarbageCollected<MockDeclarativeTool>();
  model_context->RegisterDeclarativeTool(mock_tool);

  HeapVector<Member<const ToolData>> tools = model_context->ListTools();
  ASSERT_EQ(1u, tools.size());
  EXPECT_EQ("test_tool", tools[0]->Name());
}

}  // namespace blink
