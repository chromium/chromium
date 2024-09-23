// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "net/base/address_family.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/ip_address.h"
#include "net/base/rand_callback.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mdns_client_impl.h"
#include "net/dns/mock_mdns_socket_factory.h"
#include "net/dns/record_rdata.h"
#include "net/log/net_log.h"
#include "net/socket/udp_client_socket.h"
#include "net/test/gtest_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::IgnoreResult;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace net {

namespace {

const uint8_t kSamplePacket1[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x00,                                 // TTL (4 bytes) is 1 second;
    0x00, 0x01, 0x00, 0x08,                     // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r', 0xc0,
    0x14,        // Pointer to "._tcp.local"
    0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 49 seconds.
    0x24, 0x75, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x32};

const uint8_t kSamplePacket1WithCapitalization[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 'T', 'C', 'P', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x00,                                 // TTL (4 bytes) is 1 second;
    0x00, 0x01, 0x00, 0x08,                     // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'P', 'r', 'i', 'n', 't', 'e', 'R', 0xc0,
    0x14,        // Pointer to "._tcp.local"
    0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 49 seconds.
    0x24, 0x75, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x32};

const uint8_t kCorruptedPacketBadQuestion[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x01,  // One question
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Question is corrupted and cannot be read.
    0x99, 'h', 'e', 'l', 'l', 'o', 0x00, 0x00, 0x00, 0x00, 0x00,

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x99,  // RDLENGTH is impossible
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'p', 'r',  // Useless trailing data.
};

const uint8_t kCorruptedPacketUnsalvagable[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x99,  // RDLENGTH is impossible
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'p', 'r',  // Useless trailing data.
};

const uint8_t kCorruptedPacketDoubleRecord[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x06, 'p', 'r', 'i', 'v', 'e', 't', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
    0x00, 0x01,  // TYPE is A.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x04,  // RDLENGTH is 4
    0x05, 0x03, 0xc0, 0x0c,

    // Answer 2 -- Same key
    0x06, 'p', 'r', 'i', 'v', 'e', 't', 0x05, 'l', 'o', 'c', 'a', 'l', 0x00,
    0x00, 0x01,  // TYPE is A.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x04,  // RDLENGTH is 4
    0x02, 0x03, 0x04, 0x05,
};

const uint8_t kCorruptedPacketSalvagable[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x08,         // RDLENGTH is 8 bytes.
    0x99, 'h', 'e', 'l', 'l', 'o',  // Bad RDATA format.
    0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r', 0xc0,
    0x14,        // Pointer to "._tcp.local"
    0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 49 seconds.
    0x24, 0x75, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x32};

const uint8_t kSamplePacket2[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'z', 'z', 'z', 'z', 'z', 0xc0, 0x0c,

    // Answer 2
    0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r', 0xc0,
    0x14,        // Pointer to "._tcp.local"
    0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,  // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'z', 'z', 'z', 'z', 'z', 0xc0, 0x32};

const uint8_t kSamplePacket3[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't',  //
    0x04, '_', 't', 'c', 'p',                 //
    0x05, 'l', 'o', 'c', 'a', 'l',            //
    0x00, 0x00, 0x0c,                         // TYPE is PTR.
    0x00, 0x01,                               // CLASS is IN.
    0x00, 0x00,                               // TTL (4 bytes) is 1 second;
    0x00, 0x01,                               //
    0x00, 0x08,                               // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o',            //
    0xc0, 0x0c,                               //

    // Answer 2
    0x08, '_', 'p', 'r', 'i', 'n', 't', 'e', 'r',  //
    0xc0, 0x14,                                    // Pointer to "._tcp.local"
    0x00, 0x0c,                                    // TYPE is PTR.
    0x00, 0x01,                                    // CLASS is IN.
    0x00, 0x00,                     // TTL (4 bytes) is 3 seconds.
    0x00, 0x03,                     //
    0x00, 0x08,                     // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o',  //
    0xc0, 0x32};

const uint8_t kQueryPacketPrivet[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x00, 0x00,  // No flags.
    0x00, 0x01,  // One question.
    0x00, 0x00,  // 0 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Question
    // This part is echoed back from the respective query.
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
};

const uint8_t kQueryPacketPrivetWithCapitalization[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x00, 0x00,  // No flags.
    0x00, 0x01,  // One question.
    0x00, 0x00,  // 0 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Question
    // This part is echoed back from the respective query.
    0x07, '_', 'P', 'R', 'I', 'V', 'E', 'T', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
};

const uint8_t kQueryPacketPrivetA[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x00, 0x00,  // No flags.
    0x00, 0x01,  // One question.
    0x00, 0x00,  // 0 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Question
    // This part is echoed back from the respective query.
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x01,  // TYPE is A.
    0x00, 0x01,                                 // CLASS is IN.
};

const uint8_t kSamplePacketAdditionalOnly[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x00,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x01,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x08,  // RDLENGTH is 8 bytes.
    0x05, 'h', 'e', 'l', 'l', 'o', 0xc0, 0x0c,
};

const uint8_t kSamplePacketNsec[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x2f,  // TYPE is NSEC.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x01,  // TTL (4 bytes) is 20 hours, 47 minutes, 48 seconds.
    0x24, 0x74, 0x00, 0x06,             // RDLENGTH is 6 bytes.
    0xc0, 0x0c, 0x00, 0x02, 0x00, 0x08  // Only A record present
};

const uint8_t kSamplePacketAPrivet[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x01,  // TYPE is A.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x00,                                 // TTL (4 bytes) is 5 seconds
    0x00, 0x05, 0x00, 0x04,                     // RDLENGTH is 4 bytes.
    0xc0, 0x0c, 0x00, 0x02,
};

const uint8_t kSamplePacketGoodbye[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x81, 0x80,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x01,  // 2 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Answer 1
    0x07, '_', 'p', 'r', 'i', 'v', 'e', 't', 0x04, '_', 't', 'c', 'p', 0x05,
    'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                 // CLASS is IN.
    0x00, 0x00,                                 // TTL (4 bytes) is zero;
    0x00, 0x00, 0x00, 0x08,                     // RDLENGTH is 8 bytes.
    0x05, 'z', 'z', 'z', 'z', 'z', 0xc0, 0x0c,
};

std::string MakeString(const uint8_t* data, unsigned size) {
  return std::string(reinterpret_cast<const char*>(data), size);
}

class PtrRecordCopyContainer {
 public:
  PtrRecordCopyContainer() = default;
  ~PtrRecordCopyContainer() = default;

  bool is_set() const { return set_; }

  void SaveWithDummyArg(int unused, const RecordParsed* value) {
    Save(value);
  }

  void Save(const RecordParsed* value) {
    set_ = true;
    name_ = value->name();
    ptrdomain_ = value->rdata<PtrRecordRdata>()->ptrdomain();
    ttl_ = value->ttl();
  }

  bool IsRecordWith(const std::string& name, const std::string& ptrdomain) {
    return set_ && name_ == name && ptrdomain_ == ptrdomain;
  }

  const std::string& name() { return name_; }
  const std::string& ptrdomain() { return ptrdomain_; }
  int ttl() { return ttl_; }

 private:
  bool set_;
  std::string name_;
  std::string ptrdomain_;
  int ttl_;
};

class MockClock : public base::Clock {
 public:
  MockClock() = default;

  MockClock(const MockClock&) = delete;
  MockClock& operator=(const MockClock&) = delete;

  ~MockClock() override = default;

  MOCK_CONST_METHOD0(Now, base::Time());
};

class MockTimer : public base::MockOneShotTimer {
 public:
  MockTimer() = default;

  MockTimer(const MockTimer&) = delete;
  MockTimer& operator=(const MockTimer&) = delete;

  ~MockTimer() override = default;

  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    StartObserver(posted_from, delay);
    base::MockOneShotTimer::Start(posted_from, delay, std::move(user_task));
  }

  // StartObserver is invoked when MockTimer::Start() is called.
  // Does not replace the behavior of MockTimer::Start().
  MOCK_METHOD2(StartObserver,
               void(const base::Location& posted_from, base::TimeDelta delay));
};

}  // namespace

