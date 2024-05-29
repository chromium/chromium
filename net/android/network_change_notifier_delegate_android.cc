// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/network_change_notifier_delegate_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_array.h"
#include "base/check.h"
#include "base/notreached.h"
#include "net/android/network_change_notifier_android.h"
#include "net/base/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/net_jni_headers/NetworkActiveNotifier_jni.h"
#include "net/net_jni_headers/NetworkChangeNotifier_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace net {

namespace {

// Converts a Java side connection type (integer) to
// the native side NetworkChangeNotifier::ConnectionType.
NetworkChangeNotifier::ConnectionType ConvertConnectionType(
    jint connection_type) {
  switch (connection_type) {
    case NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case NetworkChangeNotifier::CONNECTION_ETHERNET:
    case NetworkChangeNotifier::CONNECTION_WIFI:
    case NetworkChangeNotifier::CONNECTION_2G:
    case NetworkChangeNotifier::CONNECTION_3G:
    case NetworkChangeNotifier::CONNECTION_4G:
    case NetworkChangeNotifier::CONNECTION_5G:
    case NetworkChangeNotifier::CONNECTION_NONE:
    case NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown connection type received: " << connection_type;
      return NetworkChangeNotifier::CONNECTION_UNKNOWN;
  }
  return static_cast<NetworkChangeNotifier::ConnectionType>(connection_type);
}

// Converts a Java side connection cost (integer) to
// the native side NetworkChangeNotifier::ConnectionCost.
NetworkChangeNotifier::ConnectionCost ConvertConnectionCost(
    jint connection_cost) {
  switch (connection_cost) {
    case NetworkChangeNotifier::CONNECTION_COST_UNKNOWN:
    case NetworkChangeNotifier::CONNECTION_COST_UNMETERED:
    case NetworkChangeNotifier::CONNECTION_COST_METERED:
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown connection cost received: " << connection_cost;
      return NetworkChangeNotifier::CONNECTION_COST_UNKNOWN;
  }
  return static_cast<NetworkChangeNotifier::ConnectionCost>(connection_cost);
}

// Converts a Java side connection type (integer) to
// the native side NetworkChangeNotifier::ConnectionType.
NetworkChangeNotifier::ConnectionSubtype ConvertConnectionSubtype(
    jint subtype) {
  DCHECK(subtype >= 0 && subtype <= NetworkChangeNotifier::SUBTYPE_LAST);

  return static_cast<NetworkChangeNotifier::ConnectionSubtype>(subtype);
}

}  // namespace

// static
void NetworkChangeNotifierDelegateAndroid::JavaLongArrayToNetworkMap(
    JNIEnv* env,
    const JavaRef<jlongArray>& long_array,
    NetworkMap* network_map) {
  std::vector<int64_t> int64_list;
  base::android::JavaLongArrayToInt64Vector(env, long_array, &int64_list);
  network_map->clear();
  for (auto i = int64_list.begin(); i != int64_list.end(); ++i) {
    handles::NetworkHandle network_handle = *i;
    CHECK(++i != int64_list.end());
    (*network_map)[network_handle] = static_cast<ConnectionType>(*i);
  }
}

