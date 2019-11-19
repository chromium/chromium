// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ftp/ftp_request_info.h"
#include "net/socket/socket_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using net::test::IsError;
using net::test::IsOk;

namespace {

// Size we use for IOBuffers used to receive data from the test data socket.
const int kBufferSize = 128;

}  // namespace

namespace net {

class FtpSocketDataProvider : public SocketDataProvider {
 public:
  enum State {
    NONE,
    PRE_USER,
    PRE_PASSWD,
    PRE_SYST,
    PRE_PWD,
    PRE_TYPE,
    PRE_SIZE,
    PRE_LIST_EPSV,
    PRE_LIST_PASV,
    PRE_LIST,
    PRE_RETR,
    PRE_RETR_EPSV,
    PRE_RETR_PASV,
    PRE_CWD,
    PRE_QUIT,
    PRE_NOPASV,
    QUIT
  };

  FtpSocketDataProvider()
      : short_read_limit_(0),
        allow_unconsumed_reads_(false),
        failure_injection_state_(NONE),
        multiline_welcome_(false),
        use_epsv_(true),
        data_type_('I') {
    Init();
  }
  ~FtpSocketDataProvider() override = default;

  // SocketDataProvider implementation.
  MockRead OnRead() override {
    if (reads_.empty())
      return MockRead(SYNCHRONOUS, ERR_UNEXPECTED);
    MockRead result = reads_.front();
    if (short_read_limit_ == 0 || result.data_len <= short_read_limit_) {
      reads_.pop_front();
    } else {
      result.data_len = short_read_limit_;
      reads_.front().data += result.data_len;
      reads_.front().data_len -= result.data_len;
    }
    return result;
  }

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify("USER anonymous\r\n", data, PRE_PASSWD,
                      "331 Password needed\r\n");
      case PRE_PASSWD:
        {
          static const char response_one[] = "230 Welcome\r\n";
          static const char response_multi[] =
              "230- One\r\n230- Two\r\n230 Three\r\n";
          return Verify("PASS chrome@example.com\r\n", data, PRE_SYST,
                        multiline_welcome_ ? response_multi : response_one);
        }
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 UNIX\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"/\" is your current location\r\n");
      case PRE_TYPE:
        return Verify(std::string("TYPE ") + data_type_ + "\r\n", data,
                      PRE_SIZE, "200 TYPE set successfully\r\n");
      case PRE_LIST_EPSV:
        return Verify("EPSV\r\n", data, PRE_LIST,
                      "227 Entering Extended Passive Mode (|||31744|)\r\n");
      case PRE_LIST_PASV:
        return Verify("PASV\r\n", data, PRE_LIST,
                      "227 Entering Passive Mode 127,0,0,1,123,123\r\n");
      case PRE_RETR_EPSV:
        return Verify("EPSV\r\n", data, PRE_RETR,
                      "227 Entering Extended Passive Mode (|||31744|)\r\n");
      case PRE_RETR_PASV:
        return Verify("PASV\r\n", data, PRE_RETR,
                      "227 Entering Passive Mode 127,0,0,1,123,123\r\n");
      case PRE_NOPASV:
        // Use unallocated 599 FTP error code to make sure it falls into the
        // generic ERR_FTP_FAILED bucket.
        return Verify("PASV\r\n", data, PRE_QUIT,
                      "599 fail\r\n");
      case PRE_QUIT:
        return Verify("QUIT\r\n", data, QUIT, "221 Goodbye.\r\n");
      default:
        NOTREACHED() << "State not handled " << state();
        return MockWriteResult(ASYNC, ERR_UNEXPECTED);
    }
  }

  void InjectFailure(State state, State next_state, const char* response) {
    DCHECK_EQ(NONE, failure_injection_state_);
    DCHECK_NE(NONE, state);
    DCHECK_NE(NONE, next_state);
    DCHECK_NE(state, next_state);
    failure_injection_state_ = state;
    failure_injection_next_state_ = next_state;
    fault_response_ = response;
  }

  State state() const {
    return state_;
  }

  void Reset() override {
    reads_.clear();
    Init();
  }

  bool AllReadDataConsumed() const override { return state_ == QUIT; }

  bool AllWriteDataConsumed() const override { return state_ == QUIT; }

  void set_multiline_welcome(bool multiline) { multiline_welcome_ = multiline; }

  bool use_epsv() const { return use_epsv_; }
  void set_use_epsv(bool use_epsv) { use_epsv_ = use_epsv; }

  void set_data_type(char data_type) { data_type_ = data_type; }

  int short_read_limit() const { return short_read_limit_; }
  void set_short_read_limit(int limit) { short_read_limit_ = limit; }

  void set_allow_unconsumed_reads(bool allow) {
    allow_unconsumed_reads_ = allow;
  }

 protected:
  void Init() {
    state_ = PRE_USER;
    SimulateRead("220 host TestFTPd\r\n");
  }

  // If protocol fault injection has been requested, adjusts state and mocked
  // read and returns true.
  bool InjectFault() {
    if (state_ != failure_injection_state_)
      return false;
    SimulateRead(fault_response_);
    state_ = failure_injection_next_state_;
    return true;
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read,
                         const size_t next_read_length) {
    EXPECT_EQ(expected, data);
    if (expected == data) {
      state_ = next_state;
      SimulateRead(next_read, next_read_length);
      return MockWriteResult(ASYNC, data.length());
    }
    return MockWriteResult(ASYNC, ERR_UNEXPECTED);
  }

  MockWriteResult Verify(const std::string& expected,
                         const std::string& data,
                         State next_state,
                         const char* next_read) {
    return Verify(expected, data, next_state,
                  next_read, std::strlen(next_read));
  }

  // The next time there is a read from this socket, it will return |data|.
  // Before calling SimulateRead next time, the previous data must be consumed.
  void SimulateRead(const char* data, size_t length) {
    if (!allow_unconsumed_reads_) {
      EXPECT_TRUE(reads_.empty()) << "Unconsumed read: " << reads_.front().data;
    }
    reads_.push_back(MockRead(ASYNC, data, length));
  }
  void SimulateRead(const char* data) { SimulateRead(data, std::strlen(data)); }

 private:
  // List of reads to be consumed.
  base::circular_deque<MockRead> reads_;

  // Max number of bytes we will read at a time. 0 means no limit.
  int short_read_limit_;

  // If true, we'll not require the client to consume all data before we
  // mock the next read.
  bool allow_unconsumed_reads_;

  State state_;
  State failure_injection_state_;
  State failure_injection_next_state_;
  const char* fault_response_;

  // If true, we will send multiple 230 lines as response after PASS.
  bool multiline_welcome_;

  // If true, we will use EPSV command.
  bool use_epsv_;

  // Data type to be used for TYPE command.
  char data_type_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProvider);
};

class FtpSocketDataProviderDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderDirectoryListing() = default;
  ~FtpSocketDataProviderDirectoryListing() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_CWD,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD /\r\n", data,
                      use_epsv() ? PRE_LIST_EPSV : PRE_LIST_PASV, "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST -l\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderDirectoryListing);
};

class FtpSocketDataProviderDirectoryListingWithPasvFallback
    : public FtpSocketDataProviderDirectoryListing {
 public:
  FtpSocketDataProviderDirectoryListingWithPasvFallback() = default;
  ~FtpSocketDataProviderDirectoryListingWithPasvFallback() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_LIST_EPSV:
        return Verify("EPSV\r\n", data, PRE_LIST_PASV,
                      "500 no EPSV for you\r\n");
      case PRE_SIZE:
        return Verify("SIZE /\r\n", data, PRE_CWD,
                      "550 I can only retrieve regular files\r\n");
      default:
        return FtpSocketDataProviderDirectoryListing::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderDirectoryListingWithPasvFallback);
};

class FtpSocketDataProviderVMSDirectoryListing : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListing() = default;
  ~FtpSocketDataProviderVMSDirectoryListing() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_LIST_EPSV:
        return Verify("EPSV\r\n", data, PRE_LIST_PASV,
                      "500 Invalid command\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]dir\r\n", data, PRE_CWD,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[dir]\r\n", data,
                      use_epsv() ? PRE_LIST_EPSV : PRE_LIST_PASV, "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSDirectoryListing);
};

class FtpSocketDataProviderVMSDirectoryListingRootDirectory
    : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSDirectoryListingRootDirectory() = default;
  ~FtpSocketDataProviderVMSDirectoryListingRootDirectory() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_LIST_EPSV:
        return Verify("EPSV\r\n", data, PRE_LIST_PASV,
                      "500 EPSV command unknown\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT\r\n", data, PRE_CWD,
                      "550 I can only retrieve regular files\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[000000]\r\n", data,
                      use_epsv() ? PRE_LIST_EPSV : PRE_LIST_PASV, "200 OK\r\n");
      case PRE_LIST:
        return Verify("LIST *.*;0\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      FtpSocketDataProviderVMSDirectoryListingRootDirectory);
};

class FtpSocketDataProviderFileDownloadWithFileTypecode
    : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileDownloadWithFileTypecode() = default;
  ~FtpSocketDataProviderFileDownloadWithFileTypecode() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV, "213 18\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadWithFileTypecode);
};

class FtpSocketDataProviderFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileDownload() = default;
  ~FtpSocketDataProviderFileDownload() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify(base::StringPrintf("SIZE %s\r\n", file_path_.c_str()),
                      data, PRE_CWD, "213 18\r\n");
      case PRE_CWD:
        return Verify(base::StringPrintf("CWD %s\r\n", file_path_.c_str()),
                      data, use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      case PRE_RETR:
        return Verify(base::StringPrintf("RETR %s\r\n", file_path_.c_str()),
                      data, PRE_QUIT, "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

  void set_file_path(const std::string& file_path) { file_path_ = file_path; }

 private:
  std::string file_path_ = "/file";

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownload);
};

class FtpSocketDataProviderFileNotFound : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderFileNotFound() = default;
  ~FtpSocketDataProviderFileNotFound() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_CWD,
                      "550 File Not Found\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 File Not Found\r\n");
      case PRE_RETR:
        return Verify("RETR /file\r\n", data, PRE_QUIT,
                      "550 File Not Found\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileNotFound);
};

