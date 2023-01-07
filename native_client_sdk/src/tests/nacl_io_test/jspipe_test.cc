// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>

#include "dev_fs_for_testing.h"
#include "fake_ppapi/fake_messaging_interface.h"
#include "gtest/gtest.h"
#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/osdirent.h"

using namespace nacl_io;

namespace {

// Helper function for calling ki_ioctl without having
// to construct a va_list.
int ki_ioctl_wrapper(int fd, int request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_ioctl(fd, request, ap);
  va_end(ap);
  return rtn;
}

// Helper function for converting PP_Var to C++ string
std::string VarToString(VarInterface* var_iface, PP_Var var) {
  EXPECT_EQ(PP_VARTYPE_STRING, var.type);
  uint32_t len = 0;
  const char* str = var_iface->VarToUtf8(var, &len);
  return std::string(str, len);
}

PP_Var VarFromCStr(VarInterface* iface, const char* string) {
  return iface->VarFromUtf8(string, strlen(string));
}

// Helper function for creating message in the format expected by jspipe
// nodes: [ name, payload ]
PP_Var CreatePipeMessage(PepperInterface* ppapi, const char* pipe,
                         const char* operation, PP_Var payload) {
  VarInterface* var_iface = ppapi->GetVarInterface();
  VarDictionaryInterface* dict_iface = ppapi->GetVarDictionaryInterface();

  // Create a two element array containing the name of the message
  // as the first element.  Its up to the caller the then set the
  // second array element.
  PP_Var message = dict_iface->Create();
  PP_Var pipe_var = VarFromCStr(var_iface, pipe);
  PP_Var operation_var = VarFromCStr(var_iface, operation);
  PP_Var pipe_key = VarFromCStr(var_iface, "pipe");
  PP_Var payload_key = VarFromCStr(var_iface, "payload");
  PP_Var operation_key = VarFromCStr(var_iface, "operation");
  dict_iface->Set(message, pipe_key, pipe_var);
  dict_iface->Set(message, operation_key, operation_var);
  dict_iface->Set(message, payload_key, payload);
  var_iface->Release(pipe_var);
  var_iface->Release(operation_var);
  var_iface->Release(payload);
  var_iface->Release(pipe_key);
  var_iface->Release(payload_key);
  var_iface->Release(operation_key);
  return message;
}

// Helper function for creating "ack" message in format expected
// by jspipe nodes.
PP_Var CreateAckMessage(PepperInterface* ppapi, const char* pipe,
                        int32_t count) {
  return CreatePipeMessage(ppapi, pipe, "ack", PP_MakeInt32(count));
}

// Helper function for creating "write" message in format expected
// by jspipe nodes.
PP_Var CreateWriteMessage(PepperInterface* ppapi,
                          const char* pipe,
                          const char* string,
                          int length=-1) {
  VarArrayBufferInterface* buffer_iface = ppapi->GetVarArrayBufferInterface();

  if (length == -1)
    length = strlen(string);

  PP_Var buffer = buffer_iface->Create(length);
  memcpy(buffer_iface->Map(buffer), string, length);
  buffer_iface->Unmap(buffer);

  return CreatePipeMessage(ppapi, pipe, "write", buffer);
}

class JSPipeTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init_interface(&kp_, &ppapi_));
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  FakePepperInterface ppapi_;
  KernelProxy kp_;
};

class JSPipeNodeTest : public ::testing::Test {
 public:
  JSPipeNodeTest() : fs_(&ppapi_) {}

  void SetUp() {
    name_ = "jspipe1";
    ASSERT_EQ(0, fs_.Open(Path("/jspipe1"), O_RDWR, &pipe_dev_));
    ASSERT_NE(NULL_NODE, pipe_dev_.get());
    struct stat buf;
    ASSERT_EQ(0, pipe_dev_->GetStat(&buf));
    ASSERT_EQ(S_IRUSR | S_IWUSR, buf.st_mode & S_IRWXU);
  }

  /**
   * Create a PP_Var message in the same way that we expect
   * JavaScript code to, and send it to the pipe using ioctl()
   */
  int JSPipeInject(const char* string, int length=-1) {
    PP_Var message = CreateWriteMessage(&ppapi_, name_, string, length);

    // Send the message via ioctl
    int rtn = pipe_dev_->Ioctl(NACL_IOC_HANDLEMESSAGE, &message);

    // Release message
    ppapi_.GetVarInterface()->Release(message);
    return rtn;
  }

