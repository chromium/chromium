// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/request_body_collector.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/test/test_data_pipe_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class RequestBodyCollectorTest : public ::testing::Test {
 protected:
  using Result = std::vector<RequestBodyCollector::BodyEntry>;

  RequestBodyCollectorTest() = default;
  ~RequestBodyCollectorTest() override = default;

  base::OnceCallback<void(Result result)> MakeCallback() {
    return base::BindOnce(&RequestBodyCollectorTest::SetResult,
                          base::Unretained(this));
  }

  static std::unique_ptr<network::TestDataPipeGetter> AddPipeGetterWithData(
      network::ResourceRequestBody& body,
      std::string data) {
    mojo::PendingRemote<network::mojom::DataPipeGetter> pipe_getter_remote;
    auto pipe_getter = std::make_unique<network::TestDataPipeGetter>(
        std::move(data), pipe_getter_remote.InitWithNewPipeAndPassReceiver());
    body.AppendDataPipe(std::move(pipe_getter_remote));
    return pipe_getter;
  }

  void WaitCompletion() {
    base::RunLoop run_loop;
    quit_loop_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  Result result_;
  std::vector<std::string_view> text_result_;

 private:
  void SetResult(Result result) {
    result_ = std::move(result);
    std::transform(
        result_.begin(), result_.end(), std::back_inserter(text_result_),
        [](const RequestBodyCollector::BodyEntry& entry) {
          return entry.has_value()
                     ? std::string_view(
                           reinterpret_cast<const char*>(entry->data()),
                           entry->size())
                     : entry.error();
        });
    if (quit_loop_callback_) {
      std::move(quit_loop_callback_).Run();
    }
  }

  BrowserTaskEnvironment task_environment_;
  base::OnceClosure quit_loop_callback_;
};

static constexpr char kBytes[] = "don't panic!";

TEST_F(RequestBodyCollectorTest, Empty) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());
  EXPECT_THAT(collector, testing::IsNull());
  EXPECT_THAT(result_, testing::IsEmpty());
}

TEST_F(RequestBodyCollectorTest, OnlyBytes) {
  auto body =
      network::ResourceRequestBody::CreateFromBytes(kBytes, strlen(kBytes));
  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());
  EXPECT_THAT(collector, testing::IsNull());
  EXPECT_THAT(text_result_, testing::ElementsAre(kBytes));
}

TEST_F(RequestBodyCollectorTest, UnsupportedType) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("/etc/passwd")), 0, 42,
                        base::Time::Now());
  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());
  EXPECT_THAT(collector, testing::IsNull());
  EXPECT_THAT(text_result_, testing::ElementsAre("Unsupported entry"));
}

TEST_F(RequestBodyCollectorTest, Pipe) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  auto pipe_getter = AddPipeGetterWithData(*body, kBytes);

  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());

  EXPECT_THAT(collector, testing::NotNull());
  WaitCompletion();
  EXPECT_THAT(text_result_, testing::ElementsAre(kBytes));
}

TEST_F(RequestBodyCollectorTest, PipeInsufficientData) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  auto pipe_getter = AddPipeGetterWithData(*body, kBytes);

  pipe_getter->set_pipe_closed_early(true);

  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());

  EXPECT_THAT(collector, testing::NotNull());
  WaitCompletion();
  EXPECT_THAT(text_result_, testing::ElementsAre("Unexpected end of data"));
}

TEST_F(RequestBodyCollectorTest, PipeError) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  auto pipe_getter = AddPipeGetterWithData(*body, kBytes);

  pipe_getter->set_start_error(13);

  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());

  EXPECT_THAT(collector, testing::NotNull());
  WaitCompletion();
  EXPECT_THAT(text_result_, testing::ElementsAre("Error reading blob"));
}

TEST_F(RequestBodyCollectorTest, PipeDisconnect) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();

  mojo::PendingRemote<network::mojom::DataPipeGetter> pipe_getter_remote;
  mojo::PendingReceiver<network::mojom::DataPipeGetter> pipe_getter_receiver(
      pipe_getter_remote.InitWithNewPipeAndPassReceiver());

  body->AppendDataPipe(std::move(pipe_getter_remote));

  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());
  EXPECT_THAT(collector, testing::NotNull());

  pipe_getter_receiver.reset();

  WaitCompletion();
  EXPECT_THAT(text_result_, testing::ElementsAre("Error reading blob"));
}

TEST_F(RequestBodyCollectorTest, EarlyTermination) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();

  mojo::PendingRemote<network::mojom::DataPipeGetter> pipe_getter_remote;
  mojo::PendingReceiver<network::mojom::DataPipeGetter> pipe_getter_receiver(
      pipe_getter_remote.InitWithNewPipeAndPassReceiver());

  body->AppendDataPipe(std::move(pipe_getter_remote));

  auto collector = RequestBodyCollector::Collect(
      *body, base::BindOnce([](Result) { FAIL() << "Should not be called"; }));
  collector.reset();

  EXPECT_THAT(text_result_, testing::IsEmpty());
}

TEST_F(RequestBodyCollectorTest, AllTogertherNow) {
  static constexpr char kDelimiter[] = "-------------------";

  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendBytes(kBytes, strlen(kBytes));

  auto pipe1 = AddPipeGetterWithData(*body, "Pipe bytes 1");
  body->AppendBytes(kDelimiter, strlen(kDelimiter));
  auto pipe2 = AddPipeGetterWithData(*body, "Pipe bytes 2");
  body->AppendBytes(kDelimiter, strlen(kDelimiter));
  auto pipe3 = AddPipeGetterWithData(*body, "");
  pipe3->set_pipe_closed_early(true);
  body->AppendBytes(kDelimiter, strlen(kDelimiter));
  auto pipe4 = AddPipeGetterWithData(*body, "");
  body->AppendBytes(kDelimiter, strlen(kDelimiter));

  auto collector = RequestBodyCollector::Collect(*body, MakeCallback());
  EXPECT_THAT(collector, testing::NotNull());
  pipe4.reset();

  WaitCompletion();
  EXPECT_THAT(
      text_result_,
      testing::ElementsAre(kBytes, "Pipe bytes 1", kDelimiter, "Pipe bytes 2",
                           kDelimiter, "Unexpected end of data", kDelimiter,
                           "Error reading blob", kDelimiter));
}

}  // namespace
}  // namespace content