class FtpSocketDataProviderFileDownloadWithPasvFallback
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadWithPasvFallback() = default;
  ~FtpSocketDataProviderFileDownloadWithPasvFallback() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_RETR_EPSV:
        return Verify("EPSV\r\n", data, PRE_RETR_PASV, "500 No can do\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadWithPasvFallback);
};

class FtpSocketDataProviderFileDownloadZeroSize
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadZeroSize() = default;
  ~FtpSocketDataProviderFileDownloadZeroSize() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, PRE_CWD,
                      "213 0\r\n");
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadZeroSize);
};

class FtpSocketDataProviderFileDownloadCWD451
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadCWD451() = default;
  ~FtpSocketDataProviderFileDownloadCWD451() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_CWD:
        return Verify("CWD /file\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "451 not a directory\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadCWD451);
};

class FtpSocketDataProviderVMSFileDownload : public FtpSocketDataProvider {
 public:
  FtpSocketDataProviderVMSFileDownload() = default;
  ~FtpSocketDataProviderVMSFileDownload() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SYST:
        return Verify("SYST\r\n", data, PRE_PWD, "215 VMS\r\n");
      case PRE_PWD:
        return Verify("PWD\r\n", data, PRE_TYPE,
                      "257 \"ANONYMOUS_ROOT:[000000]\"\r\n");
      case PRE_LIST_EPSV:
        return Verify("EPSV\r\n", data, PRE_LIST_PASV,
                      "500 EPSV command unknown\r\n");
      case PRE_SIZE:
        return Verify("SIZE ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_CWD,
                      "213 18\r\n");
      case PRE_CWD:
        return Verify("CWD ANONYMOUS_ROOT:[file]\r\n", data,
                      use_epsv() ? PRE_RETR_EPSV : PRE_RETR_PASV,
                      "550 Not a directory\r\n");
      case PRE_RETR:
        return Verify("RETR ANONYMOUS_ROOT:[000000]file\r\n", data, PRE_QUIT,
                      "200 OK\r\n");
      default:
        return FtpSocketDataProvider::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderVMSFileDownload);
};

class FtpSocketDataProviderFileDownloadInvalidResponse
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderFileDownloadInvalidResponse() = default;
  ~FtpSocketDataProviderFileDownloadInvalidResponse() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        // Use unallocated 599 FTP error code to make sure it falls into the
        // generic ERR_FTP_FAILED bucket.
        return Verify("SIZE /file\r\n", data, PRE_QUIT,
                      "599 Evil Response\r\n"
                      "599 More Evil\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderFileDownloadInvalidResponse);
};

class FtpSocketDataProviderEvilEpsv : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilEpsv(const char* epsv_response,
                                State expected_state)
      : epsv_response_(epsv_response),
        epsv_response_length_(std::strlen(epsv_response)),
        expected_state_(expected_state) {}

  FtpSocketDataProviderEvilEpsv(const char* epsv_response,
                               size_t epsv_response_length,
                               State expected_state)
      : epsv_response_(epsv_response),
        epsv_response_length_(epsv_response_length),
        expected_state_(expected_state) {}

  ~FtpSocketDataProviderEvilEpsv() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_RETR_EPSV:
        return Verify("EPSV\r\n", data, expected_state_,
                      epsv_response_, epsv_response_length_);
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* const epsv_response_;
  const size_t epsv_response_length_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilEpsv);
};

class FtpSocketDataProviderEvilPasv
    : public FtpSocketDataProviderFileDownloadWithPasvFallback {
 public:
  FtpSocketDataProviderEvilPasv(const char* pasv_response, State expected_state)
      : pasv_response_(pasv_response),
        expected_state_(expected_state) {
  }
  ~FtpSocketDataProviderEvilPasv() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_RETR_PASV:
        return Verify("PASV\r\n", data, expected_state_, pasv_response_);
      default:
        return FtpSocketDataProviderFileDownloadWithPasvFallback::OnWrite(data);
    }
  }

 private:
  const char* const pasv_response_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilPasv);
};

class FtpSocketDataProviderEvilSize : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilSize(const char* size_response, State expected_state)
      : size_response_(size_response),
        expected_state_(expected_state) {
  }
  ~FtpSocketDataProviderEvilSize() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_SIZE:
        return Verify("SIZE /file\r\n", data, expected_state_, size_response_);
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* const size_response_;
  const State expected_state_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilSize);
};

class FtpSocketDataProviderEvilLogin
    : public FtpSocketDataProviderFileDownload {
 public:
  FtpSocketDataProviderEvilLogin(const char* expected_user,
                                const char* expected_password)
      : expected_user_(expected_user),
        expected_password_(expected_password) {
  }
  ~FtpSocketDataProviderEvilLogin() override = default;

  MockWriteResult OnWrite(const std::string& data) override {
    if (InjectFault())
      return MockWriteResult(ASYNC, data.length());
    switch (state()) {
      case PRE_USER:
        return Verify(std::string("USER ") + expected_user_ + "\r\n", data,
                      PRE_PASSWD, "331 Password needed\r\n");
      case PRE_PASSWD:
        return Verify(std::string("PASS ") + expected_password_ + "\r\n", data,
                      PRE_SYST, "230 Welcome\r\n");
      default:
        return FtpSocketDataProviderFileDownload::OnWrite(data);
    }
  }

 private:
  const char* const expected_user_;
  const char* const expected_password_;

  DISALLOW_COPY_AND_ASSIGN(FtpSocketDataProviderEvilLogin);
};

