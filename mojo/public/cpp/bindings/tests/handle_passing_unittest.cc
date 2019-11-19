// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/sample_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

const char kText1[] = "hello";
const char kText2[] = "world";

void RecordString(std::string* storage,
                  base::OnceClosure closure,
                  const std::string& str) {
  *storage = str;
  std::move(closure).Run();
}

base::OnceCallback<void(const std::string&)> MakeStringRecorder(
    std::string* storage,
    base::OnceClosure closure) {
  return base::BindOnce(&RecordString, storage, std::move(closure));
}

class ImportedInterfaceImpl : public imported::ImportedInterface {
 public:
  ImportedInterfaceImpl(PendingReceiver<imported::ImportedInterface> receiver,
                        base::OnceClosure closure)
      : receiver_(this, std::move(receiver)), closure_(std::move(closure)) {}

  void DoSomething() override {
    do_something_count_++;
    std::move(closure_).Run();
  }

  static int do_something_count() { return do_something_count_; }

 private:
  static int do_something_count_;
  Receiver<ImportedInterface> receiver_;
  base::OnceClosure closure_;
};
int ImportedInterfaceImpl::do_something_count_ = 0;

class SampleNamedObjectImpl : public sample::NamedObject {
 public:
  SampleNamedObjectImpl() {}

  void SetName(const std::string& name) override { name_ = name; }

  void GetName(GetNameCallback callback) override {
    std::move(callback).Run(name_);
  }

 private:
  std::string name_;
};

class SampleFactoryImpl : public sample::Factory {
 public:
  explicit SampleFactoryImpl(PendingReceiver<sample::Factory> receiver)
      : receiver_(this, std::move(receiver)) {}

  void DoStuff(sample::RequestPtr request,
               ScopedMessagePipeHandle pipe,
               DoStuffCallback callback) override {
    std::string text1;
    if (pipe.is_valid())
      EXPECT_TRUE(ReadTextMessage(pipe.get(), &text1));

    std::string text2;
    if (request->pipe.is_valid()) {
      EXPECT_TRUE(ReadTextMessage(request->pipe.get(), &text2));

      // Ensure that simply accessing request->pipe does not close it.
      EXPECT_TRUE(request->pipe.is_valid());
    }

    ScopedMessagePipeHandle pipe0;
    if (!text2.empty()) {
      CreateMessagePipe(nullptr, &pipe0, &pipe1_);
      EXPECT_TRUE(WriteTextMessage(pipe1_.get(), text2));
    }

    sample::ResponsePtr response(sample::Response::New(2, std::move(pipe0)));
    std::move(callback).Run(std::move(response), text1);

    if (request->obj) {
      Remote<imported::ImportedInterface> proxy(std::move(request->obj));
      proxy->DoSomething();
    }
  }

  void DoStuff2(ScopedDataPipeConsumerHandle pipe,
                DoStuff2Callback callback) override {
    // Read the data from the pipe, writing the response (as a string) to
    // DidStuff2().
    ASSERT_TRUE(pipe.is_valid());
    uint32_t data_size = 0;

    MojoHandleSignalsState state;
    ASSERT_EQ(MOJO_RESULT_OK,
              mojo::Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE, &state));
    ASSERT_TRUE(state.satisfied_signals & MOJO_HANDLE_SIGNAL_READABLE);
    ASSERT_EQ(MOJO_RESULT_OK,
              pipe->ReadData(nullptr, &data_size, MOJO_READ_DATA_FLAG_QUERY));
    ASSERT_NE(0, static_cast<int>(data_size));
    char data[64];
    ASSERT_LT(static_cast<int>(data_size), 64);
    ASSERT_EQ(MOJO_RESULT_OK, pipe->ReadData(data, &data_size,
                                             MOJO_READ_DATA_FLAG_ALL_OR_NONE));

    std::move(callback).Run(data);
  }

  void CreateNamedObject(
      PendingReceiver<sample::NamedObject> object_receiver) override {
    EXPECT_TRUE(object_receiver.is_valid());
    object_receivers_.Add(std::make_unique<SampleNamedObjectImpl>(),
                          std::move(object_receiver));
  }

  // These aren't called or implemented, but exist here to test that the
  // methods are generated with the correct argument types for imported
  // interfaces.
  void RequestImportedInterface(
      PendingReceiver<imported::ImportedInterface> imported,
      RequestImportedInterfaceCallback callback) override {}
  void TakeImportedInterface(
      PendingRemote<imported::ImportedInterface> imported,
      TakeImportedInterfaceCallback callback) override {}

 private:
  ScopedMessagePipeHandle pipe1_;
  Receiver<sample::Factory> receiver_;
  UniqueReceiverSet<sample::NamedObject> object_receivers_;
};

class HandlePassingTest : public BindingsTestBase {
 public:
  HandlePassingTest() {}

  void TearDown() override { PumpMessages(); }

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }

};

void DoStuff(bool* got_response,
             std::string* got_text_reply,
             base::OnceClosure closure,
             sample::ResponsePtr response,
             const std::string& text_reply) {
  *got_text_reply = text_reply;

  if (response->pipe.is_valid()) {
    std::string text2;
    EXPECT_TRUE(ReadTextMessage(response->pipe.get(), &text2));

    // Ensure that simply accessing response.pipe does not close it.
    EXPECT_TRUE(response->pipe.is_valid());

    EXPECT_EQ(std::string(kText2), text2);

    // Do some more tests of handle passing:
    ScopedMessagePipeHandle p = std::move(response->pipe);
    EXPECT_TRUE(p.is_valid());
    EXPECT_FALSE(response->pipe.is_valid());
  }

  *got_response = true;
  std::move(closure).Run();
}