class MDnsTest : public TestWithTaskEnvironment {
 public:
  void SetUp() override;
  void DeleteTransaction();
  void DeleteBothListeners();
  void RunFor(base::TimeDelta time_period);
  void Stop();

  MOCK_METHOD2(MockableRecordCallback, void(MDnsTransaction::Result result,
                                            const RecordParsed* record));

  MOCK_METHOD2(MockableRecordCallback2, void(MDnsTransaction::Result result,
                                             const RecordParsed* record));

 protected:
  void ExpectPacket(const uint8_t* packet, unsigned size);
  void SimulatePacketReceive(const uint8_t* packet, unsigned size);

  std::unique_ptr<base::Clock> test_clock_;  // Must outlive `test_client_`.
  std::unique_ptr<MDnsClientImpl> test_client_;
  IPEndPoint mdns_ipv4_endpoint_;
  StrictMock<MockMDnsSocketFactory> socket_factory_;

  // Transactions and listeners that can be deleted by class methods for
  // reentrancy tests.
  std::unique_ptr<MDnsTransaction> transaction_;
  std::unique_ptr<MDnsListener> listener1_;
  std::unique_ptr<MDnsListener> listener2_;
  base::RunLoop loop_;
};

class MockListenerDelegate : public MDnsListener::Delegate {
 public:
  MOCK_METHOD2(OnRecordUpdate,
               void(MDnsListener::UpdateType update,
                    const RecordParsed* records));
  MOCK_METHOD2(OnNsecRecord, void(const std::string&, unsigned));
  MOCK_METHOD0(OnCachePurged, void());
};

void MDnsTest::SetUp() {
  test_client_ = std::make_unique<MDnsClientImpl>();
  ASSERT_THAT(test_client_->StartListening(&socket_factory_), test::IsOk());
}

