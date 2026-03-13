// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_win.h"

#include <windows.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "services/device/public/proto/hid_gcpw.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class ScopedStdHandle {
 public:
  ScopedStdHandle(DWORD std_handle_type, HANDLE new_handle)
      : std_handle_type_(std_handle_type),
        original_handle_(::GetStdHandle(std_handle_type_)) {
    ::SetStdHandle(std_handle_type_, new_handle);
  }

  ~ScopedStdHandle() { ::SetStdHandle(std_handle_type_, original_handle_); }

 private:
  const DWORD std_handle_type_;
  const HANDLE original_handle_;
};

class HidServiceWinTest : public ::testing::Test {
 public:
  HidServiceWinTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

}  // namespace

TEST_F(HidServiceWinTest, TestOpenDeviceThroughGcpw) {
  // 1. Set up test parameters.
  std::wstring pipe_name =
      base::StrCat({L"\\\\.\\pipe\\hid_test_pipe_",
                    base::NumberToWString(base::RandUint64())});
  std::wstring test_device_path = L"some_device_path_for_ipc";
  HANDLE expected_handle = reinterpret_cast<HANDLE>(12345);

  // 2. Set up the named pipe for IPC.
  SECURITY_ATTRIBUTES sa = {
      .nLength = sizeof(sa),
      .lpSecurityDescriptor = nullptr,
      .bInheritHandle = TRUE,
  };
  base::win::ScopedHandle server_pipe(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, /*nMaxInstances=*/1,
      /*nOutBufferSize=*/0, /*nInBufferSize=*/0, /*nDefaultTimeOut=*/0, &sa));
  ASSERT_TRUE(server_pipe.is_valid());

  base::win::ScopedHandle client_pipe(
      ::CreateFile(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                   /*dwShareMode=*/0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                   /*hTemplateFile=*/nullptr));
  ASSERT_TRUE(client_pipe.is_valid());

  // Wait for the client to connect.
  ASSERT_TRUE(::ConnectNamedPipe(server_pipe.Get(), nullptr) ||
              ::GetLastError() == ERROR_PIPE_CONNECTED);

  // 3. Run the client-side logic (GetDeviceHandleFromGcpw) in a background
  // thread.
  base::RunLoop run_loop;
  HANDLE received_handle = INVALID_HANDLE_VALUE;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](HANDLE pipe_handle, const std::wstring& device_path,
             HANDLE* out_handle, base::OnceClosure quit_closure) {
            base::win::ScopedHandle scoped_pipe(pipe_handle);
            ScopedStdHandle scoped_stdin(STD_INPUT_HANDLE, scoped_pipe.Get());
            *out_handle = HidServiceWin::OpenDeviceThroughGcpw(device_path);
            std::move(quit_closure).Run();
          },
          client_pipe.Take(), test_device_path, &received_handle,
          run_loop.QuitClosure()));

  // 4. Run the server-side logic (mock credential provider) on the main thread.
  //    Read the request from the client.
  DWORD bytes_read = 0;
  DWORD request_size = 0;
  ASSERT_TRUE(::ReadFile(server_pipe.Get(), &request_size, sizeof(request_size),
                         &bytes_read, nullptr));
  ASSERT_EQ(sizeof(request_size), bytes_read);

  std::vector<uint8_t> request_buffer(request_size);
  ASSERT_TRUE(::ReadFile(server_pipe.Get(), request_buffer.data(), request_size,
                         &bytes_read, nullptr));
  ASSERT_EQ(request_size, bytes_read);

  gcpw::HidOpenDeviceGcpwRequest request;
  ASSERT_TRUE(request.ParseFromArray(request_buffer.data(), request_size));
  ASSERT_EQ(test_device_path, base::UTF8ToWide(request.device_path()));

  // 5. Write a response back to the client.
  gcpw::HidOpenDeviceGcpwResponse response;
  response.set_device_handle(12345);
  std::vector<uint8_t> response_buffer(response.ByteSizeLong());
  response.SerializeToArray(response_buffer.data(), response_buffer.size());
  DWORD response_size = response_buffer.size();
  DWORD bytes_written = 0;

  ASSERT_TRUE(::WriteFile(server_pipe.Get(), &response_size,
                          sizeof(response_size), &bytes_written, nullptr));
  ASSERT_EQ(sizeof(response_size), bytes_written);
  if (response_size > 0) {
    ASSERT_TRUE(::WriteFile(server_pipe.Get(), response_buffer.data(),
                            response_size, &bytes_written, nullptr));
    ASSERT_EQ(response_size, bytes_written);
  }

  // 6. Wait for the background thread to finish and verify the result.
  run_loop.Run();
  ASSERT_EQ(received_handle, expected_handle);
}
TEST_F(HidServiceWinTest, TestOpenDeviceThroughGcpw_NoHandle) {
  // 1. Set up test parameters.
  std::wstring pipe_name =
      base::StrCat({L"\\\\.\\pipe\\hid_test_pipe_",
                    base::NumberToWString(base::RandUint64())});
  std::wstring test_device_path = L"some_device_path_for_ipc";

  // 2. Set up the named pipe for IPC.
  SECURITY_ATTRIBUTES sa = {
      .nLength = sizeof(sa),
      .lpSecurityDescriptor = nullptr,
      .bInheritHandle = TRUE,
  };
  base::win::ScopedHandle server_pipe(::CreateNamedPipe(
      pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, /*nMaxInstances=*/1,
      /*nOutBufferSize=*/0, /*nInBufferSize=*/0, /*nDefaultTimeOut=*/0, &sa));
  ASSERT_TRUE(server_pipe.is_valid());

  base::win::ScopedHandle client_pipe(
      ::CreateFile(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                   /*dwShareMode=*/0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                   /*hTemplateFile=*/nullptr));
  ASSERT_TRUE(client_pipe.is_valid());

  // Wait for the client to connect.
  ASSERT_TRUE(::ConnectNamedPipe(server_pipe.Get(), nullptr) ||
              ::GetLastError() == ERROR_PIPE_CONNECTED);

  // 3. Run the client-side logic (GetDeviceHandleFromGcpw) in a background
  // thread.
  base::RunLoop run_loop;
  HANDLE received_handle = reinterpret_cast<HANDLE>(12345);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](HANDLE pipe_handle, const std::wstring& device_path,
             HANDLE* out_handle, base::OnceClosure quit_closure) {
            base::win::ScopedHandle scoped_pipe(pipe_handle);
            ScopedStdHandle scoped_stdin(STD_INPUT_HANDLE, scoped_pipe.Get());
            *out_handle = HidServiceWin::OpenDeviceThroughGcpw(device_path);
            std::move(quit_closure).Run();
          },
          client_pipe.Take(), test_device_path, &received_handle,
          run_loop.QuitClosure()));

  // 4. Run the server-side logic (mock credential provider) on the main thread.
  //    Read the request from the client.
  DWORD bytes_read = 0;
  DWORD request_size = 0;
  ASSERT_TRUE(::ReadFile(server_pipe.Get(), &request_size, sizeof(request_size),
                         &bytes_read, nullptr));
  ASSERT_EQ(sizeof(request_size), bytes_read);

  std::vector<uint8_t> request_buffer(request_size);
  ASSERT_TRUE(::ReadFile(server_pipe.Get(), request_buffer.data(), request_size,
                         &bytes_read, nullptr));
  ASSERT_EQ(request_size, bytes_read);

  gcpw::HidOpenDeviceGcpwRequest request;
  ASSERT_TRUE(request.ParseFromArray(request_buffer.data(), request_size));
  ASSERT_EQ(test_device_path, base::UTF8ToWide(request.device_path()));

  // 5. Write a response back to the client. This time, do not set the handle.
  gcpw::HidOpenDeviceGcpwResponse response;
  std::vector<uint8_t> response_buffer(response.ByteSizeLong());
  response.SerializeToArray(response_buffer.data(), response_buffer.size());
  DWORD response_size = response_buffer.size();
  DWORD bytes_written = 0;

  ASSERT_TRUE(::WriteFile(server_pipe.Get(), &response_size,
                          sizeof(response_size), &bytes_written, nullptr));
  ASSERT_EQ(sizeof(response_size), bytes_written);
  if (response_size > 0) {
    ASSERT_TRUE(::WriteFile(server_pipe.Get(), response_buffer.data(),
                            response_size, &bytes_written, nullptr));
    ASSERT_EQ(response_size, bytes_written);
  }

  // 6. Wait for the background thread to finish and verify the result.
  run_loop.Run();
  ASSERT_EQ(received_handle, INVALID_HANDLE_VALUE);
}

}  // namespace device
