// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/if.h>
#include <linux/if_addr.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/functional/bind.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/address_map_linux.h"
#include "net/base/address_tracker_linux_test_util.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/base/network_change_notifier_linux.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
constexpr unsigned char kAddress0[] = {127, 0, 0, 1};
constexpr unsigned char kAddress1[] = {10, 0, 0, 1};
constexpr unsigned char kAddress2[] = {192, 168, 0, 1};
constexpr unsigned char kAddress3[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 1};

class NCNLinuxMockedNetlinkTestUtil {
 public:
  static constexpr int kTestInterfaceEth = 1;
  static constexpr int kTestInterfaceOther = 2;
  static inline const net::IPAddress kEmpty;
  static inline const net::IPAddress kAddr0{kAddress0};
  static inline const net::IPAddress kAddr1{kAddress1};
  static inline const net::IPAddress kAddr2{kAddress2};
  static inline const net::IPAddress kAddr3{kAddress3};

  NCNLinuxMockedNetlinkTestUtil() = default;
  ~NCNLinuxMockedNetlinkTestUtil() = default;

  std::unique_ptr<net::NetworkChangeNotifierLinux> CreateNCNLinux() {
    base::ScopedFD netlink_fd_receiver;
    base::CreateSocketPair(&fake_netlink_fd_, &netlink_fd_receiver);
    auto ncn_linux =
        net::NetworkChangeNotifierLinux::CreateWithSocketForTesting(
            {}, std::move(netlink_fd_receiver));

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
  // Sets up the AddressMap with kAddr0, and kTestInterfaceEth online.
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

  void BufferAddAddrMsg(const net::IPAddress address,
                        int interface = kTestInterfaceEth,
                        uint8_t flags = IFA_F_TEMPORARY) {
    net::test::MakeAddrMessage(RTM_NEWADDR, flags,
                               address.IsIPv4() ? AF_INET : AF_INET6, interface,
                               address, kEmpty, &buffer_);
  }

  void BufferDeleteAddrMsg(const net::IPAddress& address,
                           int interface = kTestInterfaceEth) {
    net::test::MakeAddrMessage(RTM_DELADDR, 0,
                               address.IsIPv4() ? AF_INET : AF_INET6, interface,
                               address, kEmpty, &buffer_);
  }

  void BufferAddLinkMsg(int link) {
    net::test::MakeLinkMessage(RTM_NEWLINK, IFF_UP | IFF_LOWER_UP | IFF_RUNNING,
                               link, &buffer_, /*clear_output=*/false);
  }

  void BufferDeleteLinkMsg(int link) {
    net::test::MakeLinkMessage(RTM_DELLINK, 0, link, &buffer_,
                               /*clear_output=*/false);
  }

  void SendBuffer() {
    base::UnixDomainSocket::SendMsg(fake_netlink_fd_.get(), buffer_.data(),
                                    buffer_.size(), {});
    buffer_.clear();
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
  base::ScopedFD fake_netlink_fd_;

  bool initialized_ = false;
  base::RunLoop initialize_run_loop_;

  net::test::NetlinkBuffer buffer_;
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

enum class ExpectedConnectionType {
  kNone,
  kConnected,
};

class AddressMapLinuxBrowserTest : public ContentBrowserTest {
 public:
  struct ExpectedCachedInfo {
    std::vector<net::IPAddress> should_contain_addresses;
    std::vector<net::IPAddress> should_not_contain_addresses;
    std::vector<int> should_contain_links;
    std::vector<int> should_not_contain_links;
  };

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kAddressTrackerLinuxIsProxied);
    ForceOutOfProcessNetworkService();
    ncn_mocked_factory_ =
        std::make_unique<NetworkChangeNotifierLinuxMockedNetlinkFactory>();
    net::NetworkChangeNotifier::SetFactory(ncn_mocked_factory_.get());
    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager;
    GetNetworkService()->GetNetworkChangeManager(
        network_change_manager.BindNewPipeAndPassReceiver());

    mojo::PendingReceiver<network::mojom::NetworkChangeManagerClient>
        client_receiver;
    network_change_manager->RequestNotifications(
        client_receiver.InitWithNewPipeAndPassRemote());
    notification_listener_ =
        std::make_unique<NetworkChangeNotificationListener>(
            std::move(client_receiver));
  }

  void ExpectCorrectInfoInNetworkService(
      ExpectedCachedInfo expected_cached_info) {
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

    for (const net::IPAddress& address :
         expected_cached_info.should_contain_addresses) {
      SCOPED_TRACE(testing::Message()
                   << "Network service AddressMap should include "
                   << address.ToString());
      EXPECT_TRUE(network_service_addr_map.contains(address));
    }
    for (const net::IPAddress& address :
         expected_cached_info.should_not_contain_addresses) {
      SCOPED_TRACE(testing::Message()
                   << "Network service AddressMap should not include "
                   << address.ToString());
      EXPECT_FALSE(network_service_addr_map.contains(address));
    }
    for (const int link : expected_cached_info.should_contain_links) {
      SCOPED_TRACE(testing::Message()
                   << "Network service online links should include " << link);
      EXPECT_TRUE(network_service_links.contains(link));
    }
    for (const int link : expected_cached_info.should_not_contain_links) {
      SCOPED_TRACE(testing::Message()
                   << "Network service online links should not include "
                   << link);
      EXPECT_FALSE(network_service_links.contains(link));
    }
  }

  void WaitForNetworkChange(ExpectedConnectionType expected_connection_type) {
    notification_listener_->WaitForNetworkChange(expected_connection_type);
  }

 protected:
  std::unique_ptr<NetworkChangeNotifierLinuxMockedNetlinkFactory>
      ncn_mocked_factory_;

 private:
  class NetworkChangeNotificationListener
      : public network::mojom::NetworkChangeManagerClient {
   public:
    explicit NetworkChangeNotificationListener(
        mojo::PendingReceiver<network::mojom::NetworkChangeManagerClient>
            receiver)
        : receiver_(this, std::move(receiver)) {}
    void OnInitialConnectionType(network::mojom::ConnectionType type) override {
    }

    void OnNetworkChanged(network::mojom::ConnectionType type) override {
      // NetworkChangeNotifier::NetworkChangeObserver will fire a
      // CONNECTION_NONE change right before firing a non-CONNECTION_NONE
      // change. So if this is a CONNECTION_NONE event, only continue the test
      // if the test is expecting a CONNECTION_NONE event.
      // TODO(mpdenton): set timeouts to zero in the network process so tests
      // run faster.
      if ((expected_connection_type_ == ExpectedConnectionType::kNone ||
           type != network::mojom::ConnectionType::CONNECTION_NONE) &&
          run_loop_.has_value()) {
        run_loop_->Quit();
      }
    }

    void WaitForNetworkChange(ExpectedConnectionType expected_connection_type) {
      expected_connection_type_ = expected_connection_type;
      run_loop_.emplace();
      run_loop_->Run();
      run_loop_.reset();
    }

   private:
    mojo::Receiver<network::mojom::NetworkChangeManagerClient> receiver_;
    std::optional<base::RunLoop> run_loop_;
    ExpectedConnectionType expected_connection_type_;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkChangeNotificationListener> notification_listener_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(AddressMapLinuxBrowserTest, CheckInitialMapsMatch) {
  if (IsInProcessNetworkService()) {
    GTEST_SKIP();
  }

  ncn_mocked_factory_->ncn_wrapper()->WaitForInit();

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});
}

IN_PROC_BROWSER_TEST_F(AddressMapLinuxBrowserTest, CheckAddressMapDiffsApply) {
  // Delete kAddr0 from the map.
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr0);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kNone);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {},
       .should_not_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});

  // Now add kAddr0 back to the map.
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr0);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kConnected);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});

  // Now change kAddr0's ifaddrmsg. Use flags = IFA_F_HOMEADDRESS rather than
  // flags = IFA_F_TEMPORARY.
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr0,
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth, IFA_F_HOMEADDRESS);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kConnected);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});

  // Add the other addresses, and then delete one of them and delete the
  // existing address before sending diffs.
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr1);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr2);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr3);

  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr2);
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr0);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kConnected);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr1,
                                    NCNLinuxMockedNetlinkTestUtil::kAddr3},
       .should_not_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0,
                                        NCNLinuxMockedNetlinkTestUtil::kAddr2},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});
}

