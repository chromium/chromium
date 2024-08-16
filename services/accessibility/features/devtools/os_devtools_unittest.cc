// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/accessibility/features/v8_manager.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/inspector_protocol/crdtp/json.h"

namespace ax {

namespace {

crdtp::span<uint8_t> ToSpan(const std::string& string) {
  return crdtp::span<uint8_t>(reinterpret_cast<const uint8_t*>(string.data()),
                              string.size());
}

// Class that pretends to be a devtools session host so we can check our
// evaluations.
class FakeDevToolsSessionHost : public blink::mojom::DevToolsSessionHost {
 public:
  explicit FakeDevToolsSessionHost(
      base::RepeatingCallback<void(int)> expectation_complete_cb)
      : expectation_complete_cb_(std::move(expectation_complete_cb)) {}
  ~FakeDevToolsSessionHost() override {
    // Ensure all expectations have been checked.
    std::string remaining_expectations = "[";
    for (auto& it : expectations_) {
      remaining_expectations += " " + base::ToString(it.first) + " ";
    }

    remaining_expectations += "]";
    EXPECT_TRUE(expectations_.empty())
        << "Failed expectation call ids: " << remaining_expectations;
  }
  // blink::mojom::DevToolsSessionHost implementation.
  void DispatchProtocolResponse(
      blink::mojom::DevToolsMessagePtr message,
      int call_id,
      blink::mojom::DevToolsSessionStatePtr updates) override {
    // Convert message bytes to span.
    crdtp::span<uint8_t> message_span(message->data.data(),
                                      message->data.size());
    // Convert binary to json.
    std::string message_str;
    crdtp::json::ConvertCBORToJSON(message_span, &message_str);
    // Read json into object.
    auto json_parsed = base::JSONReader::Read(message_str);
    // This json should be a valid dict.
    EXPECT_TRUE(json_parsed.has_value() && json_parsed->is_dict());
    // See
    // https://chromedevtools.github.io/devtools-protocol/tot/Runtime/#type-RemoteObject
    // for details about json structure.
    std::string actual_type =
        *json_parsed->GetDict().FindStringByDottedPath("result.result.type");
    auto actual_value =
        json_parsed->GetDict().ExtractByDottedPath("result.result.value");
    RunExpectation(call_id, actual_type, std::move(actual_value));
  }

  void DispatchProtocolNotification(
      blink::mojom::DevToolsMessagePtr message,
      blink::mojom::DevToolsSessionStatePtr updates) override {}
  mojo::AssociatedReceiver<blink::mojom::DevToolsSessionHost> receiver{this};

  void ExpectEvalResult(int call_id,
                        std::string type,
                        std::optional<base::Value> value = std::nullopt) {
    expectations_[call_id] = (base::BindOnce(
        [](std::string expected_type, std::optional<base::Value> expected_value,
           std::string actual_type, std::optional<base::Value> actual_value) {
          EXPECT_EQ(expected_type, actual_type);
          EXPECT_EQ(expected_value.has_value(), actual_value.has_value());
          // If there are values to check.
          if (actual_value.has_value()) {
            EXPECT_EQ(expected_value->type(), actual_value->type());
            switch (actual_value->type()) {
              case base::Value::Type::BOOLEAN:
                EXPECT_EQ(expected_value->GetBool(), actual_value->GetBool());
                break;
              case base::Value::Type::STRING:
                EXPECT_EQ(expected_value->GetString(),
                          actual_value->GetString());
                break;
              case base::Value::Type::INTEGER:
                EXPECT_EQ(expected_value->GetInt(), actual_value->GetInt());
                break;
              case base::Value::Type::DOUBLE:
                EXPECT_EQ(expected_value->GetDouble(),
                          actual_value->GetDouble());
                break;
              default:
                // Non primitive values shouldn't be checked for in these tests.
                break;
            }
          }
        },
        type, std::move(value)));
  }

 private:
  void RunExpectation(int call_id,
                      std::string actual_type,
                      std::optional<base::Value> actual_value) {
    // Run expectation against the call id.
    if (auto it = expectations_.find(call_id); it != expectations_.end()) {
      std::move(it->second).Run(actual_type, std::move(actual_value));
    }
    expectations_.erase(call_id);
    expectation_complete_cb_.Run(expectations_.size());
  }

  std::map<int,
           base::OnceCallback<void(std::string, std::optional<base::Value>)>>
      expectations_;

