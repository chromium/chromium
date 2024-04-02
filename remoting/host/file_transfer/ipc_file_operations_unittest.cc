// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/file_transfer/ipc_file_operations.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "remoting/host/file_transfer/directory_helpers.h"
#include "remoting/host/file_transfer/ensure_user.h"
#include "remoting/host/file_transfer/fake_file_chooser.h"
#include "remoting/host/file_transfer/local_file_operations.h"
#include "remoting/host/file_transfer/session_file_operations_handler.h"
#include "remoting/host/file_transfer/test_byte_vector_utils.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/proto/file_transfer.pb.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

// Forward declare to allow for friending by the fake test classes.
class IpcFileOperationsTest;

namespace {

// A simplified DesktopSessionProxy implementation for file transfer testing.
class FakeDesktopSessionProxy : public IpcFileOperations::RequestHandler {
 public:
  FakeDesktopSessionProxy() = default;

  FakeDesktopSessionProxy(const FakeDesktopSessionProxy&) = delete;
  FakeDesktopSessionProxy& operator=(const FakeDesktopSessionProxy&) = delete;

  ~FakeDesktopSessionProxy() override = default;

  // IpcFileOperations::RequestHandler implementation.
  void BeginFileRead(IpcFileOperations::BeginFileReadCallback callback,
                     base::OnceClosure on_disconnect) override;
  void BeginFileWrite(const base::FilePath& file_path,
                      IpcFileOperations::BeginFileWriteCallback callback,
                      base::OnceClosure on_disconnect) override;

  // Binds the pending DesktopSessionControl remote to |remote_|.
  void Bind(mojo::PendingAssociatedRemote<mojom::DesktopSessionControl> remote);

  // When set, this instance will return |error| on the next IPC request.
  void SetErrorForNextRequest(protocol::FileTransfer_Error error);

  // Runs any registered disconnect handlers.
  void TriggerDisconnectHandlers();

 private:
  // This member mirrors the handler in the real DesktopSessionProxy class.
  base::OnceClosureList disconnect_handlers_;

  // Holds disconnect handler subscriptions until they are either triggered or
  // destroyed along with this test instance.
  std::vector<base::CallbackListSubscription> disconnect_subscriptions_;

  // If set, this will be returned on the next file transfer operation request.
  std::optional<protocol::FileTransfer_Error> request_error_;

