// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_loader.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-wasm.h"

using testing::ElementsAre;
using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

const char kValidScript[] = "function foo() {}";
const char kInvalidScript[] = "Invalid Script";

// The bytes of a minimal WebAssembly module, courtesy of
// v8/test/cctest/test-api-wasm.cc
const char kMinimalWasmModuleBytes[] = {0x00, 0x61, 0x73, 0x6d,
                                        0x01, 0x00, 0x00, 0x00};

// None of these tests make sure the right script is compiled, these tests
// merely check success/failure of trying to load a worklet.
class WorkletLoaderTest : public testing::Test {
 public:
  WorkletLoaderTest() {
    v8_helpers_.push_back(
        AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
    debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());
  }

  ~WorkletLoaderTest() override { task_environment_.RunUntilIdle(); }

  void LoadWorkletCallback(std::vector<WorkletLoaderBase::Result> results,
                           std::optional<std::string> error_msg) {
    results_ = std::move(results);
    error_msg_ = std::move(error_msg);

    for (size_t i = 0; i < results_.size(); ++i) {
      EXPECT_EQ(results_[i].success(), !error_msg_.has_value());
    }

    run_loop_.Quit();
  }

  std::string last_error_msg() const {
    return error_msg_.value_or("Not an error");
  }

  void RunOnV8ThreadAndWait(base::OnceClosure closure,
                            scoped_refptr<AuctionV8Helper> v8_helper) {
    base::RunLoop run_loop;
    v8_helper->v8_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::OnceClosure run, base::OnceClosure done) {
                         std::move(run).Run();
                         std::move(done).Run();
                       },
                       std::move(closure), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void RunOnV8ThreadAndWait(base::OnceClosure closure) {
    CHECK_EQ(v8_helpers_.size(), 1u);

    RunOnV8ThreadAndWait(std::move(closure), v8_helpers_[0]);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  TestAuctionNetworkEventsHandler auction_network_events_handler_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers_;
  std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids_;
  GURL url_ = GURL("https://foo.test/");
  base::RunLoop run_loop_;
  std::vector<WorkletLoaderBase::Result> results_;
  std::optional<std::string> error_msg_;
};

TEST_F(WorkletLoaderTest, NetworkError) {
  // Make this look like a valid response in all ways except the response code.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, std::nullopt,
              kValidScript, kAllowFledgeHeader, net::HTTP_NOT_FOUND);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_EQ("Failed to load https://foo.test/ HTTP status = 404 Not Found.",
            last_error_msg());
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_NetworkError) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  // Make this look like a valid response in all ways except the response code.
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType, std::nullopt,
              kValidScript, kAllowFledgeHeader, net::HTTP_NOT_FOUND);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_FALSE(results_[1].success());
  EXPECT_EQ("Failed to load https://foo.test/ HTTP status = 404 Not Found.",
            last_error_msg());
}

TEST_F(WorkletLoaderTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(last_error_msg(), HasSubstr("SyntaxError"));
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_CompileError) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_FALSE(results_[1].success());
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/:1 "));
  EXPECT_THAT(last_error_msg(), HasSubstr("SyntaxError"));
}

TEST_F(WorkletLoaderTest, CompileErrorWithDebugger) {
  ScopedInspectorSupport inspector_support(v8_helpers_[0].get());
  debug_ids_[0] =
      base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helpers_[0].get());
  TestChannel* channel = inspector_support.ConnectDebuggerSession(
      debug_ids_[0]->context_group_id());
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/mojo::NullRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].success());
  channel->WaitForMethodNotification("Debugger.scriptFailedToParse");

  debug_ids_[0]->AbortDebuggerPauses();
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_CompileErrorWithDebugger) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  ScopedInspectorSupport inspector_support(v8_helpers_[1].get());
  debug_ids_[1] =
      base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helpers_[1].get());
  TestChannel* channel = inspector_support.ConnectDebuggerSession(
      debug_ids_[1]->context_group_id());
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/mojo::NullRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_FALSE(results_[1].success());
  channel->WaitForMethodNotification("Debugger.scriptFailedToParse");

  debug_ids_[1]->AbortDebuggerPauses();
}