  const base::RepeatingCallback<void(int remaining_expectations)>
      expectation_complete_cb_;
};

// Class that pretends to be an accessibility service for the sake of setting up
// the associated mojo pipes that devtools uses.
class FakeAccessibilityService : public mojom::AccessibilityService {
 public:
  FakeAccessibilityService() : v8_manager_(std::make_unique<V8Manager>()) {
    v8_manager_->FinishContextSetUp();
  }

  void BindAccessibilityServiceClient(
      mojo::PendingRemote<mojom::AccessibilityServiceClient>
          accessibility_client_remote) override {}

  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<mojom::AssistiveTechnologyController>
          at_at_controller_receiver,
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features)
      override {}

  void ConnectDevToolsAgent(
      ::mojo::PendingAssociatedReceiver<::blink::mojom::DevToolsAgent> agent,
      ::ax::mojom::AssistiveTechnologyType type) override {
    v8_manager_->ConnectDevToolsAgent(std::move(agent));
    connect_dev_tools_count_++;
  }

  int GetConnectionAttemptCount() { return connect_dev_tools_count_; }
  std::unique_ptr<V8Manager> v8_manager_;

 private:
  int connect_dev_tools_count_ = 0;
};
}  // namespace

// Unit test for setting up and interacting with ATP devtools.
class OSDevToolsTest : public testing::Test {
 public:
  OSDevToolsTest()
      : fake_session_host_(
            base::BindRepeating(&OSDevToolsTest::ExpectationComplete,
                                base::Unretained(this))) {}
  OSDevToolsTest(const OSDevToolsTest&) = delete;
  OSDevToolsTest& operator=(const OSDevToolsTest&) = delete;
  ~OSDevToolsTest() override = default;

  void SetUp() override {
    BindingsIsolateHolder::InitializeV8();

    // Set up the fake service.
    mojo::PendingRemote<mojom::AccessibilityService> service_remote_p;
    auto fake_service = std::make_unique<FakeAccessibilityService>();
    fake_service_ = fake_service.get();
    mojo::MakeSelfOwnedReceiver(
        std::move(fake_service),
        service_remote_p.InitWithNewPipeAndPassReceiver());
    service_remote_.Bind(std::move(service_remote_p));
    ConnectDevToolsAgent();
    AttachSession();
  }

