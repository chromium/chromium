// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_MOJO_TEST_BASE_H_
#define MOJO_CORE_TEST_MOJO_TEST_BASE_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "mojo/core/test/multiprocess_test_helper.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace test {

class MojoTestBase : public testing::Test {
 public:
  // Mojo Core is configured with this message size limit in tests so that we
  // can reliably exercise code paths for oversized messages.
  static constexpr size_t kMaxMessageSizeInTests = 32 * 1024 * 1024;

  MojoTestBase();

  MojoTestBase(const MojoTestBase&) = delete;
  MojoTestBase& operator=(const MojoTestBase&) = delete;

  ~MojoTestBase() override;

  using LaunchType = MultiprocessTestHelper::LaunchType;

  class ClientController {
   public:
    ClientController(const std::string& client_name,
                     MojoTestBase* test,
                     LaunchType launch_type);

    ClientController(const ClientController&) = delete;
    ClientController& operator=(const ClientController&) = delete;

    ~ClientController();

#if BUILDFLAG(USE_BLINK)
    const base::Process& process() const { return helper_.test_child(); }
#endif

    MojoHandle pipe() const { return pipe_.get().value(); }

    int WaitForShutdown();

   private:
    friend class MojoTestBase;

#if BUILDFLAG(USE_BLINK)
    MultiprocessTestHelper helper_;
#endif
    ScopedMessagePipeHandle pipe_;
    bool was_shutdown_ = false;
  };

  ClientController& StartClient(const std::string& client_name);

  template <typename HandlerFunc>
  void RunTestClient(const std::string& client_name, HandlerFunc handler) {
    EXPECT_EQ(0, RunTestClientAndGetExitCode(client_name, handler));
  }

  template <typename HandlerFunc>
  int RunTestClientAndGetExitCode(const std::string& client_name,
                                  HandlerFunc handler) {
    ClientController& c = StartClient(client_name);
    handler(c.pipe());
    return c.WaitForShutdown();
  }

  template <typename HandlerFunc>
  void RunTestClientWithController(const std::string& client_name,
                                   HandlerFunc handler) {
    ClientController& c = StartClient(client_name);
    handler(c);
    EXPECT_EQ(0, c.WaitForShutdown());
  }

  // Closes a handle and expects success.
  static void CloseHandle(MojoHandle h);

  ////// Message pipe test utilities ///////

  // Creates a new pipe, returning endpoint handles in |p0| and |p1|.
  static void CreateMessagePipe(MojoHandle* p0, MojoHandle* p1);

  // Writes a string to the pipe, transferring handles in the process.
  static void WriteMessageWithHandles(MojoHandle mp,
                                      const std::string& message,
                                      const MojoHandle* handles,
                                      uint32_t num_handles);

  // Writes a string to the pipe with no handles.
  static void WriteMessage(MojoHandle mp, const std::string& message);

  // Reads a string from the pipe, expecting to read an exact number of handles
  // in the process. Returns the read string.
  static std::string ReadMessageWithHandles(MojoHandle mp,
                                            MojoHandle* handles,
                                            uint32_t expected_num_handles);

  // Reads a string from the pipe, expecting either zero or one handles.
  // If no handle is read, |handle| will be reset.
  static std::string ReadMessageWithOptionalHandle(MojoHandle mp,
                                                   MojoHandle* handle);

  // Reads a string from the pipe, expecting to read no handles.
  // Returns the string.
  static std::string ReadMessage(MojoHandle mp);

  // Reads a string from the pipe, expecting to read no handles and exactly
  // |num_bytes| bytes, which are read into |data|.
  static void ReadMessage(MojoHandle mp, char* data, size_t num_bytes);

  // Writes |message| to |in| and expects to read it back from |out|.
  static void VerifyTransmission(MojoHandle in,
                                 MojoHandle out,
                                 const std::string& message);

  // Writes |message| to |mp| and expects to read it back from the same handle.
  static void VerifyEcho(MojoHandle mp, const std::string& message);

  //////// Shared buffer test utilities /////////

  // Creates a new shared buffer.
  static MojoHandle CreateBuffer(uint64_t size);

  // Duplicates a shared buffer to a new handle.
  static MojoHandle DuplicateBuffer(MojoHandle h, bool read_only);

  // Maps a buffer, writes some data into it, and unmaps it.
  static void WriteToBuffer(MojoHandle h,
                            size_t offset,
                            const std::string_view& s);

  // Maps a buffer, tests the value of some of its contents, and unmaps it.
  static void ExpectBufferContents(MojoHandle h,
                                   size_t offset,
                                   const std::string_view& s);

