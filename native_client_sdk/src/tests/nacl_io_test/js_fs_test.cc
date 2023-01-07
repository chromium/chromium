// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <set>
#include <string>

#include <gmock/gmock.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_instance.h>

#include "fake_ppapi/fake_messaging_interface.h"
#include "fake_ppapi/fake_pepper_interface.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/jsfs/js_fs.h"
#include "nacl_io/jsfs/js_fs_node.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/log.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"
#include "sdk_util/auto_lock.h"
#include "sdk_util/scoped_ref.h"
#include "sdk_util/simple_lock.h"

using namespace nacl_io;
using namespace sdk_util;

namespace {

class JsFsForTesting : public JsFs {
 public:
  JsFsForTesting(PepperInterface* ppapi) {
    FsInitArgs args;
    args.ppapi = ppapi;
    Error error = Init(args);
    EXPECT_EQ(0, error);
  }
};

class FakeMessagingInterfaceJsFs : public MessagingInterface {
 public:
  explicit FakeMessagingInterfaceJsFs(VarInterface* var_interface)
      : var_interface_(var_interface), has_message_(false) {
    pthread_cond_init(&cond_, NULL);
  }

  ~FakeMessagingInterfaceJsFs() { pthread_cond_destroy(&cond_); }

  virtual void PostMessage(PP_Instance instance, PP_Var message) {
    var_interface_->AddRef(message);

    AUTO_LOCK(lock_);
    ASSERT_FALSE(has_message_);

    message_ = message;
    has_message_ = true;
    pthread_cond_signal(&cond_);
  }

  PP_Var WaitForMessage() {
    AUTO_LOCK(lock_);
    while (!has_message_) {
      pthread_cond_wait(&cond_, lock_.mutex());
    }

    has_message_ = false;
    return message_;
  }

 private:
  VarInterface* var_interface_;
  SimpleLock lock_;
  pthread_cond_t cond_;
  PP_Var message_;
  bool has_message_;
};

class FakePepperInterfaceJsFs : public FakePepperInterface {
 public:
  FakePepperInterfaceJsFs() : messaging_interface_(GetVarInterface()) {}

  virtual nacl_io::MessagingInterface* GetMessagingInterface() {
    return &messaging_interface_;
  }