  // Remote end of the DesktopSessionControl channel, the receiver is owned by
  // a FakeDesktopSessionAgent instance.
  mojo::AssociatedRemote<mojom::DesktopSessionControl> remote_;
};

void FakeDesktopSessionProxy::BeginFileRead(
    IpcFileOperations::BeginFileReadCallback callback,
    base::OnceClosure on_disconnect) {
  if (request_error_) {
    std::move(callback).Run(
        mojom::BeginFileReadResult::NewError(std::move(*request_error_)));
    return;
  }

  disconnect_subscriptions_.emplace_back(
      disconnect_handlers_.Add(std::move(on_disconnect)));
  remote_->BeginFileRead(std::move(callback));
}

void FakeDesktopSessionProxy::BeginFileWrite(
    const base::FilePath& file_path,
    IpcFileOperations::BeginFileWriteCallback callback,
    base::OnceClosure on_disconnect) {
  if (request_error_) {
    std::move(callback).Run(
        mojom::BeginFileWriteResult::NewError(std::move(*request_error_)));
    return;
  }
  disconnect_subscriptions_.emplace_back(
      disconnect_handlers_.Add(std::move(on_disconnect)));
  remote_->BeginFileWrite(file_path, std::move(callback));
}

void FakeDesktopSessionProxy::Bind(
    mojo::PendingAssociatedRemote<mojom::DesktopSessionControl> remote) {
  remote_.Bind(std::move(remote));
  remote_.set_disconnect_handler(
      base::BindOnce(&FakeDesktopSessionProxy::TriggerDisconnectHandlers,
                     base::Unretained(this)));
}

void FakeDesktopSessionProxy::SetErrorForNextRequest(
    protocol::FileTransfer_Error error) {
  request_error_ = std::move(error);
}

void FakeDesktopSessionProxy::TriggerDisconnectHandlers() {
  disconnect_handlers_.Notify();
}

// A simplified DesktopSessionAgent implementation for file transfer testing.
class FakeDesktopSessionAgent : public mojom::DesktopSessionControl {
 public:
  explicit FakeDesktopSessionAgent(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  FakeDesktopSessionAgent(const FakeDesktopSessionAgent&) = delete;
  FakeDesktopSessionAgent& operator=(const FakeDesktopSessionAgent&) = delete;

  ~FakeDesktopSessionAgent() override = default;

  // mojom::DesktopSessionControl implementation.
  void CreateVideoCapturer(int64_t desktop_display_id,
                           CreateVideoCapturerCallback callback) override;
  void SetScreenResolution(const ScreenResolution& resolution) override;
  void LockWorkstation() override;
  void InjectSendAttentionSequence() override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  void SetUpUrlForwarder() override;
  void SignalWebAuthnExtension() override;
  void BeginFileRead(BeginFileReadCallback callback) override;
  void BeginFileWrite(const base::FilePath& file_path,
                      BeginFileWriteCallback callback) override;

  // Binds the pending DesktopSessionControl receiver to |receiver_|.
  void Bind(
      mojo::PendingAssociatedReceiver<mojom::DesktopSessionControl> receiver);

  // When set, this instance will return |error| for its next IPC response.
  void SetErrorForNextResponse(protocol::FileTransfer_Error error);

  // Disconnect |receiver_| after the next request is received. This is used to
  // simulate an error while the FakeDesktopSessionProxy is waiting for a reply.
  void DisconnectReceiverOnNextRequest();

 private:
  friend class ::remoting::IpcFileOperationsTest;

  // If true, the agent will disconnect |receiver_| on the next IPC request.
  bool disconnect_on_next_request_ = false;

  // If set, |response_error_| will be returned in the next IPC response.
  std::optional<protocol::FileTransfer_Error> response_error_;

  // Handles file transfer requests over Mojo and manages receiver lifetimes.
  SessionFileOperationsHandler session_file_operations_handler_;

  // Receiver end of the DesktopSessionControl channel, the remote is owned by a
  // FakeDesktopSessionProxy instance.
  mojo::AssociatedReceiver<mojom::DesktopSessionControl> receiver_{this};
};

FakeDesktopSessionAgent::FakeDesktopSessionAgent(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : session_file_operations_handler_(
          std::make_unique<LocalFileOperations>(std::move(ui_task_runner))) {}

void FakeDesktopSessionAgent::CreateVideoCapturer(
    int64_t desktop_display_id,
    CreateVideoCapturerCallback callback) {}

void FakeDesktopSessionAgent::SetScreenResolution(
    const ScreenResolution& resolution) {}

void FakeDesktopSessionAgent::LockWorkstation() {}

void FakeDesktopSessionAgent::InjectSendAttentionSequence() {}

void FakeDesktopSessionAgent::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {}

void FakeDesktopSessionAgent::InjectKeyEvent(const protocol::KeyEvent& event) {}

void FakeDesktopSessionAgent::InjectMouseEvent(
    const protocol::MouseEvent& event) {}

void FakeDesktopSessionAgent::InjectTextEvent(
    const protocol::TextEvent& event) {}

void FakeDesktopSessionAgent::InjectTouchEvent(
    const protocol::TouchEvent& event) {}

void FakeDesktopSessionAgent::SetUpUrlForwarder() {}

void FakeDesktopSessionAgent::SignalWebAuthnExtension() {}

void FakeDesktopSessionAgent::BeginFileRead(BeginFileReadCallback callback) {
  if (disconnect_on_next_request_) {
    disconnect_on_next_request_ = false;
    receiver_.reset();
    return;
  }
  if (response_error_) {
    std::move(callback).Run(
        mojom::BeginFileReadResult::NewError(std::move(*response_error_)));
    return;
  }
  session_file_operations_handler_.BeginFileRead(std::move(callback));
}

void FakeDesktopSessionAgent::BeginFileWrite(const base::FilePath& file_path,
                                             BeginFileWriteCallback callback) {
  if (disconnect_on_next_request_) {
    disconnect_on_next_request_ = false;
    receiver_.reset();
    return;
  }
  if (response_error_) {
    std::move(callback).Run(
        mojom::BeginFileWriteResult::NewError(std::move(*response_error_)));
    return;
  }
  session_file_operations_handler_.BeginFileWrite(file_path,
                                                  std::move(callback));
}

void FakeDesktopSessionAgent::Bind(
    mojo::PendingAssociatedReceiver<mojom::DesktopSessionControl> receiver) {
  receiver_.Bind(std::move(receiver));
}

void FakeDesktopSessionAgent::SetErrorForNextResponse(
    protocol::FileTransfer_Error error) {
  response_error_ = std::move(error);
}

void FakeDesktopSessionAgent::DisconnectReceiverOnNextRequest() {
  disconnect_on_next_request_ = true;
}

}  // namespace

class IpcFileOperationsTest : public testing::Test {
 public:
  IpcFileOperationsTest();