void MDnsTest::SimulatePacketReceive(const uint8_t* packet, unsigned size) {
  socket_factory_.SimulateReceive(packet, size);
}

void MDnsTest::ExpectPacket(const uint8_t* packet, unsigned size) {
  EXPECT_CALL(socket_factory_, OnSendTo(MakeString(packet, size)))
      .Times(2);
}

void MDnsTest::DeleteTransaction() {
  transaction_.reset();
}

void MDnsTest::DeleteBothListeners() {
  listener1_.reset();
  listener2_.reset();
}

void MDnsTest::RunFor(base::TimeDelta time_period) {
  base::CancelableOnceCallback<void()> callback(
      base::BindOnce(&MDnsTest::Stop, base::Unretained(this)));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, callback.callback(), time_period);

  loop_.Run();
  callback.Cancel();
}

void MDnsTest::Stop() {
  loop_.QuitWhenIdle();
}

TEST_F(MDnsTest, PassiveListeners) {
  StrictMock<MockListenerDelegate> delegate_privet;
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_printer;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  std::unique_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);

  ASSERT_TRUE(listener_privet->Start());
  ASSERT_TRUE(listener_printer->Start());

  // Send the same packet twice to ensure no records are double-counted.

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_printer,
          &PtrRecordCopyContainer::SaveWithDummyArg));


  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  EXPECT_TRUE(record_printer.IsRecordWith("_printer._tcp.local",
                                          "hello._printer._tcp.local"));

  listener_privet.reset();
  listener_printer.reset();
}

TEST_F(MDnsTest, PassiveListenersWithCapitalization) {
  StrictMock<MockListenerDelegate> delegate_privet;
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_printer;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.LOCAL", &delegate_privet);
  std::unique_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_prinTER._Tcp.Local", &delegate_printer);

  ASSERT_TRUE(listener_privet->Start());
  ASSERT_TRUE(listener_printer->Start());

  // Send the same packet twice to ensure no records are double-counted.

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(
          Invoke(&record_privet, &PtrRecordCopyContainer::SaveWithDummyArg));

  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(
          Invoke(&record_printer, &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1WithCapitalization,
                        sizeof(kSamplePacket1WithCapitalization));
  SimulatePacketReceive(kSamplePacket1WithCapitalization,
                        sizeof(kSamplePacket1WithCapitalization));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._TCP.local",
                                         "hello._privet._TCP.local"));

  EXPECT_TRUE(record_printer.IsRecordWith("_PrinteR._TCP.local",
                                          "hello._PrinteR._TCP.local"));

  listener_privet.reset();
  listener_printer.reset();
}

TEST_F(MDnsTest, PassiveListenersCacheCleanup) {
  StrictMock<MockListenerDelegate> delegate_privet;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_privet2;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  // Expect record is removed when its TTL expires.
  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &MDnsTest::Stop),
                      Invoke(&record_privet2,
                             &PtrRecordCopyContainer::SaveWithDummyArg)));

  RunFor(base::Seconds(record_privet.ttl() + 1));

  EXPECT_TRUE(record_privet2.IsRecordWith("_privet._tcp.local",
                                          "hello._privet._tcp.local"));
}

// Ensure that the cleanup task scheduler won't schedule cleanup tasks in the
// past if the system clock creeps past the expiration time while in the
// cleanup dispatcher.
TEST_F(MDnsTest, CacheCleanupWithShortTTL) {
  // Use a nonzero starting time as a base.
  base::Time start_time = base::Time() + base::Seconds(1);

  auto timer = std::make_unique<MockTimer>();
  MockTimer* timer_ptr = timer.get();

  auto owned_clock = std::make_unique<MockClock>();
  MockClock* clock = owned_clock.get();
  test_clock_ = std::move(owned_clock);
  test_client_ = std::make_unique<MDnsClientImpl>(clock, std::move(timer));
  ASSERT_THAT(test_client_->StartListening(&socket_factory_), test::IsOk());

  EXPECT_CALL(*timer_ptr, StartObserver(_, _)).Times(1);
  EXPECT_CALL(*clock, Now())
      .Times(3)
      .WillRepeatedly(Return(start_time))
      .RetiresOnSaturation();

  // Receive two records with different TTL values.
  // TTL(privet)=1.0s
  // TTL(printer)=3.0s
  StrictMock<MockListenerDelegate> delegate_privet;
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_printer;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  std::unique_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);

  ASSERT_TRUE(listener_privet->Start());
  ASSERT_TRUE(listener_printer->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1));
  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1));

  SimulatePacketReceive(kSamplePacket3, sizeof(kSamplePacket3));

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .Times(Exactly(1));

  // Set the clock to 2.0s, which should clean up the 'privet' record, but not
  // the printer. The mock clock will change Now() mid-execution from 2s to 4s.
  // Note: expectations are FILO-ordered -- t+2 seconds is returned, then t+4.
  EXPECT_CALL(*clock, Now())
      .WillOnce(Return(start_time + base::Seconds(4)))
      .RetiresOnSaturation();
  EXPECT_CALL(*clock, Now())
      .WillOnce(Return(start_time + base::Seconds(2)))
      .RetiresOnSaturation();

  EXPECT_CALL(*timer_ptr, StartObserver(_, base::TimeDelta()));

  timer_ptr->Fire();
}