 private:
  FakeMessagingInterfaceJsFs messaging_interface_;
};

class JsFsTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init_interface(&kp_, &ppapi_));
    fs_.reset(new JsFsForTesting(&ppapi_));

    js_thread_started_ = false;
  }

  void TearDown() {
    if (js_thread_started_)
      pthread_join(js_thread_, NULL);

    for (RequestResponses::iterator it = request_responses_.begin(),
                                    end = request_responses_.end();
         it != end;
         ++it) {
      ppapi_.GetVarInterface()->Release(it->request);
      ppapi_.GetVarInterface()->Release(it->response);
    }


    ki_uninit();
  }

  void StartJsThread() {
    ASSERT_EQ(0, pthread_create(&js_thread_, NULL, JsThreadMainThunk, this));
    js_thread_started_ = true;
  }

  static void* JsThreadMainThunk(void* arg) {
    static_cast<JsFsTest*>(arg)->JsThreadMain();
    return NULL;
  }

  PP_Var WaitForRequest() {
    FakeMessagingInterfaceJsFs* messaging_if =
        static_cast<FakeMessagingInterfaceJsFs*>(
            ppapi_.GetMessagingInterface());
    return messaging_if->WaitForMessage();
  }

  void JsThreadMain() {
    for (RequestResponses::iterator it = request_responses_.begin(),
                                    end = request_responses_.end();
         it != end;
         ++it) {
      PP_Var request = WaitForRequest();
      EXPECT_TRUE(VarsAreEqual(it->request, request))
          << "Vars are not equal: " << VarToString(it->request)
          << " != " << VarToString(request);
      ppapi_.GetVarInterface()->Release(request);
      EXPECT_EQ(0,
                fs_->Filesystem_Ioctl(NACL_IOC_HANDLEMESSAGE, &it->response));
      // Passed ownership of response_ to filesystem, so set this to undefined
      // so it isn't double-released in TearDown().
      it->response = PP_MakeUndefined();
    }
  }

  void Expect(PP_Var request, PP_Var response) {
    RequestResponse rr;
    // Pass ownership of both vars from caller to callee.
    rr.request = request;
    rr.response = response;
    request_responses_.push_back(rr);
  }

  bool CreateDict(PP_Var* out_var) {
    *out_var = ppapi_.GetVarDictionaryInterface()->Create();
    return out_var->type == PP_VARTYPE_DICTIONARY;
  }

  bool SetDictKeyValue(PP_Var* var, const char* key, int32_t value) {
    return SetDictKeyValue(var, key, PP_MakeInt32(value));
  }

  bool SetDictKeyValue(PP_Var* var, const char* key, size_t value) {
    return SetDictKeyValue(var, key, PP_MakeInt32(static_cast<size_t>(value)));
  }

  bool SetDictKeyValue(PP_Var* var, const char* key, int64_t value) {
    VarArrayInterface* array_if = ppapi_.GetVarArrayInterface();
    PP_Var value_var = array_if->Create();
    return array_if->Set(value_var, 0, PP_MakeInt32(value >> 32)) &&
           array_if->Set(value_var, 1, PP_MakeInt32(value & 0xffffffff)) &&
           SetDictKeyValue(var, key, value_var);
  }

  bool SetDictKeyValue(PP_Var* var, const char* key, const char* value) {
    VarInterface* var_if = ppapi_.GetVarInterface();
    PP_Var value_var = var_if->VarFromUtf8(value, strlen(value));
    return SetDictKeyValue(var, key, value_var);
  }

  bool SetDictKeyValue(PP_Var* var, const char* key, PP_Var value_var) {
    VarDictionaryInterface* dict_if = ppapi_.GetVarDictionaryInterface();
    VarInterface* var_if = ppapi_.GetVarInterface();
    PP_Var key_var = var_if->VarFromUtf8(key, strlen(key));
    PP_Bool result = dict_if->Set(*var, key_var, value_var);
    var_if->Release(key_var);
    var_if->Release(value_var);
    return result == PP_TRUE;
  }

  bool CreateArray(PP_Var* out_var) {
    *out_var = ppapi_.GetVarArrayInterface()->Create();
    return out_var->type == PP_VARTYPE_ARRAY;
  }

  bool SetArrayValue(PP_Var* var, uint32_t i, int32_t value) {
    return SetArrayValue(var, i, PP_MakeInt32(value));
  }

  bool SetArrayValue(PP_Var* var, uint32_t i, const char* value) {
    VarInterface* var_if = ppapi_.GetVarInterface();
    PP_Var value_var = var_if->VarFromUtf8(value, strlen(value));
    return SetArrayValue(var, i, value_var);
  }

  bool SetArrayValue(PP_Var* var, uint32_t i, int64_t value) {
    VarArrayInterface* array_if = ppapi_.GetVarArrayInterface();
    PP_Var value_var = array_if->Create();
    return array_if->Set(value_var, 0, PP_MakeInt32(value >> 32)) &&
           array_if->Set(value_var, 1, PP_MakeInt32(value & 0xffffffff)) &&
           SetArrayValue(var, i, value_var);
  }

  bool SetArrayValue(PP_Var* var, uint32_t i, PP_Var value_var) {
    VarArrayInterface* array_if = ppapi_.GetVarArrayInterface();
    VarInterface* var_if = ppapi_.GetVarInterface();
    PP_Bool result = array_if->Set(*var, i, value_var);
    var_if->Release(value_var);
    return result == PP_TRUE;
  }

  std::string VarToString(PP_Var var) {
    VarDictionaryInterface* dict_if = ppapi_.GetVarDictionaryInterface();
    VarArrayInterface* array_if = ppapi_.GetVarArrayInterface();
    VarInterface* var_if = ppapi_.GetVarInterface();
    VarArrayBufferInterface* array_buffer_if =
        ppapi_.GetVarArrayBufferInterface();

    switch (var.type) {
      case PP_VARTYPE_UNDEFINED:
        return "undefined";
      case PP_VARTYPE_NULL:
        return "null";
      case PP_VARTYPE_BOOL:
        return var.value.as_bool ? "true" : "false";
      case PP_VARTYPE_INT32: {
        char buffer[32];
        snprintf(buffer, 32, "%d", var.value.as_int);
        return buffer;
      }
      case PP_VARTYPE_DOUBLE: {
        char buffer[32];
        snprintf(buffer, 32, "%g", var.value.as_double);
        return buffer;
      }
      case PP_VARTYPE_STRING: {
        uint32_t var_len;
        const char* var_str = var_if->VarToUtf8(var, &var_len);
        std::string result("\"");
        result += std::string(var_str, var_len);
        result += "\"";
        return result;
      }
      case PP_VARTYPE_ARRAY: {
        std::string result("[");
        uint32_t var_len = array_if->GetLength(var);

        for (uint32_t i = 0; i < var_len; ++i) {
          PP_Var var_item = array_if->Get(var, i);
          result += VarToString(var_item);
          var_if->Release(var_item);
          if (i != var_len - 1)
            result += ", ";
        }
        result += "]";
        return result;
      }
      case PP_VARTYPE_DICTIONARY: {
        std::string result("{");
        PP_Var var_keys = dict_if->GetKeys(var);
        uint32_t var_len = array_if->GetLength(var_keys);

        for (uint32_t i = 0; i < var_len; ++i) {
          PP_Var key = array_if->Get(var_keys, i);
          result += VarToString(key);
          result += ": ";
          PP_Var var_value = dict_if->Get(var, key);
          result += VarToString(var_value);
          var_if->Release(key);
          var_if->Release(var_value);
          if (i != var_len - 1)
            result += ", ";
        }
        result += "}";
        var_if->Release(var_keys);
        return result;
      }
      case PP_VARTYPE_ARRAY_BUFFER: {
        uint32_t var_len;
        if (!array_buffer_if->ByteLength(var, &var_len)) {
          LOG_ERROR("Unable to get byte length of var.");
          return "undefined";
        }

        std::string result("new Uint8Array([");

        void* var_ptr = array_buffer_if->Map(var);
        for (uint32_t i = 0; i < var_len; ++i) {
          char buffer[8];
          snprintf(buffer, 8, "%d", static_cast<uint8_t*>(var_ptr)[i]);
          result += buffer;
          if (i != var_len - 1)
            result += ", ";
        }
        result += "])";
        array_buffer_if->Unmap(var);
        return result;
      }

      default:
        ADD_FAILURE() << "Unexpected var type: " << var.type;
        return "undefined";
    }
  }

  bool VarsAreEqual(PP_Var expected, PP_Var var) {
    if (expected.type != var.type)
      return false;

    VarDictionaryInterface* dict_if = ppapi_.GetVarDictionaryInterface();
    VarArrayInterface* array_if = ppapi_.GetVarArrayInterface();
    VarInterface* var_if = ppapi_.GetVarInterface();
    VarArrayBufferInterface* array_buffer_if =
        ppapi_.GetVarArrayBufferInterface();

    switch (var.type) {
      case PP_VARTYPE_UNDEFINED:
      case PP_VARTYPE_NULL:
        return true;
      case PP_VARTYPE_BOOL:
        return expected.value.as_bool == var.value.as_bool;
      case PP_VARTYPE_INT32:
        return expected.value.as_int == var.value.as_int;
      case PP_VARTYPE_DOUBLE:
        return expected.value.as_double == var.value.as_double;
      case PP_VARTYPE_STRING: {
        uint32_t var_len;
        uint32_t expected_len;
        const char* var_str = var_if->VarToUtf8(var, &var_len);
        const char* expected_str = var_if->VarToUtf8(expected, &expected_len);

        if (expected_len != var_len)
          return false;

        return memcmp(expected_str, var_str, var_len) == 0;
      }
      case PP_VARTYPE_ARRAY: {
        uint32_t var_len = array_if->GetLength(var);
        uint32_t expected_len = array_if->GetLength(expected);

        if (expected_len != var_len)
          return false;

        for (uint32_t i = 0; i < var_len; ++i) {
          PP_Var var_item = array_if->Get(var, i);
          PP_Var expected_item = array_if->Get(expected, i);
          bool equal = VarsAreEqual(expected_item, var_item);
          var_if->Release(var_item);
          var_if->Release(expected_item);

          if (!equal)
            return false;
        }

        return true;
      }
      case PP_VARTYPE_DICTIONARY: {
        PP_Var var_keys = dict_if->GetKeys(var);
        PP_Var expected_keys = dict_if->GetKeys(expected);

        uint32_t var_len = array_if->GetLength(var_keys);
        uint32_t expected_len = array_if->GetLength(expected_keys);

        bool result = true;

        if (expected_len == var_len) {
          for (uint32_t i = 0; i < var_len; ++i) {
            PP_Var key = array_if->Get(var_keys, i);
            PP_Var var_value = dict_if->Get(var, key);
            PP_Var expected_value = dict_if->Get(expected, key);
            bool equal = VarsAreEqual(expected_value, var_value);
            var_if->Release(key);
            var_if->Release(var_value);
            var_if->Release(expected_value);

            if (!equal) {
              result = false;
              break;
            }
          }
        } else {
          result = false;
        }

        var_if->Release(var_keys);
        var_if->Release(expected_keys);
        return result;
      }
      case PP_VARTYPE_ARRAY_BUFFER: {
        uint32_t var_len;
        if (!array_buffer_if->ByteLength(var, &var_len))
          return false;

        uint32_t expected_len;
        if (!array_buffer_if->ByteLength(expected, &expected_len))
          return false;

        if (expected_len != var_len)
          return false;

        void* var_ptr = array_buffer_if->Map(var);
        void* expected_ptr = array_buffer_if->Map(expected);
        bool equal = memcmp(var_ptr, expected_ptr, var_len) == 0;
        array_buffer_if->Unmap(var);
        array_buffer_if->Unmap(expected);

        return equal;
      }

      default:
        ADD_FAILURE() << "Unexpected var type: " << var.type;
        return false;
    }
  }

  PP_Var CreateDummyArrayBuffer(uint32_t length) {
    VarArrayBufferInterface* array_buffer_if =
        ppapi_.GetVarArrayBufferInterface();
    PP_Var var = array_buffer_if->Create(length);
    uint8_t* data = static_cast<uint8_t*>(array_buffer_if->Map(var));
    FillDummyBuffer(data, length);
    array_buffer_if->Unmap(var);
    return var;
  }

  void FillDummyBuffer(uint8_t* buf, size_t buf_len) {
    for (uint32_t i = 0; i < buf_len; ++i) {
      buf[i] = i & 255;
    }
  }

  bool EqualsDummyArrayBuffer(uint8_t* buf, size_t buf_len) {
    for (uint32_t i = 0; i < buf_len; ++i) {
      if (buf[i] != (i & 255)) {
        LOG_ERROR("Byte %d of ArrayBuffer doesn't match: %d != %d.",
                  i,
                  buf[i],
                  i & 255);
        return false;
      }
    }
    return true;
  }

 protected:
  FakePepperInterfaceJsFs ppapi_;
  ScopedRef<JsFsForTesting> fs_;
  KernelProxy kp_;
  pthread_t js_thread_;
  bool js_thread_started_;

  struct RequestResponse {
    PP_Var request;
    PP_Var response;
  };

  typedef std::vector<RequestResponse> RequestResponses;
  RequestResponses request_responses_;
};