  int JSPipeInjectAck(int32_t count) {
    PP_Var message = CreateAckMessage(&ppapi_, name_, count);

    // Send the message via ioctl
    int rtn = pipe_dev_->Ioctl(NACL_IOC_HANDLEMESSAGE, &message);

    // Release message
    ppapi_.GetVarInterface()->Release(message);
    return rtn;
  }

  // Verify the contents of the jspipe mesage, which should be
  // {
  //   "pipe": '<pipe_name>',
  //   "operation": '<command_name>',
  //   "payload": payload
  // }
  void VerifyPipeMessage(PP_Var message,
                         const char* pipe_name,
                         const char* operation,
                         const char* payload,
                         int payload_length,
                         int32_t int_payload=0) {
    VarArrayInterface* array_iface = ppapi_.GetVarArrayInterface();
    VarDictionaryInterface* dict_iface = ppapi_.GetVarDictionaryInterface();
    VarInterface* var_iface = ppapi_.GetVarInterface();
    VarArrayBufferInterface* buffer_iface = ppapi_.GetVarArrayBufferInterface();

    // Verify we have a dictionary with 3 keys
    ASSERT_EQ(PP_VARTYPE_DICTIONARY, message.type);
    PP_Var keys = dict_iface->GetKeys(message);
    ASSERT_EQ(PP_VARTYPE_ARRAY, keys.type);
    ASSERT_EQ(3, array_iface->GetLength(keys));
    var_iface->Release(keys);

    // Verify the keys
    PP_Var key1 = VarFromCStr(var_iface, "pipe");
    PP_Var key2 = VarFromCStr(var_iface, "operation");
    PP_Var key3 = VarFromCStr(var_iface, "payload");

    // Verify pipe name and operation values
    PP_Var value1 = dict_iface->Get(message, key1);
    ASSERT_STREQ(pipe_name, VarToString(var_iface, value1).c_str());
    var_iface->Release(value1);
    var_iface->Release(key1);

    PP_Var value2 = dict_iface->Get(message, key2);
    ASSERT_STREQ(operation, VarToString(var_iface, value2).c_str());
    var_iface->Release(value2);
    var_iface->Release(key2);

    // Verify the payload
    PP_Var payload_var = dict_iface->Get(message, key3);
    if (payload != NULL) {
      ASSERT_EQ(PP_VARTYPE_ARRAY_BUFFER, payload_var.type);
      ASSERT_EQ(0, memcmp(payload, buffer_iface->Map(payload_var),
                          payload_length));
    } else {
      ASSERT_EQ(PP_VARTYPE_INT32, payload_var.type);
      ASSERT_EQ(int_payload, payload_var.value.as_int);
    }
    var_iface->Release(key3);
    var_iface->Release(payload_var);
  }

 protected:
  FakePepperInterface ppapi_;
  DevFsForTesting fs_;
  ScopedNode pipe_dev_;
  const char* name_;
};

TEST(JSPipeTestBasic, MissingPepper) {
  // Create a devfs filesystem without giving it any Pepper implemenation.
  TypedFsFactory<DevFs> factory;
  ScopedFilesystem fs;
  FsInitArgs args(1);
  factory.CreateFilesystem(args, &fs);
  ScopedNode pipe_dev;
  ASSERT_EQ(0, fs->Open(Path("/jspipe1"), O_RDWR, &pipe_dev));

  // Writing to a pipe should return EIO because Pepper is missing.
  HandleAttr attrs;
  int written = -1;
  ASSERT_EQ(EIO, pipe_dev->Write(attrs, "test", 4, &written));
}

TEST_F(JSPipeNodeTest, InvalidIoctl) {
  // 123 is not a valid ioctl request.
  EXPECT_EQ(EINVAL, pipe_dev_->Ioctl(123));
}

