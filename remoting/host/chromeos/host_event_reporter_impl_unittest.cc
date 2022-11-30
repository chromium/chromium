// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/host_event_reporter_impl.h"

#include "base/memory/ptr_util.h"
#include "base/test/repeating_test_future.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/crd_event.pb.h"
#include "remoting/protocol/transport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::StrEq;

namespace remoting {
namespace {

constexpr char kHostUser[] = "host@example.com";
constexpr char kHostIp[] = "127.0.0.1:1234";
constexpr char kClientIp[] = "99.88.77.66:4321";
constexpr char kSessionId[] = "0123456789/client@example.com";
constexpr ::ash::reporting::ConnectionType kConnectionType =
    ::ash::reporting::ConnectionType::CRD_CONNECTION_DIRECT;

class TestHostEventReporterDelegate : public HostEventReporterImpl::Delegate {
 public:
  TestHostEventReporterDelegate() = default;

  void EnqueueEvent(::ash::reporting::CRDRecord record) override {
    records_.AddValue(std::move(record));
  }

  ::ash::reporting::CRDRecord WaitForEvent() { return records_.Take(); }

  bool IsEmpty() const { return records_.IsEmpty(); }

 private:
  base::test::RepeatingTestFuture<::ash::reporting::CRDRecord> records_;
};

class HostEventReporterTest : public ::testing::Test {
 protected:
  HostEventReporterTest()
      : delegate_(new TestHostEventReporterDelegate()),
        monitor_(new HostStatusMonitor()),
        reporter_(monitor_, base::WrapUnique(delegate_)) {}

  TestHostEventReporterDelegate* const delegate_;
  scoped_refptr<HostStatusMonitor> monitor_;
  HostEventReporterImpl reporter_;
};

TEST_F(HostEventReporterTest, ReportConnectedAndDisconnected) {
  reporter_.OnHostStarted(kHostUser);
  reporter_.OnClientConnected(kHostIp);

  protocol::TransportRoute route;
  route.type = protocol::TransportRoute::RouteType::DIRECT;
  route.remote_address = net::IPEndPoint(net::IPAddress(99, 88, 77, 66), 4321);
  route.local_address = net::IPEndPoint(net::IPAddress(127, 0, 0, 1), 1234);

  reporter_.OnClientRouteChange(kSessionId, "", route);
  reporter_.OnClientDisconnected(kSessionId);
  reporter_.OnHostShutdown();

  auto received = delegate_->WaitForEvent();
  EXPECT_THAT(received.host_user().user_email(), StrEq(kHostUser));
  ASSERT_TRUE(received.has_started());

  received = delegate_->WaitForEvent();
  EXPECT_THAT(received.host_user().user_email(), StrEq(kHostUser));
  ASSERT_TRUE(received.has_connected());
  EXPECT_THAT(received.connected().host_ip(), StrEq(kHostIp));
  EXPECT_THAT(received.connected().client_ip(), StrEq(kClientIp));
  EXPECT_THAT(received.connected().session_id(), StrEq(kSessionId));
  EXPECT_THAT(received.connected().connection_type(), Eq(kConnectionType));

  received = delegate_->WaitForEvent();
  EXPECT_THAT(received.host_user().user_email(), StrEq(kHostUser));
  ASSERT_TRUE(received.has_disconnected());
  EXPECT_THAT(received.disconnected().host_ip(), StrEq(kHostIp));
  EXPECT_THAT(received.disconnected().client_ip(), StrEq(kClientIp));
  EXPECT_THAT(received.disconnected().session_id(), StrEq(kSessionId));

  received = delegate_->WaitForEvent();
  EXPECT_THAT(received.host_user().user_email(), StrEq(kHostUser));
  ASSERT_TRUE(received.has_ended());

  EXPECT_TRUE(delegate_->IsEmpty());
}

}  // namespace

// static
std::unique_ptr<HostEventReporter> HostEventReporter::Create(
    scoped_refptr<HostStatusMonitor> monitor) {
  return std::make_unique<HostEventReporterImpl>(
      monitor, std::make_unique<TestHostEventReporterDelegate>());
}

}  // namespace remoting