  IpcFileOperationsTest(const IpcFileOperationsTest&) = delete;
  IpcFileOperationsTest& operator=(const IpcFileOperationsTest&) = delete;

  ~IpcFileOperationsTest() override;

  void SetUp() override;

 protected:
  const base::FilePath kTestFilename =
      base::FilePath::FromUTF8Unsafe("test-file.txt");
  const std::vector<std::uint8_t> kTestDataOne =
      ByteArrayFrom("this is the first test string");
  const std::vector<std::uint8_t> kTestDataTwo =
      ByteArrayFrom("this is the second test string");
  const std::vector<std::uint8_t> kTestDataThree =
      ByteArrayFrom("this is the third test string");

  base::FilePath TestDir();

  size_t session_file_reader_count() const {
    return fake_desktop_session_agent_.session_file_operations_handler_
        .file_readers_.size();
  }

  size_t session_file_writer_count() const {
    return fake_desktop_session_agent_.session_file_operations_handler_
        .file_writers_.size();
  }

  // Destroys existing MojoFileReader and MojoFileWriter instances. This will
  // also trigger any disconnect handlers set on the Mojo remote owned by the
  // corresponding IpcFileReader / IpcFileWriter instances.
  void clear_session_file_receivers() {
    fake_desktop_session_agent_.session_file_operations_handler_.file_readers_
        .Clear();
    fake_desktop_session_agent_.session_file_operations_handler_.file_writers_
        .Clear();
  }

  // Points DIR_USER_DESKTOP at a scoped temporary directory.
  base::ScopedPathOverride scoped_path_override_{base::DIR_USER_DESKTOP};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  FakeDesktopSessionProxy fake_desktop_session_proxy_;
  FakeDesktopSessionAgent fake_desktop_session_agent_{
      task_environment_.GetMainThreadTaskRunner()};

  IpcFileOperationsFactory ipc_file_operations_factory_{
      &fake_desktop_session_proxy_};

  std::unique_ptr<FileOperations> file_operations_{
      ipc_file_operations_factory_.CreateFileOperations()};
};

IpcFileOperationsTest::IpcFileOperationsTest() = default;

IpcFileOperationsTest::~IpcFileOperationsTest() = default;

void IpcFileOperationsTest::SetUp() {
  // Connect the fake proxy and agent using a pair of associated endpoints. The
  // real classes would use a pre-existing IPC channel but we don't need this
  // for our testing.
  mojo::AssociatedRemote<mojom::DesktopSessionControl> remote;
  fake_desktop_session_agent_.Bind(
      remote.BindNewEndpointAndPassDedicatedReceiver());
  fake_desktop_session_proxy_.Bind(remote.Unbind());

  DisableUserContextCheckForTesting();
  SetFileUploadDirectoryForTesting(TestDir());
}

base::FilePath IpcFileOperationsTest::TestDir() {
  base::FilePath result;
  EXPECT_TRUE(base::PathService::Get(base::DIR_USER_DESKTOP, &result));
  return result;
}

// Verifies that a file consisting of three chunks can be written successfully.
TEST_F(IpcFileOperationsTest, WritesThreeChunks) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  ASSERT_EQ(FileOperations::kCreated, writer->state());

  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    open_result = std::move(result);
                                  }));
  ASSERT_EQ(FileOperations::kBusy, writer->state());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, writer->state());
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_writer_count(), size_t{1});

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    std::optional<FileOperations::Writer::Result> write_result;
    writer->WriteChunk(chunk, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    write_result = std::move(result);
                                  }));
    ASSERT_EQ(FileOperations::kBusy, writer->state());
    task_environment_.RunUntilIdle();
    ASSERT_EQ(FileOperations::kReady, writer->state());
    ASSERT_TRUE(write_result);
    ASSERT_TRUE(*write_result);
  }

  std::optional<FileOperations::Writer::Result> close_result;
  writer->Close(
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        close_result = std::move(result);
      }));
  ASSERT_EQ(FileOperations::kBusy, writer->state());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, writer->state());
  // Verify the MojoIpcWriter instance was released.
  ASSERT_EQ(session_file_writer_count(), size_t{0});

  std::string actual_file_data;
  ASSERT_TRUE(base::ReadFileToString(TestDir().Append(kTestFilename),
                                     &actual_file_data));
  EXPECT_EQ(ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree),
            ByteArrayFrom(actual_file_data));
}