TEST_F(WorkletLoaderTest, Success) {
  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].success());
  RunOnV8ThreadAndWait(base::BindOnce(
      [](scoped_refptr<AuctionV8Helper> v8_helper,
         WorkletLoaderBase::Result result) {
        AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
        ASSERT_TRUE(result.success());
        v8::Global<v8::UnboundScript> script =
            WorkletLoader::TakeScript(std::move(result));
        ASSERT_FALSE(script.IsEmpty());
        EXPECT_EQ("https://foo.test/",
                  v8_helper->FormatScriptName(v8::Local<v8::UnboundScript>::New(
                      v8_helper->isolate(), script)));

        // TakeScript is a move op, so `result` is now cleared.
        EXPECT_FALSE(result.success());
      },
      v8_helpers_[0], std::move(results_[0])));

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre("Sent URL: "
                                   "https://foo.test/",
                                   "Received URL: "
                                   "https://foo.test/",
                                   "Completion Status: net::OK"));
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_Success) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_TRUE(results_[0].success());
  EXPECT_TRUE(results_[1].success());

  RunOnV8ThreadAndWait(
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             WorkletLoaderBase::Result result) {
            AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
            ASSERT_TRUE(result.success());
            v8::Global<v8::UnboundScript> script =
                WorkletLoader::TakeScript(std::move(result));
            ASSERT_FALSE(script.IsEmpty());
            EXPECT_EQ(
                "https://foo.test/",
                v8_helper->FormatScriptName(v8::Local<v8::UnboundScript>::New(
                    v8_helper->isolate(), script)));

            // TakeScript is a move op, so `result` is now cleared.
            EXPECT_FALSE(result.success());
          },
          v8_helpers_[0], std::move(results_[0])),
      v8_helpers_[0]);

  RunOnV8ThreadAndWait(
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             WorkletLoaderBase::Result result) {
            AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
            ASSERT_TRUE(result.success());
            v8::Global<v8::UnboundScript> script =
                WorkletLoader::TakeScript(std::move(result));
            ASSERT_FALSE(script.IsEmpty());
            EXPECT_EQ(
                "https://foo.test/",
                v8_helper->FormatScriptName(v8::Local<v8::UnboundScript>::New(
                    v8_helper->isolate(), script)));

            // TakeScript is a move op, so `result` is now cleared.
            EXPECT_FALSE(result.success());
          },
          v8_helpers_[1], std::move(results_[1])),
      v8_helpers_[1]);

  // Wait until idle to ensure all requests have been observed within the
  // `auction_network_events_handler_`.
  task_environment_.RunUntilIdle();
  EXPECT_THAT(auction_network_events_handler_.GetObservedRequests(),
              testing::ElementsAre("Sent URL: "
                                   "https://foo.test/",
                                   "Received URL: "
                                   "https://foo.test/",
                                   "Completion Status: net::OK"));
}

// Make sure the V8 isolate is released before the callback is invoked on
// success, so that the loader and helper can be torn down without crashing
// during the callback.
TEST_F(WorkletLoaderTest, DeleteDuringCallbackSuccess) {
  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  auto v8_helper = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_,
          /*auction_network_events_handler=*/
          auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
          debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
          base::BindLambdaForTesting(
              [&](std::vector<WorkletLoader::Result> worklet_scripts,
                  std::optional<std::string> error_msg) {
                EXPECT_EQ(worklet_scripts.size(), 1u);
                EXPECT_TRUE(worklet_scripts[0].success());
                EXPECT_FALSE(error_msg.has_value());
                worklet_scripts[0] = WorkletLoader::Result();
                worklet_loader.reset();
                v8_helper.reset();
                run_loop.Quit();
              }));
  run_loop.Run();
}

// Make sure the V8 isolate is released before the callback is invoked on
// compile failure, so that the loader and helper can be torn down without
// crashing during the callback.
TEST_F(WorkletLoaderTest, DeleteDuringCallbackCompileError) {
  AddJavascriptResponse(&url_loader_factory_, url_, kInvalidScript);
  auto v8_helper = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  base::RunLoop run_loop;
  std::unique_ptr<WorkletLoader> worklet_loader =
      std::make_unique<WorkletLoader>(
          &url_loader_factory_,
          /*auction_network_events_handler=*/
          auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
          debug_ids_, WorkletLoader::AllowTrustedScoringSignalsCallback(),
          base::BindLambdaForTesting(
              [&](std::vector<WorkletLoader::Result> worklet_scripts,
                  std::optional<std::string> error_msg) {
                EXPECT_EQ(worklet_scripts.size(), 1u);
                EXPECT_FALSE(worklet_scripts[0].success());
                ASSERT_TRUE(error_msg.has_value());
                EXPECT_THAT(error_msg.value(),
                            StartsWith("https://foo.test/:1 "));
                EXPECT_THAT(error_msg.value(), HasSubstr("SyntaxError"));
                worklet_loader.reset();
                v8_helper.reset();
                run_loop.Quit();
              }));
  run_loop.Run();
}

