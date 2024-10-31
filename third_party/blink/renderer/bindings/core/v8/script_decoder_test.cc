// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/script_decoder.h"

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

const unsigned char kFooUTF8WithBOM[] = {0xef, 0xbb, 0xbf, 0x66, 0x6f, 0x6f};
// SHA256 hash of 'foo' in hex:
//   echo -n 'foo' | sha256sum | xxd -r -p | xxd -i
const unsigned char kExpectedDigest[] = {
    0x2c, 0x26, 0xb4, 0x6b, 0x68, 0xff, 0xc6, 0x8f, 0xf9, 0x9b, 0x45,
    0x3c, 0x1d, 0x30, 0x41, 0x34, 0x13, 0x42, 0x2d, 0x70, 0x64, 0x83,
    0xbf, 0xa0, 0xf9, 0x8a, 0x5e, 0x88, 0x62, 0x66, 0xe7, 0xae};

class DummyResponseBodyLoaderClient
    : public GarbageCollected<DummyResponseBodyLoaderClient>,
      public ResponseBodyLoaderClient {
 public:
  DummyResponseBodyLoaderClient() = default;
  void DidReceiveData(base::span<const char> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    raw_data_.emplace_back(Vector<char>(data));
  }
  void DidReceiveDecodedData(
      const String& decoded_data,
      std::unique_ptr<ParkableStringImpl::SecureDigest> digest) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    decoded_data_ = decoded_data;
    digest_ = std::move(digest);
  }
  void DidFinishLoadingBody() override { NOTREACHED_IN_MIGRATION(); }
  void DidFailLoadingBody() override { NOTREACHED_IN_MIGRATION(); }
  void DidCancelLoadingBody() override { NOTREACHED_IN_MIGRATION(); }

  const Deque<Vector<char>>& raw_data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return raw_data_;
  }
  const String& decoded_data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return decoded_data_;
  }
  const std::unique_ptr<ParkableStringImpl::SecureDigest>& digest() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return digest_;
  }

 private:
  Deque<Vector<char>> raw_data_;
  String decoded_data_;
  std::unique_ptr<ParkableStringImpl::SecureDigest> digest_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

class ScriptDecoderTest : public ::testing::Test {
 public:
  ~ScriptDecoderTest() override = default;

  ScriptDecoderTest(const ScriptDecoderTest&) = delete;
  ScriptDecoderTest& operator=(const ScriptDecoderTest&) = delete;

 protected:
  ScriptDecoderTest() = default;

 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(ScriptDecoderTest, WithClient) {
  scoped_refptr<base::SequencedTaskRunner> default_task_runner =
      scheduler::GetSequencedTaskRunnerForTesting();
  DummyResponseBodyLoaderClient* client =
      MakeGarbageCollected<DummyResponseBodyLoaderClient>();
  ScriptDecoderWithClientPtr decoder = ScriptDecoderWithClient::Create(
      client,
      std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      default_task_runner);
  decoder->DidReceiveData(Vector<char>(base::make_span(kFooUTF8WithBOM)),
                          /*send_to_client=*/true);

  base::RunLoop run_loop;
  decoder->FinishDecode(CrossThreadBindOnce(
      [&](scoped_refptr<base::SequencedTaskRunner> default_task_runner,
          base::RunLoop* run_loop) {
        CHECK(default_task_runner->RunsTasksInCurrentSequence());
        run_loop->Quit();
      },
      default_task_runner, CrossThreadUnretained(&run_loop)));
  run_loop.Run();

  ASSERT_EQ(client->raw_data().size(), 1u);
  EXPECT_THAT(client->raw_data().front(),
              Vector<char>(base::make_span(kFooUTF8WithBOM)));
  EXPECT_EQ(client->decoded_data(), "foo");
  EXPECT_THAT(
      client->digest(),
      testing::Pointee(Vector<uint8_t>(base::make_span(kExpectedDigest))));
}

TEST_F(ScriptDecoderTest, PartiallySendDifferentThread) {
  scoped_refptr<base::SequencedTaskRunner> default_task_runner =
      scheduler::GetSequencedTaskRunnerForTesting();
  DummyResponseBodyLoaderClient* client =
      MakeGarbageCollected<DummyResponseBodyLoaderClient>();
  ScriptDecoderWithClientPtr decoder = ScriptDecoderWithClient::Create(
      client,
      std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions::CreateUTF8Decode()),
      default_task_runner);

