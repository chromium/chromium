// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <winsock2.h>

#include <iphlpapi.h>

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "net/base/features.h"
#include "net/base/network_cost_change_notifier_win.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"

namespace net {

namespace {

// Time between NotifyAddrChange retries, on failure.
const int kWatchForAddressChangeRetryIntervalMs = 500;

}  // namespace

NetworkChangeNotifierWin::NetworkChangeNotifierWin()
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsWin()),
      blocking_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      last_computed_connection_type_(RecomputeCurrentConnectionType()),
      last_announced_offline_(last_computed_connection_type_ ==
                              CONNECTION_NONE),
      sequence_runner_for_registration_(
          base::SequencedTaskRunner::GetCurrentDefault()) {
  memset(&addr_overlapped_, 0, sizeof addr_overlapped_);
  addr_overlapped_.hEvent = WSACreateEvent();

  cost_change_notifier_ = NetworkCostChangeNotifierWin::CreateInstance(
      base::BindRepeating(&NetworkChangeNotifierWin::OnCostChanged,
                          weak_factory_.GetWeakPtr()));
}

NetworkChangeNotifierWin::~NetworkChangeNotifierWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearGlobalPointer();
  if (is_watching_) {
    CancelIPChangeNotify(&addr_overlapped_);
    addr_watcher_.StopWatching();
  }
  WSACloseEvent(addr_overlapped_.hEvent);
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierWin::NetworkChangeCalculatorParamsWin() {
  NetworkChangeCalculatorParams params;
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::Milliseconds(1500);
  params.ip_address_online_delay_ = base::Milliseconds(1500);
  params.connection_type_offline_delay_ = base::Milliseconds(1500);
  params.connection_type_online_delay_ = base::Milliseconds(500);
  return params;
}

// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierWin::RecomputeCurrentConnectionTypeModern() {
  using GetNetworkConnectivityHintType =
      decltype(&::GetNetworkConnectivityHint);

  // This API is only available on Windows 10 Build 19041. However, it works
  // inside the Network Service Sandbox, so is preferred. See
  GetNetworkConnectivityHintType get_network_connectivity_hint =
      reinterpret_cast<GetNetworkConnectivityHintType>(::GetProcAddress(
          ::GetModuleHandleA("iphlpapi.dll"), "GetNetworkConnectivityHint"));
  if (!get_network_connectivity_hint) {
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
  NL_NETWORK_CONNECTIVITY_HINT hint;
  // https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getnetworkconnectivityhint.
  auto ret = get_network_connectivity_hint(&hint);
  if (ret != NO_ERROR) {
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

  switch (hint.ConnectivityLevel) {
    case NetworkConnectivityLevelHintUnknown:
      return NetworkChangeNotifier::CONNECTION_UNKNOWN;
    case NetworkConnectivityLevelHintNone:
    case NetworkConnectivityLevelHintHidden:
      return NetworkChangeNotifier::CONNECTION_NONE;
    case NetworkConnectivityLevelHintLocalAccess:
    case NetworkConnectivityLevelHintInternetAccess:
    case NetworkConnectivityLevelHintConstrainedInternetAccess:
      // TODO(droger): Return something more detailed than CONNECTION_UNKNOWN.
      return ConnectionTypeFromInterfaces();
  }

  NOTREACHED();
}

// This implementation does not return the actual connection type but merely
// determines if the user is "online" (in which case it returns
// CONNECTION_UNKNOWN) or "offline" (and then it returns CONNECTION_NONE).
// This is challenging since the only thing we can test with certainty is
// whether a *particular* host is reachable.
//
// While we can't conclusively determine when a user is "online", we can at
// least reliably recognize some of the situtations when they are clearly
// "offline". For example, if the user's laptop is not plugged into an ethernet
// network and is not connected to any wireless networks, it must be offline.
//
// There are a number of different ways to implement this on Windows, each with
// their pros and cons. Here is a comparison of various techniques considered:
//
// (1) Use InternetGetConnectedState (wininet.dll). This function is really easy
// to use (literally a one-liner), and runs quickly. The drawback is it adds a
// dependency on the wininet DLL.
//
// (2) Enumerate all of the network interfaces using GetAdaptersAddresses
// (iphlpapi.dll), and assume we are "online" if there is at least one interface
// that is connected, and that interface is not a loopback or tunnel.
//
// Safari on Windows has a fairly simple implementation that does this:
// http://trac.webkit.org/browser/trunk/WebCore/platform/network/win/NetworkStateNotifierWin.cpp.
//
// Mozilla similarly uses this approach:
// http://mxr.mozilla.org/mozilla1.9.2/source/netwerk/system/win32/nsNotifyAddrListener.cpp
//
// The biggest drawback to this approach is it is quite complicated.
// WebKit's implementation for example doesn't seem to test for ICS gateways
// (internet connection sharing), whereas Mozilla's implementation has extra
// code to guess that.
//
// (3) The method used in this file comes from google talk, and is similar to
// method (2). The main difference is it enumerates the winsock namespace
// providers rather than the actual adapters.
//
// I ran some benchmarks comparing the performance of each on my Windows 7
// workstation. Here is what I found:
//   * Approach (1) was pretty much zero-cost after the initial call.
//   * Approach (2) took an average of 3.25 milliseconds to enumerate the
//     adapters.
//   * Approach (3) took an average of 0.8 ms to enumerate the providers.
//
// In terms of correctness, all three approaches were comparable for the simple
// experiments I ran... However none of them correctly returned "offline" when
// executing 'ipconfig /release'.
//
// static
NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierWin::RecomputeCurrentConnectionType() {
  if (base::win::GetVersion() >= base::win::Version::WIN10_20H1 &&
      base::FeatureList::IsEnabled(
          features::kEnableGetNetworkConnectivityHintAPI)) {
    return RecomputeCurrentConnectionTypeModern();
  }

  EnsureWinsockInit();

  // The following code was adapted from:
  // http://src.chromium.org/viewvc/chrome/trunk/src/chrome/common/net/notifier/base/win/async_network_alive_win32.cc?view=markup&pathrev=47343
  // The main difference is we only call WSALookupServiceNext once, whereas
  // the earlier code would traverse the entire list and pass LUP_FLUSHPREVIOUS
  // to skip past the large results.

  HANDLE ws_handle;
  WSAQUERYSET query_set = {0};
  query_set.dwSize = sizeof(WSAQUERYSET);
  query_set.dwNameSpace = NS_NLA;
  // Initiate a client query to iterate through the
  // currently connected networks.
  if (0 != WSALookupServiceBegin(&query_set, LUP_RETURN_ALL, &ws_handle)) {
    LOG(ERROR) << "WSALookupServiceBegin failed with: " << WSAGetLastError();
    return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }

  bool found_connection = false;

  // Retrieve the first available network. In this function, we only
  // need to know whether or not there is network connection.
  // Allocate 256 bytes for name, it should be enough for most cases.
  // If the name is longer, it is OK as we will check the code returned and
  // set correct network status.
  char result_buffer[sizeof(WSAQUERYSET) + 256] = {0};
  DWORD length = sizeof(result_buffer);
  reinterpret_cast<WSAQUERYSET*>(&result_buffer[0])->dwSize =
      sizeof(WSAQUERYSET);
  int result =
      WSALookupServiceNext(ws_handle, LUP_RETURN_NAME, &length,
                           reinterpret_cast<WSAQUERYSET*>(&result_buffer[0]));

  if (result == 0) {
    // Found a connection!
    found_connection = true;
  } else {
    DCHECK_EQ(SOCKET_ERROR, result);
    result = WSAGetLastError();

    // Error code WSAEFAULT means there is a network connection but the
    // result_buffer size is too small to contain the results. The
    // variable "length" returned from WSALookupServiceNext is the minimum
    // number of bytes required. We do not need to retrieve detail info,
    // it is enough knowing there was a connection.
    if (result == WSAEFAULT) {
      found_connection = true;
    } else if (result == WSA_E_NO_MORE || result == WSAENOMORE) {
      // There was nothing to iterate over!
    } else {
      LOG(WARNING) << "WSALookupServiceNext() failed with:" << result;
    }
  }

  result = WSALookupServiceEnd(ws_handle);
  LOG_IF(ERROR, result != 0) << "WSALookupServiceEnd() failed with: " << result;

  // TODO(droger): Return something more detailed than CONNECTION_UNKNOWN.
  return found_connection ? ConnectionTypeFromInterfaces()
                          : NetworkChangeNotifier::CONNECTION_NONE;
}

void NetworkChangeNotifierWin::RecomputeCurrentConnectionTypeOnBlockingSequence(
    base::OnceCallback<void(ConnectionType)> reply_callback) const {
  // Unretained is safe in this call because this object owns the thread and the
  // thread is stopped in this object's destructor.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&NetworkChangeNotifierWin::RecomputeCurrentConnectionType),
      std::move(reply_callback));
}

NetworkChangeNotifier::ConnectionCost
NetworkChangeNotifierWin::GetCurrentConnectionCost() {
  if (last_computed_connection_cost_ ==
      ConnectionCost::CONNECTION_COST_UNKNOWN) {
    // Use the default logic when the Windows OS APIs do not have a cost for the
    // current connection.
    return NetworkChangeNotifier::GetCurrentConnectionCost();
  }
  return last_computed_connection_cost_;
}