class JsFsNodeTest : public JsFsTest {
 public:
  static const int fd;

  virtual void SetUp() {
    JsFsTest::SetUp();

    PP_Var expected;
    ASSERT_EQ(true, CreateDict(&expected));
    ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
    ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "open"));
    ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));
    ASSERT_EQ(true, SetDictKeyValue(&expected, "oflag", O_RDONLY));

    PP_Var response;
    ASSERT_EQ(true, CreateDict(&response));
    ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
    ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
    ASSERT_EQ(true, SetDictKeyValue(&response, "fd", fd));

    Expect(expected, response);
  }

  virtual void TearDown() {
    JsFsTest::TearDown();
  }

  void OpenNode() {
    EXPECT_EQ(0, fs_->Open(Path("/foo"), O_RDONLY, &node_));
    EXPECT_EQ(fd, sdk_util::static_scoped_ref_cast<JsFsNode>(node_)->fd());
  }

 protected:
  ScopedNode node_;
};

const int JsFsNodeTest::fd = 123;

}  // namespace

TEST_F(JsFsTest, Open) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "open"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "oflag", O_RDONLY));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
  ASSERT_EQ(true, SetDictKeyValue(&response, "fd", 123));

  Expect(expected, response);
  StartJsThread();

  ScopedNode node;
  EXPECT_EQ(0, fs_->Open(Path("/foo"), O_RDONLY, &node));
  EXPECT_EQ(123, sdk_util::static_scoped_ref_cast<JsFsNode>(node)->fd());
}