TEST_F(MDnsTest, StopListening) {
  ASSERT_TRUE(test_client_->IsListening());

  test_client_->StopListening();
  EXPECT_FALSE(test_client_->IsListening());
}

TEST_F(MDnsTest, StopListening_CacheCleanupScheduled) {
  auto owned_clock = std::make_unique<base::SimpleTestClock>();
  base::SimpleTestClock* clock = owned_clock.get();
  test_clock_ = std::move(owned_clock);

  // Use a nonzero starting time as a base.
  clock->SetNow(base::Time() + base::Seconds(1));
  auto cleanup_timer = std::make_unique<base::MockOneShotTimer>();
  base::OneShotTimer* cleanup_timer_ptr = cleanup_timer.get();

  test_client_ =
      std::make_unique<MDnsClientImpl>(clock, std::move(cleanup_timer));
  ASSERT_THAT(test_client_->StartListening(&socket_factory_), test::IsOk());
  ASSERT_TRUE(test_client_->IsListening());

  // Receive one record (privet) with TTL=1s to schedule cleanup.
  SimulatePacketReceive(kSamplePacket3, sizeof(kSamplePacket3));
  ASSERT_TRUE(cleanup_timer_ptr->IsRunning());

  test_client_->StopListening();
  EXPECT_FALSE(test_client_->IsListening());

  // Expect cleanup unscheduled.
  EXPECT_FALSE(cleanup_timer_ptr->IsRunning());
}

TEST_F(MDnsTest, MalformedPacket) {
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_printer;

  std::unique_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);

  ASSERT_TRUE(listener_printer->Start());

  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_printer,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  // First, send unsalvagable packet to ensure we can deal with it.
  SimulatePacketReceive(kCorruptedPacketUnsalvagable,
                        sizeof(kCorruptedPacketUnsalvagable));

  // Regression test: send a packet where the question cannot be read.
  SimulatePacketReceive(kCorruptedPacketBadQuestion,
                        sizeof(kCorruptedPacketBadQuestion));

  // Then send salvagable packet to ensure we can extract useful records.
  SimulatePacketReceive(kCorruptedPacketSalvagable,
                        sizeof(kCorruptedPacketSalvagable));

  EXPECT_TRUE(record_printer.IsRecordWith("_printer._tcp.local",
                                          "hello._printer._tcp.local"));
}

TEST_F(MDnsTest, TransactionWithEmptyCache) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  PtrRecordCopyContainer record_privet;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, TransactionWithEmptyCacheAndCapitalization) {
  ExpectPacket(kQueryPacketPrivetWithCapitalization,
               sizeof(kQueryPacketPrivetWithCapitalization));

  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_PRIVET._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  PtrRecordCopyContainer record_privet;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(1))
      .WillOnce(
          Invoke(&record_privet, &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1WithCapitalization,
                        sizeof(kSamplePacket1WithCapitalization));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._TCP.local",
                                         "hello._privet._TCP.local"));
}

TEST_F(MDnsTest, TransactionCacheOnlyNoResult) {
  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_CACHE | MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS, _))
      .Times(Exactly(1));

  ASSERT_TRUE(transaction_privet->Start());
}

TEST_F(MDnsTest, TransactionWithCache) {
  // Listener to force the client to listen
  StrictMock<MockListenerDelegate> delegate_irrelevant;
  std::unique_ptr<MDnsListener> listener_irrelevant =
      test_client_->CreateListener(dns_protocol::kTypeA,
                                   "codereview.chromium.local",
                                   &delegate_irrelevant);

  ASSERT_TRUE(listener_irrelevant->Start());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));


  PtrRecordCopyContainer record_privet;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, AdditionalRecords) {
  StrictMock<MockListenerDelegate> delegate_privet;

  PtrRecordCopyContainer record_privet;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(Invoke(
          &record_privet,
          &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacketAdditionalOnly,
                        sizeof(kSamplePacketAdditionalOnly));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));
}

TEST_F(MDnsTest, TransactionTimeout) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS,
                                            nullptr))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::Stop));

  RunFor(base::Seconds(4));
}

TEST_F(MDnsTest, TransactionMultipleRecords) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet->Start());

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_privet2;

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(2))
      .WillOnce(Invoke(&record_privet,
                       &PtrRecordCopyContainer::SaveWithDummyArg))
      .WillOnce(Invoke(&record_privet2,
                       &PtrRecordCopyContainer::SaveWithDummyArg));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
  SimulatePacketReceive(kSamplePacket2, sizeof(kSamplePacket2));

  EXPECT_TRUE(record_privet.IsRecordWith("_privet._tcp.local",
                                         "hello._privet._tcp.local"));

  EXPECT_TRUE(record_privet2.IsRecordWith("_privet._tcp.local",
                                          "zzzzz._privet._tcp.local"));

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_DONE, nullptr))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::Stop));

  RunFor(base::Seconds(4));
}