NetworkChangeNotifierDelegateAndroid::NetworkChangeNotifierDelegateAndroid()
    : java_network_change_notifier_(Java_NetworkChangeNotifier_init(
          base::android::AttachCurrentThread())),
      register_network_callback_failed_(
          Java_NetworkChangeNotifier_registerNetworkCallbackFailed(
              base::android::AttachCurrentThread(),
              java_network_change_notifier_)) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_addNativeObserver(
      env, java_network_change_notifier_, reinterpret_cast<intptr_t>(this));
  SetCurrentConnectionType(
      ConvertConnectionType(Java_NetworkChangeNotifier_getCurrentConnectionType(
          env, java_network_change_notifier_)));
  SetCurrentConnectionCost(
      ConvertConnectionCost(Java_NetworkChangeNotifier_getCurrentConnectionCost(
          env, java_network_change_notifier_)));
  auto connection_subtype = ConvertConnectionSubtype(
      Java_NetworkChangeNotifier_getCurrentConnectionSubtype(
          env, java_network_change_notifier_));
  SetCurrentConnectionSubtype(connection_subtype);
  SetCurrentMaxBandwidth(
      NetworkChangeNotifierAndroid::GetMaxBandwidthMbpsForConnectionSubtype(
          connection_subtype));
  SetCurrentDefaultNetwork(Java_NetworkChangeNotifier_getCurrentDefaultNetId(
      env, java_network_change_notifier_));
  NetworkMap network_map;
  ScopedJavaLocalRef<jlongArray> networks_and_types =
      Java_NetworkChangeNotifier_getCurrentNetworksAndTypes(
          env, java_network_change_notifier_);
  JavaLongArrayToNetworkMap(env, networks_and_types, &network_map);
  SetCurrentNetworksAndTypes(network_map);
  java_network_active_notifier_ = Java_NetworkActiveNotifier_build(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

NetworkChangeNotifierDelegateAndroid::~NetworkChangeNotifierDelegateAndroid() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(default_network_active_observers_, 0);
  {
    base::AutoLock auto_lock(observer_lock_);
    DCHECK(!observer_);
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_removeNativeObserver(
      env, java_network_change_notifier_, reinterpret_cast<intptr_t>(this));
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionType() const {
  base::AutoLock auto_lock(connection_lock_);
  return connection_type_;
}

NetworkChangeNotifier::ConnectionCost
NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionCost() {
  base::AutoLock auto_lock(connection_lock_);
  return connection_cost_;
}

NetworkChangeNotifier::ConnectionSubtype
NetworkChangeNotifierDelegateAndroid::GetCurrentConnectionSubtype() const {
  if (base::FeatureList::IsEnabled(net::features::kStoreConnectionSubtype)) {
    base::AutoLock auto_lock(connection_lock_);
    return connection_subtype_;
  }
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ConvertConnectionSubtype(
      Java_NetworkChangeNotifier_getCurrentConnectionSubtype(
          base::android::AttachCurrentThread(), java_network_change_notifier_));
}

void NetworkChangeNotifierDelegateAndroid::
    GetCurrentMaxBandwidthAndConnectionType(
        double* max_bandwidth_mbps,
        ConnectionType* connection_type) const {
  base::AutoLock auto_lock(connection_lock_);
  *connection_type = connection_type_;
  *max_bandwidth_mbps = connection_max_bandwidth_;
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierDelegateAndroid::GetNetworkConnectionType(
    handles::NetworkHandle network) const {
  base::AutoLock auto_lock(connection_lock_);
  auto network_entry = network_map_.find(network);
  if (network_entry == network_map_.end())
    return ConnectionType::CONNECTION_UNKNOWN;
  return network_entry->second;
}

handles::NetworkHandle
NetworkChangeNotifierDelegateAndroid::GetCurrentDefaultNetwork() const {
  base::AutoLock auto_lock(connection_lock_);
  return default_network_;
}

void NetworkChangeNotifierDelegateAndroid::GetCurrentlyConnectedNetworks(
    NetworkList* network_list) const {
  network_list->clear();
  base::AutoLock auto_lock(connection_lock_);
  for (auto i : network_map_)
    network_list->push_back(i.first);
}

bool NetworkChangeNotifierDelegateAndroid::IsDefaultNetworkActive() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_NetworkActiveNotifier_isDefaultNetworkActive(
      env, java_network_active_notifier_);
}

void NetworkChangeNotifierDelegateAndroid::NotifyConnectionCostChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint new_connection_cost) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const ConnectionCost actual_connection_cost =
      ConvertConnectionCost(new_connection_cost);
  SetCurrentConnectionCost(actual_connection_cost);
  base::AutoLock auto_lock(observer_lock_);
  if (observer_)
    observer_->OnConnectionCostChanged();
}

void NetworkChangeNotifierDelegateAndroid::NotifyConnectionTypeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint new_connection_type,
    jlong default_netid) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const ConnectionType actual_connection_type = ConvertConnectionType(
      new_connection_type);
  SetCurrentConnectionType(actual_connection_type);
  handles::NetworkHandle default_network = default_netid;
  if (default_network != GetCurrentDefaultNetwork()) {
    SetCurrentDefaultNetwork(default_network);
    bool default_exists;
    {
      base::AutoLock auto_lock(connection_lock_);
      // |default_network| may be an invalid value (i.e. -1) in cases where
      // the device is disconnected or when run on Android versions prior to L,
      // in which case |default_exists| will correctly be false and no
      // OnNetworkMadeDefault notification will be sent.
      default_exists = network_map_.find(default_network) != network_map_.end();
    }
    // Android Lollipop had race conditions where CONNECTIVITY_ACTION intents
    // were sent out before the network was actually made the default.
    // Delay sending the OnNetworkMadeDefault notification until we are
    // actually notified that the network connected in NotifyOfNetworkConnect.
    if (default_exists) {
      base::AutoLock auto_lock(observer_lock_);
      if (observer_)
        observer_->OnNetworkMadeDefault(default_network);
    }
  }

  base::AutoLock auto_lock(observer_lock_);
  if (observer_)
    observer_->OnConnectionTypeChanged();
}