class FtpNetworkTransactionTest : public PlatformTest,
                                  public ::testing::WithParamInterface<int>,
                                  public WithTaskEnvironment {
 public:
  FtpNetworkTransactionTest() : host_resolver_(new MockHostResolver) {
    SetUpTransaction();

    scoped_refptr<RuleBasedHostResolverProc> rules(
        new RuleBasedHostResolverProc(nullptr));
    if (GetFamily() == AF_INET) {
      rules->AddIPLiteralRule("*", "127.0.0.1", "127.0.0.1");
    } else if (GetFamily() == AF_INET6) {
      rules->AddIPLiteralRule("*", "::1", "::1");
    } else {
      NOTREACHED();
    }
    host_resolver_->set_rules(rules.get());
  }
  ~FtpNetworkTransactionTest() override = default;

  // Sets up an FtpNetworkTransaction and MocketClientSocketFactory, replacing
  // the default one. Only needs to be called if a test runs multiple
  // transactions.
  void SetUpTransaction() {
    mock_socket_factory_ = std::make_unique<MockClientSocketFactory>();
    transaction_ = std::make_unique<FtpNetworkTransaction>(
        host_resolver_.get(), mock_socket_factory_.get());
  }

 protected:
  // Accessor to make code refactoring-friendly, e.g. when we change the way
  // parameters are passed (like more parameters).
  int GetFamily() {
    return GetParam();
  }

  FtpRequestInfo GetRequestInfo(const std::string& url) {
    FtpRequestInfo info;
    info.url = GURL(url);
    return info;
  }

  void ExecuteTransaction(FtpSocketDataProvider* ctrl_socket,
                          const char* request,
                          int expected_result) {
    // Expect EPSV usage for non-IPv4 control connections.
    ctrl_socket->set_use_epsv((GetFamily() != AF_INET));

    mock_socket_factory_->AddSocketDataProvider(ctrl_socket);

    std::string mock_data("mock-data");
    MockRead data_reads[] = {
      // Usually FTP servers close the data connection after the entire data has
      // been received.
      MockRead(SYNCHRONOUS, ERR_TEST_PEER_CLOSE_AFTER_NEXT_MOCK_READ),
      MockRead(mock_data.c_str()),
    };

    std::unique_ptr<StaticSocketDataProvider> data_socket =
        std::make_unique<StaticSocketDataProvider>(data_reads,
                                                   base::span<MockWrite>());
    mock_socket_factory_->AddSocketDataProvider(data_socket.get());
    FtpRequestInfo request_info = GetRequestInfo(request);
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_->GetLoadState());
    ASSERT_EQ(
        ERR_IO_PENDING,
        transaction_->Start(&request_info, callback_.callback(),
                            NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
    EXPECT_NE(LOAD_STATE_IDLE, transaction_->GetLoadState());
    ASSERT_EQ(expected_result, callback_.WaitForResult());
    if (expected_result == OK) {
      scoped_refptr<IOBuffer> io_buffer =
          base::MakeRefCounted<IOBuffer>(kBufferSize);
      memset(io_buffer->data(), 0, kBufferSize);
      ASSERT_EQ(ERR_IO_PENDING, transaction_->Read(io_buffer.get(), kBufferSize,
                                                   callback_.callback()));
      ASSERT_EQ(static_cast<int>(mock_data.length()),
                callback_.WaitForResult());
      EXPECT_EQ(mock_data, std::string(io_buffer->data(), mock_data.length()));

      // Do another Read to detect that the data socket is now closed.
      int rv = transaction_->Read(io_buffer.get(), kBufferSize,
                                  callback_.callback());
      if (rv == ERR_IO_PENDING) {
        EXPECT_EQ(0, callback_.WaitForResult());
      } else {
        EXPECT_EQ(0, rv);
      }
    }
    EXPECT_EQ(FtpSocketDataProvider::QUIT, ctrl_socket->state());
    EXPECT_EQ(LOAD_STATE_IDLE, transaction_->GetLoadState());
  }

  void TransactionFailHelper(FtpSocketDataProvider* ctrl_socket,
                             const char* request,
                             FtpSocketDataProvider::State state,
                             FtpSocketDataProvider::State next_state,
                             const char* response,
                             int expected_result) {
    ctrl_socket->InjectFailure(state, next_state, response);
    ExecuteTransaction(ctrl_socket, request, expected_result);
  }

  std::unique_ptr<MockHostResolver> host_resolver_;
  std::unique_ptr<MockClientSocketFactory> mock_socket_factory_;
  std::unique_ptr<FtpNetworkTransaction> transaction_;
  TestCompletionCallback callback_;
};

TEST_P(FtpNetworkTransactionTest, FailedLookup) {
  FtpRequestInfo request_info = GetRequestInfo("ftp://badhost");
  scoped_refptr<RuleBasedHostResolverProc> rules(
      new RuleBasedHostResolverProc(nullptr));
  rules->AddSimulatedFailure("badhost");
  host_resolver_->set_rules(rules.get());

  EXPECT_EQ(LOAD_STATE_IDLE, transaction_->GetLoadState());
  ASSERT_EQ(
      ERR_IO_PENDING,
      transaction_->Start(&request_info, callback_.callback(),
                          NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_THAT(callback_.WaitForResult(), IsError(ERR_NAME_NOT_RESOLVED));
  EXPECT_EQ(LOAD_STATE_IDLE, transaction_->GetLoadState());
}

// Check that when determining the host, the square brackets decorating IPv6
// literals in URLs are stripped.
TEST_P(FtpNetworkTransactionTest, StripBracketsFromIPv6Literals) {
  // This test only makes sense for IPv6 connections.
  if (GetFamily() != AF_INET6)
    return;

  host_resolver_->rules()->AddSimulatedFailure("[::1]");

  // We start a transaction that is expected to fail with ERR_INVALID_RESPONSE.
  // The important part of this test is to make sure that we don't fail with
  // ERR_NAME_NOT_RESOLVED, since that would mean the decorated hostname
  // was used.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://[::1]/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransaction) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);

  EXPECT_TRUE(transaction_->GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_->GetResponseInfo()->expected_content_size);
  EXPECT_EQ(
      (GetFamily() == AF_INET) ? "127.0.0.1" : "::1",
      transaction_->GetResponseInfo()->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(21, transaction_->GetResponseInfo()->remote_endpoint.port());
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionWithPasvFallback) {
  FtpSocketDataProviderDirectoryListingWithPasvFallback ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);

  EXPECT_TRUE(transaction_->GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_->GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionWithTypecode) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/;type=d", OK);

  EXPECT_TRUE(transaction_->GetResponseInfo()->is_directory_listing);
  EXPECT_EQ(-1, transaction_->GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcome) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionShortReads2) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionShortReads5) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionMultilineWelcomeShort) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // The client will not consume all three 230 lines. That's good, we want to
  // test that scenario.
  ctrl_socket.set_allow_unconsumed_reads(true);
  ctrl_socket.set_multiline_welcome(true);
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

// Regression test for http://crbug.com/60555.
TEST_P(FtpNetworkTransactionTest, DirectoryTransactionZeroSize) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_SIZE,
                            FtpSocketDataProvider::PRE_CWD, "213 0\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionVMS) {
  FtpSocketDataProviderVMSDirectoryListing ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/dir", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionVMSRootDirectory) {
  FtpSocketDataProviderVMSDirectoryListingRootDirectory ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionTransferStarting) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_LIST,
                            FtpSocketDataProvider::PRE_QUIT,
                            "125-Data connection already open.\r\n"
                            "125  Transfer starting.\r\n"
                            "226 Transfer complete.\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransaction) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_->GetResponseInfo()->expected_content_size);
  EXPECT_EQ(
      (GetFamily() == AF_INET) ? "127.0.0.1" : "::1",
      transaction_->GetResponseInfo()->remote_endpoint.ToStringWithoutPort());
  EXPECT_EQ(21, transaction_->GetResponseInfo()->remote_endpoint.port());
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithPasvFallback) {
  FtpSocketDataProviderFileDownloadWithPasvFallback ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_->GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeA) {
  FtpSocketDataProviderFileDownloadWithFileTypecode ctrl_socket;
  ctrl_socket.set_data_type('A');
  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=a", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_->GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionWithTypecodeI) {
  FtpSocketDataProviderFileDownloadWithFileTypecode ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file;type=i", OK);

  // We pass an artificial value of 18 as a response to the SIZE command.
  EXPECT_EQ(18, transaction_->GetResponseInfo()->expected_content_size);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionMultilineWelcome) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_multiline_welcome(true);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionShortReads2) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(2);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionShortReads5) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.set_short_read_limit(5);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionZeroSize) {
  FtpSocketDataProviderFileDownloadZeroSize ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionCWD451) {
  FtpSocketDataProviderFileDownloadCWD451 ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionVMS) {
  FtpSocketDataProviderVMSFileDownload ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionTransferStarting) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_RETR,
                            FtpSocketDataProvider::PRE_QUIT,
                            "125-Data connection already open.\r\n"
                            "125  Transfer starting.\r\n"
                            "226 Transfer complete.\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionInvalidResponse) {
  FtpSocketDataProviderFileDownloadInvalidResponse ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvReallyBadFormat) {
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort1) {
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,0,22)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort2) {
  // Still unsafe. 1 * 256 + 2 = 258, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,1,2)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort3) {
  // Still unsafe. 3 * 256 + 4 = 772, which is < 1024.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,3,4)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafePort4) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket("227 Portscan (127,0,0,1,8,1)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvInvalidPort1) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (127,0,0,1,256,100)\r\n", FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvInvalidPort2) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (127,0,0,1,100,256)\r\n", FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvInvalidPort3) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (127,0,0,1,-100,100)\r\n", FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvInvalidPort4) {
  // Unsafe. 8 * 256 + 1 = 2049, which is used by nfs.
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (127,0,0,1,100,-100)\r\n", FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilPasvUnsafeHost) {
  FtpSocketDataProviderEvilPasv ctrl_socket(
      "227 Portscan (10,1,2,3,123,123)\r\n", FtpSocketDataProvider::PRE_RETR);
  ctrl_socket.set_use_epsv(GetFamily() != AF_INET);
  std::string mock_data("mock-data");
  MockRead data_reads[] = {
    MockRead(mock_data.c_str()),
  };
  StaticSocketDataProvider data_socket1;
  StaticSocketDataProvider data_socket2(data_reads, base::span<MockWrite>());
  mock_socket_factory_->AddSocketDataProvider(&ctrl_socket);
  mock_socket_factory_->AddSocketDataProvider(&data_socket1);
  mock_socket_factory_->AddSocketDataProvider(&data_socket2);
  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  // Start the transaction.
  ASSERT_EQ(
      ERR_IO_PENDING,
      transaction_->Start(&request_info, callback_.callback(),
                          NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_THAT(callback_.WaitForResult(), IsOk());

  // The transaction fires the callback when we can start reading data. That
  // means that the data socket should be open.
  MockTCPClientSocket* data_socket =
      static_cast<MockTCPClientSocket*>(transaction_->data_socket_.get());
  ASSERT_TRUE(data_socket);
  ASSERT_TRUE(data_socket->IsConnected());

  // Even if the PASV response specified some other address, we connect
  // to the address we used for control connection (which could be 127.0.0.1
  // or ::1 depending on whether we use IPv6).
  for (auto it = data_socket->addresses().begin();
       it != data_socket->addresses().end(); ++it) {
    EXPECT_NE("10.1.2.3", it->ToStringWithoutPort());
  }
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat1) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||22)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat2) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (||\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat3) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat4) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (||||)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvReallyBadFormat5) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  // Breaking the string in the next line prevents MSVC warning C4125.
  const char response[] = "227 Portscan (\0\0\031" "773\0)\r\n";
  FtpSocketDataProviderEvilEpsv ctrl_socket(response, sizeof(response)-1,
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort1) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||22|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort2) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||258|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort3) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||772|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvUnsafePort4) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||2049|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvInvalidPort) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|||4294973296|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvWeirdSep) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan ($$$31744$)\r\n",
                                            FtpSocketDataProvider::PRE_RETR);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
}