TEST_F(MDnsTest, TransactionReentrantDelete) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  transaction_ = test_client_->CreateTransaction(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
          MDnsTransaction::SINGLE_RESULT,
      base::BindRepeating(&MDnsTest::MockableRecordCallback,
                          base::Unretained(this)));

  ASSERT_TRUE(transaction_->Start());

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_NO_RESULTS,
                                            nullptr))
      .Times(Exactly(1))
      .WillOnce(DoAll(InvokeWithoutArgs(this, &MDnsTest::DeleteTransaction),
                      InvokeWithoutArgs(this, &MDnsTest::Stop)));

  RunFor(base::Seconds(4));

  EXPECT_EQ(nullptr, transaction_.get());
}

TEST_F(MDnsTest, TransactionReentrantDeleteFromCache) {
  StrictMock<MockListenerDelegate> delegate_irrelevant;
  std::unique_ptr<MDnsListener> listener_irrelevant =
      test_client_->CreateListener(dns_protocol::kTypeA,
                                   "codereview.chromium.local",
                                   &delegate_irrelevant);
  ASSERT_TRUE(listener_irrelevant->Start());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  transaction_ = test_client_->CreateTransaction(
      dns_protocol::kTypePTR, "_privet._tcp.local",
      MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE,
      base::BindRepeating(&MDnsTest::MockableRecordCallback,
                          base::Unretained(this)));

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD, _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::DeleteTransaction));

  ASSERT_TRUE(transaction_->Start());

  EXPECT_EQ(nullptr, transaction_.get());
}

TEST_F(MDnsTest, TransactionReentrantCacheLookupStart) {
  ExpectPacket(kQueryPacketPrivet, sizeof(kQueryPacketPrivet));

  std::unique_ptr<MDnsTransaction> transaction1 =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  std::unique_ptr<MDnsTransaction> transaction2 =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_printer._tcp.local",
          MDnsTransaction::QUERY_CACHE | MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback2,
                              base::Unretained(this)));

  EXPECT_CALL(*this, MockableRecordCallback2(MDnsTransaction::RESULT_RECORD,
                                             _))
      .Times(Exactly(1));

  EXPECT_CALL(*this, MockableRecordCallback(MDnsTransaction::RESULT_RECORD,
                                            _))
      .Times(Exactly(1))
      .WillOnce(IgnoreResult(InvokeWithoutArgs(transaction2.get(),
                                               &MDnsTransaction::Start)));

  ASSERT_TRUE(transaction1->Start());

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
}

TEST_F(MDnsTest, GoodbyePacketNotification) {
  StrictMock<MockListenerDelegate> delegate_privet;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  ASSERT_TRUE(listener_privet->Start());

  SimulatePacketReceive(kSamplePacketGoodbye, sizeof(kSamplePacketGoodbye));

  RunFor(base::Seconds(2));
}

TEST_F(MDnsTest, GoodbyePacketRemoval) {
  StrictMock<MockListenerDelegate> delegate_privet;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1));

  SimulatePacketReceive(kSamplePacket2, sizeof(kSamplePacket2));

  SimulatePacketReceive(kSamplePacketGoodbye, sizeof(kSamplePacketGoodbye));

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .Times(Exactly(1));

  RunFor(base::Seconds(2));
}

// In order to reliably test reentrant listener deletes, we create two listeners
// and have each of them delete both, so we're guaranteed to try and deliver a
// callback to at least one deleted listener.

TEST_F(MDnsTest, ListenerReentrantDelete) {
  StrictMock<MockListenerDelegate> delegate_privet;

  listener1_ = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);

  listener2_ = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);

  ASSERT_TRUE(listener1_->Start());

  ASSERT_TRUE(listener2_->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs(this, &MDnsTest::DeleteBothListeners));

  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));

  EXPECT_EQ(nullptr, listener1_.get());
  EXPECT_EQ(nullptr, listener2_.get());
}

ACTION_P(SaveIPAddress, ip_container) {
  ::testing::StaticAssertTypeEq<const RecordParsed*, arg1_type>();
  ::testing::StaticAssertTypeEq<IPAddress*, ip_container_type>();

  *ip_container = arg1->template rdata<ARecordRdata>()->address();
}

TEST_F(MDnsTest, DoubleRecordDisagreeing) {
  IPAddress address;
  StrictMock<MockListenerDelegate> delegate_privet;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypeA, "privet.local", &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(Exactly(1))
      .WillOnce(SaveIPAddress(&address));

  SimulatePacketReceive(kCorruptedPacketDoubleRecord,
                        sizeof(kCorruptedPacketDoubleRecord));

  EXPECT_EQ("2.3.4.5", address.ToString());
}

