// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "mojo/core/core.h"
#include "mojo/core/shared_buffer_dispatcher.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

using SharedBufferTest = test::MojoTestBase;

TEST_F(SharedBufferTest, CreateSharedBuffer) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);
  ExpectBufferContents(h, 0, message);
}

TEST_F(SharedBufferTest, DuplicateSharedBuffer) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);

  MojoHandle dupe = DuplicateBuffer(h, false);
  ExpectBufferContents(dupe, 0, message);
}

TEST_F(SharedBufferTest, PassSharedBufferLocal) {
  const std::string message = "hello";
  MojoHandle h = CreateBuffer(message.size());
  WriteToBuffer(h, 0, message);

  MojoHandle dupe = DuplicateBuffer(h, false);
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  WriteMessageWithHandles(p0, "...", &dupe, 1);
  EXPECT_EQ("...", ReadMessageWithHandles(p1, &dupe, 1));

  ExpectBufferContents(dupe, 0, message);
}

#if !defined(OS_IOS)

// Reads a single message with a shared buffer handle, maps the buffer, copies
// the message contents into it, then exits.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CopyToBufferClient, SharedBufferTest, h) {
  MojoHandle b;
  std::string message = ReadMessageWithHandles(h, &b, 1);
  WriteToBuffer(b, 0, message);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferCrossProcess) {
  const std::string message = "hello";
  MojoHandle b = CreateBuffer(message.size());

  RunTestClient("CopyToBufferClient", [&](MojoHandle h) {
    MojoHandle dupe = DuplicateBuffer(b, false);
    WriteMessageWithHandles(h, message, &dupe, 1);
    WriteMessage(h, "quit");
  });

  ExpectBufferContents(b, 0, message);
}