  //////// Data pipe test utilities /////////

  // Creates a new data pipe.
  static void CreateDataPipe(MojoHandle* producer,
                             MojoHandle* consumer,
                             size_t capacity);

  // Writes data to a data pipe.
  static void WriteData(MojoHandle producer, const std::string& data);

  // Reads data from a data pipe.
  static std::string ReadData(MojoHandle consumer, size_t size);

  // Queries the signals state of |handle|.
  static MojoHandleSignalsState GetSignalsState(MojoHandle handle);

  // Helper to block the calling thread waiting for signals to go high or low.
  static MojoResult WaitForSignals(MojoHandle handle,
                                   MojoHandleSignals signals,
                                   MojoTriggerCondition condition,
                                   MojoHandleSignalsState* state = nullptr);

  // Like above but only waits for signals to go high.
  static MojoResult WaitForSignals(MojoHandle handle,
                                   MojoHandleSignals signals,
                                   MojoHandleSignalsState* state = nullptr);

  void set_launch_type(LaunchType launch_type) { launch_type_ = launch_type; }

 private:
  friend class ClientController;

  std::vector<std::unique_ptr<ClientController>> clients_;

  LaunchType launch_type_ = LaunchType::CHILD;
};

// Use this to declare the child process's "main()" function for tests using
// MojoTestBase and MultiprocessTestHelper. It returns an |int|, which will
// will be the process's exit code (but see the comment about
// WaitForChildShutdown()).
//
// The function is defined as a subclass of |test_base| to facilitate shared
// code between test clients and to allow clients to spawn children
// themselves.
//
// |pipe_name| will be bound to the MojoHandle of a message pipe connected
// to the test process (see RunTestClient* above.) This pipe handle is
// automatically closed on test client teardown.
#if BUILDFLAG(USE_BLINK)
#define DEFINE_TEST_CLIENT_WITH_PIPE(client_name, test_base, pipe_name) \
  class client_name##_MainFixture : public test_base {                  \
    void TestBody() override {}                                         \
                                                                        \
   public:                                                              \
    int Main(MojoHandle);                                               \
  };                                                                    \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                    \
      client_name##TestChildMain,                                       \
      ::mojo::core::test::MultiprocessTestHelper::ChildSetup) {         \
    client_name##_MainFixture test;                                     \
    return ::mojo::core::test::MultiprocessTestHelper::RunClientMain(   \
        base::BindOnce(&client_name##_MainFixture::Main,                \
                       base::Unretained(&test)));                       \
  }                                                                     \
  int client_name##_MainFixture::Main(MojoHandle pipe_name)

// This is a version of DEFINE_TEST_CLIENT_WITH_PIPE which can be used with
// gtest ASSERT/EXPECT macros.
#define DEFINE_TEST_CLIENT_TEST_WITH_PIPE(client_name, test_base, pipe_name) \
  class client_name##_MainFixture : public test_base {                       \
    void TestBody() override {}                                              \
                                                                             \
   public:                                                                   \
    void Main(MojoHandle);                                                   \
  };                                                                         \
  MULTIPROCESS_TEST_MAIN_WITH_SETUP(                                         \
      client_name##TestChildMain,                                            \
      ::mojo::core::test::MultiprocessTestHelper::ChildSetup) {              \
    client_name##_MainFixture test;                                          \
    return ::mojo::core::test::MultiprocessTestHelper::RunClientTestMain(    \
        base::BindOnce(&client_name##_MainFixture::Main,                     \
                       base::Unretained(&test)));                            \
  }                                                                          \
  void client_name##_MainFixture::Main(MojoHandle pipe_name)
#else  // BUILDFLAG(USE_BLINK)
#define DEFINE_TEST_CLIENT_WITH_PIPE(client_name, test_base, pipe_name) \
  class client_name##_MainFixture : public test_base {                  \
    void TestBody() override {}                                         \
                                                                        \
   public:                                                              \
    int Main(MojoHandle);                                               \
  };                                                                    \
  int client_name##_MainFixture::Main(MojoHandle pipe_name)
#define DEFINE_TEST_CLIENT_TEST_WITH_PIPE(client_name, test_base, pipe_name) \
  class client_name##_MainFixture : public test_base {                       \
    void TestBody() override {}                                              \
                                                                             \
   public:                                                                   \
    void Main(MojoHandle);                                                   \
  };                                                                         \
  void client_name##_MainFixture::Main(MojoHandle pipe_name)
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace test
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_TEST_MOJO_TEST_BASE_H_