jint NetworkChangeNotifierDelegateAndroid::GetConnectionType(JNIEnv*,
                                                             jobject) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCurrentConnectionType();
}

jint NetworkChangeNotifierDelegateAndroid::GetConnectionCost(JNIEnv*, jobject) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetCurrentConnectionCost();
}

void NetworkChangeNotifierDelegateAndroid::NotifyConnectionSubtypeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint subtype) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  double new_max_bandwidth =
      NetworkChangeNotifierAndroid::GetMaxBandwidthMbpsForConnectionSubtype(
          ConvertConnectionSubtype(subtype));
  SetCurrentConnectionSubtype(ConvertConnectionSubtype(subtype));
  SetCurrentMaxBandwidth(new_max_bandwidth);
  const ConnectionType connection_type = GetCurrentConnectionType();
  base::AutoLock auto_lock(observer_lock_);
  if (observer_) {
    observer_->OnMaxBandwidthChanged(new_max_bandwidth, connection_type);
  }
}

void NetworkChangeNotifierDelegateAndroid::NotifyOfNetworkConnect(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong net_id,
    jint connection_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  handles::NetworkHandle network = net_id;
  bool already_exists;
  bool is_default_network;
  {
    base::AutoLock auto_lock(connection_lock_);
    already_exists = network_map_.find(network) != network_map_.end();
    network_map_[network] = static_cast<ConnectionType>(connection_type);
    is_default_network = (network == default_network_);
  }
  // Android Lollipop would send many duplicate notifications.
  // This was later fixed in Android Marshmallow.
  // Deduplicate them here by avoiding sending duplicate notifications.
  if (!already_exists) {
    base::AutoLock auto_lock(observer_lock_);
    if (observer_) {
      observer_->OnNetworkConnected(network);
      if (is_default_network)
        observer_->OnNetworkMadeDefault(network);
    }
  }
}

void NetworkChangeNotifierDelegateAndroid::NotifyOfNetworkSoonToDisconnect(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong net_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  handles::NetworkHandle network = net_id;
  {
    base::AutoLock auto_lock(connection_lock_);
    if (network_map_.find(network) == network_map_.end())
      return;
  }
  base::AutoLock auto_lock(observer_lock_);
  if (observer_)
    observer_->OnNetworkSoonToDisconnect(network);
}

void NetworkChangeNotifierDelegateAndroid::NotifyOfNetworkDisconnect(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong net_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  handles::NetworkHandle network = net_id;
  {
    base::AutoLock auto_lock(connection_lock_);
    if (network == default_network_)
      default_network_ = handles::kInvalidNetworkHandle;
    if (network_map_.erase(network) == 0)
      return;
  }
  base::AutoLock auto_lock(observer_lock_);
  if (observer_)
    observer_->OnNetworkDisconnected(network);
}

void NetworkChangeNotifierDelegateAndroid::NotifyPurgeActiveNetworkList(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jlongArray>& active_networks) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkList active_network_list;
  base::android::JavaLongArrayToInt64Vector(env, active_networks,
                                            &active_network_list);
  NetworkList disconnected_networks;
  {
    base::AutoLock auto_lock(connection_lock_);
    for (auto i : network_map_) {
      bool found = false;
      for (auto j : active_network_list) {
        if (j == i.first) {
          found = true;
          break;
        }
      }
      if (!found) {
        disconnected_networks.push_back(i.first);
      }
    }
  }
  for (auto disconnected_network : disconnected_networks)
    NotifyOfNetworkDisconnect(env, obj, disconnected_network);
}

void NetworkChangeNotifierDelegateAndroid::NotifyOfDefaultNetworkActive(
    JNIEnv* env) {
  base::AutoLock auto_lock(observer_lock_);
  if (observer_)
    observer_->OnDefaultNetworkActive();
}