  base::span<const char> data_span =
      base::make_span(reinterpret_cast<const char*>(kFooUTF8WithBOM),
                      sizeof(kFooUTF8WithBOM) / sizeof(unsigned char));

  base::span<const char> first_chunk = base::make_span(data_span.begin(), 3u);
  base::span<const char> second_chunk =
      base::make_span(data_span.begin() + 3, data_span.end());

  // Directly send the first chunk to `client`.
  client->DidReceiveData(first_chunk);
  // Call DidReceiveData() with the first chunk and false `send_to_client`.
  decoder->DidReceiveData(Vector<char>(first_chunk),
                          /*send_to_client=*/false);
  // Create a worker task runner.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});

  // Call DidReceiveData() with the second chunk and true `send_to_client` on
  // the worker task runner.
  worker_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ScriptDecoderWithClient::DidReceiveData,
                                base::Unretained(decoder.get()),
                                Vector<char>(second_chunk),
                                /*send_to_client=*/true));

  // Call FinishDecode() on the worker task runner.
  base::RunLoop run_loop;
  worker_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ScriptDecoderWithClient::FinishDecode,
          base::Unretained(decoder.get()),
          CrossThreadBindOnce(
              [&](scoped_refptr<base::SequencedTaskRunner> default_task_runner,
                  base::RunLoop* run_loop) {
                CHECK(default_task_runner->RunsTasksInCurrentSequence());
                run_loop->Quit();
              },
              default_task_runner, CrossThreadUnretained(&run_loop))));
  run_loop.Run();

  ASSERT_EQ(client->raw_data().size(), 2u);
  EXPECT_THAT(client->raw_data().front(), Vector<char>(first_chunk));
  EXPECT_THAT(client->raw_data().back(), Vector<char>(second_chunk));
  EXPECT_EQ(client->decoded_data(), "foo");
  EXPECT_THAT(
      client->digest(),
      testing::Pointee(Vector<uint8_t>(base::make_span(kExpectedDigest))));
}

TEST_F(ScriptDecoderTest, Simple) {
  scoped_refptr<base::SequencedTaskRunner> default_task_runner =
      scheduler::GetSequencedTaskRunnerForTesting();
  ScriptDecoderPtr decoder =
      ScriptDecoder::Create(std::make_unique<TextResourceDecoder>(
                                TextResourceDecoderOptions::CreateUTF8Decode()),
                            default_task_runner);
  // Create a worker task runner.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner =
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
  // Call DidReceiveData() on the worker task runner.
  worker_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ScriptDecoder::DidReceiveData,
                     base::Unretained(decoder.get()),
                     Vector<char>(base::make_span(kFooUTF8WithBOM))));
  // Call FinishDecode() on the worker task runner.
  base::RunLoop run_loop;
  worker_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ScriptDecoder::FinishDecode, base::Unretained(decoder.get()),
          CrossThreadBindOnce(
              [&](scoped_refptr<base::SequencedTaskRunner> default_task_runner,
                  base::RunLoop* run_loop, ScriptDecoder::Result result) {
                CHECK(default_task_runner->RunsTasksInCurrentSequence());

                ASSERT_FALSE(result.raw_data.empty());
                EXPECT_THAT(*result.raw_data.begin(),
                            Vector<char>(base::make_span(kFooUTF8WithBOM)));
                EXPECT_EQ(result.decoded_data, "foo");
                EXPECT_THAT(result.digest,
                            testing::Pointee(Vector<uint8_t>(
                                base::make_span(kExpectedDigest))));
                run_loop->Quit();
              },
              default_task_runner, CrossThreadUnretained(&run_loop))));
  run_loop.Run();
}

}  // namespace blink