// Verifies that dropping early cancels the remote writer.
TEST_F(IpcFileOperationsTest, DroppingCancelsRemoteWriter) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    open_result = std::move(result);
                                  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_writer_count(), size_t{1});

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    std::optional<FileOperations::Writer::Result> write_result;
    writer->WriteChunk(chunk, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    write_result = std::move(result);
                                  }));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(write_result);
    ASSERT_TRUE(*write_result);
  }

  writer.reset();
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(base::IsDirectoryEmpty(TestDir()));

  // Verify the MojoIpcWriter instance was released.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
}

// Verifies that dropping works while an operation is pending.
TEST_F(IpcFileOperationsTest, CancelsWhileOperationPending) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    open_result = std::move(result);
                                  }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_writer_count(), size_t{1});

  std::optional<FileOperations::Writer::Result> write_result;
  writer->WriteChunk(
      kTestDataOne,
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        write_result = std::move(result);
      }));

  EXPECT_EQ(FileOperations::kBusy, writer->state());
  writer.reset();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(write_result);
  EXPECT_TRUE(base::IsDirectoryEmpty(TestDir()));

  // Verify the MojoIpcWriter instance was released.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
}

// Verifies that a file can be successfully read in three chunks.
TEST_F(IpcFileOperationsTest, ReadsThreeChunks) {
  base::FilePath path = TestDir().Append(kTestFilename);
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  ASSERT_TRUE(base::WriteFile(path, contents));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  ASSERT_EQ(FileOperations::kCreated, reader->state());

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  ASSERT_EQ(FileOperations::kBusy, reader->state());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    std::optional<FileOperations::Reader::ReadResult> read_result;
    reader->ReadChunk(chunk.size(),
                      base::BindLambdaForTesting(
                          [&](FileOperations::Reader::ReadResult result) {
                            read_result = std::move(result);
                          }));
    ASSERT_EQ(FileOperations::kBusy, reader->state());
    task_environment_.RunUntilIdle();
    ASSERT_EQ(FileOperations::kReady, reader->state());
    ASSERT_TRUE(read_result);
    ASSERT_TRUE(*read_result);
    EXPECT_EQ(chunk, **read_result);
  }
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  reader.reset();
  task_environment_.RunUntilIdle();

  // Verify the MojoIpcReader instance was released.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Verifies proper EOF handling.