TEST_P(FtpNetworkTransactionTest,
       DownloadTransactionEvilEpsvWeirdSepUnsafePort) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan ($$$317$)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_UNSAFE_PORT);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilEpsvIllegalHost) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderEvilEpsv ctrl_socket("227 Portscan (|2|::1|31744|)\r\n",
                                            FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadUsername) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello%0Aworld", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%0Aworld:test@host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilLoginBadPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello%0Dworld");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%0Dworld@host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionSpaceInLogin) {
  FtpSocketDataProviderEvilLogin ctrl_socket("hello world", "test");
  ExecuteTransaction(&ctrl_socket, "ftp://hello%20world:test@host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionSpaceInPassword) {
  FtpSocketDataProviderEvilLogin ctrl_socket("test", "hello world");
  ExecuteTransaction(&ctrl_socket, "ftp://test:hello%20world@host/file", OK);
}

TEST_P(FtpNetworkTransactionTest, FailOnInvalidUrls) {
  const char* const kBadUrls[]{
      // Make sure FtpNetworkTransaction doesn't request paths like
      // "/foo/../bar".  Doing so wouldn't be a security issue, client side, but
      // just doesn't seem like a good idea.
      "ftp://host/foo%2f..%2fbar%5c",

      // LF
      "ftp://host/foo%10.txt",
      // CR
      "ftp://host/foo%13.txt",

      "ftp://host/foo%00.txt",
  };

  for (const char* bad_url : kBadUrls) {
    SCOPED_TRACE(bad_url);

    SetUpTransaction();
    FtpRequestInfo request_info = GetRequestInfo(bad_url);
    ASSERT_EQ(
        ERR_INVALID_URL,
        transaction_->Start(&request_info, callback_.callback(),
                            NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
  }
}

TEST_P(FtpNetworkTransactionTest, EvilRestartUser) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_->AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(
      ERR_IO_PENDING,
      transaction_->Start(&request_info, callback_.callback(),
                          NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_THAT(callback_.WaitForResult(), IsError(ERR_FTP_FAILED));

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, ctrl_writes);
  mock_socket_factory_->AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_->RestartWithAuth(
                AuthCredentials(base::ASCIIToUTF16("foo\nownz0red"),
                                base::ASCIIToUTF16("innocent")),
                callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_MALFORMED_IDENTITY));
}

TEST_P(FtpNetworkTransactionTest, EvilRestartPassword) {
  FtpSocketDataProvider ctrl_socket1;
  ctrl_socket1.InjectFailure(FtpSocketDataProvider::PRE_PASSWD,
                             FtpSocketDataProvider::PRE_QUIT,
                             "530 Login authentication failed\r\n");
  mock_socket_factory_->AddSocketDataProvider(&ctrl_socket1);

  FtpRequestInfo request_info = GetRequestInfo("ftp://host/file");

  ASSERT_EQ(
      ERR_IO_PENDING,
      transaction_->Start(&request_info, callback_.callback(),
                          NetLogWithSource(), TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_THAT(callback_.WaitForResult(), IsError(ERR_FTP_FAILED));

  MockRead ctrl_reads[] = {
    MockRead("220 host TestFTPd\r\n"),
    MockRead("331 User okay, send password\r\n"),
    MockRead("221 Goodbye!\r\n"),
    MockRead(SYNCHRONOUS, OK),
  };
  MockWrite ctrl_writes[] = {
    MockWrite("USER innocent\r\n"),
    MockWrite("QUIT\r\n"),
  };
  StaticSocketDataProvider ctrl_socket2(ctrl_reads, ctrl_writes);
  mock_socket_factory_->AddSocketDataProvider(&ctrl_socket2);
  ASSERT_EQ(ERR_IO_PENDING,
            transaction_->RestartWithAuth(
                AuthCredentials(base::ASCIIToUTF16("innocent"),
                                base::ASCIIToUTF16("foo\nownz0red")),
                callback_.callback()));
  EXPECT_THAT(callback_.WaitForResult(), IsError(ERR_MALFORMED_IDENTITY));
}

TEST_P(FtpNetworkTransactionTest, Escaping) {
  const struct TestCase {
    const char* url;
    const char* expected_path;
  } kTestCases[] = {
      {"ftp://host/%20%21%22%23%24%25%79%80%81", "/ !\"#$%y\200\201"},
      // This is no allowed to be unescaped by UnescapeURLComponent, since it's
      // a lock icon. But it has no special meaning or security concern in the
      // context of making FTP requests.
      {"ftp://host/%F0%9F%94%92", "/\xF0\x9F\x94\x92"},
      // Invalid UTF-8 character, which again has no special meaning over FTP.
      {"ftp://host/%81", "/\x81"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.url);

    SetUpTransaction();
    FtpSocketDataProviderFileDownload ctrl_socket;
    ctrl_socket.set_file_path(test_case.expected_path);
    ExecuteTransaction(&ctrl_socket, test_case.url, OK);
  }
}

// Test for http://crbug.com/23794.
TEST_P(FtpNetworkTransactionTest, DownloadTransactionEvilSize) {
  // Try to overflow int64_t in the response.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 99999999999999999999999999999999\r\n",
      FtpSocketDataProvider::PRE_QUIT);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
}

// Test for http://crbug.com/36360.
TEST_P(FtpNetworkTransactionTest, DownloadTransactionBigSize) {
  // Pass a valid, but large file size. The transaction should not fail.
  FtpSocketDataProviderEvilSize ctrl_socket(
      "213 3204427776\r\n",
      FtpSocketDataProvider::PRE_CWD);
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", OK);
  EXPECT_EQ(3204427776LL,
            transaction_->GetResponseInfo()->expected_content_size);
}

// Regression test for http://crbug.com/25023.
TEST_P(FtpNetworkTransactionTest, CloseConnection) {
  FtpSocketDataProvider ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_USER,
                            FtpSocketDataProvider::PRE_QUIT, "");
  ExecuteTransaction(&ctrl_socket, "ftp://host", ERR_EMPTY_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailUser) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPass) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FTP_FAILED);
}