void DoStuff2(bool* got_response,
              std::string* got_text_reply,
              base::OnceClosure closure,
              const std::string& text_reply) {
  *got_response = true;
  *got_text_reply = text_reply;
  std::move(closure).Run();
}

TEST_P(HandlePassingTest, Basic) {
  Remote<sample::Factory> factory;
  SampleFactoryImpl factory_impl(factory.BindNewPipeAndPassReceiver());

  MessagePipe pipe0;
  EXPECT_TRUE(WriteTextMessage(pipe0.handle1.get(), kText1));

  MessagePipe pipe1;
  EXPECT_TRUE(WriteTextMessage(pipe1.handle1.get(), kText2));

  PendingRemote<imported::ImportedInterface> imported;
  base::RunLoop run_loop;
  ImportedInterfaceImpl imported_impl(imported.InitWithNewPipeAndPassReceiver(),
                                      run_loop.QuitClosure());

  sample::RequestPtr request(sample::Request::New(
      1, std::move(pipe1.handle0), base::nullopt, std::move(imported)));
  bool got_response = false;
  std::string got_text_reply;
  base::RunLoop run_loop2;
  factory->DoStuff(std::move(request), std::move(pipe0.handle0),
                   base::BindOnce(&DoStuff, &got_response, &got_text_reply,
                                  run_loop2.QuitClosure()));

  EXPECT_FALSE(got_response);
  int count_before = ImportedInterfaceImpl::do_something_count();

  run_loop.Run();
  run_loop2.Run();

  EXPECT_TRUE(got_response);
  EXPECT_EQ(kText1, got_text_reply);
  EXPECT_EQ(1, ImportedInterfaceImpl::do_something_count() - count_before);
}

TEST_P(HandlePassingTest, PassInvalid) {
  Remote<sample::Factory> factory;
  SampleFactoryImpl factory_impl(factory.BindNewPipeAndPassReceiver());

  sample::RequestPtr request(sample::Request::New(1, ScopedMessagePipeHandle(),
                                                  base::nullopt, NullRemote()));

  bool got_response = false;
  std::string got_text_reply;
  base::RunLoop run_loop;
  factory->DoStuff(std::move(request), ScopedMessagePipeHandle(),
                   base::BindOnce(&DoStuff, &got_response, &got_text_reply,
                                  run_loop.QuitClosure()));

  EXPECT_FALSE(got_response);

  run_loop.Run();

  EXPECT_TRUE(got_response);
}

// Verifies DataPipeConsumer can be passed and read from.
TEST_P(HandlePassingTest, DataPipe) {
  Remote<sample::Factory> factory;
  SampleFactoryImpl factory_impl(factory.BindNewPipeAndPassReceiver());

  // Writes a string to a data pipe and passes the data pipe (consumer) to the
  // factory.
  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options = {sizeof(MojoCreateDataPipeOptions),
                                       MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
                                       1024};
  ASSERT_EQ(MOJO_RESULT_OK,
            CreateDataPipe(&options, &producer_handle, &consumer_handle));
  std::string expected_text_reply = "got it";
  // +1 for \0.
  uint32_t data_size = static_cast<uint32_t>(expected_text_reply.size() + 1);
  ASSERT_EQ(MOJO_RESULT_OK,
            producer_handle->WriteData(expected_text_reply.c_str(), &data_size,
                                       MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));

  bool got_response = false;
  std::string got_text_reply;
  base::RunLoop run_loop;
  factory->DoStuff2(std::move(consumer_handle),
                    base::BindOnce(&DoStuff2, &got_response, &got_text_reply,
                                   run_loop.QuitClosure()));

  EXPECT_FALSE(got_response);

  run_loop.Run();

  EXPECT_TRUE(got_response);
  EXPECT_EQ(expected_text_reply, got_text_reply);
}

TEST_P(HandlePassingTest, CreateNamedObject) {
  Remote<sample::Factory> factory;
  SampleFactoryImpl factory_impl(factory.BindNewPipeAndPassReceiver());

  Remote<sample::NamedObject> object1;
  EXPECT_FALSE(object1);

  auto object1_receiver = object1.BindNewPipeAndPassReceiver();
  EXPECT_TRUE(object1_receiver);
  factory->CreateNamedObject(std::move(object1_receiver));
  EXPECT_FALSE(object1_receiver);

  ASSERT_TRUE(object1);
  object1->SetName("object1");

  Remote<sample::NamedObject> object2;
  factory->CreateNamedObject(object2.BindNewPipeAndPassReceiver());
  object2->SetName("object2");

  base::RunLoop run_loop, run_loop2;
  std::string name1;
  object1->GetName(MakeStringRecorder(&name1, run_loop.QuitClosure()));

  std::string name2;
  object2->GetName(MakeStringRecorder(&name2, run_loop2.QuitClosure()));

  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(std::string("object1"), name1);
  EXPECT_EQ(std::string("object2"), name2);
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(HandlePassingTest);

}  // namespace
}  // namespace test
}  // namespace mojo