TEST_F(JsFsTest, Unlink) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "unlink"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();

  EXPECT_EQ(0, fs_->Unlink(Path("/foo")));
}

TEST_F(JsFsTest, Mkdir) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "mkdir"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "mode", 0644));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();

  EXPECT_EQ(0, fs_->Mkdir(Path("/foo"), 0644));
}

TEST_F(JsFsTest, Rmdir) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "rmdir"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();

  EXPECT_EQ(0, fs_->Rmdir(Path("/foo")));
}

TEST_F(JsFsTest, Remove) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "remove"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "path", "/foo"));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();

  EXPECT_EQ(0, fs_->Remove(Path("/foo")));
}

TEST_F(JsFsTest, Rename) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "rename"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "old", "/foo"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "new", "/bar"));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();

  EXPECT_EQ(0, fs_->Rename(Path("/foo"), Path("/bar")));
}

TEST_F(JsFsNodeTest, GetStat) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "fstat"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_ino", (int64_t) 1));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_mode", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_nlink", 3));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_uid", 4));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_gid", 5));
#ifdef __APPLE__
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_rdev", (dev_t) 6));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_size", (off_t) 7));
#else
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_rdev", (int64_t) 6));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_size", (int64_t) 7));
#endif
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_blksize", 8));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_blocks", 9));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_atime", (int64_t) 10));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_mtime", (int64_t) 11));
  ASSERT_EQ(true, SetDictKeyValue(&response, "st_ctime", (int64_t) 12));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  struct stat statbuf;
  EXPECT_EQ(0, node_->GetStat(&statbuf));
  EXPECT_EQ(fs_->dev(), statbuf.st_dev);
  EXPECT_EQ(1, statbuf.st_ino);
  EXPECT_EQ(2, statbuf.st_mode);
  EXPECT_EQ(3, statbuf.st_nlink);
  EXPECT_EQ(4, statbuf.st_uid);
  EXPECT_EQ(5, statbuf.st_gid);
  EXPECT_EQ(6, statbuf.st_rdev);
  EXPECT_EQ(7, statbuf.st_size);
  EXPECT_EQ(8, statbuf.st_blksize);
  EXPECT_EQ(9, statbuf.st_blocks);
  EXPECT_EQ(10, statbuf.st_atime);
  EXPECT_EQ(11, statbuf.st_mtime);
  EXPECT_EQ(12, statbuf.st_ctime);
}

