// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_decoder.h"

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

const unsigned char kFooUTF8WithBOM[] = {0xef, 0xbb, 0xbf, 0x66, 0x6f, 0x6f};
// SHA256 hash of 'foo\1' in hex (the end byte indicates the character width):
//   python3 -c "print('foo\1', end='')" | sha256sum | xxd -r -p | xxd -i
const unsigned char kExpectedDigest[] = {
    0xb9, 0x95, 0x4a, 0x58, 0x84, 0xf9, 0x8c, 0xce, 0x22, 0x57, 0x2a,
    0xd0, 0xf0, 0xd8, 0xb8, 0x42, 0x5b, 0x19, 0x6d, 0xca, 0xba, 0xc9,
    0xf7, 0xcb, 0xec, 0x9f, 0x14, 0x27, 0x93, 0x0d, 0x8c, 0x44};

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
  void DidFinishLoadingBody() override { NOTREACHED(); }
  void DidFailLoadingBody() override { NOTREACHED(); }
  void DidCancelLoadingBody() override { NOTREACHED(); }

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
  decoder->DidReceiveData(Vector<char>(base::span(kFooUTF8WithBOM)),
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
              Vector<char>(base::span(kFooUTF8WithBOM)));
  EXPECT_EQ(client->decoded_data(), "foo");
  EXPECT_THAT(client->digest(),
              testing::Pointee(Vector<uint8_t>(base::span(kExpectedDigest))));
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

  auto data_span = base::as_chars(base::span(kFooUTF8WithBOM));
  const auto [first_chunk, second_chunk] = data_span.split_at<3>();

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
  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(&ScriptDecoderWithClient::DidReceiveData,
                          CrossThreadUnretained(decoder.get()),
                          Vector<char>(second_chunk),
                          /*send_to_client=*/true));

  // Call FinishDecode() on the worker task runner.
  base::RunLoop run_loop;
  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &ScriptDecoderWithClient::FinishDecode,
          CrossThreadUnretained(decoder.get()),
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
  EXPECT_THAT(client->digest(),
              testing::Pointee(Vector<uint8_t>(base::span(kExpectedDigest))));
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
  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(&ScriptDecoder::DidReceiveData,
                          CrossThreadUnretained(decoder.get()),
                          Vector<char>(base::span(kFooUTF8WithBOM))));
  // Call FinishDecode() on the worker task runner.
  base::RunLoop run_loop;
  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &ScriptDecoder::FinishDecode, CrossThreadUnretained(decoder.get()),
          CrossThreadBindOnce(
              [&](scoped_refptr<base::SequencedTaskRunner> default_task_runner,
                  base::RunLoop* run_loop, ScriptDecoder::Result result) {
                CHECK(default_task_runner->RunsTasksInCurrentSequence());

                ASSERT_FALSE(result.raw_data.empty());
                EXPECT_THAT(*result.raw_data.begin(),
                            Vector<char>(base::span(kFooUTF8WithBOM)));
                EXPECT_EQ(result.decoded_data, "foo");
                EXPECT_THAT(result.digest, testing::Pointee(Vector<uint8_t>(
                                               base::span(kExpectedDigest))));
                run_loop->Quit();
              },
              default_task_runner, CrossThreadUnretained(&run_loop))));
  run_loop.Run();
}

}  // namespace blink