void NetworkChangeNotifierDelegateAndroid::RegisterObserver(
    Observer* observer) {
  base::AutoLock auto_lock(observer_lock_);
  DCHECK(!observer_);
  observer_ = observer;
}

void NetworkChangeNotifierDelegateAndroid::UnregisterObserver(
    Observer* observer) {
  base::AutoLock auto_lock(observer_lock_);
  DCHECK_EQ(observer_, observer);
  observer_ = nullptr;
}

void NetworkChangeNotifierDelegateAndroid::DefaultNetworkActiveObserverAdded() {
  if (default_network_active_observers_.fetch_add(1) == 0)
    EnableDefaultNetworkActiveNotifications();
}

void NetworkChangeNotifierDelegateAndroid::
    DefaultNetworkActiveObserverRemoved() {
  if (default_network_active_observers_.fetch_sub(1) == 1)
    DisableDefaultNetworkActiveNotifications();
}

void NetworkChangeNotifierDelegateAndroid::
    EnableDefaultNetworkActiveNotifications() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkActiveNotifier_enableNotifications(env,
                                                 java_network_active_notifier_);
}

void NetworkChangeNotifierDelegateAndroid::
    DisableDefaultNetworkActiveNotifications() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkActiveNotifier_disableNotifications(
      env, java_network_active_notifier_);
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentConnectionType(
    ConnectionType new_connection_type) {
  base::AutoLock auto_lock(connection_lock_);
  connection_type_ = new_connection_type;
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentConnectionSubtype(
    ConnectionSubtype new_connection_subtype) {
  base::AutoLock auto_lock(connection_lock_);
  connection_subtype_ = new_connection_subtype;
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentConnectionCost(
    ConnectionCost new_connection_cost) {
  base::AutoLock auto_lock(connection_lock_);
  connection_cost_ = new_connection_cost;
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentMaxBandwidth(
    double max_bandwidth) {
  base::AutoLock auto_lock(connection_lock_);
  connection_max_bandwidth_ = max_bandwidth;
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentDefaultNetwork(
    handles::NetworkHandle default_network) {
  base::AutoLock auto_lock(connection_lock_);
  default_network_ = default_network;
}

void NetworkChangeNotifierDelegateAndroid::SetCurrentNetworksAndTypes(
    NetworkMap network_map) {
  base::AutoLock auto_lock(connection_lock_);
  network_map_ = network_map;
}

void NetworkChangeNotifierDelegateAndroid::SetOnline() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, true);
}

void NetworkChangeNotifierDelegateAndroid::SetOffline() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_forceConnectivityState(env, false);
}

void NetworkChangeNotifierDelegateAndroid::FakeNetworkConnected(
    handles::NetworkHandle network,
    ConnectionType type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeNetworkConnected(env, network, type);
}

void NetworkChangeNotifierDelegateAndroid::FakeNetworkSoonToBeDisconnected(
    handles::NetworkHandle network) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeNetworkSoonToBeDisconnected(env, network);
}

void NetworkChangeNotifierDelegateAndroid::FakeNetworkDisconnected(
    handles::NetworkHandle network) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeNetworkDisconnected(env, network);
}

void NetworkChangeNotifierDelegateAndroid::FakePurgeActiveNetworkList(
    NetworkChangeNotifier::NetworkList networks) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakePurgeActiveNetworkList(
      env, base::android::ToJavaLongArray(env, networks));
}

void NetworkChangeNotifierDelegateAndroid::FakeDefaultNetwork(
    handles::NetworkHandle network,
    ConnectionType type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeDefaultNetwork(env, network, type);
}

void NetworkChangeNotifierDelegateAndroid::FakeConnectionCostChanged(
    ConnectionCost cost) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeConnectionCostChanged(env, cost);
}

void NetworkChangeNotifierDelegateAndroid::FakeConnectionSubtypeChanged(
    ConnectionSubtype subtype) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_fakeConnectionSubtypeChanged(env, subtype);
}

void NetworkChangeNotifierDelegateAndroid::FakeDefaultNetworkActive() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkActiveNotifier_fakeDefaultNetworkActive(
      env, java_network_active_notifier_);
}

void NetworkChangeNotifierDelegateAndroid::
    EnableNetworkChangeNotifierAutoDetectForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NetworkChangeNotifier_setAutoDetectConnectivityState(env, true);
}

}  // namespace net