TEST_F(JSPipeNodeTest, JSPipeInput) {
  std::string message("hello, how are you?\n");

  // First we send some data into the pipe.  This is how messages
  // from javascript are injected into the pipe nodes.
  ASSERT_EQ(0, JSPipeInject(message.c_str()));

  // Now we make buffer we'll read into.
  // We fill the buffer and a backup buffer with arbitrary data
  // and compare them after reading to make sure read doesn't
  // clobber parts of the buffer it shouldn't.
  int bytes_read;
  char buffer[100];
  char backup_buffer[100];
  memset(buffer, 'a', sizeof(buffer));
  memset(backup_buffer, 'a', sizeof(backup_buffer));

  // We read a small chunk first to ensure it doesn't give us
  // more than we ask for.
  HandleAttr attrs;
  ASSERT_EQ(0, pipe_dev_->Read(attrs, buffer, 5, &bytes_read));
  EXPECT_EQ(5, bytes_read);
  EXPECT_EQ(0, memcmp(message.data(), buffer, 5));
  EXPECT_EQ(0, memcmp(buffer + 5, backup_buffer + 5, sizeof(buffer)-5));

  // Now we ask for more data than is left in the pipe, to ensure
  // it doesn't give us more than there is.
  ASSERT_EQ(0, pipe_dev_->Read(attrs, buffer + 5, sizeof(buffer)-5,
                               &bytes_read));
  EXPECT_EQ(bytes_read, message.size() - 5);
  EXPECT_EQ(0, memcmp(message.data(), buffer, message.size()));
  EXPECT_EQ(0, memcmp(buffer + message.size(),
                      backup_buffer + message.size(),
                      100 - message.size()));
}

TEST_F(JSPipeNodeTest, JSPipeOutput) {
  std::string message("hello");

  int bytes_written = 999;
  HandleAttr attrs;
  ASSERT_EQ(0, pipe_dev_->Write(attrs, message.c_str(), message.size(),
                                &bytes_written));
  ASSERT_EQ(message.size(), bytes_written);

  FakeMessagingInterface* iface =
      (FakeMessagingInterface*)ppapi_.GetMessagingInterface();

  // Verify that exactly one message sent.
  ASSERT_EQ(1, iface->messages.size());
  PP_Var message_var = iface->messages[0];

  // Verify the content of the message.
  VerifyPipeMessage(message_var, "jspipe1", "write", message.c_str(),
                    message.size());
}

TEST_F(JSPipeNodeTest, JSPipeOutputWithNulls) {
  char message[20];
  int message_len = sizeof(message);

  // Construct a 20-byte  message containing the string 'hello' but with
  // null chars on either end.
  memset(message, 0 , message_len);
  memcpy(message+10, "hello", 5);

  int bytes_written = 999;
  HandleAttr attrs;
  EXPECT_EQ(0, pipe_dev_->Write(attrs, message, message_len, &bytes_written));
  EXPECT_EQ(message_len, bytes_written);

  // Verify that the correct messages was sent via PostMessage.
  FakeMessagingInterface* iface =
      (FakeMessagingInterface*)ppapi_.GetMessagingInterface();

  // Verify that exaclty one message sent.
  ASSERT_EQ(1, iface->messages.size());
  PP_Var message_var = iface->messages[0];

  // Verify the content of the message.
  VerifyPipeMessage(message_var, "jspipe1", "write", message, message_len);
}

#define CHUNK_SIZE 678
TEST_F(JSPipeNodeTest, JSPipeOutputBuffer) {
  int ospace_orig = -1;
  ASSERT_EQ(0, pipe_dev_->Ioctl(NACL_IOC_PIPE_GETOSPACE, &ospace_orig));
  ASSERT_GT(ospace_orig, 0);

  HandleAttr attrs;
  attrs.flags = O_NONBLOCK;
  char* message = (char*)malloc(CHUNK_SIZE);

  // Keep writing data until we block.
  int total_written = 0;
  while (1) {
    int bytes_written;
    // Write some data
    int rtn = pipe_dev_->Write(attrs, message, CHUNK_SIZE, &bytes_written);
    if (rtn != 0) {
      ASSERT_EQ(EWOULDBLOCK, rtn);
      int ospace = -1;
      ASSERT_EQ(0, pipe_dev_->Ioctl(NACL_IOC_PIPE_GETOSPACE, &ospace));
      ASSERT_EQ(0, ospace);
      ASSERT_EQ(total_written, ospace_orig);
      break;
    }
    total_written += bytes_written;
  }

  // At this point writes should always block
  int bytes_written;
  int rtn = pipe_dev_->Write(attrs, message, CHUNK_SIZE, &bytes_written);
  ASSERT_EQ(EWOULDBLOCK, rtn);

  // Now inject and ACK message from JavaScript.
  ASSERT_EQ(0, JSPipeInjectAck(10));

  // Now it should be possible to write 10 bytes to the pipe.
  rtn = pipe_dev_->Write(attrs, message, CHUNK_SIZE, &bytes_written);
  ASSERT_EQ(0, rtn);
  ASSERT_EQ(10, bytes_written);

  free(message);
}

