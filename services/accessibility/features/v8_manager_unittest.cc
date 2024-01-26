// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_manager.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/c/system/types.h"
#include "services/accessibility/features/mojo/test/js_test_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8_manager.h"

namespace ax {

class V8ManagerTest : public testing::Test {
 public:
  V8ManagerTest() = default;
  V8ManagerTest(const V8ManagerTest&) = delete;
  V8ManagerTest& operator=(const V8ManagerTest&) = delete;
  ~V8ManagerTest() override = default;

  void SetUp() override { BindingsIsolateHolder::InitializeV8(); }

  std::string GetMojoTestSupportJS() {
    base::FilePath gen_test_data_root;
    base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen_test_data_root);
    base::FilePath source_path = gen_test_data_root.Append(FILE_PATH_LITERAL(
        "services/accessibility/features/mojo/test/mojom_test_support.js"));
    std::string script;
    EXPECT_TRUE(ReadFileToString(source_path, &script));
    return script;
  }

  void RunJSMojoTest(const std::string& js_script) {
    base::RunLoop waiter;
    std::unique_ptr<JSTestInterface> test_interface =
        std::make_unique<JSTestInterface>(
            base::BindLambdaForTesting([&waiter](bool success) {
              EXPECT_TRUE(success) << "Mojo JS was not successful";
              waiter.Quit();
            }));
    V8Manager manager;
    manager.AddInterfaceForTest(std::move(test_interface));
    manager.FinishContextSetUp();

    base::RunLoop script_waiter;
    manager.RunScriptForTest(GetMojoTestSupportJS(),
                             script_waiter.QuitClosure());
    // Wait for the script to be executed.
    script_waiter.Run();

    manager.RunScriptForTest(js_script, base::DoNothing());
    // Wait for the test mojom API testComplete method.
    waiter.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// Test to execute Javascript that doesn't involve Mojo.
TEST_F(V8ManagerTest, ExecutesSimpleScript) {
  V8Manager manager;
  manager.FinishContextSetUp();
  base::RunLoop script_waiter;
  // Test that this script compiles and runs. That indicates that
  // the atpconsole.log binding was added and that JS works in general.
  manager.RunScriptForTest(R"JS(
    const d = 22;
    var m = 1;
    let y = 1973;
    // By checking there's no error here, we know that V8 bindings
    // can be installed on the context.
    atpconsole.log('Green is the loneliest color');
  )JS",
                           script_waiter.QuitClosure());
  script_waiter.Run();
}

// Sanity check of TextEncoder/TextDecoder.
TEST_F(V8ManagerTest, SanityCheckTextEncoder) {
  V8Manager manager;
  manager.FinishContextSetUp();
  base::RunLoop script_waiter;
  // Test that this script compiles and runs. That indicates there
  // is no issue creating and using TextEncoder/Decoder, but does
  // not verify that the values are as expected.
  manager.RunScriptForTest(R"JS(
    let encoder = new TextEncoder();
    let decoder = new TextDecoder();
    // With contents.
    let encoded = encoder.encode('Hello, world');
    let response = decoder.decode(encoded);
    // Empty.
    encoded = encoder.encode('');
    response = decoder.decode(encoded);
  )JS",
                           script_waiter.QuitClosure());
  script_waiter.Run();
}

// Test that we can do a simple mojom connection. This test will time out
// if the JS remote is not bound to the test_interface.
// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_SanityCheckMojoBindings DISABLED_SanityCheckMojoBindings
#else
#define MAYBE_SanityCheckMojoBindings SanityCheckMojoBindings
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_SanityCheckMojoBindings) {
  RunJSMojoTest(R"JS(
    const TestBindingInterface = axtest.mojom.TestBindingInterface;
    const remote = TestBindingInterface.getRemote();
    remote.testComplete(/*success=*/true);)JS");
}