// Testcase where the loader is deleted after it queued the parsing of
// the script on V8 thread, but before that parsing completes.
//
// Also make sure that in this case we can cleanup without relying on main
// thread event loop spinning, as that's needed for synchronizing with
// v8 shutdown.
// (See https://crbug.com/1421754)
TEST_F(WorkletLoaderTest, DeleteBeforeCallback) {
  // Wedge the V8 thread so we can order loader deletion before script parsing.
  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helpers_[0].get());

  scoped_refptr<AuctionV8Helper> v8_helper = std::move(v8_helpers_[0]);
  base::WaitableEvent wait_for_v8_shutdown;
  v8_helper->SetDestroyedCallback(
      base::BindLambdaForTesting([&]() { wait_for_v8_shutdown.Signal(); }));

  AddJavascriptResponse(&url_loader_factory_, url_, kValidScript);
  auto worklet_loader = std::make_unique<WorkletLoader>(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_,
      std::vector{v8_helper}, debug_ids_,
      WorkletLoader::AllowTrustedScoringSignalsCallback(),
      base::BindOnce([](std::vector<WorkletLoader::Result> worklet_scripts,
                        std::optional<std::string> error_msg) {
        ADD_FAILURE() << "Callback should not be invoked since loader deleted";
      }));
  run_loop_.RunUntilIdle();
  worklet_loader.reset();
  event_handle->Signal();

  // Make sure that AuctionV8Helper can get shut down cleanly even though we
  // are not spinning the event loop for main thread here.
  v8_helper.reset();
  wait_for_v8_shutdown.Wait();
}

TEST_F(WorkletLoaderTest, LoadWasmSuccess) {
  AddResponse(
      &url_loader_factory_, url_, "application/wasm",
      /*charset=*/std::nullopt,
      std::string(kMinimalWasmModuleBytes, std::size(kMinimalWasmModuleBytes)));
  WorkletWasmLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].success());
  RunOnV8ThreadAndWait(base::BindOnce(
      [](scoped_refptr<AuctionV8Helper> v8_helper,
         WorkletLoaderBase::Result result) {
        AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
        v8::Local<v8::Context> context = v8_helper->CreateContext();
        v8::Context::Scope context_scope(context);

        ASSERT_TRUE(result.success());
        v8::MaybeLocal<v8::WasmModuleObject> module =
            WorkletWasmLoader::MakeModule(result);
        ASSERT_FALSE(module.IsEmpty());

        // MakeModule makes new ones, so `result` is still valid.
        EXPECT_TRUE(result.success());
        v8::MaybeLocal<v8::WasmModuleObject> module2 =
            WorkletWasmLoader::MakeModule(result);
        ASSERT_FALSE(module2.IsEmpty());
        EXPECT_NE(module.ToLocalChecked(), module2.ToLocalChecked());
      },
      v8_helpers_[0], std::move(results_[0])));
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_LoadWasmSuccess) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  AddResponse(
      &url_loader_factory_, url_, "application/wasm",
      /*charset=*/std::nullopt,
      std::string(kMinimalWasmModuleBytes, std::size(kMinimalWasmModuleBytes)));
  WorkletWasmLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_TRUE(results_[0].success());
  EXPECT_TRUE(results_[1].success());

  for (size_t i : {0, 1}) {
    RunOnV8ThreadAndWait(
        base::BindOnce(
            [](scoped_refptr<AuctionV8Helper> v8_helper,
               WorkletLoaderBase::Result result) {
              AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
              v8::Local<v8::Context> context = v8_helper->CreateContext();
              v8::Context::Scope context_scope(context);

              ASSERT_TRUE(result.success());
              v8::MaybeLocal<v8::WasmModuleObject> module =
                  WorkletWasmLoader::MakeModule(result);
              ASSERT_FALSE(module.IsEmpty());

              // MakeModule makes new ones, so `result` is still valid.
              EXPECT_TRUE(result.success());
              v8::MaybeLocal<v8::WasmModuleObject> module2 =
                  WorkletWasmLoader::MakeModule(result);
              ASSERT_FALSE(module2.IsEmpty());
              EXPECT_NE(module.ToLocalChecked(), module2.ToLocalChecked());
            },
            v8_helpers_[i], std::move(results_[i])),
        v8_helpers_[i]);
  }
}

TEST_F(WorkletLoaderTest, LoadWasmError) {
  AddResponse(&url_loader_factory_, url_, "application/wasm",
              /*charset=*/std::nullopt, "not wasm");
  WorkletWasmLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/ "));
  EXPECT_THAT(last_error_msg(),
              HasSubstr("Uncaught CompileError: WasmModuleObject::Compile"));
}