TEST_F(IpcFileOperationsTest, ReaderHandlesEof) {
  constexpr std::size_t kOverreadAmount = 5;
  base::FilePath path = TestDir().Append(kTestFilename);
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  ASSERT_TRUE(base::WriteFile(path, contents));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(
      contents.size() +
          kOverreadAmount,  // Attempt to read more than is in file.
      base::BindLambdaForTesting(
          [&](FileOperations::Reader::ReadResult result) {
            read_result = std::move(result);
          }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(contents, **read_result);
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  read_result.reset();
  reader->ReadChunk(kOverreadAmount,
                    base::BindLambdaForTesting(
                        [&](FileOperations::Reader::ReadResult result) {
                          read_result = std::move(result);
                        }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(std::size_t{0}, (*read_result)->size());

  // Verify the MojoIpcReader instance was released.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Verifies proper handling of zero-size file
TEST_F(IpcFileOperationsTest, ReaderHandlesZeroSize) {
  constexpr std::size_t kChunkSize = 5;
  base::FilePath path = TestDir().Append(kTestFilename);
  ASSERT_TRUE(base::WriteFile(path, ""));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(kChunkSize,
                    base::BindLambdaForTesting(
                        [&](FileOperations::Reader::ReadResult result) {
                          read_result = std::move(result);
                        }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kComplete, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*read_result);
  EXPECT_EQ(std::size_t{0}, (*read_result)->size());

  // Verify the MojoIpcReader instance was released.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Concurrent Read operations are handled and valid data is retuned.
TEST_F(IpcFileOperationsTest, ConcurrentReadOperationsSupported) {
  base::FilePath base_path = TestDir().Append(kTestFilename);
  std::vector<base::FilePath> paths{
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(0)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(1)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(2)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(3)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(4)"))};

  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  for (const auto& path : paths) {
    ASSERT_TRUE(base::WriteFile(path, contents));
  }

  std::vector<std::unique_ptr<FileOperations::Reader>> readers;
  for (size_t i = 0; i < paths.size(); i++) {
    readers.emplace_back(file_operations_->CreateReader());
  }

  int reader_count = static_cast<int>(readers.size());
  for (int i = 0; i < reader_count; i++) {
    FakeFileChooser::SetResult(paths[i]);
    std::optional<FileOperations::Reader::OpenResult> open_result;
    readers[i]->Open(base::BindLambdaForTesting(
        [&](FileOperations::Reader::OpenResult result) {
          open_result = std::move(result);
        }));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(open_result);
    ASSERT_TRUE(*open_result);
    ASSERT_EQ(session_file_reader_count(), static_cast<size_t>(i + 1));
  }
  ASSERT_EQ(session_file_reader_count(), readers.size());

  for (int i = 0; i < reader_count; i++) {
    for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
      std::optional<FileOperations::Reader::ReadResult> read_result;
      readers[i]->ReadChunk(chunk.size(),
                            base::BindLambdaForTesting(
                                [&](FileOperations::Reader::ReadResult result) {
                                  read_result = std::move(result);
                                }));
      ASSERT_EQ(FileOperations::kBusy, readers[i]->state());
      task_environment_.RunUntilIdle();
      ASSERT_EQ(FileOperations::kReady, readers[i]->state());
      ASSERT_TRUE(read_result);
      ASSERT_TRUE(*read_result);
      EXPECT_EQ(chunk, **read_result);
    }
  }
  ASSERT_EQ(session_file_reader_count(), readers.size());

  for (int i = reader_count - 1; i >= 0; i--) {
    std::optional<FileOperations::Reader::ReadResult> read_result;
    // Simulate EOF by reading 1 additional byte.
    readers[i]->ReadChunk(1,
                          base::BindLambdaForTesting(
                              [&](FileOperations::Reader::ReadResult result) {
                                read_result = std::move(result);
                              }));
    ASSERT_EQ(FileOperations::kBusy, readers[i]->state());
    task_environment_.RunUntilIdle();
    ASSERT_EQ(FileOperations::kComplete, readers[i]->state());
    ASSERT_TRUE(read_result);
    // Verify each MojoIpcReader instance is released as it completes and that
    // other instances are retained.
    ASSERT_EQ(session_file_reader_count(), static_cast<size_t>(i));
  }
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Concurrent Write operations handled.
TEST_F(IpcFileOperationsTest, ConcurrentWriteOperationsSupported) {
  base::FilePath base_path = TestDir().Append(kTestFilename);
  std::vector<base::FilePath> paths{
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(0)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(1)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(2)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(3)")),
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(4)"))};

  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);

  std::vector<std::unique_ptr<FileOperations::Writer>> writers;
  for (size_t i = 0; i < paths.size(); i++) {
    writers.emplace_back(file_operations_->CreateWriter());
  }

  int writer_count = static_cast<int>(writers.size());
  for (int i = 0; i < writer_count; i++) {
    std::optional<FileOperations::Writer::Result> open_result;
    writers[i]->Open(paths[i], base::BindLambdaForTesting(
                                   [&](FileOperations::Writer::Result result) {
                                     open_result = std::move(result);
                                   }));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(open_result);
    ASSERT_TRUE(*open_result);
    ASSERT_EQ(session_file_writer_count(), static_cast<size_t>(i + 1));
  }
  ASSERT_EQ(session_file_writer_count(), writers.size());

  for (const auto& chunk : {kTestDataOne, kTestDataTwo, kTestDataThree}) {
    for (int i = 0; i < writer_count; i++) {
      std::optional<FileOperations::Writer::Result> write_result;
      writers[i]->WriteChunk(chunk,
                             base::BindLambdaForTesting(
                                 [&](FileOperations::Writer::Result result) {
                                   write_result = std::move(result);
                                 }));
      EXPECT_EQ(writers[i]->state(), FileOperations::kBusy);
      task_environment_.RunUntilIdle();
      EXPECT_EQ(writers[i]->state(), FileOperations::kReady);
      ASSERT_TRUE(write_result);
      ASSERT_FALSE(write_result->is_error());
      ASSERT_TRUE(*write_result);
    }
  }
  ASSERT_EQ(session_file_writer_count(), writers.size());

  for (int i = writer_count - 1; i >= 0; i--) {
    std::optional<FileOperations::Writer::Result> close_result;
    writers[i]->Close(
        base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
          close_result = std::move(result);
        }));
    ASSERT_EQ(writers[i]->state(), FileOperations::kBusy);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(writers[i]->state(), FileOperations::kComplete);
    ASSERT_TRUE(close_result);
    ASSERT_FALSE(close_result->is_error());
    ASSERT_TRUE(*close_result);
    // Verify each MojoIpcWriter instance is released as it completes and that
    // any other instances are still retained.
    ASSERT_EQ(session_file_writer_count(), static_cast<size_t>(i));
  }
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_FALSE(base::IsDirectoryEmpty(TestDir()));
}

// Concurrent Read and Write operations handled.
TEST_F(IpcFileOperationsTest, ConcurrentReadAndWriteOperationsSupported) {
  std::vector<std::uint8_t> contents =
      ByteArrayFrom(kTestDataOne, kTestDataTwo, kTestDataThree);
  base::FilePath base_path = TestDir().Append(kTestFilename);

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  // Write to the read path first so that the writer can select a 'unique' file
  // which doesn't conflict with this |read_path|.
  base::FilePath read_path(
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(read)")));
  ASSERT_TRUE(base::WriteFile(read_path, contents));

  // Pending open file operations.
  std::optional<FileOperations::Writer::Result> open_for_write_result;
  writer->Open(
      base_path.InsertBeforeExtension(FILE_PATH_LITERAL("(write)")),
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        open_for_write_result = std::move(result);
      }));
  FakeFileChooser::SetResult(read_path);
  std::optional<FileOperations::Reader::OpenResult> open_for_read_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_for_read_result = std::move(result);
      }));
  EXPECT_EQ(writer->state(), FileOperations::kBusy);
  EXPECT_EQ(reader->state(), FileOperations::kBusy);
  // Complete the operations.
  task_environment_.RunUntilIdle();
  // Validate results.
  EXPECT_EQ(writer->state(), FileOperations::kReady);
  EXPECT_EQ(reader->state(), FileOperations::kReady);
  ASSERT_TRUE(open_for_write_result);
  ASSERT_TRUE(open_for_read_result);
  ASSERT_TRUE(*open_for_write_result);
  ASSERT_TRUE(*open_for_read_result);
  ASSERT_EQ(session_file_writer_count(), size_t{1});
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  // Pending write operation.
  std::optional<FileOperations::Writer::Result> write_result;
  writer->WriteChunk(contents, base::BindLambdaForTesting(
                                   [&](FileOperations::Writer::Result result) {
                                     write_result = std::move(result);
                                   }));
  // Pending read operation.
  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(contents.size(),
                    base::BindLambdaForTesting(
                        [&](FileOperations::Reader::ReadResult result) {
                          read_result = std::move(result);
                        }));
  EXPECT_EQ(writer->state(), FileOperations::kBusy);
  EXPECT_EQ(reader->state(), FileOperations::kBusy);
  // Complete the pending operations.
  task_environment_.RunUntilIdle();
  // Validate the results.
  EXPECT_EQ(writer->state(), FileOperations::kReady);
  EXPECT_EQ(reader->state(), FileOperations::kReady);
  ASSERT_TRUE(write_result);
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(*write_result);
  ASSERT_TRUE(*read_result);
  ASSERT_EQ(session_file_writer_count(), size_t{1});
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  // Close the writer.
  std::optional<FileOperations::Writer::Result> close_result;
  writer->Close(
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        close_result = std::move(result);
      }));
  ASSERT_EQ(writer->state(), FileOperations::kBusy);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(writer->state(), FileOperations::kComplete);
  ASSERT_TRUE(close_result);
  ASSERT_TRUE(*close_result);
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  // Close the reader by reading 1 additional byte to simulate EOF.
  reader->ReadChunk(1, base::BindLambdaForTesting(
                           [&](FileOperations::Reader::ReadResult result) {
                             read_result = std::move(result);
                           }));
  ASSERT_EQ(reader->state(), FileOperations::kBusy);
  task_environment_.RunUntilIdle();
  ASSERT_EQ(reader->state(), FileOperations::kComplete);
  ASSERT_TRUE(read_result);
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_EQ(session_file_reader_count(), size_t{0});

  ASSERT_FALSE(base::IsDirectoryEmpty(TestDir()));

  // Reset the handlers, then trigger a 'disconnect' to verify it is a no-op.
  reader.reset();
  writer.reset();
  fake_desktop_session_proxy_.TriggerDisconnectHandlers();
}