// Regression test for http://crbug.com/38707.
TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPass503) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "503 Bad sequence of commands\r\n",
                        ERR_FTP_BAD_COMMAND_SEQUENCE);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailSyst) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "599 fail\r\n",
                        OK);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailPwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailType) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailEpsv) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(
      &ctrl_socket, "ftp://host", FtpSocketDataProvider::PRE_LIST_EPSV,
      FtpSocketDataProvider::PRE_NOPASV, "599 fail\r\n", ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailCwd) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host",
                        FtpSocketDataProvider::PRE_CWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DirectoryTransactionFailList) {
  FtpSocketDataProviderVMSDirectoryListing ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/dir",
                        FtpSocketDataProvider::PRE_LIST,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailUser) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_USER,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailPass) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PASSWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "530 Login authentication failed\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailSyst) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_SYST,
                        FtpSocketDataProvider::PRE_PWD,
                        "599 fail\r\n",
                        OK);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailPwd) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailType) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_TYPE,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailEpsv) {
  // This test makes no sense for IPv4 connections (we don't use EPSV there).
  if (GetFamily() == AF_INET)
    return;

  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(
      &ctrl_socket, "ftp://host/file", FtpSocketDataProvider::PRE_RETR_EPSV,
      FtpSocketDataProvider::PRE_NOPASV, "599 fail\r\n", ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, DownloadTransactionFailRetr) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  // Use unallocated 599 FTP error code to make sure it falls into the generic
  // ERR_FTP_FAILED bucket.
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_RETR,
                        FtpSocketDataProvider::PRE_QUIT,
                        "599 fail\r\n",
                        ERR_FTP_FAILED);
}