TEST_F(JsFsNodeTest, FSync) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "fsync"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  EXPECT_EQ(0, node_->FSync());
}

TEST_F(JsFsNodeTest, FTruncate) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "ftruncate"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "length", 0));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  EXPECT_EQ(0, node_->FTruncate(0));
}

TEST_F(JsFsNodeTest, Read) {
  const size_t kReadLength = 100;

  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "pread"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));
  ASSERT_EQ(true,
            SetDictKeyValue(&expected, "nbyte", static_cast<int>(kReadLength)));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "offset", 200));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
  ASSERT_EQ(
      true,
      SetDictKeyValue(&response, "buf", CreateDummyArrayBuffer(kReadLength)));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  HandleAttr attr;
  attr.offs = 200;
  uint8_t buf[kReadLength];
  int bytes_read;
  EXPECT_EQ(0, node_->Read(attr, buf, kReadLength, &bytes_read));
  EXPECT_EQ(kReadLength, bytes_read);
  EXPECT_TRUE(EqualsDummyArrayBuffer(buf, kReadLength));
}

TEST_F(JsFsNodeTest, Write) {
  const size_t kWriteLength = 100;

  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "pwrite"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));
  ASSERT_EQ(
      true,
      SetDictKeyValue(&expected, "buf", CreateDummyArrayBuffer(kWriteLength)));
  ASSERT_EQ(
      true,
      SetDictKeyValue(&expected, "nbyte", static_cast<int>(kWriteLength)));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "offset", 200));

  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
  ASSERT_EQ(true, SetDictKeyValue(&response, "nwrote", kWriteLength));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  HandleAttr attr;
  attr.offs = 200;

  uint8_t buf[kWriteLength];
  FillDummyBuffer(buf, kWriteLength);

  int bytes_written;
  EXPECT_EQ(0, node_->Write(attr, buf, kWriteLength, &bytes_written));
  EXPECT_EQ(kWriteLength, bytes_written);
}