// Verify a file chooser error is propagated.
TEST_F(IpcFileOperationsTest, ReaderPropagatesErrorFromFileChooser) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  // Currently non-existent file.
  FakeFileChooser::SetResult(TestDir().Append(kTestFilename));
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(open_result);
  ASSERT_FALSE(*open_result);
  // Verify the MojoIpcReader instance was not retained.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Verify IpcFileReader handles an error returned from the request handler.
TEST_F(IpcFileOperationsTest, ErrorReturnedByFileReadRequestHandler) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  fake_desktop_session_proxy_.SetErrorForNextRequest(
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  ASSERT_TRUE(open_result->is_error());
  ASSERT_EQ(reader->state(), FileOperations::kFailed);
  // Verify the MojoIpcReader instance was not created.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Verify IpcFileWriter handles an error returned from the request handler.
TEST_F(IpcFileOperationsTest, ErrorReturnedByFileWriteRequestHandler) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  fake_desktop_session_proxy_.SetErrorForNextRequest(
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(
      TestDir().Append(kTestFilename),
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        open_result = std::move(result);
      }));
  ASSERT_TRUE(open_result->is_error());
  ASSERT_EQ(writer->state(), FileOperations::kFailed);
  // Verify the MojoIpcWriter instance was not created.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

// Verify IpcFileReader handles an error returned from the mojo receiver.
TEST_F(IpcFileOperationsTest, ErrorReturnedByMojoReceiverForFileRead) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  fake_desktop_session_agent_.SetErrorForNextResponse(
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result->is_error());
  ASSERT_EQ(reader->state(), FileOperations::kFailed);
  // Verify a MojoIpcReader instance was not created.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