void NetworkChangeNotifierWin::OnCostChanged(
    NetworkChangeNotifier::ConnectionCost new_cost) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only notify if there's actually a change.
  if (last_computed_connection_cost_ != new_cost) {
    last_computed_connection_cost_ = new_cost;
    NotifyObserversOfConnectionCostChange();
  }
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierWin::GetCurrentConnectionType() const {
  base::AutoLock auto_lock(last_computed_connection_type_lock_);
  return last_computed_connection_type_;
}

void NetworkChangeNotifierWin::SetCurrentConnectionType(
    ConnectionType connection_type) {
  base::AutoLock auto_lock(last_computed_connection_type_lock_);
  last_computed_connection_type_ = connection_type;
}

void NetworkChangeNotifierWin::OnObjectSignaled(HANDLE object) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_watching_);
  is_watching_ = false;

  // Start watching for the next address change.
  WatchForAddressChange();

  RecomputeCurrentConnectionTypeOnBlockingSequence(base::BindOnce(
      &NetworkChangeNotifierWin::NotifyObservers, weak_factory_.GetWeakPtr()));
}

void NetworkChangeNotifierWin::NotifyObservers(ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetCurrentConnectionType(connection_type);
  NotifyObserversOfIPAddressChange();

  // Calling GetConnectionType() at this very moment is likely to give
  // the wrong result, so we delay that until a little bit later.
  //
  // The one second delay chosen here was determined experimentally
  // by adamk on Windows 7.
  // If after one second we determine we are still offline, we will
  // delay again.
  offline_polls_ = 0;
  timer_.Start(FROM_HERE, base::Seconds(1), this,
               &NetworkChangeNotifierWin::NotifyParentOfConnectionTypeChange);
}

void NetworkChangeNotifierWin::WatchForAddressChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_watching_);

  // NotifyAddrChange occasionally fails with ERROR_OPEN_FAILED for unknown
  // reasons.  More rarely, it's also been observed failing with
  // ERROR_NO_SYSTEM_RESOURCES.  When either of these happens, we retry later.
  if (!WatchForAddressChangeInternal()) {
    ++sequential_failures_;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkChangeNotifierWin::WatchForAddressChange,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(kWatchForAddressChangeRetryIntervalMs));
    return;
  }

  // Treat the transition from NotifyAddrChange failing to succeeding as a
  // network change event, since network changes were not being observed in
  // that interval.
  if (sequential_failures_ > 0) {
    RecomputeCurrentConnectionTypeOnBlockingSequence(
        base::BindOnce(&NetworkChangeNotifierWin::NotifyObservers,
                       weak_factory_.GetWeakPtr()));
  }

  is_watching_ = true;
  sequential_failures_ = 0;
}

bool NetworkChangeNotifierWin::WatchForAddressChangeInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ResetEventIfSignaled(addr_overlapped_.hEvent);
  HANDLE handle = nullptr;
  DWORD ret = NotifyAddrChange(&handle, &addr_overlapped_);
  if (ret != ERROR_IO_PENDING)
    return false;

  addr_watcher_.StartWatchingOnce(addr_overlapped_.hEvent, this);
  return true;
}

void NetworkChangeNotifierWin::NotifyParentOfConnectionTypeChange() {
  RecomputeCurrentConnectionTypeOnBlockingSequence(base::BindOnce(
      &NetworkChangeNotifierWin::NotifyParentOfConnectionTypeChangeImpl,
      weak_factory_.GetWeakPtr()));
}

void NetworkChangeNotifierWin::NotifyParentOfConnectionTypeChangeImpl(
    ConnectionType connection_type) {
  SetCurrentConnectionType(connection_type);
  bool current_offline = IsOffline();
  offline_polls_++;
  // If we continue to appear offline, delay sending out the notification in
  // case we appear to go online within 20 seconds.  UMA histogram data shows
  // we may not detect the transition to online state after 1 second but within
  // 20 seconds we generally do.
  if (last_announced_offline_ && current_offline && offline_polls_ <= 20) {
    timer_.Start(FROM_HERE, base::Seconds(1), this,
                 &NetworkChangeNotifierWin::NotifyParentOfConnectionTypeChange);
    return;
  }
  if (last_announced_offline_)
    UMA_HISTOGRAM_CUSTOM_COUNTS("NCN.OfflinePolls", offline_polls_, 1, 50, 50);
  last_announced_offline_ = current_offline;

  NotifyObserversOfConnectionTypeChange();
  double max_bandwidth_mbps = 0.0;
  ConnectionType max_connection_type = CONNECTION_NONE;
  GetCurrentMaxBandwidthAndConnectionType(&max_bandwidth_mbps,
                                          &max_connection_type);
  NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps, max_connection_type);
}

}  // namespace net