// Creates a new buffer, maps it, writes a message contents to it, unmaps it,
// and finally passes it back to the parent.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateBufferClient, SharedBufferTest, h) {
  std::string message = ReadMessage(h);
  MojoHandle b = CreateBuffer(message.size());
  WriteToBuffer(b, 0, message);
  WriteMessageWithHandles(h, "have a buffer", &b, 1);

  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferFromChild) {
  const std::string message = "hello";
  MojoHandle b;
  RunTestClient("CreateBufferClient", [&](MojoHandle h) {
    WriteMessage(h, message);
    ReadMessageWithHandles(h, &b, 1);
    WriteMessage(h, "quit");
  });

  ExpectBufferContents(b, 0, message);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateAndPassBuffer, SharedBufferTest, h) {
  // Receive a pipe handle over the primordial pipe. This will be connected to
  // another child process.
  MojoHandle other_child;
  std::string message = ReadMessageWithHandles(h, &other_child, 1);

  // Create a new shared buffer.
  MojoHandle b = CreateBuffer(message.size());

  // Send a copy of the buffer to the parent and the other child.
  MojoHandle dupe = DuplicateBuffer(b, false);
  WriteMessageWithHandles(h, "", &b, 1);
  WriteMessageWithHandles(other_child, "", &dupe, 1);

  EXPECT_EQ("quit", ReadMessage(h));
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveAndEditBuffer, SharedBufferTest, h) {
  // Receive a pipe handle over the primordial pipe. This will be connected to
  // another child process (running CreateAndPassBuffer).
  MojoHandle other_child;
  std::string message = ReadMessageWithHandles(h, &other_child, 1);

  // Receive a shared buffer from the other child.
  MojoHandle b;
  ReadMessageWithHandles(other_child, &b, 1);

  // Write the message from the parent into the buffer and exit.
  WriteToBuffer(b, 0, message);
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, PassSharedBufferFromChildToChild) {
  const std::string message = "hello";
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  MojoHandle b;
  RunTestClient("CreateAndPassBuffer", [&](MojoHandle h0) {
    RunTestClient("ReceiveAndEditBuffer", [&](MojoHandle h1) {
      // Send one end of the pipe to each child. The first child will create
      // and pass a buffer to the second child and back to us. The second child
      // will write our message into the buffer.
      WriteMessageWithHandles(h0, message, &p0, 1);
      WriteMessageWithHandles(h1, message, &p1, 1);

      // Receive the buffer back from the first child.
      ReadMessageWithHandles(h0, &b, 1);

      WriteMessage(h1, "quit");
    });
    WriteMessage(h0, "quit");
  });

  // The second child should have written this message.
  ExpectBufferContents(b, 0, message);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateAndPassBufferParent,
                                  SharedBufferTest,
                                  parent) {
  RunTestClient("CreateAndPassBuffer", [&](MojoHandle child) {
    // Read a pipe from the parent and forward it to our child.
    MojoHandle pipe;
    std::string message = ReadMessageWithHandles(parent, &pipe, 1);

    WriteMessageWithHandles(child, message, &pipe, 1);

    // Read a buffer handle from the child and pass it back to the parent.
    MojoHandle buffer;
    EXPECT_EQ("", ReadMessageWithHandles(child, &buffer, 1));
    WriteMessageWithHandles(parent, "", &buffer, 1);

    EXPECT_EQ("quit", ReadMessage(parent));
    WriteMessage(child, "quit");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReceiveAndEditBufferParent,
                                  SharedBufferTest,
                                  parent) {
  RunTestClient("ReceiveAndEditBuffer", [&](MojoHandle child) {
    // Read a pipe from the parent and forward it to our child.
    MojoHandle pipe;
    std::string message = ReadMessageWithHandles(parent, &pipe, 1);
    WriteMessageWithHandles(child, message, &pipe, 1);

    EXPECT_EQ("quit", ReadMessage(parent));
    WriteMessage(child, "quit");
  });
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
// Android multi-process tests are not executing the new process. This is flaky.
// Passing shared memory handles between cousins is not currently supported on
// OSX.
#define MAYBE_PassHandleBetweenCousins DISABLED_PassHandleBetweenCousins
#else
#define MAYBE_PassHandleBetweenCousins PassHandleBetweenCousins
#endif
TEST_F(SharedBufferTest, MAYBE_PassHandleBetweenCousins) {
  const std::string message = "hello";
  MojoHandle p0, p1;
  CreateMessagePipe(&p0, &p1);

  // Spawn two children who will each spawn their own child. Make sure the
  // grandchildren (cousins to each other) can pass platform handles.
  MojoHandle b;
  RunTestClient("CreateAndPassBufferParent", [&](MojoHandle child1) {
    RunTestClient("ReceiveAndEditBufferParent", [&](MojoHandle child2) {
      MojoHandle pipe[2];
      CreateMessagePipe(&pipe[0], &pipe[1]);

      WriteMessageWithHandles(child1, message, &pipe[0], 1);
      WriteMessageWithHandles(child2, message, &pipe[1], 1);

      // Receive the buffer back from the first child.
      ReadMessageWithHandles(child1, &b, 1);

      WriteMessage(child2, "quit");
    });
    WriteMessage(child1, "quit");
  });

  // The second grandchild should have written this message.
  ExpectBufferContents(b, 0, message);
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(ReadAndMapWriteSharedBuffer,
                                  SharedBufferTest,
                                  h) {
  // Receive the shared buffer.
  MojoHandle b;
  EXPECT_EQ("hello", ReadMessageWithHandles(h, &b, 1));

  // Read from the bufer.
  ExpectBufferContents(b, 0, "hello");

  // Extract the shared memory handle and verify that it is read-only.
  auto* dispatcher =
      static_cast<SharedBufferDispatcher*>(Core::Get()->GetDispatcher(b).get());
  base::subtle::PlatformSharedMemoryRegion& region =
      dispatcher->GetRegionForTesting();
  EXPECT_EQ(region.GetMode(),
            base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly);

  WriteMessage(h, "ok");
  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, CreateAndPassReadOnlyBuffer) {
  RunTestClient("ReadAndMapWriteSharedBuffer", [&](MojoHandle h) {
    // Create a new shared buffer.
    MojoHandle b = CreateBuffer(1234);
    WriteToBuffer(b, 0, "hello");

    // Send a read-only copy of the buffer to the child.
    MojoHandle dupe = DuplicateBuffer(b, true /* read_only */);
    WriteMessageWithHandles(h, "hello", &dupe, 1);

    EXPECT_EQ("ok", ReadMessage(h));
    WriteMessage(h, "quit");
  });
}

DEFINE_TEST_CLIENT_TEST_WITH_PIPE(CreateAndPassReadOnlyBuffer,
                                  SharedBufferTest,
                                  h) {
  // Create a new shared buffer.
  MojoHandle b = CreateBuffer(1234);
  WriteToBuffer(b, 0, "hello");

  // Send a read-only copy of the buffer to the parent.
  MojoHandle dupe = DuplicateBuffer(b, true /* read_only */);
  WriteMessageWithHandles(h, "", &dupe, 1);

  WriteMessage(h, "ok");
  EXPECT_EQ("quit", ReadMessage(h));
}

TEST_F(SharedBufferTest, CreateAndPassFromChildReadOnlyBuffer) {
  RunTestClient("CreateAndPassReadOnlyBuffer", [&](MojoHandle h) {
    MojoHandle b;
    EXPECT_EQ("", ReadMessageWithHandles(h, &b, 1));
    ExpectBufferContents(b, 0, "hello");

    // Extract the shared memory handle and verify that it is read-only.
    auto* dispatcher = static_cast<SharedBufferDispatcher*>(
        Core::Get()->GetDispatcher(b).get());
    base::subtle::PlatformSharedMemoryRegion& region =
        dispatcher->GetRegionForTesting();
    EXPECT_EQ(region.GetMode(),
              base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly);

    EXPECT_EQ("ok", ReadMessage(h));
    WriteMessage(h, "quit");
  });
}

#endif  // !defined(OS_IOS)

}  // namespace
}  // namespace core
}  // namespace mojo