TEST_F(JSPipeNodeTest, JSPipeInputBuffer) {
  char* message = (char*)malloc(CHUNK_SIZE);
  memset(message, 1, CHUNK_SIZE);

  int ispace_orig = -1;
  ASSERT_EQ(0, pipe_dev_->Ioctl(NACL_IOC_PIPE_GETISPACE, &ispace_orig));

  // Keep injecting data until the ioctl fails
  int total_written = 0;
  while (1) {
    int rtn = JSPipeInject(message, CHUNK_SIZE);
    if (rtn != 0) {
      ASSERT_LT(total_written, ispace_orig);
      ASSERT_GT(total_written, ispace_orig - CHUNK_SIZE - 1);
      break;
    }
    total_written += CHUNK_SIZE;
  }

  int ispace = -1;
  ASSERT_EQ(0, pipe_dev_->Ioctl(NACL_IOC_PIPE_GETISPACE, &ispace));
  ASSERT_EQ(0, ispace);

  // Check that no messages have thus far been sent to JavaScript
  FakeMessagingInterface* iface =
      (FakeMessagingInterface*)ppapi_.GetMessagingInterface();
  ASSERT_EQ(0, iface->messages.size());

  // Read some data from the pipe, which should trigger an ack message
  int bytes_read = -1;
  HandleAttr attrs;
  ASSERT_EQ(0, pipe_dev_->Read(attrs, message, 5, &bytes_read));
  ASSERT_EQ(5, bytes_read);

  // Verify that an ack was sent to JavaScript
  ASSERT_EQ(1, iface->messages.size());
  PP_Var message_var = iface->messages[0];
  VerifyPipeMessage(message_var, "jspipe1", "ack", NULL, 0, 5);

  free(message);
}

// Returns:
//   0 -> Not readable
//   1 -> Readable
//  -1 -> Error occurred
int IsReadable(int fd) {
  struct timeval timeout = {0, 0};
  fd_set readfds;
  fd_set errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(fd, &readfds);
  FD_SET(fd, &errorfds);
  int rtn = ki_select(fd + 1, &readfds, NULL, &errorfds, &timeout);
  if (rtn == 0)
    return 0;  // not readable
  if (rtn != 1)
    return -1;  // error
  if (FD_ISSET(fd, &errorfds))
    return -2;  // error
  if (!FD_ISSET(fd, &readfds))
    return -3;  // error
  return 1;     // readable
}

TEST_F(JSPipeTest, JSPipeSelect) {
  struct timeval timeout;
  fd_set readfds;
  fd_set writefds;
  fd_set errorfds;

  int pipe_fd = ki_open("/dev/jspipe1", O_RDONLY, 0);
  ASSERT_GT(pipe_fd, 0) << "jspipe1 open failed: " << errno;

  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(pipe_fd, &readfds);
  FD_SET(pipe_fd, &errorfds);
  // 10 millisecond timeout
  timeout.tv_sec = 0;
  timeout.tv_usec = 10 * 1000;
  // Should timeout when no input is available.
  int rtn = ki_select(pipe_fd + 1, &readfds, NULL, &errorfds, &timeout);
  ASSERT_EQ(0, rtn) << "select failed: " << rtn << " err=" << strerror(errno);
  ASSERT_FALSE(FD_ISSET(pipe_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &errorfds));

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(pipe_fd, &readfds);
  FD_SET(pipe_fd, &writefds);
  FD_SET(pipe_fd, &errorfds);
  // Pipe should be writable on startup.
  rtn = ki_select(pipe_fd + 1, &readfds, &writefds, &errorfds, NULL);
  ASSERT_EQ(1, rtn);
  ASSERT_TRUE(FD_ISSET(pipe_fd, &writefds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &errorfds));

  // Send 4 bytes to the pipe via ioctl
  PP_Var message = CreateWriteMessage(&ppapi_, "jspipe1", "test");
  ASSERT_EQ(0, ki_ioctl_wrapper(pipe_fd, NACL_IOC_HANDLEMESSAGE, &message));
  ppapi_.GetVarInterface()->Release(message);

  // Pipe should now be readable
  ASSERT_EQ(1, IsReadable(pipe_fd));

  ki_close(pipe_fd);
}

}
