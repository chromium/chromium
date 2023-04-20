// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <unordered_set>
#include <vector>

#include "base/functional/bind.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux_test_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/base/network_change_notifier_linux.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
constexpr unsigned char kAddress0[] = {127, 0, 0, 1};

class NCNLinuxMockedNetlinkTestUtil {
 public:
  static constexpr int kTestInterfaceEth = 1;
  static inline const net::IPAddress kEmpty;
  static inline const net::IPAddress kAddr0{kAddress0};

  NCNLinuxMockedNetlinkTestUtil() = default;
  ~NCNLinuxMockedNetlinkTestUtil() = default;

  std::unique_ptr<net::NetworkChangeNotifierLinux> CreateNCNLinux() {
    base::ScopedFD netlink_fd_receiver;
    base::CreateSocketPair(&fake_netlink_fd_, &netlink_fd_receiver);
    auto ncn_linux =
        net::NetworkChangeNotifierLinux::CreateWithSocketForTesting(
            {}, std::move(netlink_fd_receiver));
    ncn_linux_ = ncn_linux.get();

    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &NCNLinuxMockedNetlinkTestUtil::SimulateAddressTrackerLinuxStart,
            base::Unretained(this)),
        base::BindOnce(&NCNLinuxMockedNetlinkTestUtil::SetInitialized,
                       base::Unretained(this)));

    return ncn_linux;
  }

  // This should run on a thread pool thread because it blocks.
  void SimulateAddressTrackerLinuxStart() {
    struct {
      struct nlmsghdr header;
      struct rtgenmsg msg;
    } request = {};

    // Receive the RTM_GETADDR request.
    std::vector<base::ScopedFD> fds;
    ssize_t expected_size = NLMSG_LENGTH(sizeof(request.msg));
    EXPECT_EQ(base::UnixDomainSocket::RecvMsg(fake_netlink_fd_.get(), &request,
                                              expected_size, &fds),
              expected_size);
    EXPECT_TRUE(fds.empty());
    EXPECT_EQ(request.header.nlmsg_type, RTM_GETADDR);

    // Send a response.
    net::test::NetlinkBuffer buffer;
    net::test::MakeAddrMessage(RTM_NEWADDR, IFA_F_TEMPORARY, AF_INET,
                               kTestInterfaceEth, kAddr0, kEmpty, &buffer);
    base::UnixDomainSocket::SendMsg(fake_netlink_fd_.get(), buffer.data(),
                                    buffer.size(), {});

    // Receive the RTM_GETLINK request.
    EXPECT_EQ(base::UnixDomainSocket::RecvMsg(fake_netlink_fd_.get(), &request,
                                              expected_size, &fds),
              expected_size);
    EXPECT_EQ(request.header.nlmsg_type, RTM_GETLINK);

    // Send a response.
    buffer.clear();
    net::test::MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                               kTestInterfaceEth, &buffer);
    base::UnixDomainSocket::SendMsg(fake_netlink_fd_.get(), buffer.data(),
                                    buffer.size(), {});
  }

  void SetInitialized() {
    initialized_ = true;
    initialize_run_loop_.Quit();
  }

  void WaitForInit() {
    if (initialized_) {
      return;
    }
    initialize_run_loop_.Run();
  }

 private:
  raw_ptr<net::NetworkChangeNotifierLinux> ncn_linux_;
  base::ScopedFD fake_netlink_fd_;

  bool initialized_ = false;
  base::RunLoop initialize_run_loop_;
};

class NetworkChangeNotifierLinuxMockedNetlinkFactory
    : public net::NetworkChangeNotifierFactory {
 public:
  std::unique_ptr<net::NetworkChangeNotifier> CreateInstanceWithInitialTypes(
      net::NetworkChangeNotifier::ConnectionType /*initial_type*/,
      net::NetworkChangeNotifier::ConnectionSubtype /*nitial_subtype*/)
      override {
    // There should only be one called to this factory function.
    DCHECK(!ncn_wrapper_);
    ncn_wrapper_ = std::make_unique<NCNLinuxMockedNetlinkTestUtil>();
    return ncn_wrapper_->CreateNCNLinux();
  }

  NCNLinuxMockedNetlinkTestUtil* ncn_wrapper() { return ncn_wrapper_.get(); }

 private:
  std::unique_ptr<NCNLinuxMockedNetlinkTestUtil> ncn_wrapper_;
};

class AddressMapLinuxBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    ncn_mocked_factory_ = new NetworkChangeNotifierLinuxMockedNetlinkFactory();
    net::NetworkChangeNotifier::SetFactory(ncn_mocked_factory_);
    ContentBrowserTest::SetUp();
  }

 protected:
  raw_ptr<NetworkChangeNotifierLinuxMockedNetlinkFactory> ncn_mocked_factory_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(AddressMapLinuxBrowserTest, CheckInitialMapsMatch) {
  if (IsInProcessNetworkService()) {
    GTEST_SKIP();
  }

  ncn_mocked_factory_->ncn_wrapper()->WaitForInit();

  const net::AddressMapOwnerLinux* address_map_owner =
      net::NetworkChangeNotifier::GetAddressMapOwner();

  net::AddressMapOwnerLinux::AddressMap network_service_addr_map;
  std::unordered_set<int> network_service_links;
  {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test()->GetAddressMapCacheLinux(&network_service_addr_map,
                                                    &network_service_links);
  }

  net::AddressMapOwnerLinux::AddressMap browser_process_addr_map =
      address_map_owner->GetAddressMap();
  std::unordered_set<int> browser_process_links =
      address_map_owner->GetOnlineLinks();
  EXPECT_EQ(browser_process_addr_map, network_service_addr_map);
  EXPECT_EQ(browser_process_links, network_service_links);
  EXPECT_TRUE(
      network_service_addr_map.contains(NCNLinuxMockedNetlinkTestUtil::kAddr0));
  EXPECT_TRUE(network_service_links.contains(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth));
}
}  // namespace content