  void EvalJS(int call_id, std::string script, bool use_io = false) {
    static constexpr char kCmdTemplate[] = R"JSON(
      {
        "id": %d,
        "method": "Runtime.evaluate",
        "params": {
          "expression": "%s",
          "contextId": 1
        }
      }
    )JSON";
    std::string to_eval =
        base::StringPrintf(kCmdTemplate, call_id, script.c_str());
    EvalCommand(call_id, "Runtime.evaluate", to_eval, use_io);
  }

  void EvalCommand(int call_id,
                   std::string command_name,
                   std::string command,
                   bool use_io = false) {
    base::span<const uint8_t> message;
    std::vector<uint8_t> cbor;
    // JSON -> CBOR.
    crdtp::Status status =
        crdtp::json::ConvertJSONToCBOR(ToSpan(command), &cbor);
    CHECK(status.ok()) << status.Message();
    message = base::span<const uint8_t>(cbor.data(), cbor.size());
    if (!use_io) {
      session_remote_->DispatchProtocolCommand(call_id, command_name, message);
    } else {
      io_session_remote_->DispatchProtocolCommand(call_id, command_name,
                                                  message);
    }
  }

  void ExpectEvalResult(int call_id,
                        std::string type,
                        std::optional<base::Value> value = std::nullopt) {
    fake_session_host_.ExpectEvalResult(call_id, type, std::move(value));
  }

  void OnExpectationsComplete(base::OnceClosure callback) {
    expectations_complete_callbacks_.push_back(std::move(callback));
  }

  void ConnectDevToolsAgent() {
    // Connect the agent.
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgent> agent_remote_p;
    service_remote_->ConnectDevToolsAgent(
        agent_remote_p.InitWithNewEndpointAndPassReceiver(),
        ax::mojom::AssistiveTechnologyType::kChromeVox);
    // Wait for connect devtools agent to be called.
    service_remote_.FlushForTesting();
    EXPECT_EQ(fake_service_->GetConnectionAttemptCount(), 1);
    agent_remote.Bind(std::move(agent_remote_p));
  }

  void AttachSession() {
    // Attach the session.
    agent_remote->AttachDevToolsSession(
        fake_session_host_.receiver.BindNewEndpointAndPassRemote(),
        session_remote_.BindNewEndpointAndPassReceiver(),
        io_session_remote_.BindNewPipeAndPassReceiver(),
        std::move(reattach_session_state_), /*client_expects_binary*/ true,
        /*client_is_trusted=*/true, /*session_id=*/"session",
        /*session_waits_for_debugger=*/false);
  }

  // The agent lives for as long as the manager does. It goes down when an AT is
  // disabled, which is accomplished by deleting the v8_manager.
  void DisconnectDevToolsAgent() { fake_service_->v8_manager_.reset(); }

 protected:
  mojo::Remote<mojom::AccessibilityService> service_remote_;
  // Associated remotes must be passed through an existing mojo connection.
  // Simulate the crossing the service boundary with a fake accessibility
  // service.
  raw_ptr<FakeAccessibilityService> fake_service_;
  // Session Host
  FakeDevToolsSessionHost fake_session_host_;
  // Session Remote
  mojo::AssociatedRemote<blink::mojom::DevToolsSession> session_remote_;
  // IO Session Remote
  mojo::Remote<blink::mojom::DevToolsSession> io_session_remote_;
  // Agent Remote
  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_remote;
  // Session State
  blink::mojom::DevToolsSessionStatePtr reattach_session_state_;

 private:
  void ExpectationComplete(int remaining_expecations) {
    if (remaining_expecations == 0) {
      for (auto& cb : expectations_complete_callbacks_) {
        std::move(cb).Run();
      }
      expectations_complete_callbacks_.clear();
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::vector<base::OnceClosure> expectations_complete_callbacks_;
};

TEST_F(OSDevToolsTest, ConnectAndEvalJS) {
  // Set expectations.
  ExpectEvalResult(1, "undefined");
  ExpectEvalResult(2, "undefined");
  ExpectEvalResult(3, "undefined");
  ExpectEvalResult(4, "number", base::Value(9));
  base::RunLoop expectation_waiter;
  OnExpectationsComplete(
      base::BindOnce([]() { LOG(INFO) << "Expectations complete."; }));
  OnExpectationsComplete(expectation_waiter.QuitClosure());
  // Test some evaluations.
  EvalJS(1, "atpconsole.log('Hello World!');");
  EvalJS(2, "console.log('Hello world!');");
  // Send a commands via IO.
  EvalJS(3, "const x = 9;", true);
  EvalJS(4, "x;", true);
  LOG(INFO) << "Waiting for expectations to complete...";
  expectation_waiter.Run();
  // Disconnect the session.
  session_remote_.reset();
  // Flush the service remote since it is the associated pipe and will send the
  // disconnect message.
  service_remote_.FlushForTesting();
  // disconnect the io session
  io_session_remote_.reset();
  // Post a task to the main runner to make sure IOSession is deleted before the
  // test ends.
  auto tr = base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop loop;
  tr->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();
  // Send the disconnect message to delete the io session.
  // Tear down the agent.
  DisconnectDevToolsAgent();
}

// This test checks some evaluations and ensures there are no crashes if the
// agent is deleted before the session.
TEST_F(OSDevToolsTest, DisableATWhileSessionConnected) {
  // Prepare expecations.
  ExpectEvalResult(1, "string", base::Value("Hello World!"));
  base::RunLoop expectation_waiter;
  OnExpectationsComplete(
      base::BindOnce([]() { LOG(INFO) << "Expectations complete."; }));
  OnExpectationsComplete(expectation_waiter.QuitClosure());
  // Test some evaluations.
  EvalJS(1, "'Hello' + ' World!'");
  // Wait for expecations to complete.
  LOG(INFO) << "Waiting for expectations to complete...";
  expectation_waiter.Run();
  // Tear down the agent without disconnecting the session first.
  DisconnectDevToolsAgent();
  // Disconnect the io session.
  io_session_remote_.reset();
  // Post a task to the main runner to make sure IOSession is deleted before the
  // test ends.
  auto tr = base::SequencedTaskRunner::GetCurrentDefault();
  base::RunLoop loop;
  tr->PostTask(FROM_HERE, loop.QuitClosure());
  loop.Run();
  // Flush the service remote since it is the associated pipe and will send the
  // disconnect message for agent.
  service_remote_.FlushForTesting();
}

}  // namespace ax