TEST_F(WorkletLoaderTest, TwoV8Helpers_LoadWasmError) {
  v8_helpers_.push_back(
      AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner()));
  debug_ids_.push_back(scoped_refptr<AuctionV8Helper::DebugId>());

  AddResponse(&url_loader_factory_, url_, "application/wasm",
              /*charset=*/std::nullopt, "not wasm");
  WorkletWasmLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_,
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 2u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_FALSE(results_[1].success());
  EXPECT_THAT(last_error_msg(), StartsWith("https://foo.test/ "));
  EXPECT_THAT(last_error_msg(),
              HasSubstr("Uncaught CompileError: WasmModuleObject::Compile"));
}

TEST_F(WorkletLoaderTest, TrustedSignalsHeader) {
  const char kHeader[] =
      "Ad-Auction-Allowed: true\r\n"
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: \"https://a.com\", "
      "\"https://b.com\"";

  base::test::TestFuture<std::vector<url::Origin>> allow_trusted_signals_from;
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType,
              /*charset=*/std::nullopt, kValidScript, kHeader);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, allow_trusted_signals_from.GetCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].success()) << last_error_msg();
  EXPECT_TRUE(allow_trusted_signals_from.IsReady());
  EXPECT_THAT(allow_trusted_signals_from.Get(),
              ElementsAre(url::Origin::Create(GURL("https://a.com")),
                          url::Origin::Create(GURL("https://b.com"))));
}

// Test that a response with Ad-Auction-Allow-Trusted-Scoring-Signals-From but
// no Ad-Auction-Allowed does not invoke the trusted signals permissions
// callback, only just a regular failure.
TEST_F(WorkletLoaderTest, TrustedSignalsHeaderNoAllowed) {
  const char kHeader[] =
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: \"https://a.com\", "
      "\"https://b.com\"";

  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType,
              /*charset=*/std::nullopt, kValidScript, kHeader);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, base::BindOnce([](std::vector<url::Origin> allowed_origins) {
        ADD_FAILURE() << "Should not be called here";
      }),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_FALSE(results_[0].success());
  EXPECT_EQ(
      "Rejecting load of https://foo.test/ due to lack of Ad-Auction-Allowed: "
      "true (or the deprecated X-Allow-FLEDGE: true).",
      last_error_msg());
}

// List headers are combined in the usual HTTP way when specified more than
// once.
TEST_F(WorkletLoaderTest, TrustedSignalsHeaderCombine) {
  const char kHeader[] =
      "Ad-Auction-Allowed: true\r\n"
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: \"https://a.com\"\r\n"
      "Ad-Auction-Allow-Trusted-Scoring-Signals-From: \"https://b.com\"\r\n";

  base::test::TestFuture<std::vector<url::Origin>> allow_trusted_signals_from;
  AddResponse(&url_loader_factory_, url_, kJavascriptMimeType,
              /*charset=*/std::nullopt, kValidScript, kHeader);
  WorkletLoader worklet_loader(
      &url_loader_factory_,
      /*auction_network_events_handler=*/
      auction_network_events_handler_.CreateRemote(), url_, v8_helpers_,
      debug_ids_, allow_trusted_signals_from.GetCallback(),
      base::BindOnce(&WorkletLoaderTest::LoadWorkletCallback,
                     base::Unretained(this)));
  run_loop_.Run();
  EXPECT_EQ(results_.size(), 1u);
  EXPECT_TRUE(results_[0].success()) << last_error_msg();
  EXPECT_TRUE(allow_trusted_signals_from.IsReady());
  EXPECT_THAT(allow_trusted_signals_from.Get(),
              ElementsAre(url::Origin::Create(GURL("https://a.com")),
                          url::Origin::Create(GURL("https://b.com"))));
}

TEST_F(WorkletLoaderTest, ParseAllowTrustedScoringSignalsFromHeader) {
  // Not a valid structured headers list
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader("1.1.1"),
              ElementsAre());
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(""),
              ElementsAre());

  // List with a non-string (tokens in this case)
  EXPECT_THAT(
      WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader("foo, bar"),
      ElementsAre());

  // Valid one.
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
                  R"("https://example.org/a", "https://example.com")"),
              ElementsAre(url::Origin::Create(GURL("https://example.org")),
                          url::Origin::Create(GURL("https://example.com"))));

  // non-https isn't OK.
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
                  R"("http://example.org/a", "gopher://example.com")"),
              ElementsAre());

  // Parameters are ignored.
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
                  R"("https://example.org/a";v=1, "https://example.com";v=2)"),
              ElementsAre(url::Origin::Create(GURL("https://example.org")),
                          url::Origin::Create(GURL("https://example.com"))));

  // Inner lists are not OK.
  EXPECT_THAT(WorkletLoader::ParseAllowTrustedScoringSignalsFromHeader(
                  R"(("https://example.org/a" "https://example.ie"))"),
              ElementsAre());
}

}  // namespace
}  // namespace auction_worklet