TEST_P(FtpNetworkTransactionTest, FileNotFound) {
  FtpSocketDataProviderFileNotFound ctrl_socket;
  ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_FTP_FAILED);
}

// Test for http://crbug.com/38845.
TEST_P(FtpNetworkTransactionTest, ZeroLengthDirInPWD) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(&ctrl_socket,
                        "ftp://host/file",
                        FtpSocketDataProvider::PRE_PWD,
                        FtpSocketDataProvider::PRE_TYPE,
                        "257 \"\"\r\n",
                        OK);
}

TEST_P(FtpNetworkTransactionTest, UnexpectedInitiatedResponseForDirectory) {
  // The states for a directory listing where an initiated response will cause
  // an error.  Includes all commands sent on the directory listing path, except
  // CWD, SIZE, LIST, and QUIT commands.
  FtpSocketDataProvider::State kFailingStates[] = {
      FtpSocketDataProvider::PRE_USER, FtpSocketDataProvider::PRE_PASSWD,
      FtpSocketDataProvider::PRE_SYST, FtpSocketDataProvider::PRE_PWD,
      FtpSocketDataProvider::PRE_TYPE,
      GetFamily() != AF_INET ? FtpSocketDataProvider::PRE_LIST_EPSV
                             : FtpSocketDataProvider::PRE_LIST_PASV,
      FtpSocketDataProvider::PRE_CWD,
  };

  for (FtpSocketDataProvider::State state : kFailingStates) {
    SetUpTransaction();
    FtpSocketDataProviderDirectoryListing ctrl_socket;
    ctrl_socket.InjectFailure(state, FtpSocketDataProvider::PRE_QUIT,
                              "157 Foo\r\n");
    ExecuteTransaction(&ctrl_socket, "ftp://host/", ERR_INVALID_RESPONSE);
  }
}