// Verify IpcFileWriter handles an error returned from the mojo receiver.
TEST_F(IpcFileOperationsTest, ErrorReturnedByMojoReceiverForFileWrite) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  fake_desktop_session_agent_.SetErrorForNextResponse(
      protocol::MakeFileTransferError(
          FROM_HERE, protocol::FileTransfer_Error_Type_UNEXPECTED_ERROR));
  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(
      TestDir().Append(kTestFilename),
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result->is_error());
  ASSERT_EQ(writer->state(), FileOperations::kFailed);
  // Verify the MojoIpcWriter instance was not retained.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

TEST_F(IpcFileOperationsTest, ReaderNotifiedOfIpcChannelDisconnect) {
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  // This will trigger the DesktopSessionControl remote disconnect handler.
  fake_desktop_session_agent_.DisconnectReceiverOnNextRequest();
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  // Verify a MojoIpcReader instance was not created.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
  ASSERT_EQ(reader->state(), FileOperations::kFailed);
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(open_result->is_error());
}

TEST_F(IpcFileOperationsTest, WriterNotifiedOfIpcChannelDisconnect) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  // This will trigger the DesktopSessionControl remote disconnect handler.
  fake_desktop_session_agent_.DisconnectReceiverOnNextRequest();
  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(
      TestDir().Append(kTestFilename),
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  // Verify a MojoIpcWriter instance was not created.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  ASSERT_EQ(writer->state(), FileOperations::kFailed);
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(open_result->is_error());
  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

TEST_F(IpcFileOperationsTest, ErrorWhenReadChunkCalledBeforeOpen) {
  constexpr std::size_t kChunkSize = 5;
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();
  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(kChunkSize,
                    base::BindLambdaForTesting(
                        [&](FileOperations::Reader::ReadResult result) {
                          read_result = std::move(result);
                        }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(read_result->is_error());
  ASSERT_FALSE(*read_result);
  // Verify a MojoIpcReader instance was not created.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
}

TEST_F(IpcFileOperationsTest, ErrorWhenWriteChunkCalledBeforeOpen) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  std::optional<FileOperations::Writer::Result> write_result;
  writer->WriteChunk(
      std::vector<uint8_t>{16, 16, 16, 16, 16},
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        write_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, writer->state());
  ASSERT_TRUE(write_result);
  ASSERT_TRUE(write_result->is_error());
  ASSERT_FALSE(*write_result);
  // Verify a MojoIpcWriter instance was not created.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
}

TEST_F(IpcFileOperationsTest, ErrorWhenCloseCalledBeforeOpen) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();
  std::optional<FileOperations::Writer::Result> close_result;
  writer->Close(
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        close_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, writer->state());
  ASSERT_TRUE(close_result);
  ASSERT_TRUE(close_result->is_error());
  ASSERT_FALSE(*close_result);
  // Verify a MojoIpcWriter instance was not created.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
}

TEST_F(IpcFileOperationsTest, ErrorWhenReadChunkCalledAfterReceiverDisconnect) {
  constexpr std::size_t kChunkSize = 5;
  base::FilePath path = TestDir().Append(kTestFilename);
  ASSERT_TRUE(base::WriteFile(path, ""));

  std::unique_ptr<FileOperations::Reader> reader =
      file_operations_->CreateReader();

  FakeFileChooser::SetResult(path);
  std::optional<FileOperations::Reader::OpenResult> open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        open_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(open_result);
  ASSERT_TRUE(*open_result);
  ASSERT_EQ(session_file_reader_count(), size_t{1});

  clear_session_file_receivers();
  // Verify the MojoIpcReader instance was released.
  ASSERT_EQ(session_file_reader_count(), size_t{0});
  task_environment_.RunUntilIdle();

  std::optional<FileOperations::Reader::ReadResult> read_result;
  reader->ReadChunk(kChunkSize,
                    base::BindLambdaForTesting(
                        [&](FileOperations::Reader::ReadResult result) {
                          read_result = std::move(result);
                        }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, reader->state());
  ASSERT_TRUE(read_result);
  ASSERT_TRUE(read_result->is_error());
  ASSERT_FALSE(*read_result);
}

TEST_F(IpcFileOperationsTest,
       ErrorWhenWriteChunkCalledAfterReceiverDisconnect) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    open_result = std::move(result);
                                  }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, writer->state());
  ASSERT_EQ(session_file_writer_count(), size_t{1});

  clear_session_file_receivers();
  // Verify the MojoIpcWriter instance was released.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  task_environment_.RunUntilIdle();

  std::optional<FileOperations::Writer::Result> write_result;
  writer->WriteChunk(
      std::vector<uint8_t>{16, 16, 16, 16, 16},
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        write_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, writer->state());
  ASSERT_TRUE(write_result);
  ASSERT_TRUE(write_result->is_error());
  ASSERT_FALSE(*write_result);
  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