// Test that Mojo constants are defined.
// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_CheckMojoConstants DISABLED_CheckMojoConstants
#else
#define MAYBE_CheckMojoConstants CheckMojoConstants
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_CheckMojoConstants) {
  base::RunLoop waiter;
  std::unique_ptr<JSTestInterface> test_interface =
      std::make_unique<JSTestInterface>(
          base::BindLambdaForTesting([&waiter](bool success) {
            EXPECT_TRUE(success) << "Mojo JS was not successful";
            waiter.Quit();
          }));
  V8Manager manager;
  manager.AddInterfaceForTest(std::move(test_interface));
  manager.FinishContextSetUp();

  base::RunLoop script_waiter;
  manager.RunScriptForTest(GetMojoTestSupportJS(), script_waiter.QuitClosure());
  // Wait for the script to be executed.
  script_waiter.Run();

  struct TestCase {
    std::string name;
    MojoResult value;
  };
  const TestCase kTestCases[] = {
      {"RESULT_OK", MOJO_RESULT_OK},
      {"RESULT_CANCELLED", MOJO_RESULT_CANCELLED},
      {"RESULT_UNKNOWN", MOJO_RESULT_UNKNOWN},
      {"RESULT_INVALID_ARGUMENT", MOJO_RESULT_INVALID_ARGUMENT},
      {"RESULT_DEADLINE_EXCEEDED", MOJO_RESULT_DEADLINE_EXCEEDED},
      {"RESULT_NOT_FOUND", MOJO_RESULT_NOT_FOUND},
      {"RESULT_ALREADY_EXISTS", MOJO_RESULT_ALREADY_EXISTS},
      {"RESULT_PERMISSION_DENIED", MOJO_RESULT_PERMISSION_DENIED},
      {"RESULT_RESOURCE_EXHAUSTED", MOJO_RESULT_RESOURCE_EXHAUSTED},
      {"RESULT_FAILED_PRECONDITION", MOJO_RESULT_FAILED_PRECONDITION},
      {"RESULT_ABORTED", MOJO_RESULT_ABORTED},
      {"RESULT_OUT_OF_RANGE", MOJO_RESULT_OUT_OF_RANGE},
      {"RESULT_UNIMPLEMENTED", MOJO_RESULT_UNIMPLEMENTED},
      {"RESULT_INTERNAL", MOJO_RESULT_INTERNAL},
      {"RESULT_UNAVAILABLE", MOJO_RESULT_UNAVAILABLE},
      {"RESULT_DATA_LOSS", MOJO_RESULT_DATA_LOSS},
      {"RESULT_BUSY", MOJO_RESULT_BUSY},
      {"RESULT_SHOULD_WAIT", MOJO_RESULT_SHOULD_WAIT},
  };
  uint32_t index = 0;
  for (auto test : kTestCases) {
    // Make sure the test doesn't skip any MojoResult.
    EXPECT_EQ(test.value, index);
    index++;

    // This will call testComplete(false) if an enum value is missing.
    const std::string script =
        base::StringPrintf(R"JS(
      if (Mojo.%s !== %i) {
        TestBindingInterface.getRemote().testComplete(/*success=*/false);
      }
      )JS",
                           test.name.c_str(), test.value);
    base::RunLoop test_waiter;
    manager.RunScriptForTest(script, test_waiter.QuitClosure());
    test_waiter.Run();
  }
}

// Test to load Mojo bindings and test that an asynchronous callback from JS
// to the remote interface in C++ returns the expected value.
// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_MojoBindingsGetsCallback DISABLED_MojoBindingsGetsCallback
#else
#define MAYBE_MojoBindingsGetsCallback MojoBindingsGetsCallback
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_MojoBindingsGetsCallback) {
  RunJSMojoTest(R"JS(
    class TestWrapper {
      constructor() {
        const TestBindingInterface = axtest.mojom.TestBindingInterface;
        this.remote_ = TestBindingInterface.getRemote();
        this.remote_.onConnectionError.addListener(() => {
            console.error('Connection error');
            this.remote_.testComplete(/*success=*/false);
        });
        this.init_();
      }

      async init_() {
        const response = await this.remote_.getTestStruct(41, "RGB");
        // Expect the result struct to have the number passed in plus 1,
        // and the name passed in plus ' rocks'.
        if (response.result.isStructy && response.result.num === 42 &&
            response.result.name === "RGB rocks") {
          this.remote_.testComplete(/*success=*/true);
        } else {
          this.remote_.testComplete(/*success=*/false);
        }
      }
    };
    new TestWrapper();)JS");
}

// Test to see that a PendingReceiver can be send from C++ to
// be bound in Javascript and that it can receive calls from C++.
// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_MojoBindingsPendingReceiver DISABLED_MojoBindingsPendingReceiver
#else
#define MAYBE_MojoBindingsPendingReceiver MojoBindingsPendingReceiver
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_MojoBindingsPendingReceiver) {
  RunJSMojoTest(R"JS(
    class TestReceiver {
      constructor(pendingReceiver, callback) {
        this.callback_ = callback;
        this.receiver_ = new axtest.mojom.TestInterfaceReceiver(this);
        this.receiver_.$.bindHandle(pendingReceiver.handle);
      }

      /** @override */
      testMethod(num) {
        this.callback_(num);
      }
    };

    class TestWrapper {
      constructor() {
        const TestBindingInterface = axtest.mojom.TestBindingInterface;
        this.remote_ = TestBindingInterface.getRemote();
        this.remote_.onConnectionError.addListener(() => {
            console.error('Connection error');
            this.remote_.testComplete(/*success=*/false);
        });
        this.init_();
      }

      async init_() {
        let pendingReceiver =
            (await this.remote_.addTestInterface()).interfaceReceiver;
        const receiver = new TestReceiver(pendingReceiver, (num) => {
          if (num == axtest.mojom.TestEnum.kThird) {
            this.remote_.testComplete(/*success=*/true);
          } else {
            this.remote_.testComplete(/*success=*/false);
          }
        });
        await this.remote_.sendEnumToTestInterface(
            axtest.mojom.TestEnum.kThird);
      }
    };
    new TestWrapper();
  )JS");
}