TEST_F(MDnsTest, NsecWithListener) {
  StrictMock<MockListenerDelegate> delegate_privet;
  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypeA, "_privet._tcp.local", &delegate_privet);

  // Test to make sure nsec callback is NOT called for PTR
  // (which is marked as existing).
  StrictMock<MockListenerDelegate> delegate_privet2;
  std::unique_ptr<MDnsListener> listener_privet2 = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet2);

  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet,
              OnNsecRecord("_privet._tcp.local", dns_protocol::kTypeA));

  SimulatePacketReceive(kSamplePacketNsec,
                        sizeof(kSamplePacketNsec));
}

TEST_F(MDnsTest, NsecWithTransactionFromNetwork) {
  std::unique_ptr<MDnsTransaction> transaction_privet =
      test_client_->CreateTransaction(
          dns_protocol::kTypeA, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  ASSERT_TRUE(transaction_privet->Start());

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_NSEC, nullptr));

  SimulatePacketReceive(kSamplePacketNsec,
                        sizeof(kSamplePacketNsec));
}

TEST_F(MDnsTest, NsecWithTransactionFromCache) {
  // Force mDNS to listen.
  StrictMock<MockListenerDelegate> delegate_irrelevant;
  std::unique_ptr<MDnsListener> listener_irrelevant =
      test_client_->CreateListener(dns_protocol::kTypePTR, "_privet._tcp.local",
                                   &delegate_irrelevant);
  listener_irrelevant->Start();

  SimulatePacketReceive(kSamplePacketNsec,
                        sizeof(kSamplePacketNsec));

  EXPECT_CALL(*this,
              MockableRecordCallback(MDnsTransaction::RESULT_NSEC, nullptr));

  std::unique_ptr<MDnsTransaction> transaction_privet_a =
      test_client_->CreateTransaction(
          dns_protocol::kTypeA, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  ASSERT_TRUE(transaction_privet_a->Start());

  // Test that a PTR transaction does NOT consider the same NSEC record to be a
  // valid answer to the query

  std::unique_ptr<MDnsTransaction> transaction_privet_ptr =
      test_client_->CreateTransaction(
          dns_protocol::kTypePTR, "_privet._tcp.local",
          MDnsTransaction::QUERY_NETWORK | MDnsTransaction::QUERY_CACHE |
              MDnsTransaction::SINGLE_RESULT,
          base::BindRepeating(&MDnsTest::MockableRecordCallback,
                              base::Unretained(this)));

  EXPECT_CALL(socket_factory_, OnSendTo(_)).Times(2);

  ASSERT_TRUE(transaction_privet_ptr->Start());
}

TEST_F(MDnsTest, NsecConflictRemoval) {
  StrictMock<MockListenerDelegate> delegate_privet;
  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypeA, "_privet._tcp.local", &delegate_privet);

  ASSERT_TRUE(listener_privet->Start());

  const RecordParsed* record1;
  const RecordParsed* record2;

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .WillOnce(SaveArg<1>(&record1));

  SimulatePacketReceive(kSamplePacketAPrivet,
                        sizeof(kSamplePacketAPrivet));

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .WillOnce(SaveArg<1>(&record2));

  EXPECT_CALL(delegate_privet,
              OnNsecRecord("_privet._tcp.local", dns_protocol::kTypeA));

  SimulatePacketReceive(kSamplePacketNsec,
                        sizeof(kSamplePacketNsec));

  EXPECT_EQ(record1, record2);
}

// TODO(crbug.com/40807339): Flaky on fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RefreshQuery DISABLED_RefreshQuery
#else
#define MAYBE_RefreshQuery RefreshQuery
#endif
TEST_F(MDnsTest, MAYBE_RefreshQuery) {
  StrictMock<MockListenerDelegate> delegate_privet;
  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypeA, "_privet._tcp.local", &delegate_privet);

  listener_privet->SetActiveRefresh(true);
  ASSERT_TRUE(listener_privet->Start());

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _));

  SimulatePacketReceive(kSamplePacketAPrivet,
                        sizeof(kSamplePacketAPrivet));

  // Expecting 2 calls (one for ipv4 and one for ipv6) for each of the 2
  // scheduled refresh queries.
  EXPECT_CALL(socket_factory_, OnSendTo(
      MakeString(kQueryPacketPrivetA, sizeof(kQueryPacketPrivetA))))
      .Times(4);

  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _));

  RunFor(base::Seconds(6));
}

// MDnsSocketFactory implementation that creates a single socket that will
// always fail on RecvFrom. Passing this to MdnsClient is expected to result in
// the client failing to start listening.
class FailingSocketFactory : public MDnsSocketFactory {
  void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) override {
    auto socket =
        std::make_unique<MockMDnsDatagramServerSocket>(ADDRESS_FAMILY_IPV4);
    EXPECT_CALL(*socket, RecvFrom(_, _, _, _))
        .WillRepeatedly(Return(ERR_FAILED));
    sockets->push_back(std::move(socket));
  }
};