TEST_F(IpcFileOperationsTest, ErrorWhenCloseCalledAfterReceiverDisconnect) {
  std::unique_ptr<FileOperations::Writer> writer =
      file_operations_->CreateWriter();

  std::optional<FileOperations::Writer::Result> open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    open_result = std::move(result);
                                  }));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(FileOperations::kReady, writer->state());
  ASSERT_EQ(session_file_writer_count(), size_t{1});

  clear_session_file_receivers();
  // Verify the MojoIpcWriter instance was released.
  ASSERT_EQ(session_file_writer_count(), size_t{0});
  task_environment_.RunUntilIdle();

  std::optional<FileOperations::Writer::Result> close_result;
  writer->Close(
      base::BindLambdaForTesting([&](FileOperations::Writer::Result result) {
        close_result = std::move(result);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FileOperations::kFailed, writer->state());
  ASSERT_TRUE(close_result);
  ASSERT_TRUE(close_result->is_error());
  ASSERT_FALSE(*close_result);
  ASSERT_TRUE(base::IsDirectoryEmpty(TestDir()));
}

TEST_F(IpcFileOperationsTest,
       ErrorWhenUsingExistingFileOperationsAfterFactoryDestroyed) {
  std::unique_ptr<IpcFileOperationsFactory> ipc_file_operations_factory =
      std::make_unique<IpcFileOperationsFactory>(&fake_desktop_session_proxy_);
  std::unique_ptr<FileOperations> file_operations =
      ipc_file_operations_factory->CreateFileOperations();

  std::unique_ptr<FileOperations::Writer> writer =
      file_operations->CreateWriter();
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations->CreateReader();

  ipc_file_operations_factory.reset();

  std::optional<FileOperations::Writer::Result> writer_open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    writer_open_result = std::move(result);
                                  }));
  // We expect this to immediately fail.
  ASSERT_EQ(writer->state(), FileOperations::kFailed);
  ASSERT_TRUE(writer_open_result->is_error());

  std::optional<FileOperations::Reader::OpenResult> reader_open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        reader_open_result = std::move(result);
      }));
  // We expect this to immediately fail.
  ASSERT_EQ(reader->state(), FileOperations::kFailed);
  ASSERT_TRUE(reader_open_result->is_error());
}

TEST_F(IpcFileOperationsTest,
       ErrorWhenUsingNewFileOperationsAfterFactoryDestroyed) {
  std::unique_ptr<IpcFileOperationsFactory> ipc_file_operations_factory =
      std::make_unique<IpcFileOperationsFactory>(&fake_desktop_session_proxy_);
  std::unique_ptr<FileOperations> file_operations =
      ipc_file_operations_factory->CreateFileOperations();

  ipc_file_operations_factory.reset();

  std::unique_ptr<FileOperations::Writer> writer =
      file_operations->CreateWriter();
  std::unique_ptr<FileOperations::Reader> reader =
      file_operations->CreateReader();

  std::optional<FileOperations::Writer::Result> writer_open_result;
  writer->Open(kTestFilename, base::BindLambdaForTesting(
                                  [&](FileOperations::Writer::Result result) {
                                    writer_open_result = std::move(result);
                                  }));
  // We expect this to immediately fail.
  ASSERT_EQ(writer->state(), FileOperations::kFailed);
  ASSERT_TRUE(writer_open_result->is_error());

  std::optional<FileOperations::Reader::OpenResult> reader_open_result;
  reader->Open(base::BindLambdaForTesting(
      [&](FileOperations::Reader::OpenResult result) {
        reader_open_result = std::move(result);
      }));
  // We expect this to immediately fail.
  ASSERT_EQ(reader->state(), FileOperations::kFailed);
  ASSERT_TRUE(reader_open_result->is_error());
}

}  // namespace remoting