// Test to load Mojo bindings and then disconnect.
// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_MojoCancelBindings DISABLED_MojoCancelBindings
#else
#define MAYBE_MojoCancelBindings MojoCancelBindings
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_MojoCancelBindings) {
  RunJSMojoTest(R"JS(
    class TestWrapper {
      constructor() {
        const TestBindingInterface = axtest.mojom.TestBindingInterface;
        this.remote_ = TestBindingInterface.getRemote();
        this.remote_.onConnectionError.addListener(() => {
            this.remote_ = TestBindingInterface.getRemote();
            this.remote_.testComplete(true);
        });
        // Disconnecting from C++ will cause onConnectionError to be
        // called, which is the plumbing we're testing here.
        this.remote_.disconnect();
      }
    };
    new TestWrapper();)JS");
}

// TODO(b:262637071) Fails on Fuchsia due to ReadFileToString failing.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ExecuteModuleWithImports DISABLED_ExecuteModuleWithImports
#else
#define MAYBE_ExecuteModuleWithImports ExecuteModuleWithImports
#endif  // BUILDFLAG(IS_FUCHSIA)
TEST_F(V8ManagerTest, MAYBE_ExecuteModuleWithImports) {
  base::FilePath gen_test_data_root;
  CHECK(base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT,
                               &gen_test_data_root));
  base::FilePath file_1_path = gen_test_data_root.Append(FILE_PATH_LITERAL(
      "services/accessibility/features/mojo/test/test_api.test-mojom.m.js"));
  base::File file1(file_1_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file1.IsValid());
  base::FilePath file_2_path = gen_test_data_root.Append(FILE_PATH_LITERAL(
      "services/accessibility/features/mojo/test/module_import.js"));
  base::File file2(file_2_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file2.IsValid());

  base::FilePath file_3_path = gen_test_data_root.Append(
      FILE_PATH_LITERAL("mojo/public/js/bindings.js"));
  base::File file3(file_3_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(file3.IsValid());

  base::RunLoop module_waiter;
  std::unique_ptr<JSTestInterface> test_interface =
      std::make_unique<JSTestInterface>(
          base::BindLambdaForTesting([&module_waiter](bool success) {
            EXPECT_TRUE(success) << "Mojo JS was not successful";
            module_waiter.Quit();
          }));
  V8Manager manager;
  manager.AddInterfaceForTest(std::move(test_interface));

  // Important: files are added in fifo order (first file 2 will be evaluated
  // which will request 1 and then 3).
  manager.AddFileForTest(std::move(file2));
  manager.AddFileForTest(std::move(file1));
  manager.AddFileForTest(std::move(file3));
  manager.FinishContextSetUp();
  manager.ExecuteModule(file_2_path, base::DoNothing());
  module_waiter.Run();
}

TEST_F(V8ManagerTest, NormalizesPaths) {
  std::string result = V8Environment::NormalizeRelativePath("a.txt", "b/c");
  EXPECT_EQ(result, "b/c/a.txt");
  result = V8Environment::NormalizeRelativePath("./a.txt", "b/c");
  EXPECT_EQ(result, "b/c/a.txt");
  result = V8Environment::NormalizeRelativePath("../d.txt", "b/c");
  EXPECT_EQ(result, "b/d.txt");

  // Should fail, no base directory to resolve to.
  EXPECT_DEATH(V8Environment::NormalizeRelativePath("e.txt", ""), "");

  // Should fail, base directory ends with '/'.
  EXPECT_DEATH(V8Environment::NormalizeRelativePath("e.txt", "a/"), "");

  // Should fail, relative path references parent of base directory.
  EXPECT_DEATH(V8Environment::NormalizeRelativePath("../../../e.txt", "a/b"),
               "");
}

// TODO(b:313924294): add test to handle missing files.

}  // namespace ax
