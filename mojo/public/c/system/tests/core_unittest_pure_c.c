// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef __cplusplus
#error "This file should be compiled as C, not C++."
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Include all the header files that are meant to be compilable as C. Start with
// core.h, since it's the most important one.
#include "mojo/public/c/system/core.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/thunks.h"

// The joys of the C preprocessor....
#define STRINGIFY(x) #x
#define STRINGIFY2(x) STRINGIFY(x)
#define FAILURE(message) \
  __FILE__ "(" STRINGIFY2(__LINE__) "): Failure: " message

// Makeshift gtest.
#define EXPECT_EQ(a, b)                                                  \
  do {                                                                   \
    if ((a) != (b))                                                      \
      return FAILURE(STRINGIFY(a) " != " STRINGIFY(b) " (expected ==)"); \
  } while (0)
#define EXPECT_NE(a, b)                                                  \
  do {                                                                   \
    if ((a) == (b))                                                      \
      return FAILURE(STRINGIFY(a) " == " STRINGIFY(b) " (expected !=)"); \
  } while (0)

// This function exists mainly to be compiled and linked. We do some cursory
// checks and call it from a unit test, to make sure that link problems aren't
// missed due to deadstripping. Returns null on success and a string on failure
// (describing the failure).
const char* MinimalCTest(void) {
  // MSVS before 2013 *really* only supports C90: All variables must be declared
  // at the top. (MSVS 2013 is more reasonable.)
  MojoTimeTicks ticks;
  MojoHandle handle0, handle1;

  ticks = MojoGetTimeTicksNow();
  EXPECT_NE(ticks, 0);

  handle0 = MOJO_HANDLE_INVALID;
  EXPECT_NE(MOJO_RESULT_OK, MojoClose(handle0));

  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryHandleSignalsState(handle0, NULL));

  handle1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessagePipe(NULL, &handle0, &handle1));

  MojoMessageHandle message;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateMessage(NULL, &message));
  const uint64_t message_contents = 42424242;
  uint32_t message_size = sizeof(message_contents);
  void* buffer;
  const struct MojoAppendMessageDataOptions options = {
    .struct_size = sizeof(options),
    .flags = MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE,
  };
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAppendMessageData(message, message_size, NULL, 0, &options,
                                  &buffer, NULL));
  memcpy(buffer, &message_contents, message_size);
  EXPECT_EQ(MOJO_RESULT_OK, MojoWriteMessage(handle0, message, NULL));
  EXPECT_EQ(MOJO_RESULT_OK, MojoReadMessage(handle1, NULL, &message));

  EXPECT_EQ(MOJO_RESULT_OK, MojoGetMessageData(message, NULL, &buffer,
                                               &message_size, NULL, NULL));
  EXPECT_EQ(sizeof(message_contents), message_size);
  EXPECT_EQ(message_contents, *(uint64_t*)buffer);
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handle0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handle1));

  return NULL;
}