TEST_F(MDnsTest, StartListeningFailure) {
  test_client_ = std::make_unique<MDnsClientImpl>();
  FailingSocketFactory socket_factory;

  EXPECT_THAT(test_client_->StartListening(&socket_factory),
              test::IsError(ERR_FAILED));
}

// Test that the cache is cleared when it gets filled to unreasonable sizes.
TEST_F(MDnsTest, ClearOverfilledCache) {
  test_client_->core()->cache_for_testing()->set_entry_limit_for_testing(1);

  StrictMock<MockListenerDelegate> delegate_privet;
  StrictMock<MockListenerDelegate> delegate_printer;

  PtrRecordCopyContainer record_privet;
  PtrRecordCopyContainer record_printer;

  std::unique_ptr<MDnsListener> listener_privet = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_privet._tcp.local", &delegate_privet);
  std::unique_ptr<MDnsListener> listener_printer = test_client_->CreateListener(
      dns_protocol::kTypePTR, "_printer._tcp.local", &delegate_printer);

  ASSERT_TRUE(listener_privet->Start());
  ASSERT_TRUE(listener_printer->Start());

  bool privet_added = false;
  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(AtMost(1))
      .WillOnce(Assign(&privet_added, true));
  EXPECT_CALL(delegate_privet, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .WillRepeatedly(Assign(&privet_added, false));

  bool printer_added = false;
  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_ADDED, _))
      .Times(AtMost(1))
      .WillOnce(Assign(&printer_added, true));
  EXPECT_CALL(delegate_printer, OnRecordUpdate(MDnsListener::RECORD_REMOVED, _))
      .WillRepeatedly(Assign(&printer_added, false));

  // Fill past capacity and expect everything to eventually be removed.
  SimulatePacketReceive(kSamplePacket1, sizeof(kSamplePacket1));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(privet_added);
  EXPECT_FALSE(printer_added);
}

// Note: These tests assume that the ipv4 socket will always be created first.
// This is a simplifying assumption based on the way the code works now.
class SimpleMockSocketFactory : public MDnsSocketFactory {
 public:
  void CreateSockets(
      std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) override {
    sockets->clear();
    sockets->swap(sockets_);
  }

  void PushSocket(std::unique_ptr<DatagramServerSocket> socket) {
    sockets_.push_back(std::move(socket));
  }

 private:
  std::vector<std::unique_ptr<DatagramServerSocket>> sockets_;
};

class MockMDnsConnectionDelegate : public MDnsConnection::Delegate {
 public:
  void HandlePacket(DnsResponse* response, int size) override {
    HandlePacketInternal(std::string(response->io_buffer()->data(), size));
  }

  MOCK_METHOD1(HandlePacketInternal, void(std::string packet));

  MOCK_METHOD1(OnConnectionError, void(int error));
};

class MDnsConnectionTest : public TestWithTaskEnvironment {
 public:
  MDnsConnectionTest() : connection_(&delegate_) {
  }

 protected:
  // Follow successful connection initialization.
  void SetUp() override {
    auto socket_ipv4 =
        std::make_unique<MockMDnsDatagramServerSocket>(ADDRESS_FAMILY_IPV4);
    auto socket_ipv6 =
        std::make_unique<MockMDnsDatagramServerSocket>(ADDRESS_FAMILY_IPV6);
    socket_ipv4_ptr_ = socket_ipv4.get();
    socket_ipv6_ptr_ = socket_ipv6.get();
    factory_.PushSocket(std::move(socket_ipv4));
    factory_.PushSocket(std::move(socket_ipv6));
    sample_packet_ = MakeString(kSamplePacket1, sizeof(kSamplePacket1));
    sample_buffer_ = base::MakeRefCounted<StringIOBuffer>(sample_packet_);
  }

  int InitConnection() { return connection_.Init(&factory_); }

  StrictMock<MockMDnsConnectionDelegate> delegate_;

  raw_ptr<MockMDnsDatagramServerSocket, DanglingUntriaged> socket_ipv4_ptr_;
  raw_ptr<MockMDnsDatagramServerSocket, DanglingUntriaged> socket_ipv6_ptr_;
  SimpleMockSocketFactory factory_;
  MDnsConnection connection_;
  TestCompletionCallback callback_;
  std::string sample_packet_;
  scoped_refptr<IOBuffer> sample_buffer_;
};

TEST_F(MDnsConnectionTest, ReceiveSynchronous) {
  socket_ipv6_ptr_->SetResponsePacket(sample_packet_);
  EXPECT_CALL(*socket_ipv4_ptr_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_ptr_, RecvFrom(_, _, _, _))
      .WillOnce(Invoke(socket_ipv6_ptr_.get(),
                       &MockMDnsDatagramServerSocket::HandleRecvNow))
      .WillOnce(Return(ERR_IO_PENDING));

  EXPECT_CALL(delegate_, HandlePacketInternal(sample_packet_));
  EXPECT_THAT(InitConnection(), test::IsOk());
}

