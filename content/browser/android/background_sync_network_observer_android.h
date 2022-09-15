// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_NETWORK_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_NETWORK_OBSERVER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// BackgroundSyncNetworkObserverAndroid is a specialized
// BackgroundSyncNetworkObserver which is backed by a NetworkConnectionTracker
// that listens for network events even when the browser is paused, unlike the
// standard NetworkConnectionTracker. This ensures that sync events can be fired
// even when the browser is backgrounded, and other network observers are
// disabled.
//
// Lives on the UI thread.
class BackgroundSyncNetworkObserverAndroid
    : public BackgroundSyncNetworkObserver {
 public:
  // Creates a BackgroundSyncNetworkObserver. |network_changed_callback| is
  // called via PostMessage when the network connection changes.
  BackgroundSyncNetworkObserverAndroid(
      base::RepeatingClosure network_changed_callback);

  ~BackgroundSyncNetworkObserverAndroid() override;

  // Creates and initializes the Observer (below) instance.
  void Init() override;

  // This class mediates all access to the Java BackgroundSyncNetworkObserver,
  // which it creates and owns. It is in turn owned by the
  // BackgroundSyncNetworkObserverAndroid.
  class Observer : public base::RefCounted<
                       BackgroundSyncNetworkObserverAndroid::Observer> {
   public:
    static scoped_refptr<BackgroundSyncNetworkObserverAndroid::Observer> Create(
        base::RepeatingCallback<void(network::mojom::ConnectionType)> callback);

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void Init();

    // Called from BackgroundSyncNetworkObserver.java over JNI whenever the
    // connection type changes. This updates the current connection type seen by
    // this class and calls the |network_changed_callback| provided to the
    // constructor with the new connection type.
    void NotifyConnectionTypeChanged(
        JNIEnv* env,
        const base::android::JavaParamRef<jobject>& jcaller,
        jint new_connection_type);

   private:
    friend class base::RefCounted<
        BackgroundSyncNetworkObserverAndroid::Observer>;

    explicit Observer(
        base::RepeatingCallback<void(network::mojom::ConnectionType)> callback);
    ~Observer();

    // This callback is run whenever the connection type changes.
    base::RepeatingCallback<void(network::mojom::ConnectionType)> callback_;
    base::android::ScopedJavaGlobalRef<jobject> j_observer_;
  };

 private:
  void RegisterWithNetworkConnectionTracker(
      network::NetworkConnectionTracker* network_connection_tracker) override;

  // Null until Init() is called.
  scoped_refptr<Observer> observer_;

  base::WeakPtrFactory<BackgroundSyncNetworkObserverAndroid> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_NETWORK_OBSERVER_ANDROID_H_