IN_PROC_BROWSER_TEST_F(AddressMapLinuxBrowserTest, CheckOnlineLinksDiffsApply) {
  // Delete the link.
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kNone);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links = {},
       .should_not_contain_links = {
           NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth}});

  // Add the link back.
  ncn_mocked_factory_->ncn_wrapper()->BufferAddLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  WaitForNetworkChange(ExpectedConnectionType::kConnected);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth},
       .should_not_contain_links = {}});

  // Delete link 1, add it back, delete it again. Also add link 2, delete it,
  // and add it back again.
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);

  ncn_mocked_factory_->ncn_wrapper()->BufferAddLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther);
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther);
  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  // Unconnected because kTestInterfaceOther has no addresses.
  WaitForNetworkChange(ExpectedConnectionType::kNone);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0},
       .should_not_contain_addresses = {},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther},
       .should_not_contain_links = {
           NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth}});
}

IN_PROC_BROWSER_TEST_F(AddressMapLinuxBrowserTest, CheckBothDiffsApply) {
  // Delete the link, add another.
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddLinkMsg(
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther);
  // Add some addresses
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr1,
      NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr2);
  ncn_mocked_factory_->ncn_wrapper()->BufferAddAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr3);
  // Delete some addresses.
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr2);
  ncn_mocked_factory_->ncn_wrapper()->BufferDeleteAddrMsg(
      NCNLinuxMockedNetlinkTestUtil::kAddr0);

  ncn_mocked_factory_->ncn_wrapper()->SendBuffer();

  // Connected because kAddr1 is associated with kTestInterfaceOther.
  WaitForNetworkChange(ExpectedConnectionType::kConnected);

  ExpectCorrectInfoInNetworkService(
      {.should_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr1,
                                    NCNLinuxMockedNetlinkTestUtil::kAddr3},
       .should_not_contain_addresses = {NCNLinuxMockedNetlinkTestUtil::kAddr0,
                                        NCNLinuxMockedNetlinkTestUtil::kAddr2},
       .should_contain_links =
           {NCNLinuxMockedNetlinkTestUtil::kTestInterfaceOther},
       .should_not_contain_links = {
           NCNLinuxMockedNetlinkTestUtil::kTestInterfaceEth}});
}

}  // namespace content