TEST_F(JsFsNodeTest, GetDents) {
  PP_Var expected;
  ASSERT_EQ(true, CreateDict(&expected));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "cmd", "getdents"));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "fildes", fd));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "offs", 0));
  ASSERT_EQ(true, SetDictKeyValue(&expected, "count", 2));

  PP_Var entry0;
  ASSERT_EQ(true, CreateDict(&entry0));
  ASSERT_EQ(true, SetDictKeyValue(&entry0, "d_ino", 2));
  ASSERT_EQ(true, SetDictKeyValue(&entry0, "d_name", "."));
  PP_Var entry1;
  ASSERT_EQ(true, CreateDict(&entry1));
  ASSERT_EQ(true, SetDictKeyValue(&entry1, "d_ino", 3));
  ASSERT_EQ(true, SetDictKeyValue(&entry1, "d_name", ".."));
  PP_Var array;
  ASSERT_EQ(true, CreateArray(&array));
  ASSERT_EQ(true, SetArrayValue(&array, 0, entry0));
  ASSERT_EQ(true, SetArrayValue(&array, 1, entry1));
  PP_Var response;
  ASSERT_EQ(true, CreateDict(&response));
  ASSERT_EQ(true, SetDictKeyValue(&response, "id", 2));
  ASSERT_EQ(true, SetDictKeyValue(&response, "error", 0));
  ASSERT_EQ(true, SetDictKeyValue(&response, "dirents", array));

  Expect(expected, response);
  StartJsThread();
  OpenNode();

  dirent buf[2];
  int bytes_written;
  EXPECT_EQ(0, node_->GetDents(0, buf, sizeof(dirent) * 2, &bytes_written));
  EXPECT_EQ(sizeof(dirent) * 2, bytes_written);
  EXPECT_EQ(2, buf[0].d_ino);
  EXPECT_EQ(sizeof(dirent), buf[0].d_reclen);
  EXPECT_STREQ(".", buf[0].d_name);
  EXPECT_EQ(3, buf[1].d_ino);
  EXPECT_EQ(sizeof(dirent), buf[1].d_reclen);
  EXPECT_STREQ("..", buf[1].d_name);
}