TEST_F(MDnsConnectionTest, ReceiveAsynchronous) {
  socket_ipv6_ptr_->SetResponsePacket(sample_packet_);

  EXPECT_CALL(*socket_ipv4_ptr_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_ptr_, RecvFrom(_, _, _, _))
      .Times(2)
      .WillOnce(Invoke(socket_ipv6_ptr_.get(),
                       &MockMDnsDatagramServerSocket::HandleRecvLater))
      .WillOnce(Return(ERR_IO_PENDING));

  ASSERT_THAT(InitConnection(), test::IsOk());

  EXPECT_CALL(delegate_, HandlePacketInternal(sample_packet_));

  base::RunLoop().RunUntilIdle();
}

TEST_F(MDnsConnectionTest, Error) {
  CompletionOnceCallback callback;

  EXPECT_CALL(*socket_ipv4_ptr_, RecvFrom(_, _, _, _))
      .WillOnce(Return(ERR_IO_PENDING));
  EXPECT_CALL(*socket_ipv6_ptr_, RecvFrom(_, _, _, _))
      .WillOnce([&](auto, auto, auto, auto cb) {
        callback = std::move(cb);
        return ERR_IO_PENDING;
      });

  ASSERT_THAT(InitConnection(), test::IsOk());

  EXPECT_CALL(delegate_, OnConnectionError(ERR_SOCKET_NOT_CONNECTED));
  std::move(callback).Run(ERR_SOCKET_NOT_CONNECTED);
  base::RunLoop().RunUntilIdle();
}

class MDnsConnectionSendTest : public MDnsConnectionTest {
 protected:
  void SetUp() override {
    MDnsConnectionTest::SetUp();
    EXPECT_CALL(*socket_ipv4_ptr_, RecvFrom(_, _, _, _))
        .WillOnce(Return(ERR_IO_PENDING));
    EXPECT_CALL(*socket_ipv6_ptr_, RecvFrom(_, _, _, _))
        .WillOnce(Return(ERR_IO_PENDING));
    EXPECT_THAT(InitConnection(), test::IsOk());
  }
};

TEST_F(MDnsConnectionSendTest, Send) {
  EXPECT_CALL(*socket_ipv4_ptr_,
              SendToInternal(sample_packet_, "224.0.0.251:5353", _));
  EXPECT_CALL(*socket_ipv6_ptr_,
              SendToInternal(sample_packet_, "[ff02::fb]:5353", _));

  connection_.Send(sample_buffer_, sample_packet_.size());
}

TEST_F(MDnsConnectionSendTest, SendError) {
  EXPECT_CALL(*socket_ipv4_ptr_,
              SendToInternal(sample_packet_, "224.0.0.251:5353", _));
  EXPECT_CALL(*socket_ipv6_ptr_,
              SendToInternal(sample_packet_, "[ff02::fb]:5353", _))
      .WillOnce(Return(ERR_SOCKET_NOT_CONNECTED));

  connection_.Send(sample_buffer_, sample_packet_.size());
  EXPECT_CALL(delegate_, OnConnectionError(ERR_SOCKET_NOT_CONNECTED));
  base::RunLoop().RunUntilIdle();
}

TEST_F(MDnsConnectionSendTest, SendQueued) {
  // Send data immediately.
  EXPECT_CALL(*socket_ipv4_ptr_,
              SendToInternal(sample_packet_, "224.0.0.251:5353", _))
      .Times(2)
      .WillRepeatedly(Return(OK));

  CompletionOnceCallback callback;
  // Delay sending data. Only the first call should be made.
  EXPECT_CALL(*socket_ipv6_ptr_,
              SendToInternal(sample_packet_, "[ff02::fb]:5353", _))
      .WillOnce([&](auto, auto, auto cb) {
        callback = std::move(cb);
        return ERR_IO_PENDING;
      });

  connection_.Send(sample_buffer_, sample_packet_.size());
  connection_.Send(sample_buffer_, sample_packet_.size());

  // The second IPv6 packet is not sent yet.
  EXPECT_CALL(*socket_ipv4_ptr_,
              SendToInternal(sample_packet_, "224.0.0.251:5353", _))
      .Times(0);
  // Expect call for the second IPv6 packet.
  EXPECT_CALL(*socket_ipv6_ptr_,
              SendToInternal(sample_packet_, "[ff02::fb]:5353", _))
      .WillOnce(Return(OK));
  std::move(callback).Run(OK);
}

TEST(MDnsSocketTest, CreateSocket) {
  // Verifies that socket creation hasn't been broken.
  auto socket = CreateAndBindMDnsSocket(AddressFamily::ADDRESS_FAMILY_IPV4, 1,
                                        net::NetLog::Get());
  EXPECT_TRUE(socket);
  socket->Close();
}

}  // namespace net