TEST_P(FtpNetworkTransactionTest, UnexpectedInitiatedResponseForFile) {
  // The states for a download where an initiated response will cause an error.
  // Includes all commands sent on the file download path, except CWD, SIZE, and
  // QUIT commands.
  const FtpSocketDataProvider::State kFailingStates[] = {
      FtpSocketDataProvider::PRE_USER, FtpSocketDataProvider::PRE_PASSWD,
      FtpSocketDataProvider::PRE_SYST, FtpSocketDataProvider::PRE_PWD,
      FtpSocketDataProvider::PRE_TYPE,
      GetFamily() != AF_INET ? FtpSocketDataProvider::PRE_RETR_EPSV
                             : FtpSocketDataProvider::PRE_RETR_PASV,
      FtpSocketDataProvider::PRE_CWD};

  for (FtpSocketDataProvider::State state : kFailingStates) {
    LOG(ERROR) << "??: " << state;
    SetUpTransaction();
    FtpSocketDataProviderFileDownload ctrl_socket;
    ctrl_socket.InjectFailure(state, FtpSocketDataProvider::PRE_QUIT,
                              "157 Foo\r\n");
    ExecuteTransaction(&ctrl_socket, "ftp://host/file", ERR_INVALID_RESPONSE);
  }
}

// Make sure that receiving extra unexpected responses correctly results in
// sending a QUIT message, without triggering a DCHECK.
TEST_P(FtpNetworkTransactionTest, ExtraResponses) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_TYPE,
                            FtpSocketDataProvider::PRE_QUIT,
                            "157 Foo\r\n"
                            "157 Bar\r\n"
                            "157 Trombones\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host/", ERR_INVALID_RESPONSE);
}

// Make sure that receiving extra unexpected responses to a QUIT message
// correctly results in ending the transaction with an error, without triggering
// a DCHECK.
TEST_P(FtpNetworkTransactionTest, ExtraQuitResponses) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_QUIT,
                            FtpSocketDataProvider::QUIT,
                            "221 Foo\r\n"
                            "221 Bar\r\n"
                            "221 Trombones\r\n");
  ExecuteTransaction(&ctrl_socket, "ftp://host/", ERR_INVALID_RESPONSE);
}

// Test case for https://crbug.com/633841 - similar to the ExtraQuitResponses
// test case, but with an empty response.
TEST_P(FtpNetworkTransactionTest, EmptyQuitResponse) {
  FtpSocketDataProviderDirectoryListing ctrl_socket;
  ctrl_socket.InjectFailure(FtpSocketDataProvider::PRE_QUIT,
                            FtpSocketDataProvider::QUIT, "");
  ExecuteTransaction(&ctrl_socket, "ftp://host/", OK);
}

TEST_P(FtpNetworkTransactionTest, InvalidRemoteDirectory) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(
      &ctrl_socket, "ftp://host/file", FtpSocketDataProvider::PRE_PWD,
      FtpSocketDataProvider::PRE_QUIT,
      "257 \"foo\rbar\" is your current location\r\n", ERR_INVALID_RESPONSE);
}

TEST_P(FtpNetworkTransactionTest, InvalidRemoteDirectory2) {
  FtpSocketDataProviderFileDownload ctrl_socket;
  TransactionFailHelper(
      &ctrl_socket, "ftp://host/file", FtpSocketDataProvider::PRE_PWD,
      FtpSocketDataProvider::PRE_QUIT,
      "257 \"foo\nbar\" is your current location\r\n", ERR_INVALID_RESPONSE);
}

INSTANTIATE_TEST_SUITE_P(Ftp,
                         FtpNetworkTransactionTest,
                         ::testing::Values(AF_INET, AF_INET6));

}  // namespace net
